#pragma once

#include <core/time.hpp>
#include <atomic>
#include <chrono>
#include <memory>

namespace ve::audio {

/**
 * @brief Audio clock configuration
 */
struct AudioClockConfig {
    uint32_t sample_rate = 48000;           ///< Audio sample rate
    double drift_threshold = 0.001;         ///< Drift threshold in seconds (1ms)
    double correction_rate = 0.1;           ///< Correction rate (0.0-1.0)
    bool enable_drift_compensation = true;  ///< Enable automatic drift compensation
    uint32_t measurement_window = 1000;     ///< Samples to average for drift measurement
    
    // Phase 2 Week 2: Enhanced Synchronization
    bool enable_frame_accurate_sync = true; ///< Enable frame-accurate video synchronization
    double max_correction_per_second = 0.1; ///< Maximum correction per second to avoid artifacts
    uint32_t sync_validation_samples = 100; ///< Samples between sync validation checks
    bool enable_predictive_sync = true;     ///< Enable predictive synchronization algorithms
    double video_frame_rate = 24.0;        ///< Target video frame rate for sync
    bool enable_adaptive_threshold = true;  ///< Automatically adjust drift threshold based on performance
    
    // Clock source preferences
    enum class ClockSource {
        SystemTime,     // Use system high-resolution clock
        AudioHardware,  // Use audio hardware clock (when available) 
        External        // Use external timing source
    } clock_source = ClockSource::SystemTime;
};

/**
 * @brief Audio clock statistics for monitoring
 */
struct AudioClockStats {
    double current_drift_seconds = 0.0;     ///< Current clock drift in seconds
    double max_drift_seconds = 0.0;         ///< Maximum observed drift
    double avg_drift_seconds = 0.0;         ///< Average drift over measurement window
    uint64_t drift_corrections = 0;         ///< Number of drift corrections applied
    uint64_t samples_processed = 0;         ///< Total samples processed
    TimePoint last_correction_time;         ///< Time of last correction
    bool is_stable = false;                 ///< Clock stability indicator
    
    // Phase 2 Week 2: Enhanced Statistics
    uint64_t frame_sync_corrections = 0;    ///< Frame-accurate sync corrections
    double max_correction_applied = 0.0;    ///< Maximum single correction applied
    double predictive_accuracy = 0.0;       ///< Accuracy of predictive synchronization (0.0-1.0)
    uint32_t sync_validation_failures = 0;  ///< Number of sync validation failures
    double adaptive_threshold_current = 0.0; ///< Current adaptive threshold value
    bool frame_sync_active = false;         ///< Whether frame-accurate sync is currently active
};

/**
 * @brief High-precision audio timeline synchronization
 * 
 * This clock provides:
 * - High-precision audio timeline synchronization
 * - Drift compensation and clock recovery
 * - Integration with existing rational time system
 * - ±1 sample accuracy over 60 seconds target
 * 
 * Phase 2 Week 2: Enhanced with frame-accurate video synchronization
 */
class AudioClock {
public:
    /**
     * @brief Create audio clock with specified configuration
     * @param config Clock configuration
     * @return Unique pointer to clock, or nullptr on failure
     */
    static std::unique_ptr<AudioClock> create(const AudioClockConfig& config);
    
    /**
     * @brief Constructor
     * @param config Clock configuration
     */
    explicit AudioClock(const AudioClockConfig& config);
    
    /**
     * @brief Destructor
     */
    ~AudioClock();
    
    // Non-copyable and non-movable (due to atomic members)
    AudioClock(const AudioClock&) = delete;
    AudioClock& operator=(const AudioClock&) = delete;
    AudioClock(AudioClock&&) = delete;
    AudioClock& operator=(AudioClock&&) = delete;
    
    /**
     * @brief Initialize the audio clock
     * @return True on success
     */
    bool initialize();
    
    /**
     * @brief Check if clock is initialized
     */
    bool is_initialized() const { return initialized_; }
    
    /**
     * @brief Start the audio clock
     * @param start_time Initial timeline position
     * @return True on success
     */
    bool start(const TimePoint& start_time = TimePoint());
    
    /**
     * @brief Stop the audio clock
     */
    void stop();
    
    /**
     * @brief Check if clock is running
     */
    bool is_running() const { return running_; }
    
    /**
     * @brief Get current audio timeline position
     * @return Current timeline position in rational time
     */
    TimePoint get_time() const;
    
    /**
     * @brief Get time for specific sample offset from current position
     * @param sample_offset Offset in samples (can be negative)
     * @return Timeline position for the sample
     */
    TimePoint get_time_for_sample_offset(int64_t sample_offset) const;
    
