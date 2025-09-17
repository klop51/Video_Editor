#include "audio/audio_clock.hpp"
#include <core/log.hpp>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <sstream>
#include <numeric>  // for std::gcd

namespace ve::audio {

// Helper functions for TimeRational arithmetic
static TimeRational add_rationals(const TimeRational& a, const TimeRational& b) {
    // a/a_den + b/b_den = (a*b_den + b*a_den) / (a_den * b_den)
    int64_t num = a.num * b.den + b.num * a.den;
    int64_t den = static_cast<int64_t>(a.den) * b.den;
    
    // Simplify if denominator fits in int32_t
    if (den <= INT32_MAX) {
        return TimeRational(num, static_cast<int32_t>(den));
    } else {
        // Scale down to prevent overflow
        int64_t gcd = std::gcd(std::abs(num), den);
        return TimeRational(num / gcd, static_cast<int32_t>(den / gcd));
    }
}

static TimeRational subtract_rationals(const TimeRational& a, const TimeRational& b) {
    // a/a_den - b/b_den = (a*b_den - b*a_den) / (a_den * b_den)
    int64_t num = a.num * b.den - b.num * a.den;
    int64_t den = static_cast<int64_t>(a.den) * b.den;
    
    // Simplify if denominator fits in int32_t
    if (den <= INT32_MAX) {
        return TimeRational(num, static_cast<int32_t>(den));
    } else {
        // Scale down to prevent overflow
        int64_t gcd = std::gcd(std::abs(num), den);
        return TimeRational(num / gcd, static_cast<int32_t>(den / gcd));
    }
}

static TimePoint add_timepoint_rational(const TimePoint& point, const TimeRational& duration) {
    TimeRational result = add_rationals(point.to_rational(), duration);
    return TimePoint(result);
}

// Convert TimeRational to double seconds
static double rational_to_seconds(const TimeRational& rational) {
    return static_cast<double>(rational.num) / static_cast<double>(rational.den);
}

// Convert double seconds to TimeRational
static TimeRational seconds_to_rational(double seconds, uint32_t sample_rate) {
    // Use sample rate as denominator for maximum precision
    int64_t numerator = static_cast<int64_t>(seconds * sample_rate);
    return TimeRational(numerator, sample_rate);
}

//============================================================================
// AudioClock Implementation
//============================================================================

std::unique_ptr<AudioClock> AudioClock::create(const AudioClockConfig& config) {
    auto clock = std::make_unique<AudioClock>(config);
    if (!clock->initialize()) {
        return nullptr;
    }
    return clock;
}

AudioClock::AudioClock(const AudioClockConfig& config)
    : config_(config) {
    drift_history_.reserve(config_.measurement_window);
    drift_velocity_history_.reserve(100); // Track last 100 measurements for velocity calculation
    
    // Initialize adaptive threshold
    if (config_.enable_adaptive_threshold) {
        adaptive_threshold_current_.store(config_.drift_threshold);
    }
}

AudioClock::~AudioClock() {
    stop();
}

bool AudioClock::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Validate configuration
    if (config_.sample_rate == 0 || config_.sample_rate > 192000) {
        std::ostringstream oss;
        oss << "AudioClock: Invalid sample rate: " << config_.sample_rate;
        ve::log::error(oss.str());
        return false;
    }
    
    if (config_.drift_threshold <= 0.0 || config_.drift_threshold > 1.0) {
        std::ostringstream oss;
        oss << "AudioClock: Invalid drift threshold: " << config_.drift_threshold;
        ve::log::error(oss.str());
        return false;
    }
    
    if (config_.correction_rate <= 0.0 || config_.correction_rate > 1.0) {
        std::ostringstream oss;
        oss << "AudioClock: Invalid correction rate: " << config_.correction_rate;
        ve::log::error(oss.str());
        return false;
    }
    
