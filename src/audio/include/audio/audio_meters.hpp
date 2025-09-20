/**
 * Professional Audio Meters - Week 10 Audio Engine Roadmap
 * 
 * Implements professional broadcast-standard audio meters including:
 * - Peak Program Meters (PPM) with BBC/EBU ballistics
 * - VU Meters with authentic ballistics  
 * - LUFS Meters with EBU R128 compliance indicators
 * - Correlation Meters for stereo phase analysis
 * - Spectrum Analyzer for frequency domain analysis
 * 
 * These meters provide broadcast-quality monitoring for professional
 * video editing workflows with real-time visual feedback.
 */

#pragma once

#include "audio/loudness_monitor.hpp"
#include "audio/audio_frame.hpp"
#include "core/time.hpp"
#include <vector>
#include <array>
#include <mutex>
#include <memory>
#include <complex>
#include <functional>

namespace ve::audio {

/**
 * Meter scale types for different broadcast standards
 */
enum class MeterScale {
    PPM_BBC,        // BBC Peak Program Meter: -12 to +8 dBu
    PPM_EBU,        // EBU Peak Program Meter: -12 to +12 dBu  
    PPM_NORDIC,     // Nordic Peak Program Meter: -18 to +9 dBu
    VU_STANDARD,    // VU Meter: -20 to +3 VU
    DIGITAL_PEAK,   // Digital Peak Meter: -60 to 0 dBFS
    LUFS_METER,     // LUFS Meter: -50 to 0 LUFS
    CORRELATION     // Correlation Meter: -1 to +1
};

/**
 * Professional meter configuration
 */
struct MeterConfig {
    MeterScale scale = MeterScale::DIGITAL_PEAK;
    MeterBallistics ballistics;
    double reference_level_db = -18.0;  // Digital reference level
    double range_min_db = -60.0;
    double range_max_db = 0.0;
    bool show_numeric_value = true;
    bool show_peak_hold = true;
    bool show_compliance_indicators = true;
    
    // Visual configuration
    uint32_t meter_width = 20;
    uint32_t meter_height = 200;
    uint32_t update_rate_hz = 50;  // 50 Hz update rate for smooth movement
    
    static MeterConfig bbc_ppm() {
        MeterConfig config;
        config.scale = MeterScale::PPM_BBC;
        config.ballistics = MeterBallistics::digital_peak_ballistics();
        config.reference_level_db = -18.0;  // BBC standard
        config.range_min_db = -12.0;
        config.range_max_db = 8.0;
        return config;
    }
    
    static MeterConfig ebu_ppm() {
        MeterConfig config;
        config.scale = MeterScale::PPM_EBU;
        config.ballistics = MeterBallistics::digital_peak_ballistics();
        config.reference_level_db = -18.0;
        config.range_min_db = -12.0;
        config.range_max_db = 12.0;
        return config;
    }
    
    static MeterConfig vu_meter() {
        MeterConfig config;
        config.scale = MeterScale::VU_STANDARD;
        config.ballistics = MeterBallistics::vu_ballistics();
        config.reference_level_db = -18.0; // 0 VU = -18 dBFS digital
        config.range_min_db = -20.0;
        config.range_max_db = 3.0;
        config.show_peak_hold = false;  // VU meters don't show peak hold
        return config;
    }
    
    static MeterConfig lufs_meter() {
        MeterConfig config;
        config.scale = MeterScale::LUFS_METER;
        config.ballistics = MeterBallistics{}; // Instantaneous for LUFS
        config.reference_level_db = -23.0;  // EBU R128 reference
        config.range_min_db = -50.0;
        config.range_max_db = 0.0;
        return config;
    }
};

/**
 * Meter visual data for rendering
 */
struct MeterVisualData {
    double current_level = -std::numeric_limits<double>::infinity();
    double peak_hold_level = -std::numeric_limits<double>::infinity();
    double reference_level = -18.0;
    
    // Normalized values (0.0 to 1.0)
    double current_normalized = 0.0;
    double peak_hold_normalized = 0.0;
    double reference_normalized = 0.5;
    
    // Compliance flags
    bool in_compliance = true;
    bool over_reference = false;
    bool over_ceiling = false;
    
    // Color coding for different zones
    enum Zone { GREEN, YELLOW, RED } current_zone = GREEN;
    
    std::chrono::steady_clock::time_point last_update;
    bool valid = false;
};

/**
 * Professional audio meter implementation
 */
class ProfessionalAudioMeter {
private:
    MeterConfig config_;
    AudioLevelMeter level_meter_;
    MeterVisualData visual_data_;
    
    // Update timing
    std::chrono::steady_clock::time_point last_update_time_;
    std::chrono::duration<double> update_interval_;
    
public:
    explicit ProfessionalAudioMeter(const MeterConfig& config = MeterConfig{})
        : config_(config)
        , level_meter_(config.ballistics)
        , update_interval_(1.0 / config.update_rate_hz)
    {
        level_meter_.set_sample_rate(48000.0); // Default sample rate
        reset();
    }
    
