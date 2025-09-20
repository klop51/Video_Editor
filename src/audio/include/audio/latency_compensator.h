/**
 * @file latency_compensator.h
 * @brief Latency Compensation System for Professional A/V Synchronization
 * 
 * Provides automatic latency detection and compensation to achieve ±10ms A/V sync accuracy.
 * Handles plugin delay compensation (PDC), system latency, and look-ahead processing.
 */

#pragma once

#include "core/time.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>

namespace ve::audio {

// Forward declarations
class MasterClock;

/**
 * @brief Configuration for latency compensation system
 */
struct LatencyCompensatorConfig {
    // System configuration
    double max_compensation_ms = 100.0;        // Maximum compensation allowed
    double measurement_interval_ms = 50.0;     // How often to measure latency
    double adaptation_speed = 0.1;             // How fast to adapt to changes (0-1)
    
    // Plugin delay compensation
    bool enable_pdc = true;                    // Enable plugin delay compensation
    double pdc_lookahead_ms = 10.0;           // Look-ahead buffer for zero-latency processing
    double pdc_tolerance_ms = 1.0;            // Tolerance for PDC calculations
    
    // System latency management
    bool enable_system_latency_compensation = true;  // Compensate for audio driver latency
    double system_latency_ms = 20.0;          // Initial estimate of system latency
    bool auto_detect_system_latency = true;   // Automatically measure system latency
    
    // Performance tuning
    size_t measurement_history_size = 100;    // Number of measurements to keep
    double outlier_threshold = 2.0;          // Standard deviations to consider outlier
    bool enable_predictive_compensation = true; // Use predictive algorithms
};

/**
 * @brief Latency measurement result
 */
struct LatencyMeasurement {
    std::chrono::high_resolution_clock::time_point timestamp;
    double plugin_latency_ms;        // Combined plugin processing delay
    double system_latency_ms;        // Audio driver + hardware latency
    double total_latency_ms;         // Total measured latency
    double compensation_applied_ms;   // Compensation currently applied
    double confidence_score;         // Measurement confidence (0-1)
    
    LatencyMeasurement() = default;
    LatencyMeasurement(double plugin_lat, double system_lat, double total_lat)
        : timestamp(std::chrono::high_resolution_clock::now())
        , plugin_latency_ms(plugin_lat)
        , system_latency_ms(system_lat)
        , total_latency_ms(total_lat)
        , compensation_applied_ms(0.0)
        , confidence_score(1.0) {}
};

/**
 * @brief Plugin latency information
 */
struct PluginLatencyInfo {
    std::string plugin_id;           // Unique plugin identifier
    double processing_latency_ms;    // Processing delay in milliseconds
    double lookahead_samples;        // Required look-ahead in samples
    bool has_variable_latency;       // Whether latency can change during processing
    bool is_bypassed;               // Whether plugin is currently bypassed
    
    PluginLatencyInfo() = default;
    PluginLatencyInfo(const std::string& id, double latency_ms)
        : plugin_id(id)
        , processing_latency_ms(latency_ms)
        , lookahead_samples(0.0)
        , has_variable_latency(false)
        , is_bypassed(false) {}
};

/**
 * @brief Latency compensation statistics
 */
struct LatencyStats {
    size_t measurement_count = 0;
    double mean_latency_ms = 0.0;
    double median_latency_ms = 0.0;
    double std_deviation_ms = 0.0;
    double min_latency_ms = 0.0;
    double max_latency_ms = 0.0;
    
    double current_compensation_ms = 0.0;
    double total_compensation_applied_ms = 0.0;
    size_t compensation_adjustments = 0;
    
    std::chrono::high_resolution_clock::time_point last_measurement;
    std::chrono::microseconds measurement_duration_us{0};
};

/**
 * @brief Callback for latency events
 */
enum class LatencyEventType {
    CompensationChanged,    // Compensation amount changed
    PluginLatencyChanged,   // Plugin latency updated
    SystemLatencyChanged,   // System latency changed
    CompensationLimitReached, // Hit maximum compensation limit
    MeasurementOutlier     // Detected measurement outlier
};

struct LatencyEvent {
    LatencyEventType type;
    double latency_ms;
    std::string description;
    std::chrono::high_resolution_clock::time_point timestamp;
};

using LatencyEventCallback = std::function<void(const LatencyEvent&)>;

/**
 * @brief Abstract interface for latency compensation
 */
class LatencyCompensator {
public:
    virtual ~LatencyCompensator() = default;
    
    // Factory method
    static std::unique_ptr<LatencyCompensator> create(
        const LatencyCompensatorConfig& config,
        std::shared_ptr<MasterClock> master_clock = nullptr);
    
    // Lifecycle
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    
    // Configuration
    virtual void update_config(const LatencyCompensatorConfig& config) = 0;
    virtual LatencyCompensatorConfig get_config() const = 0;
    
    // Plugin delay compensation
    virtual void register_plugin(const PluginLatencyInfo& plugin_info) = 0;
    virtual void unregister_plugin(const std::string& plugin_id) = 0;
    virtual void update_plugin_latency(const std::string& plugin_id, double latency_ms) = 0;
    virtual void set_plugin_bypass(const std::string& plugin_id, bool bypassed) = 0;
    virtual double get_total_plugin_latency_ms() const = 0;
    