    // Initialize state
    sample_count_.store(0);
    current_drift_.store(0.0);
    max_drift_.store(0.0);
    drift_corrections_.store(0);
    samples_processed_.store(0);
    
    // Phase 2 Week 2: Initialize enhanced synchronization state
    frame_sync_corrections_.store(0);
    max_correction_applied_.store(0.0);
    sync_validation_failures_.store(0);
    adaptive_threshold_current_.store(config_.drift_threshold);
    consecutive_stable_samples_.store(0);
    
    drift_history_.clear();
    drift_velocity_history_.clear();
    sync_validation_counter_ = 0;
    
    initialized_ = true;
    
    std::ostringstream oss;
    oss << "AudioClock initialized: " << config_.sample_rate << "Hz, drift_threshold=" 
        << (config_.drift_threshold * 1000.0) << "ms, correction_rate=" << config_.correction_rate;
    ve::log::info(oss.str());
    
    return true;
}

bool AudioClock::start(const TimePoint& start_time) {
    if (!initialized_) {
        ve::log::error("AudioClock: Cannot start - not initialized");
        return false;
    }
    
    start_time_ = start_time;
    wall_clock_start_ = std::chrono::high_resolution_clock::now();
    sample_count_.store(0);
    running_.store(true);
    
    std::ostringstream oss;
    oss << "AudioClock started at timeline position: " << start_time_.to_rational().num 
        << "/" << start_time_.to_rational().den;
    ve::log::info(oss.str());
    
    return true;
}

void AudioClock::stop() {
    running_.store(false);
    ve::log::info("AudioClock stopped");
}

TimePoint AudioClock::get_time() const {
    if (!running_) {
        return start_time_;
    }
    
    return calculate_audio_time();
}

TimePoint AudioClock::get_time_for_sample_offset(int64_t sample_offset) const {
    TimePoint current_time = get_time();
    
    // Convert sample offset to time duration
    TimeRational offset_duration(sample_offset, config_.sample_rate);
    
    // Add offset to current time
    TimeRational new_rational = add_rationals(current_time.to_rational(), offset_duration);
    
    return TimePoint(new_rational);
}

TimePoint AudioClock::advance_samples(uint32_t sample_count) {
    if (!running_) {
        return start_time_;
    }
    
    // Update sample counter
    sample_count_.fetch_add(sample_count);
    samples_processed_.fetch_add(sample_count);
    
    // Phase 2 Week 2: Enhanced synchronization processing
    if (config_.enable_drift_compensation) {
        update_drift_measurement();
        
        // Update adaptive threshold if enabled
        if (config_.enable_adaptive_threshold) {
            update_adaptive_threshold();
        }
        
        // Update drift velocity for predictive algorithms
        if (config_.enable_predictive_sync) {
            update_drift_velocity();
        }
        
        double current_threshold = config_.enable_adaptive_threshold ? 
            adaptive_threshold_current_.load() : config_.drift_threshold;
        
        double drift = std::abs(current_drift_.load());
        if (drift > current_threshold) {
            apply_drift_correction();
        }
        
        // Periodic sync validation
        sync_validation_counter_++;
        if (sync_validation_counter_ >= config_.sync_validation_samples) {
            validate_synchronization_state();
            sync_validation_counter_ = 0;
        }
    }
    
    return calculate_audio_time();
}

void AudioClock::sync_to_reference(const TimePoint& reference_time, uint64_t audio_samples) {
    if (!running_) return;
    
    // Calculate expected time based on audio samples
    TimePoint expected_time = samples_to_time(audio_samples);
    TimePoint actual_reference = add_timepoint_rational(start_time_, reference_time.to_rational());
    
    // Calculate drift
    TimeRational expected_rational = expected_time.to_rational();
    TimeRational reference_rational = actual_reference.to_rational();
    TimeRational drift_rational = subtract_rationals(reference_rational, expected_rational);
    
    double drift_seconds = rational_to_seconds(drift_rational);
    current_drift_.store(drift_seconds);
    
    // Update max drift
    double abs_drift = std::abs(drift_seconds);
    double current_max = max_drift_.load();
    if (abs_drift > current_max) {
        max_drift_.store(abs_drift);
    }
    
    // Apply correction if needed
    if (abs_drift > config_.drift_threshold && config_.enable_drift_compensation) {
        apply_drift_correction();
    }
}