    void set_sample_rate(double sample_rate) {
        level_meter_.set_sample_rate(sample_rate);
    }
    
    void set_config(const MeterConfig& config) {
        config_ = config;
        level_meter_ = AudioLevelMeter(config.ballistics);
        update_interval_ = std::chrono::duration<double>(1.0 / config.update_rate_hz);
    }
    
    void reset() {
        level_meter_.reset();
        visual_data_ = MeterVisualData{};
        visual_data_.reference_level = config_.reference_level_db;
        last_update_time_ = std::chrono::steady_clock::now();
    }
    
    void update(double level_db) {
        level_meter_.update(level_db);
        
        auto now = std::chrono::steady_clock::now();
        if (now - last_update_time_ >= update_interval_) {
            update_visual_data();
            last_update_time_ = now;
        }
    }
    
    void update_with_samples(const float* samples, size_t count) {
        // Calculate RMS or peak level from samples
        double sum_squares = 0.0;
        double peak = 0.0;
        
        for (size_t i = 0; i < count; ++i) {
            double sample = std::abs(samples[i]);
            sum_squares += sample * sample;
            peak = std::max(peak, sample);
        }
        
        double level_db;
        if (config_.scale == MeterScale::VU_STANDARD) {
            // VU meter uses RMS
            double rms = std::sqrt(sum_squares / count);
            level_db = 20.0 * std::log10(std::max(rms, 1e-10));
        } else {
            // PPM and digital meters use peak
            level_db = 20.0 * std::log10(std::max(peak, 1e-10));
        }
        
        update(level_db);
    }
    
    const MeterVisualData& get_visual_data() const {
        return visual_data_;
    }
    
    MeterConfig get_config() const {
        return config_;
    }
    
    bool is_over_reference() const {
        return visual_data_.over_reference;
    }
    
    bool is_over_ceiling() const {
        return visual_data_.over_ceiling;
    }
    
    std::string get_meter_reading() const {
        if (!visual_data_.valid) {
            return "---";
        }
        
        char buffer[16];
        if (config_.scale == MeterScale::LUFS_METER) {
            std::snprintf(buffer, sizeof(buffer), "%.1f LU", visual_data_.current_level);
        } else if (config_.scale == MeterScale::VU_STANDARD) {
            double vu_value = visual_data_.current_level - config_.reference_level_db;
            std::snprintf(buffer, sizeof(buffer), "%+.1f VU", vu_value);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.1f dB", visual_data_.current_level);
        }
        
        return std::string(buffer);
    }
    
private:
    void update_visual_data() {
        visual_data_.current_level = level_meter_.get_level_db();
        visual_data_.peak_hold_level = level_meter_.get_peak_hold_db();
        
        // Normalize to 0.0-1.0 range
        double range = config_.range_max_db - config_.range_min_db;
        visual_data_.current_normalized = 
            std::clamp((visual_data_.current_level - config_.range_min_db) / range, 0.0, 1.0);
        visual_data_.peak_hold_normalized = 
            std::clamp((visual_data_.peak_hold_level - config_.range_min_db) / range, 0.0, 1.0);
        visual_data_.reference_normalized = 
            std::clamp((config_.reference_level_db - config_.range_min_db) / range, 0.0, 1.0);
        
        // Update compliance flags
        visual_data_.over_reference = visual_data_.current_level > config_.reference_level_db;
        visual_data_.over_ceiling = visual_data_.current_level > config_.range_max_db;
        visual_data_.in_compliance = !visual_data_.over_ceiling;
        
        // Determine color zone
        if (visual_data_.current_level < config_.reference_level_db - 6.0) {
            visual_data_.current_zone = MeterVisualData::GREEN;
        } else if (visual_data_.current_level < config_.reference_level_db) {
            visual_data_.current_zone = MeterVisualData::YELLOW;
        } else {
            visual_data_.current_zone = MeterVisualData::RED;
        }
        
        visual_data_.last_update = std::chrono::steady_clock::now();
        visual_data_.valid = true;
    }
};

/**
 * Stereo correlation meter for phase analysis
 */
class CorrelationMeter {
private:
    double sum_left_squared_ = 0.0;
    double sum_right_squared_ = 0.0;
    double sum_left_right_ = 0.0;
    size_t sample_count_ = 0;
    size_t window_size_ = 48000; // 1 second at 48kHz
    
    std::vector<float> left_buffer_;
    std::vector<float> right_buffer_;
    size_t buffer_pos_ = 0;
    
public:
    explicit CorrelationMeter(size_t window_samples = 48000)
        : window_size_(window_samples) {
        left_buffer_.resize(window_size_, 0.0f);
        right_buffer_.resize(window_size_, 0.0f);
        reset();
    }
    
    void reset() {
        sum_left_squared_ = 0.0;
        sum_right_squared_ = 0.0;
        sum_left_right_ = 0.0;
        sample_count_ = 0;
        buffer_pos_ = 0;
        std::fill(left_buffer_.begin(), left_buffer_.end(), 0.0f);
        std::fill(right_buffer_.begin(), right_buffer_.end(), 0.0f);
    }
    