    // System latency measurement
    virtual void measure_system_latency() = 0;
    virtual double get_system_latency_ms() const = 0;
    virtual void set_system_latency_ms(double latency_ms) = 0;
    
    // Compensation calculation
    virtual double get_current_compensation_ms() const = 0;
    virtual ve::TimePoint calculate_compensated_position(const ve::TimePoint& position) const = 0;
    virtual void apply_compensation_to_pipeline() = 0;
    
    // Measurement and statistics
    virtual LatencyMeasurement measure_total_latency() = 0;
    virtual LatencyStats get_statistics() const = 0;
    virtual std::vector<LatencyMeasurement> get_recent_measurements(size_t count = 0) const = 0;
    
    // Events
    virtual void set_event_callback(LatencyEventCallback callback) = 0;
    
    // Debugging and validation
    virtual std::string generate_report() const = 0;
    virtual bool validate_compensation() const = 0;
    virtual void force_recalculation() = 0;
};

/**
 * @brief Concrete implementation of latency compensation
 */
class LatencyCompensatorImpl : public LatencyCompensator {
public:
    explicit LatencyCompensatorImpl(const LatencyCompensatorConfig& config,
                                   std::shared_ptr<MasterClock> master_clock = nullptr);
    ~LatencyCompensatorImpl() override;
    
    // LatencyCompensator interface
    bool start() override;
    void stop() override;
    void reset() override;
    
    void update_config(const LatencyCompensatorConfig& config) override;
    LatencyCompensatorConfig get_config() const override;
    
    void register_plugin(const PluginLatencyInfo& plugin_info) override;
    void unregister_plugin(const std::string& plugin_id) override;
    void update_plugin_latency(const std::string& plugin_id, double latency_ms) override;
    void set_plugin_bypass(const std::string& plugin_id, bool bypassed) override;
    double get_total_plugin_latency_ms() const override;
    
    void measure_system_latency() override;
    double get_system_latency_ms() const override;
    void set_system_latency_ms(double latency_ms) override;
    
    double get_current_compensation_ms() const override;
    ve::TimePoint calculate_compensated_position(const ve::TimePoint& position) const override;
    void apply_compensation_to_pipeline() override;
    
    LatencyMeasurement measure_total_latency() override;
    LatencyStats get_statistics() const override;
    std::vector<LatencyMeasurement> get_recent_measurements(size_t count = 0) const override;
    
    void set_event_callback(LatencyEventCallback callback) override;
    
    std::string generate_report() const override;
    bool validate_compensation() const override;
    void force_recalculation() override;

private:
    // Internal methods
    void update_compensation();
    void emit_latency_event(LatencyEventType type, double latency_ms, const std::string& description);
    double calculate_adaptive_compensation();
    bool is_measurement_outlier(const LatencyMeasurement& measurement) const;
    void update_statistics();
    double calculate_plugin_compensation() const;
    double calculate_system_compensation() const;
    
    // Configuration
    LatencyCompensatorConfig config_;
    std::shared_ptr<MasterClock> master_clock_;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<double> current_compensation_ms_{0.0};
    std::atomic<double> system_latency_ms_{0.0};
    
    // Plugin management
    mutable std::mutex plugins_mutex_;
    std::unordered_map<std::string, PluginLatencyInfo> registered_plugins_;
    
    // Measurements
    mutable std::mutex measurements_mutex_;
    std::vector<LatencyMeasurement> measurements_;
    LatencyMeasurement latest_measurement_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    LatencyStats statistics_;
    
    // Events
    mutable std::mutex callback_mutex_;
    LatencyEventCallback event_callback_;
    
    // Timing
    std::chrono::high_resolution_clock::time_point last_update_;
    std::chrono::high_resolution_clock::time_point last_system_measurement_;
    
    // Constants
    static constexpr double MIN_COMPENSATION_MS = 0.1;
    static constexpr double MAX_SAMPLES_FOR_STATS = 500;
    static constexpr double SYSTEM_LATENCY_MEASUREMENT_INTERVAL_MS = 1000.0;
};

// Utility functions
namespace latency_utils {

/**
 * @brief Calculate compensation statistics from measurements
 */
LatencyStats calculate_statistics(const std::vector<LatencyMeasurement>& measurements);

/**
 * @brief Detect outliers in latency measurements
 */
std::vector<bool> detect_outliers(const std::vector<LatencyMeasurement>& measurements, double threshold = 2.0);

/**
 * @brief Convert latency in milliseconds to samples
 */
int64_t latency_ms_to_samples(double latency_ms, double sample_rate);

/**
 * @brief Convert samples to latency in milliseconds
 */
double samples_to_latency_ms(int64_t samples, double sample_rate);

/**
 * @brief Format latency measurement for display
 */
std::string format_latency(double latency_ms);

/**
 * @brief Generate comprehensive latency report
 */
std::string format_latency_report(const LatencyStats& stats, 
                                 const std::vector<LatencyMeasurement>& recent_measurements);

} // namespace latency_utils

} // namespace ve::audio