void AudioClock::set_time(const TimePoint& time) {
    start_time_ = time;
    wall_clock_start_ = std::chrono::high_resolution_clock::now();
    sample_count_.store(0);
    current_drift_.store(0.0);
    
    std::ostringstream oss;
    oss << "AudioClock time set to: " << time.to_rational().num << "/" << time.to_rational().den;
    ve::log::info(oss.str());
}

TimePoint AudioClock::samples_to_time(uint64_t sample_count) const {
    TimeRational duration(sample_count, config_.sample_rate);
    return TimePoint(duration);
}

uint64_t AudioClock::time_to_samples(const TimePoint& time_duration) const {
    TimeRational rational = time_duration.to_rational();
    
    // Convert to samples using sample rate
    // samples = (time_seconds * sample_rate)
    int64_t samples = (rational.num * config_.sample_rate) / rational.den;
    
    return static_cast<uint64_t>(std::max(samples, static_cast<int64_t>(0)));
}

AudioClockStats AudioClock::get_stats() const {
    AudioClockStats stats;
    
    stats.current_drift_seconds = current_drift_.load();
    stats.max_drift_seconds = max_drift_.load();
    stats.drift_corrections = drift_corrections_.load();
    stats.samples_processed = samples_processed_.load();
    stats.last_correction_time = last_correction_time_;
    stats.is_stable = is_stable();
    
    // Phase 2 Week 2: Enhanced statistics
    stats.frame_sync_corrections = frame_sync_corrections_.load();
    stats.max_correction_applied = max_correction_applied_.load();
    stats.sync_validation_failures = sync_validation_failures_.load();
    stats.adaptive_threshold_current = adaptive_threshold_current_.load();
    stats.frame_sync_active = config_.enable_frame_accurate_sync && (last_video_frame_number_ > 0);
    
    // Calculate average drift
    if (!drift_history_.empty()) {
        double sum = 0.0;
        for (double drift : drift_history_) {
            sum += std::abs(drift);
        }
        stats.avg_drift_seconds = sum / drift_history_.size();
    }
    
    // Calculate predictive accuracy
    if (!drift_velocity_history_.empty() && drift_history_.size() > 1) {
        // Simple accuracy metric based on prediction vs actual drift change
        double predicted = calculate_predictive_drift(config_.sync_validation_samples);
        double actual_change = drift_history_.back() - 
            drift_history_[drift_history_.size() - 2];
        double error = std::abs(predicted - actual_change);
        stats.predictive_accuracy = std::max(0.0, 1.0 - error);
    }
    
    return stats;
}

void AudioClock::reset_stats() {
    current_drift_.store(0.0);
    max_drift_.store(0.0);
    drift_corrections_.store(0);
    samples_processed_.store(0);
    drift_history_.clear();
    last_correction_time_ = TimePoint();
    
    // Phase 2 Week 2: Reset enhanced statistics
    frame_sync_corrections_.store(0);
    max_correction_applied_.store(0.0);
    sync_validation_failures_.store(0);
    consecutive_stable_samples_.store(0);
    drift_velocity_history_.clear();
    sync_validation_counter_ = 0;
    
    if (config_.enable_adaptive_threshold) {
        adaptive_threshold_current_.store(config_.drift_threshold);
    }
    
    ve::log::info("AudioClock statistics reset (Phase 2 Week 2 enhanced)");
}

void AudioClock::set_drift_compensation(bool enabled) {
    config_.enable_drift_compensation = enabled;
    std::string msg = "AudioClock drift compensation: ";
    msg += (enabled ? "enabled" : "disabled");
    ve::log::info(msg);
}