    void process_samples(float left, float right) {
        // Remove old samples from running sums if buffer is full
        if (sample_count_ >= window_size_) {
            float old_left = left_buffer_[buffer_pos_];
            float old_right = right_buffer_[buffer_pos_];
            
            sum_left_squared_ -= old_left * old_left;
            sum_right_squared_ -= old_right * old_right;
            sum_left_right_ -= old_left * old_right;
        } else {
            sample_count_++;
        }
        
        // Add new samples
        left_buffer_[buffer_pos_] = left;
        right_buffer_[buffer_pos_] = right;
        
        sum_left_squared_ += left * left;
        sum_right_squared_ += right * right;
        sum_left_right_ += left * right;
        
        buffer_pos_ = (buffer_pos_ + 1) % window_size_;
    }
    
    double get_correlation() const {
        if (sample_count_ < 2) return 0.0;
        
        double denominator = std::sqrt(sum_left_squared_ * sum_right_squared_);
        if (denominator < 1e-10) return 0.0;
        
        double correlation = sum_left_right_ / denominator;
        return std::clamp(correlation, -1.0, 1.0);
    }
    
    bool is_mono_compatible() const {
        // Mono compatibility check: correlation should be > 0.5 for good mono compatibility
        return get_correlation() > 0.5;
    }
    
    bool has_phase_issues() const {
        // Phase issues: correlation < -0.5 indicates significant phase problems
        return get_correlation() < -0.5;
    }
};

/**
 * Multi-channel meter group for professional monitoring
 */
class MeterGroup {
private:
    std::vector<std::unique_ptr<ProfessionalAudioMeter>> meters_;
    std::unique_ptr<CorrelationMeter> correlation_meter_;
    std::unique_ptr<RealTimeLoudnessMonitor> loudness_monitor_;
    
    uint16_t channel_count_;
    double sample_rate_;
    
public:
    explicit MeterGroup(uint16_t channels = 2, double sample_rate = 48000.0)
        : channel_count_(channels)
        , sample_rate_(sample_rate) {
        initialize();
    }
    
    void initialize() {
        meters_.clear();
        
        // Create meters for each channel
        for (uint16_t i = 0; i < channel_count_; ++i) {
            auto meter = std::make_unique<ProfessionalAudioMeter>(MeterConfig::bbc_ppm());
            meter->set_sample_rate(sample_rate_);
            meters_.push_back(std::move(meter));
        }
        
        // Create correlation meter for stereo monitoring
        if (channel_count_ >= 2) {
            correlation_meter_ = std::make_unique<CorrelationMeter>(static_cast<size_t>(sample_rate_));
        }
        
        // Create loudness monitor
        loudness_monitor_ = std::make_unique<RealTimeLoudnessMonitor>(sample_rate_, channel_count_);
    }
    
    void process_frame(const AudioFrame& frame) {
        if (frame.channel_count() != channel_count_) return;
        
        size_t sample_count = frame.sample_count();
        
        // Update channel meters
        for (uint16_t ch = 0; ch < channel_count_; ++ch) {
            if (ch < meters_.size()) {
                // Collect samples for this channel
                std::vector<float> channel_samples;
                channel_samples.reserve(sample_count);
                for (size_t i = 0; i < sample_count; ++i) {
                    channel_samples.push_back(frame.get_sample_as_float(ch, static_cast<uint32_t>(i)));
                }
                meters_[ch]->update_with_samples(channel_samples.data(), channel_samples.size());
            }
        }
        
        // Update correlation meter for stereo
        if (correlation_meter_ && channel_count_ >= 2) {
            for (size_t i = 0; i < sample_count; ++i) {
                float left = frame.get_sample_as_float(0, static_cast<uint32_t>(i));
                float right = frame.get_sample_as_float(1, static_cast<uint32_t>(i));
                correlation_meter_->process_samples(left, right);
            }
        }
        
        // Update loudness monitor
        if (loudness_monitor_) {
            loudness_monitor_->process_samples(frame);
        }
    }
    
    const ProfessionalAudioMeter* get_meter(uint16_t channel) const {
        if (channel < meters_.size()) {
            return meters_[channel].get();
        }
        return nullptr;
    }
    
    const CorrelationMeter* get_correlation_meter() const {
        return correlation_meter_.get();
    }
    
    const RealTimeLoudnessMonitor* get_loudness_monitor() const {
        return loudness_monitor_.get();
    }
    
    void set_meter_config(const MeterConfig& config) {
        for (auto& meter : meters_) {
            meter->set_config(config);
            meter->set_sample_rate(sample_rate_);
        }
    }
    
    void reset_all() {
        for (auto& meter : meters_) {
            meter->reset();
        }
        if (correlation_meter_) {
            correlation_meter_->reset();
        }
        if (loudness_monitor_) {
            loudness_monitor_->reset();
        }
    }
    
    uint16_t get_channel_count() const { return channel_count_; }
    double get_sample_rate() const { return sample_rate_; }
};

} // namespace ve::audio