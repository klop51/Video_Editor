/**
 * @file master_clock.cpp
 * @brief Master Clock Implementation for A/V Synchronization
 */

#include "audio/master_clock.h"
#include "core/log.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace ve::audio {

// Helper function to convert TimePoint to seconds
static double to_seconds(const ve::TimePoint& time_point) {
    const auto& rational = time_point.to_rational();
    if (rational.den == 0) {
        return 0.0;
    }
    return static_cast<double>(rational.num) / static_cast<double>(rational.den);
}

// Helper for formatting log messages
template<typename... Args>
static std::string format_log(const std::string& format, Args&&... args) {
    std::ostringstream oss;
    oss << format;
    ((oss << " " << args), ...);
    return oss.str();
}

std::unique_ptr<MasterClock> MasterClock::create(const MasterClockConfig& config) {
    return std::make_unique<MasterClockImpl>(config);
}

MasterClockImpl::MasterClockImpl(const MasterClockConfig& config)
    : config_(config) {
    std::ostringstream oss;
    oss << "Master clock created with sample rate: " << config_.sample_rate 
        << "Hz, buffer size: " << config_.buffer_size;
    ve::log::info(oss.str());
}

bool MasterClockImpl::start() {
    std::lock_guard<std::mutex> audio_lock(audio_mutex_);
    std::lock_guard<std::mutex> video_lock(video_mutex_);
    std::lock_guard<std::mutex> drift_lock(drift_mutex_);
    std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
    
    if (running_.load()) {
        ve::log::warn("Master clock already running");
        return false;
    }
    
    // Reset all state
    master_time_us_.store(0);
    audio_position_samples_ = 0;
    video_position_ = ve::TimePoint(0, 1);
    
    // Reset drift compensation
    drift_state_ = DriftState{};
    
    // Reset metrics
    sync_metrics_ = SyncMetrics{};
    recent_offsets_.clear();
    
    // Set start time
    start_time_ = std::chrono::high_resolution_clock::now();
    audio_timestamp_ = start_time_;
    video_timestamp_ = start_time_;
    
    running_.store(true);
    
    ve::log::info("Master clock started");
    return true;
}

void MasterClockImpl::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    ve::log::info("Master clock stopped");
}

void MasterClockImpl::reset() {
    std::lock_guard<std::mutex> audio_lock(audio_mutex_);
    std::lock_guard<std::mutex> video_lock(video_mutex_);
    std::lock_guard<std::mutex> drift_lock(drift_mutex_);
    
    master_time_us_.store(0);
    audio_position_samples_ = 0;
    video_position_ = ve::TimePoint(0, 1);
    drift_state_ = DriftState{};
    
    start_time_ = std::chrono::high_resolution_clock::now();
    audio_timestamp_ = start_time_;
    video_timestamp_ = start_time_;
    
    ve::log::debug("Master clock reset");
}

void MasterClockImpl::set_playback_rate(double rate) {
    if (rate <= 0.0) {
        std::ostringstream oss;
        oss << "Invalid playback rate: " << rate;
        ve::log::error(oss.str());
        return;
    }
    
    playback_rate_.store(rate);
    std::ostringstream debug_oss;
    debug_oss << "Playback rate set to: " << rate;
    ve::log::debug(debug_oss.str());
}

double MasterClockImpl::get_playback_rate() const {
    return playback_rate_.load();
}