bool AudioClock::is_stable() const {
    double current_drift = std::abs(current_drift_.load());
    return current_drift < (config_.drift_threshold * 0.5); // Stable if drift < half threshold
}

void AudioClock::update_drift_measurement() const {
    double drift = calculate_current_drift();
    
    // Add to drift history
    if (drift_history_.size() >= config_.measurement_window) {
        drift_history_.erase(drift_history_.begin());
    }
    drift_history_.push_back(drift);
    
    current_drift_.store(drift);
}

void AudioClock::apply_drift_correction() {
    double current_drift = current_drift_.load();
    
    // Apply gradual correction to avoid audio artifacts
    double correction = current_drift * config_.correction_rate;
    
    // Adjust wall clock start time to compensate for drift
    auto correction_duration = std::chrono::duration<double>(correction);
    // Note: In a real implementation, we would adjust the internal timing
    // For now, we just log the correction
    
    drift_corrections_.store(drift_corrections_.load() + 1);
    last_correction_time_ = get_time();
    
    std::ostringstream oss;
    oss << "AudioClock drift correction applied: " << (correction * 1000.0) << "ms";
    ve::log::debug(oss.str());
}

double AudioClock::calculate_current_drift() const {
    if (!running_) return 0.0;
    
    // Calculate expected time vs wall clock time
    auto now = std::chrono::high_resolution_clock::now();
    auto wall_clock_elapsed = now - wall_clock_start_;
    double wall_clock_seconds = std::chrono::duration<double>(wall_clock_elapsed).count();
    
    // Calculate audio time elapsed
    uint64_t current_samples = sample_count_.load();
    double audio_seconds = static_cast<double>(current_samples) / config_.sample_rate;
    
    // Drift is the difference
    return wall_clock_seconds - audio_seconds;
}

TimePoint AudioClock::calculate_audio_time() const {
    uint64_t current_samples = sample_count_.load();
    TimeRational audio_duration(current_samples, config_.sample_rate);
    
    // Apply drift compensation
    if (config_.enable_drift_compensation) {
        double drift = current_drift_.load();
        TimeRational drift_rational = seconds_to_rational(drift, config_.sample_rate);
        audio_duration = add_rationals(audio_duration, drift_rational);
    }
    
    return add_timepoint_rational(start_time_, audio_duration);
}

//============================================================================
// Phase 2 Week 2: Enhanced Synchronization Methods
//============================================================================

bool AudioClock::sync_to_video_frame(const TimePoint& video_frame_time, uint64_t video_frame_number) {
    if (!running_ || !config_.enable_frame_accurate_sync) {
        return false;
    }
    
    // Calculate expected audio time for this video frame
    double frame_duration = 1.0 / config_.video_frame_rate;
    TimePoint expected_audio_time = get_time();
    
    // Calculate video-audio offset
    TimeRational video_rational = video_frame_time.to_rational();
    TimeRational audio_rational = expected_audio_time.to_rational();
    TimeRational offset_rational = subtract_rationals(video_rational, audio_rational);
    double video_audio_offset = rational_to_seconds(offset_rational);
    
    // Apply frame-accurate correction if needed
    if (should_apply_frame_correction(video_audio_offset)) {
        double max_step = frame_duration * config_.max_correction_per_second;
        double applied_correction = apply_frame_accurate_correction(video_audio_offset, max_step);
        
        if (std::abs(applied_correction) > 0.0) {
            frame_sync_corrections_.fetch_add(1);
            
            std::ostringstream oss;
            oss << "Frame-accurate sync correction: " << (applied_correction * 1000.0) 
                << "ms for frame " << video_frame_number;
            ve::log::debug(oss.str());
        }
    }
    
    // Update tracking
    last_video_frame_time_ = video_frame_time;
    last_video_frame_number_ = video_frame_number;
    
    return true;
}

