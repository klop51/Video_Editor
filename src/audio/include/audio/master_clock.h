/**
 * @file master_clock.h
 * @brief Master Clock Implementation for A/V Synchronization
 * 
 * Phase 2 Week 6: Professional A/V synchronization with audio-driven master clock.
 * Provides frame-accurate timing, drift detection, and automatic sync correction.
 */

#pragma once

#include "core/time.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace ve::audio {

/**
 * @brief Drift compensation state
 */
struct DriftState {
    double accumulated_drift_ms = 0.0;
    int64_t last_correction_time_us = 0;
    bool correction_active = false;
    double drift_rate_ms_per_sec = 0.0;
};

/**
 * @brief Sync quality metrics
 */
struct SyncMetrics {
    double mean_offset_ms = 0.0;
    double max_offset_ms = 0.0;
    double min_offset_ms = 0.0;
    double drift_rate_ms_per_min = 0.0;
    int64_t measurement_count = 0;
    double confidence_score = 0.0;
};

/**
 * @brief Master clock configuration
 */
struct MasterClockConfig {
    double sample_rate = 48000.0;
    uint32_t buffer_size = 1024;
    double drift_tolerance_ms = 5.0;
    double correction_speed = 0.1;
    bool enable_drift_compensation = true;
    bool enable_quality_monitoring = true;
};

/**
 * @brief Audio-driven master clock for A/V synchronization
 * 
 * This class implements a professional-grade master clock system that uses
 * the audio pipeline as the master timebase. Video follows the audio clock
 * with automatic drift detection and correction.
 * 
 * Key Features:
 * - Sample-accurate audio positioning
 * - Video frame boundary alignment
 * - Automatic drift compensation
 * - Real-time quality monitoring
 * - Frame-accurate synchronization
 */
class MasterClock {
public:
    /**
     * @brief Create master clock instance
     */
    static std::unique_ptr<MasterClock> create(const MasterClockConfig& config);

    /**
     * @brief Destructor
     */
    virtual ~MasterClock() = default;

    /**
     * @brief Start the master clock
     * @return True if started successfully
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the master clock
     */
    virtual void stop() = 0;

    /**
     * @brief Reset clock to zero position
     */
    virtual void reset() = 0;

    /**
     * @brief Set playback rate
     * @param rate Playback rate (1.0 = normal speed)
     */
    virtual void set_playback_rate(double rate) = 0;

    /**
     * @brief Get current playback rate
     */
    virtual double get_playback_rate() const = 0;

    /**
     * @brief Update audio position (called by audio pipeline)
     * @param position Current audio position in samples
     * @param timestamp System timestamp when position was captured
     */
    virtual void update_audio_position(int64_t position_samples, 
                                     std::chrono::high_resolution_clock::time_point timestamp) = 0;

    /**
     * @brief Get current master time in microseconds
     */
    virtual int64_t get_master_time_us() const = 0;

    /**
     * @brief Get current audio position
     */
    virtual ve::TimePoint get_audio_position() const = 0;

    /**
     * @brief Get expected video position based on master clock
     */
    virtual ve::TimePoint get_video_position() const = 0;

    /**
     * @brief Report video position for sync monitoring
     * @param position Current video position
     * @param timestamp System timestamp when position was captured
     */
    virtual void report_video_position(const ve::TimePoint& position,
                                     std::chrono::high_resolution_clock::time_point timestamp) = 0;

    /**
     * @brief Get current A/V offset in milliseconds
     * @return Offset in ms (positive = video ahead, negative = audio ahead)
     */
    virtual double get_av_offset_ms() const = 0;

    /**
     * @brief Check if sync is within tolerance
     */
    virtual bool is_in_sync() const = 0;

    /**
     * @brief Get drift compensation state
     */
    virtual DriftState get_drift_state() const = 0;

    /**
     * @brief Get sync quality metrics
     */
    virtual SyncMetrics get_sync_metrics() const = 0;

    /**
     * @brief Enable/disable drift compensation
     */
    virtual void set_drift_compensation_enabled(bool enabled) = 0;

    /**
     * @brief Set drift tolerance threshold
     * @param tolerance_ms Maximum allowed drift in milliseconds
     */
    virtual void set_drift_tolerance(double tolerance_ms) = 0;

    /**
     * @brief Force sync correction
     * Immediately apply correction to bring A/V back into sync
     */
    virtual void force_sync_correction() = 0;

protected:
    MasterClock() = default;
};

/**
 * @brief Concrete master clock implementation
 */
class MasterClockImpl : public MasterClock {
public:
    explicit MasterClockImpl(const MasterClockConfig& config);
    ~MasterClockImpl() override = default;

    // MasterClock interface
    bool start() override;
    void stop() override;
    void reset() override;
    void set_playback_rate(double rate) override;
    double get_playback_rate() const override;
    void update_audio_position(int64_t position_samples, 
                             std::chrono::high_resolution_clock::time_point timestamp) override;
    int64_t get_master_time_us() const override;
    ve::TimePoint get_audio_position() const override;
    ve::TimePoint get_video_position() const override;
    void report_video_position(const ve::TimePoint& position,
                             std::chrono::high_resolution_clock::time_point timestamp) override;
    double get_av_offset_ms() const override;
    bool is_in_sync() const override;
    DriftState get_drift_state() const override;
    SyncMetrics get_sync_metrics() const override;
    void set_drift_compensation_enabled(bool enabled) override;
    void set_drift_tolerance(double tolerance_ms) override;
    void force_sync_correction() override;

private:
    void update_drift_state();
    void apply_drift_correction(double current_offset);
    void update_sync_metrics(double offset_ms);
    
    MasterClockConfig config_;
    
    // Master timebase (atomic for lock-free access)
    std::atomic<int64_t> master_time_us_{0};
    std::atomic<double> playback_rate_{1.0};
    std::atomic<bool> running_{false};
    
    // Audio position tracking
    mutable std::mutex audio_mutex_;
    int64_t audio_position_samples_{0};
    std::chrono::high_resolution_clock::time_point audio_timestamp_;
    std::chrono::high_resolution_clock::time_point start_time_;
    
    // Video position tracking
    mutable std::mutex video_mutex_;
    ve::TimePoint video_position_;
    std::chrono::high_resolution_clock::time_point video_timestamp_;
    
    // Drift compensation
    mutable std::mutex drift_mutex_;
    DriftState drift_state_;
    
    // Sync metrics
    mutable std::mutex metrics_mutex_;
    SyncMetrics sync_metrics_;
    std::vector<double> recent_offsets_;
    static constexpr size_t MAX_OFFSET_HISTORY = 1000;
};

} // namespace ve::audio