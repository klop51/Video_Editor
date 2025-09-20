/**
 * @file sync_validator.h
 * @brief A/V Synchronization Validation Framework
 * 
 * Phase 2 Week 6: Sync validation system for professional A/V synchronization.
 * Provides real-time offset measurement, lip-sync validation, and automated testing.
 */

#pragma once

#include "core/time.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>

namespace ve::audio {

/**
 * @brief Single sync measurement data point
 */
struct SyncMeasurement {
    int64_t timestamp_us;           // System timestamp when measurement was taken
    double av_offset_ms;            // A/V offset in milliseconds (+ = video ahead)
    double confidence_score;        // Confidence in measurement (0.0-1.0)
    ve::TimePoint audio_pos;  // Audio position at measurement
    ve::TimePoint video_pos;  // Video position at measurement
    
    SyncMeasurement() = default;
    SyncMeasurement(int64_t ts, double offset, double confidence, 
                   const ve::TimePoint& a_pos, const ve::TimePoint& v_pos)
        : timestamp_us(ts), av_offset_ms(offset), confidence_score(confidence)
        , audio_pos(a_pos), video_pos(v_pos) {}
};

/**
 * @brief Comprehensive sync quality metrics
 */
struct SyncQualityMetrics {
    // Basic statistics
    double mean_offset_ms = 0.0;
    double median_offset_ms = 0.0;
    double std_deviation_ms = 0.0;
    double max_offset_ms = 0.0;
    double min_offset_ms = 0.0;
    
    // Quality indicators
    double drift_rate_ms_per_min = 0.0;
    double sync_stability_score = 0.0;  // 0.0-1.0, higher is better
    double overall_quality_score = 0.0; // 0.0-1.0, higher is better
    
    // Timing statistics
    int64_t measurement_count = 0;
    int64_t in_sync_count = 0;
    int64_t out_of_sync_count = 0;
    double sync_percentage = 0.0;
    
    // Performance metrics
    int64_t measurement_duration_us = 0;
    std::chrono::high_resolution_clock::time_point first_measurement;
    std::chrono::high_resolution_clock::time_point last_measurement;
};

/**
 * @brief Sync validation configuration
 */
struct SyncValidatorConfig {
    double sync_tolerance_ms = 10.0;        // Sync tolerance threshold
    double measurement_interval_ms = 100.0;  // How often to measure sync
    size_t max_measurement_history = 10000;  // Maximum measurements to keep
    bool enable_automatic_correction = true; // Auto-correct sync issues
    bool enable_lip_sync_detection = true;   // Enable lip-sync analysis
    double lip_sync_threshold_ms = 40.0;     // Lip-sync tolerance
    bool enable_quality_monitoring = true;   // Enable quality metrics
    double correction_aggression = 0.5;      // How aggressively to correct (0.0-1.0)
};

/**
 * @brief Sync event types for callbacks
 */
enum class SyncEventType {
    InSync,              // A/V is within tolerance
    OutOfSync,           // A/V is outside tolerance
    SyncCorrected,       // Sync correction was applied
    DriftDetected,       // Significant drift detected
    QualityDegraded,     // Sync quality has degraded
    LipSyncIssue         // Lip-sync specific problem
};

/**
 * @brief Sync event data
 */
struct SyncEvent {
    SyncEventType type;
    double offset_ms;
    double confidence;
    std::chrono::high_resolution_clock::time_point timestamp;
    std::string description;
};

/**
 * @brief Callback function for sync events
 */
using SyncEventCallback = std::function<void(const SyncEvent&)>;

/**
 * @brief Professional A/V synchronization validator
 * 
 * This class provides comprehensive sync validation capabilities including:
 * - Real-time A/V offset measurement
 * - Statistical analysis of sync quality
 * - Automatic sync correction recommendations
 * - Lip-sync specific validation
 * - Quality metrics and reporting
 */
class SyncValidator {
public:
    /**
     * @brief Create sync validator instance
     */
    static std::unique_ptr<SyncValidator> create(const SyncValidatorConfig& config);

    /**
     * @brief Destructor
     */
    virtual ~SyncValidator() = default;

    /**
     * @brief Start sync validation
     */
    virtual bool start() = 0;

    /**
     * @brief Stop sync validation
     */
    virtual void stop() = 0;

    /**
     * @brief Reset all measurements and statistics
     */
    virtual void reset() = 0;

    /**
     * @brief Record a sync measurement
     * @param audio_position Current audio position
     * @param video_position Current video position
     * @param timestamp System timestamp of measurement
     * @return The calculated sync measurement
     */
    virtual SyncMeasurement record_measurement(
        const ve::TimePoint& audio_position,
        const ve::TimePoint& video_position,
        std::chrono::high_resolution_clock::time_point timestamp) = 0;

    /**
     * @brief Get current A/V offset
     * @return Current offset in milliseconds (+ = video ahead)
     */
    virtual double get_current_offset_ms() const = 0;