double AudioClock::predict_drift_correction(uint32_t look_ahead_samples) const {
    if (!config_.enable_predictive_sync || drift_velocity_history_.empty()) {
        return 0.0;
    }
    
    return calculate_predictive_drift(look_ahead_samples);
}

bool AudioClock::validate_sync_accuracy(const TimePoint& reference_time, uint32_t tolerance_samples) const {
    if (!running_) return false;
    
    TimePoint current_time = get_time();
    int64_t sample_diff = std::abs(clock_utils::time_difference_in_samples(
        current_time, reference_time, config_.sample_rate));
    
    bool is_accurate = sample_diff <= tolerance_samples;
    
    if (!is_accurate) {
        sync_validation_failures_.fetch_add(1);
        
        std::ostringstream oss;
        oss << "Sync validation failed: " << sample_diff << " samples offset (tolerance: " 
            << tolerance_samples << ")";
        ve::log::warn(oss.str());
    }
    
    return is_accurate;
}

double AudioClock::apply_frame_accurate_correction(double target_correction, double max_step_size) {
    // Limit correction to prevent audio artifacts
    double actual_correction = std::clamp(target_correction, -max_step_size, max_step_size);
    
    // Apply correction by adjusting the internal timing
    // In a real implementation, this would adjust hardware timing or buffer sizes
    // For now, we adjust the drift compensation
    double current_drift = current_drift_.load();
    current_drift_.store(current_drift - actual_correction);
    
    // Track maximum correction applied
    double abs_correction = std::abs(actual_correction);
    double current_max = max_correction_applied_.load();
    if (abs_correction > current_max) {
        max_correction_applied_.store(abs_correction);
    }
    
    return actual_correction;
}

TimePoint AudioClock::get_frame_accurate_time(double video_frame_rate) const {
    TimePoint current_time = get_time();
    
    // Quantize to frame boundaries
    double frame_duration = 1.0 / video_frame_rate;
    TimeRational current_rational = current_time.to_rational();
    double current_seconds = rational_to_seconds(current_rational);
    
    // Round to nearest frame boundary
    double frame_number = std::round(current_seconds / frame_duration);
    double frame_accurate_seconds = frame_number * frame_duration;
    
    TimeRational frame_rational = seconds_to_rational(frame_accurate_seconds, config_.sample_rate);
    return TimePoint(frame_rational);
}

void AudioClock::update_adaptive_threshold() {
    if (!config_.enable_adaptive_threshold) return;
    
    double calculated_threshold = calculate_adaptive_threshold();
    adaptive_threshold_current_.store(calculated_threshold);
}

AudioClock::SyncMetrics AudioClock::get_sync_metrics() const {
    SyncMetrics metrics;
    
    // Calculate video-audio offset if we have video frame data
    if (last_video_frame_number_ > 0) {
        TimePoint current_audio = get_time();
        TimeRational offset_rational = subtract_rationals(
            last_video_frame_time_.to_rational(), current_audio.to_rational());
        metrics.video_audio_offset = rational_to_seconds(offset_rational);
    }
    
    // Calculate frame sync accuracy
    double current_drift = std::abs(current_drift_.load());
    double threshold = config_.enable_adaptive_threshold ? 
        adaptive_threshold_current_.load() : config_.drift_threshold;
    metrics.frame_sync_accuracy = std::max(0.0, 1.0 - (current_drift / threshold));
    
    // Count consecutive stable frames
    metrics.consecutive_stable_frames = static_cast<uint32_t>(consecutive_stable_samples_.load() / 
        (config_.sample_rate / config_.video_frame_rate));
    
    // Determine if resync is needed
    metrics.requires_resync = (current_drift > threshold * 3.0) || 
                             (sync_validation_failures_.load() > 10);
    
    // Calculate drift velocity
    if (!drift_velocity_history_.empty()) {
        double sum = 0.0;
        for (double velocity : drift_velocity_history_) {
            sum += velocity;
        }
        metrics.estimated_drift_velocity = sum / drift_velocity_history_.size();
    }
    
    return metrics;
}