    /**
     * @brief Advance clock by specified number of samples
     * @param sample_count Number of samples to advance
     * @return New timeline position
     */
    TimePoint advance_samples(uint32_t sample_count);
    
    /**
     * @brief Synchronize clock with external timing reference
     * @param reference_time External reference time
     * @param audio_samples Number of audio samples at reference time
     */
    void sync_to_reference(const TimePoint& reference_time, uint64_t audio_samples);
    
    /**
     * @brief Set timeline position (seeking)
     * @param time New timeline position
     */
    void set_time(const TimePoint& time);
    
    /**
     * @brief Convert samples to timeline duration
     * @param sample_count Number of samples
     * @return Duration as TimePoint
     */
    TimePoint samples_to_time(uint64_t sample_count) const;
    
    /**
     * @brief Convert timeline duration to samples
     * @param time_duration Duration in timeline time
     * @return Number of samples
     */
    uint64_t time_to_samples(const TimePoint& time_duration) const;
    
    /**
     * @brief Get sample rate
     */
    uint32_t sample_rate() const { return config_.sample_rate; }
    
    /**
     * @brief Get current clock statistics
     */
    AudioClockStats get_stats() const;
    
    /**
     * @brief Reset clock statistics
     */
    void reset_stats();
    
    /**
     * @brief Enable/disable drift compensation
     */
    void set_drift_compensation(bool enabled);
    
    /**
     * @brief Check if drift compensation is enabled
     */
    bool is_drift_compensation_enabled() const { return config_.enable_drift_compensation; }
    
    /**
     * @brief Get current drift in seconds
     */
    double get_current_drift() const { return current_drift_; }
    
    /**
     * @brief Check if clock is stable (low drift)
     */
    bool is_stable() const;
    
    // Phase 2 Week 2: Enhanced Synchronization Methods
    
    /**
     * @brief Synchronize with video frame timing for frame-accurate playback
     * @param video_frame_time Current video frame timestamp
     * @param video_frame_number Current video frame number
     * @return True if synchronization was successful
     */
    bool sync_to_video_frame(const TimePoint& video_frame_time, uint64_t video_frame_number);
    
    /**
     * @brief Perform predictive synchronization based on historical drift patterns
     * @param look_ahead_samples Number of samples to predict ahead
     * @return Predicted drift correction needed
     */
    double predict_drift_correction(uint32_t look_ahead_samples) const;
    
    /**
     * @brief Validate current synchronization accuracy
     * @param reference_time Expected timeline position
     * @param tolerance_samples Acceptable tolerance in samples
     * @return True if synchronization is within tolerance
     */
    bool validate_sync_accuracy(const TimePoint& reference_time, uint32_t tolerance_samples = 1) const;
    
    /**
     * @brief Apply frame-accurate correction with artifact prevention
     * @param target_correction Desired correction amount
     * @param max_step_size Maximum correction per step to prevent artifacts
     * @return Actual correction applied
     */
    double apply_frame_accurate_correction(double target_correction, double max_step_size);
    
    /**
     * @brief Get frame-accurate position for video synchronization
     * @param video_frame_rate Target video frame rate
     * @return Audio time quantized to video frame boundaries
     */
    TimePoint get_frame_accurate_time(double video_frame_rate) const;
    
    /**
     * @brief Update adaptive threshold based on system performance
     */
    void update_adaptive_threshold();
    
    /**
     * @brief Get detailed synchronization metrics for monitoring
     */
    struct SyncMetrics {
        double video_audio_offset = 0.0;    ///< Current video-audio offset in seconds
        double frame_sync_accuracy = 0.0;   ///< Frame synchronization accuracy (0.0-1.0)
        uint32_t consecutive_stable_frames = 0; ///< Consecutive frames with stable sync
        bool requires_resync = false;       ///< Whether resynchronization is recommended
        double estimated_drift_velocity = 0.0; ///< Rate of drift change per second
    };
    
    /**
     * @brief Get comprehensive synchronization metrics
     */
    SyncMetrics get_sync_metrics() const;

private:
    AudioClockConfig config_;
    bool initialized_ = false;
    std::atomic<bool> running_{false};
    
    // Clock state
    mutable std::atomic<uint64_t> sample_count_{0};
    TimePoint start_time_;
    std::chrono::high_resolution_clock::time_point wall_clock_start_;
    
    // Drift compensation
    mutable std::atomic<double> current_drift_{0.0};
    mutable std::atomic<double> max_drift_{0.0};
    std::atomic<uint64_t> drift_corrections_{0};
    