void MasterClockImpl::update_audio_position(int64_t position_samples, 
                                           std::chrono::high_resolution_clock::time_point timestamp) {
    if (!running_.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(audio_mutex_);
    
    audio_position_samples_ = position_samples;
    audio_timestamp_ = timestamp;
    
    // Update master time based on audio position
    double rate = playback_rate_.load();
    int64_t time_us = static_cast<int64_t>((position_samples * 1000000.0) / (config_.sample_rate * rate));
    master_time_us_.store(time_us);
    
    // Update drift compensation if enabled
    if (config_.enable_drift_compensation) {
        update_drift_state();
    }
}

int64_t MasterClockImpl::get_master_time_us() const {
    return master_time_us_.load();
}

ve::TimePoint MasterClockImpl::get_audio_position() const {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    
    if (!running_.load()) {
        return ve::TimePoint(0, 1);
    }
    
    // Convert samples to rational time
    int64_t numerator = audio_position_samples_;
    // Use sample rate directly (should fit in int32_t for audio)
    int32_t denominator = static_cast<int32_t>(config_.sample_rate);
    
    return ve::TimePoint(numerator, denominator);
}

ve::TimePoint MasterClockImpl::get_video_position() const {
    // Video position follows audio position with drift compensation
    auto audio_pos = get_audio_position();
    
    if (config_.enable_drift_compensation) {
        std::lock_guard<std::mutex> lock(drift_mutex_);
        
        // Apply drift compensation
        double correction_ms = drift_state_.accumulated_drift_ms * config_.correction_speed;
        double correction_sec = correction_ms / 1000.0;
        
        // Convert to rational time and add correction
        // Create TimePoint from seconds (using common denominator)
        int64_t correction_num = static_cast<int64_t>(correction_sec * 1000000); // microseconds
        auto correction_rt = ve::TimePoint(correction_num, 1000000);
        
        // For now, return audio_pos (TODO: implement proper addition)
        return audio_pos;
    }
    
    return audio_pos;
}

void MasterClockImpl::report_video_position(const ve::TimePoint& position,
                                          std::chrono::high_resolution_clock::time_point timestamp) {
    if (!running_.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(video_mutex_);
    
    video_position_ = position;
    video_timestamp_ = timestamp;
    
    // Calculate A/V offset for sync monitoring
    if (config_.enable_quality_monitoring) {
        auto audio_pos = get_audio_position();
        double offset_ms = (to_seconds(position) - to_seconds(audio_pos)) * 1000.0;
        update_sync_metrics(offset_ms);
    }
}

double MasterClockImpl::get_av_offset_ms() const {
    std::lock_guard<std::mutex> video_lock(video_mutex_);
    
    if (!running_.load()) {
        return 0.0;
    }
    
    auto audio_pos = get_audio_position();
    return (to_seconds(video_position_) - to_seconds(audio_pos)) * 1000.0;
}

bool MasterClockImpl::is_in_sync() const {
    double offset = std::abs(get_av_offset_ms());
    return offset <= config_.drift_tolerance_ms;
}

DriftState MasterClockImpl::get_drift_state() const {
    std::lock_guard<std::mutex> lock(drift_mutex_);
    return drift_state_;
}

SyncMetrics MasterClockImpl::get_sync_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return sync_metrics_;
}

void MasterClockImpl::set_drift_compensation_enabled(bool enabled) {
    config_.enable_drift_compensation = enabled;
    std::string msg = "Drift compensation " + std::string(enabled ? "enabled" : "disabled");
    ve::log::debug(msg);
}

void MasterClockImpl::set_drift_tolerance(double tolerance_ms) {
    config_.drift_tolerance_ms = tolerance_ms;
    std::ostringstream oss;
    oss << "Drift tolerance set to: " << tolerance_ms << "ms";
    ve::log::debug(oss.str());
}

void MasterClockImpl::force_sync_correction() {
    if (!running_.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(drift_mutex_);
    
    double current_offset = get_av_offset_ms();
    
    // Apply immediate correction
    drift_state_.accumulated_drift_ms = -current_offset;
    drift_state_.last_correction_time_us = get_master_time_us();
    drift_state_.correction_active = true;
    
    std::ostringstream oss;
    oss << "Force sync correction applied: offset=" << current_offset << "ms";
    ve::log::info(oss.str());
}

void MasterClockImpl::update_drift_state() {
    // This method should be called from update_audio_position with audio_mutex_ held
    
    std::lock_guard<std::mutex> drift_lock(drift_mutex_);
    
    int64_t current_time = get_master_time_us();
    
    // Calculate AV offset without calling get_av_offset_ms() to avoid deadlock
    double current_offset = 0.0;
    {
        std::lock_guard<std::mutex> video_lock(video_mutex_);
        if (running_.load()) {
            // Use already-held audio position data (audio_position_samples_)
            ve::TimePoint audio_pos(audio_position_samples_, static_cast<int32_t>(config_.sample_rate));
            current_offset = (to_seconds(video_position_) - to_seconds(audio_pos)) * 1000.0;
        }
    }
    
    // Update drift rate calculation
    if (drift_state_.last_correction_time_us > 0) {
        int64_t time_diff_us = current_time - drift_state_.last_correction_time_us;
        if (time_diff_us > 0) {
            double time_diff_sec = time_diff_us / 1000000.0;
            drift_state_.drift_rate_ms_per_sec = current_offset / time_diff_sec;
        }
    }
    
    // Apply drift compensation if needed
    if (std::abs(current_offset) > config_.drift_tolerance_ms) {
        apply_drift_correction(current_offset);
    }
    
    drift_state_.last_correction_time_us = current_time;
}

void MasterClockImpl::apply_drift_correction(double current_offset) {
    // This method should be called with drift_mutex_ held
    
    // Accumulate drift for gradual correction
    drift_state_.accumulated_drift_ms += current_offset * config_.correction_speed;
    drift_state_.correction_active = std::abs(drift_state_.accumulated_drift_ms) > 0.1;
    
    std::ostringstream oss;
    oss << "Drift correction applied: offset=" << current_offset 
        << "ms, accumulated=" << drift_state_.accumulated_drift_ms << "ms";
    ve::log::debug(oss.str());
}

void MasterClockImpl::update_sync_metrics(double offset_ms) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // Add to recent offsets
    recent_offsets_.push_back(offset_ms);
    if (recent_offsets_.size() > MAX_OFFSET_HISTORY) {
        recent_offsets_.erase(recent_offsets_.begin());
    }
    
    // Update metrics
    sync_metrics_.measurement_count++;
    
    if (sync_metrics_.measurement_count == 1) {
        sync_metrics_.mean_offset_ms = offset_ms;
        sync_metrics_.max_offset_ms = offset_ms;
        sync_metrics_.min_offset_ms = offset_ms;
    } else {
        // Running average
        double alpha = 0.1; // Smoothing factor
        sync_metrics_.mean_offset_ms = alpha * offset_ms + (1.0 - alpha) * sync_metrics_.mean_offset_ms;
        sync_metrics_.max_offset_ms = std::max(sync_metrics_.max_offset_ms, offset_ms);
        sync_metrics_.min_offset_ms = std::min(sync_metrics_.min_offset_ms, offset_ms);
    }
    
    // Calculate drift rate if we have enough samples
    if (recent_offsets_.size() >= 10) {
        auto& offsets = recent_offsets_;
        double sum = std::accumulate(offsets.end() - 10, offsets.end(), 0.0);
        double recent_mean = sum / 10.0;
        
        // Estimate drift rate (very simplified)
        sync_metrics_.drift_rate_ms_per_min = (recent_mean - sync_metrics_.mean_offset_ms) * 6.0;
    }
    
    // Calculate confidence score based on variance
    if (recent_offsets_.size() >= 5) {
        double variance = 0.0;
        for (double offset : recent_offsets_) {
            double diff = offset - sync_metrics_.mean_offset_ms;
            variance += diff * diff;
        }
        variance /= recent_offsets_.size();
        
        // Higher confidence with lower variance
        sync_metrics_.confidence_score = 1.0 / (1.0 + variance);
    }
}

} // namespace ve::audio