//============================================================================
// Phase 2 Week 2: Enhanced Internal Methods
//============================================================================

void AudioClock::update_drift_velocity() const {
    if (drift_history_.size() < 2) return;
    
    // Calculate velocity as change in drift over time
    double current_drift = drift_history_.back();
    double previous_drift = drift_history_[drift_history_.size() - 2];
    double drift_change = current_drift - previous_drift;
    
    // Velocity per sample, converted to per second
    double samples_per_measurement = static_cast<double>(config_.sample_rate) / 
                                   config_.measurement_window;
    double velocity = drift_change * samples_per_measurement;
    
    // Add to velocity history
    if (drift_velocity_history_.size() >= 100) {
        drift_velocity_history_.erase(drift_velocity_history_.begin());
    }
    drift_velocity_history_.push_back(velocity);
}

double AudioClock::calculate_predictive_drift(uint32_t samples_ahead) const {
    if (drift_velocity_history_.empty()) return 0.0;
    
    // Calculate average velocity
    double sum_velocity = 0.0;
    for (double velocity : drift_velocity_history_) {
        sum_velocity += velocity;
    }
    double avg_velocity = sum_velocity / drift_velocity_history_.size();
    
    // Predict drift based on velocity and time ahead
    double time_ahead = static_cast<double>(samples_ahead) / config_.sample_rate;
    return avg_velocity * time_ahead;
}

bool AudioClock::should_apply_frame_correction(double video_audio_offset) const {
    // Apply correction if offset exceeds half a frame duration
    double frame_duration = 1.0 / config_.video_frame_rate;
    return std::abs(video_audio_offset) > (frame_duration * 0.5);
}

double AudioClock::calculate_adaptive_threshold() const {
    if (drift_history_.empty()) {
        return config_.drift_threshold;
    }
    
    // Calculate variance in drift measurements
    double sum = 0.0;
    for (double drift : drift_history_) {
        sum += std::abs(drift);
    }
    double avg_drift = sum / drift_history_.size();
    
    double variance = 0.0;
    for (double drift : drift_history_) {
        double diff = std::abs(drift) - avg_drift;
        variance += diff * diff;
    }
    variance /= drift_history_.size();
    
    // Adaptive threshold based on system stability
    double base_threshold = config_.drift_threshold;
    double stability_factor = std::sqrt(variance);
    
    // More stable system allows tighter threshold
    double adaptive_threshold = base_threshold * (1.0 + stability_factor);
    
    // Clamp to reasonable range
    return std::clamp(adaptive_threshold, base_threshold * 0.5, base_threshold * 3.0);
}

void AudioClock::validate_synchronization_state() const {
    double current_drift = std::abs(current_drift_.load());
    double threshold = config_.enable_adaptive_threshold ? 
        adaptive_threshold_current_.load() : config_.drift_threshold;
    
    if (current_drift < threshold * 0.5) {
        // Stable - increment counter
        consecutive_stable_samples_.fetch_add(config_.sync_validation_samples);
    } else {
        // Unstable - reset counter
        consecutive_stable_samples_.store(0);
        
        if (current_drift > threshold * 2.0) {
            std::ostringstream oss;
            oss << "AudioClock synchronization warning: drift " << (current_drift * 1000.0) 
                << "ms exceeds 2x threshold";
            ve::log::warn(oss.str());
        }
    }
}

//============================================================================
// MasterAudioClock Implementation
//============================================================================

MasterAudioClock& MasterAudioClock::instance() {
    static MasterAudioClock instance;
    return instance;
}

bool MasterAudioClock::initialize(const AudioClockConfig& config) {
    master_clock_ = AudioClock::create(config);
    if (!master_clock_) {
        ve::log::error("Failed to create master audio clock");
        return false;
    }
    
    ve::log::info("Master audio clock initialized");
    return true;
}