    // Statistics
    mutable std::vector<double> drift_history_;
    mutable std::atomic<uint64_t> samples_processed_{0};
    TimePoint last_correction_time_;
    
    // Phase 2 Week 2: Enhanced Synchronization State
    mutable std::atomic<uint64_t> frame_sync_corrections_{0};
    mutable std::atomic<double> max_correction_applied_{0.0};
    mutable std::atomic<uint32_t> sync_validation_failures_{0};
    mutable std::atomic<double> adaptive_threshold_current_{0.0};
    mutable std::atomic<uint32_t> consecutive_stable_samples_{0};
    
    // Video synchronization tracking
    TimePoint last_video_frame_time_;
    uint64_t last_video_frame_number_ = 0;
    mutable std::vector<double> drift_velocity_history_;
    uint32_t sync_validation_counter_ = 0;
    
    // Internal methods
    void update_drift_measurement() const;
    void apply_drift_correction();
    double calculate_current_drift() const;
    TimePoint calculate_audio_time() const;
    
    // Phase 2 Week 2: Enhanced Internal Methods
    void update_drift_velocity() const;
    double calculate_predictive_drift(uint32_t samples_ahead) const;
    bool should_apply_frame_correction(double video_audio_offset) const;
    double calculate_adaptive_threshold() const;
    void validate_synchronization_state() const;
};

/**
 * @brief Master audio clock for system-wide synchronization
 * 
 * Singleton pattern for coordinating multiple audio components
 */
class MasterAudioClock {
public:
    /**
     * @brief Get the master audio clock instance
     */
    static MasterAudioClock& instance();
    
    /**
     * @brief Initialize master clock
     * @param config Clock configuration
     * @return True on success
     */
    bool initialize(const AudioClockConfig& config);
    
    /**
     * @brief Get the master clock
     */
    AudioClock* get_clock() { return master_clock_.get(); }
    
    /**
     * @brief Check if master clock is available
     */
    bool is_available() const { return master_clock_ != nullptr; }
    
    /**
     * @brief Start master clock
     */
    bool start(const TimePoint& start_time = TimePoint());
    
    /**
     * @brief Stop master clock
     */
    void stop();
    
    /**
     * @brief Get current master time
     */
    TimePoint get_time() const;

private:
    MasterAudioClock() = default;
    std::unique_ptr<AudioClock> master_clock_;
};

/**
 * @brief Audio clock synchronizer for multi-clock scenarios
 */
class AudioClockSynchronizer {
public:
    /**
     * @brief Constructor
     * @param master_clock Master clock for synchronization
     */
    explicit AudioClockSynchronizer(AudioClock* master_clock);
    
    /**
     * @brief Add slave clock for synchronization
     * @param slave_clock Clock to synchronize with master
     * @param sync_interval_ms Synchronization interval in milliseconds
     */
    void add_slave_clock(AudioClock* slave_clock, double sync_interval_ms = 100.0);
    
    /**
     * @brief Remove slave clock
     */
    void remove_slave_clock(AudioClock* slave_clock);
    
    /**
     * @brief Perform synchronization update
     */
    void update_synchronization();
    
    /**
     * @brief Enable/disable automatic synchronization
     */
    void set_auto_sync(bool enabled) { auto_sync_enabled_ = enabled; }

private:
    struct SlaveClock {
        AudioClock* clock;
        double sync_interval_ms;
        std::chrono::high_resolution_clock::time_point last_sync;
    };
    
    AudioClock* master_clock_;
    std::vector<SlaveClock> slave_clocks_;
    bool auto_sync_enabled_ = true;
};

/**
 * @brief Utility functions for audio clock operations
 */
namespace clock_utils {
    /**
     * @brief Convert rational time to high-precision seconds
     */
    double rational_to_seconds(const TimeRational& rational);
    
    /**
     * @brief Convert seconds to rational time with sample rate precision
     */
    TimeRational seconds_to_rational(double seconds, uint32_t sample_rate);
    
    /**
     * @brief Calculate sample-accurate timestamp
     */
    TimePoint sample_accurate_time(uint64_t sample_count, uint32_t sample_rate);
    
    /**
     * @brief Measure time difference in samples
     */
    int64_t time_difference_in_samples(const TimePoint& time1, const TimePoint& time2, uint32_t sample_rate);
    
    /**
     * @brief Check if two times are within sample accuracy
     */
    bool times_are_sample_accurate(const TimePoint& time1, const TimePoint& time2, uint32_t sample_rate);
    
    /**
     * @brief Get recommended drift threshold for sample rate
     */
    double recommend_drift_threshold(uint32_t sample_rate);
}

} // namespace ve::audio