    /**
     * @brief Check if currently in sync
     */
    virtual bool is_in_sync() const = 0;

    /**
     * @brief Get comprehensive quality metrics
     */
    virtual SyncQualityMetrics get_quality_metrics() const = 0;

    /**
     * @brief Get recent measurements
     * @param count Number of recent measurements to return (0 = all)
     */
    virtual std::vector<SyncMeasurement> get_recent_measurements(size_t count = 100) const = 0;

    /**
     * @brief Calculate sync correction recommendation
     * @return Recommended correction in milliseconds
     */
    virtual double calculate_correction_recommendation() const = 0;

    /**
     * @brief Validate lip-sync quality
     * @param audio_content Audio content for analysis (optional)
     * @param video_content Video content for analysis (optional)
     * @return Lip-sync quality score (0.0-1.0)
     */
    virtual double validate_lip_sync(const void* audio_content = nullptr, 
                                   const void* video_content = nullptr) const = 0;

    /**
     * @brief Set sync event callback
     */
    virtual void set_event_callback(SyncEventCallback callback) = 0;

    /**
     * @brief Update configuration
     */
    virtual void update_config(const SyncValidatorConfig& config) = 0;

    /**
     * @brief Get current configuration
     */
    virtual SyncValidatorConfig get_config() const = 0;

    /**
     * @brief Export measurements to file
     * @param filename Output filename
     * @return True if successful
     */
    virtual bool export_measurements(const std::string& filename) const = 0;

    /**
     * @brief Generate sync quality report
     * @return Human-readable sync quality report
     */
    virtual std::string generate_quality_report() const = 0;

protected:
    SyncValidator() = default;
};

/**
 * @brief Concrete sync validator implementation
 */
class SyncValidatorImpl : public SyncValidator {
public:
    explicit SyncValidatorImpl(const SyncValidatorConfig& config);
    ~SyncValidatorImpl() override = default;

    // SyncValidator interface
    bool start() override;
    void stop() override;
    void reset() override;
    SyncMeasurement record_measurement(
        const ve::TimePoint& audio_position,
        const ve::TimePoint& video_position,
        std::chrono::high_resolution_clock::time_point timestamp) override;
    double get_current_offset_ms() const override;
    bool is_in_sync() const override;
    SyncQualityMetrics get_quality_metrics() const override;
    std::vector<SyncMeasurement> get_recent_measurements(size_t count) const override;
    double calculate_correction_recommendation() const override;
    double validate_lip_sync(const void* audio_content, const void* video_content) const override;
    void set_event_callback(SyncEventCallback callback) override;
    void update_config(const SyncValidatorConfig& config) override;
    SyncValidatorConfig get_config() const override;
    bool export_measurements(const std::string& filename) const override;
    std::string generate_quality_report() const override;

private:
    void update_quality_metrics();
    void check_sync_events(const SyncMeasurement& measurement);
    void emit_sync_event(SyncEventType type, double offset, const std::string& description);
    double calculate_confidence_score(const SyncMeasurement& measurement) const;
    double calculate_drift_rate() const;
    double calculate_stability_score() const;
    
    SyncValidatorConfig config_;
    std::atomic<bool> running_{false};
    
    // Measurement storage
    mutable std::mutex measurements_mutex_;
    std::vector<SyncMeasurement> measurements_;
    SyncMeasurement latest_measurement_;
    
    // Quality metrics
    mutable std::mutex metrics_mutex_;
    SyncQualityMetrics quality_metrics_;
    
    // Event handling
    mutable std::mutex callback_mutex_;
    SyncEventCallback event_callback_;
    
    // State tracking
    std::atomic<bool> was_in_sync_{true};
    std::chrono::high_resolution_clock::time_point last_event_time_;
    
    // Statistics calculation helpers
    static constexpr size_t MIN_SAMPLES_FOR_STATS = 10;
    static constexpr double DRIFT_CALCULATION_WINDOW_MS = 30000.0; // 30 seconds
};

/**
 * @brief Utility functions for sync validation
 */
namespace sync_utils {

/**
 * @brief Calculate statistical measures from measurements
 */
struct SyncStatistics {
    double mean;
    double median;
    double std_deviation;
    double min_value;
    double max_value;
};

SyncStatistics calculate_statistics(const std::vector<double>& values);

/**
 * @brief Detect sync patterns and anomalies
 */
struct SyncPattern {
    double period_ms;           // Pattern period in milliseconds
    double amplitude_ms;        // Pattern amplitude
    double confidence;          // Pattern detection confidence
    std::string description;    // Human-readable description
};

std::vector<SyncPattern> detect_sync_patterns(const std::vector<SyncMeasurement>& measurements);

/**
 * @brief Format sync measurement for display
 */
std::string format_measurement(const SyncMeasurement& measurement);

/**
 * @brief Format quality metrics for display
 */
std::string format_quality_metrics(const SyncQualityMetrics& metrics);

} // namespace sync_utils

} // namespace ve::audio