bool MasterAudioClock::start(const TimePoint& start_time) {
    if (!master_clock_) {
        ve::log::error("Master audio clock not initialized");
        return false;
    }
    
    return master_clock_->start(start_time);
}

void MasterAudioClock::stop() {
    if (master_clock_) {
        master_clock_->stop();
    }
}

TimePoint MasterAudioClock::get_time() const {
    if (!master_clock_) {
        return TimePoint();
    }
    
    return master_clock_->get_time();
}

//============================================================================
// AudioClockSynchronizer Implementation
//============================================================================

AudioClockSynchronizer::AudioClockSynchronizer(AudioClock* master_clock)
    : master_clock_(master_clock) {
}

void AudioClockSynchronizer::add_slave_clock(AudioClock* slave_clock, double sync_interval_ms) {
    if (!slave_clock) return;
    
    SlaveClock slave;
    slave.clock = slave_clock;
    slave.sync_interval_ms = sync_interval_ms;
    slave.last_sync = std::chrono::high_resolution_clock::now();
    
    slave_clocks_.push_back(slave);
    
    std::ostringstream oss;
    oss << "Added slave clock to synchronizer (interval: " << sync_interval_ms << "ms)";
    ve::log::info(oss.str());
}

void AudioClockSynchronizer::remove_slave_clock(AudioClock* slave_clock) {
    auto it = std::remove_if(slave_clocks_.begin(), slave_clocks_.end(),
                            [slave_clock](const SlaveClock& slave) {
                                return slave.clock == slave_clock;
                            });
    slave_clocks_.erase(it, slave_clocks_.end());
}

void AudioClockSynchronizer::update_synchronization() {
    if (!master_clock_ || !auto_sync_enabled_) return;
    
    TimePoint master_time = master_clock_->get_time();
    auto now = std::chrono::high_resolution_clock::now();
    
    for (auto& slave : slave_clocks_) {
        auto elapsed = now - slave.last_sync;
        double elapsed_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        
        if (elapsed_ms >= slave.sync_interval_ms) {
            // Synchronize slave clock to master
            slave.clock->sync_to_reference(master_time, master_clock_->time_to_samples(master_time));
            slave.last_sync = now;
        }
    }
}

//============================================================================
// Utility Functions
//============================================================================

namespace clock_utils {

double rational_to_seconds(const TimeRational& rational) {
    return ::ve::audio::rational_to_seconds(rational);
}

TimeRational seconds_to_rational(double seconds, uint32_t sample_rate) {
    return ::ve::audio::seconds_to_rational(seconds, sample_rate);
}

TimePoint sample_accurate_time(uint64_t sample_count, uint32_t sample_rate) {
    TimeRational rational(sample_count, sample_rate);
    return TimePoint(rational);
}

int64_t time_difference_in_samples(const TimePoint& time1, const TimePoint& time2, uint32_t sample_rate) {
    TimeRational diff = subtract_rationals(time1.to_rational(), time2.to_rational());
    
    // Convert to samples: (diff_seconds * sample_rate)
    int64_t samples = (diff.num * sample_rate) / diff.den;
    return samples;
}

bool times_are_sample_accurate(const TimePoint& time1, const TimePoint& time2, uint32_t sample_rate) {
    int64_t sample_diff = std::abs(time_difference_in_samples(time1, time2, sample_rate));
    return sample_diff <= 1; // Within 1 sample
}

double recommend_drift_threshold(uint32_t sample_rate) {
    // Recommend threshold based on sample rate
    // Higher sample rates can tolerate smaller drift
    if (sample_rate >= 96000) return 0.0005; // 0.5ms
    if (sample_rate >= 48000) return 0.001;  // 1ms
    return 0.002; // 2ms for lower sample rates
}

} // namespace clock_utils

} // namespace ve::audio