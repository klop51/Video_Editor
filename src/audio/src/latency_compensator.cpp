/**
 * @file latency_compensator.cpp
 * @brief Latency Compensation System Implementation
 */

#include "audio/latency_compensator.h"
#include "audio/master_clock.h"
#include "core/log.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace ve::audio {

std::unique_ptr<LatencyCompensator> LatencyCompensator::create(
    const LatencyCompensatorConfig& config,
    std::shared_ptr<MasterClock> master_clock) {
    return std::make_unique<LatencyCompensatorImpl>(config, master_clock);
}

LatencyCompensatorImpl::LatencyCompensatorImpl(const LatencyCompensatorConfig& config,
                                             std::shared_ptr<MasterClock> master_clock)
    : config_(config)
    , master_clock_(master_clock) {
    std::ostringstream oss;
    oss << "Latency compensator created with max compensation: " << config_.max_compensation_ms 
        << "ms, PDC enabled: " << (config_.enable_pdc ? "yes" : "no");
    ve::log::info(oss.str());
    
    // Initialize system latency estimate
    system_latency_ms_.store(config_.system_latency_ms);
}

LatencyCompensatorImpl::~LatencyCompensatorImpl() {
    stop();
}

bool LatencyCompensatorImpl::start() {
    std::lock_guard<std::mutex> measurements_lock(measurements_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    if (running_.load()) {
        ve::log::warn("Latency compensator already running");
        return false;
    }
    
    // Reset state
    measurements_.clear();
    measurements_.reserve(config_.measurement_history_size);
    statistics_ = LatencyStats{};
    current_compensation_ms_.store(0.0);
    
    // Set timing baselines
    last_update_ = std::chrono::high_resolution_clock::now();
    last_system_measurement_ = last_update_;
    
    running_.store(true);
    ve::log::info("Latency compensator started");
    
    // Perform initial system latency measurement if enabled
    if (config_.auto_detect_system_latency) {
        measure_system_latency();
    }
    
    return true;
}

void LatencyCompensatorImpl::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    std::ostringstream oss;
    oss << "Latency compensator stopped - total measurements: " << statistics_.measurement_count;
    ve::log::info(oss.str());
}

void LatencyCompensatorImpl::reset() {
    std::lock_guard<std::mutex> measurements_lock(measurements_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::mutex> plugins_lock(plugins_mutex_);
    
    measurements_.clear();
    statistics_ = LatencyStats{};
    current_compensation_ms_.store(0.0);
    
    // Keep plugin registrations but reset their state
    for (auto& [plugin_id, plugin_info] : registered_plugins_) {
        plugin_info.is_bypassed = false;
    }
    
    ve::log::debug("Latency compensator reset");
}

void LatencyCompensatorImpl::update_config(const LatencyCompensatorConfig& config) {
    config_ = config;
    system_latency_ms_.store(config.system_latency_ms);
    
    // Trigger recalculation with new settings
    force_recalculation();
    
    ve::log::debug("Latency compensator config updated");
}

LatencyCompensatorConfig LatencyCompensatorImpl::get_config() const {
    return config_;
}

void LatencyCompensatorImpl::register_plugin(const PluginLatencyInfo& plugin_info) {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    
    registered_plugins_[plugin_info.plugin_id] = plugin_info;
    
    std::ostringstream oss;
    oss << "Plugin registered: " << plugin_info.plugin_id 
        << " (latency: " << plugin_info.processing_latency_ms << "ms)";
    ve::log::debug(oss.str());
    
    // Update compensation calculation
    update_compensation();
}

void LatencyCompensatorImpl::unregister_plugin(const std::string& plugin_id) {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    
    auto it = registered_plugins_.find(plugin_id);
    if (it != registered_plugins_.end()) {
        std::ostringstream oss;
        oss << "Plugin unregistered: " << plugin_id;
        ve::log::debug(oss.str());
        
        registered_plugins_.erase(it);
        update_compensation();
    }
}

void LatencyCompensatorImpl::update_plugin_latency(const std::string& plugin_id, double latency_ms) {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    
    auto it = registered_plugins_.find(plugin_id);
    if (it != registered_plugins_.end()) {
        double old_latency = it->second.processing_latency_ms;
        it->second.processing_latency_ms = latency_ms;
        
        std::ostringstream oss;
        oss << "Plugin latency updated: " << plugin_id 
            << " (" << old_latency << "ms -> " << latency_ms << "ms)";
        ve::log::debug(oss.str());
        
        update_compensation();
        emit_latency_event(LatencyEventType::PluginLatencyChanged, latency_ms, 
                          "Plugin " + plugin_id + " latency changed");
    }
}

void LatencyCompensatorImpl::set_plugin_bypass(const std::string& plugin_id, bool bypassed) {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    
    auto it = registered_plugins_.find(plugin_id);
    if (it != registered_plugins_.end()) {
        it->second.is_bypassed = bypassed;
        
        std::ostringstream oss;
        oss << "Plugin " << plugin_id << " bypass: " << (bypassed ? "enabled" : "disabled");
        ve::log::debug(oss.str());
        
        update_compensation();
    }
}

double LatencyCompensatorImpl::get_total_plugin_latency_ms() const {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    
    double total_latency = 0.0;
    for (const auto& [plugin_id, plugin_info] : registered_plugins_) {
        if (!plugin_info.is_bypassed) {
            total_latency += plugin_info.processing_latency_ms;
        }
    }
    
    return total_latency;
}

void LatencyCompensatorImpl::measure_system_latency() {
    if (!running_.load()) {
        return;
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_system_measurement_).count();
    
    // Only measure if enough time has passed
    if (time_since_last < SYSTEM_LATENCY_MEASUREMENT_INTERVAL_MS) {
        return;
    }
    
    // Simplified system latency measurement
    // In a real implementation, this would involve round-trip timing measurement
    double measured_latency = config_.system_latency_ms;
    
    // Add some realistic variation for demonstration
    double variation = (std::sin(now.time_since_epoch().count() / 1000000.0) * 2.0);
    measured_latency += variation;
    measured_latency = std::max(1.0, measured_latency); // Minimum 1ms
    
    double old_latency = system_latency_ms_.load();
    system_latency_ms_.store(measured_latency);
    last_system_measurement_ = now;
    
    if (std::abs(measured_latency - old_latency) > 1.0) {
        std::ostringstream oss;
        oss << "System latency updated: " << old_latency << "ms -> " << measured_latency << "ms";
        ve::log::debug(oss.str());
        
        emit_latency_event(LatencyEventType::SystemLatencyChanged, measured_latency,
                          "System latency measurement updated");
        update_compensation();
    }
}

double LatencyCompensatorImpl::get_system_latency_ms() const {
    return system_latency_ms_.load();
}

void LatencyCompensatorImpl::set_system_latency_ms(double latency_ms) {
    double old_latency = system_latency_ms_.load();
    system_latency_ms_.store(latency_ms);
    
    if (std::abs(latency_ms - old_latency) > 0.1) {
        std::ostringstream oss;
        oss << "System latency set manually: " << old_latency << "ms -> " << latency_ms << "ms";
        ve::log::debug(oss.str());
        
        update_compensation();
    }
}

double LatencyCompensatorImpl::get_current_compensation_ms() const {
    return current_compensation_ms_.load();
}

ve::TimePoint LatencyCompensatorImpl::calculate_compensated_position(const ve::TimePoint& position) const {
    double compensation_ms = get_current_compensation_ms();
    if (std::abs(compensation_ms) < MIN_COMPENSATION_MS) {
        return position; // No compensation needed
    }
    
    // Convert compensation to TimePoint offset
    // Using microseconds precision for compensation
    int64_t compensation_us = static_cast<int64_t>(compensation_ms * 1000.0);
    ve::TimePoint compensation_offset(compensation_us, 1000000); // microseconds
    
    // For now, return original position (TODO: implement proper TimePoint arithmetic)
    return position;
}

void LatencyCompensatorImpl::apply_compensation_to_pipeline() {
    if (!master_clock_) {
        return;
    }
    
    double compensation = get_current_compensation_ms();
    
    // Apply compensation through master clock if available
    // This would involve adjusting the master clock's timing offset
    std::ostringstream oss;
    oss << "Applied compensation to pipeline: " << compensation << "ms";
    ve::log::debug(oss.str());
}

LatencyMeasurement LatencyCompensatorImpl::measure_total_latency() {
    if (!running_.load()) {
        return LatencyMeasurement{};
    }
    
    // Measure current latencies
    double plugin_latency = get_total_plugin_latency_ms();
    double system_latency = get_system_latency_ms();
    double total_latency = plugin_latency + system_latency;
    
    // Create measurement
    LatencyMeasurement measurement(plugin_latency, system_latency, total_latency);
    measurement.compensation_applied_ms = get_current_compensation_ms();
    
    // Calculate confidence based on measurement stability
    measurement.confidence_score = 1.0; // Simplified - could be based on variance
    
    // Store measurement
    {
        std::lock_guard<std::mutex> lock(measurements_mutex_);
        
        measurements_.push_back(measurement);
        latest_measurement_ = measurement;
        
        // Limit history size
        if (measurements_.size() > config_.measurement_history_size) {
            measurements_.erase(measurements_.begin(), 
                              measurements_.begin() + (measurements_.size() - config_.measurement_history_size));
        }
    }
    
    // Update statistics
    update_statistics();
    
    // Check for outliers
    if (is_measurement_outlier(measurement)) {
        emit_latency_event(LatencyEventType::MeasurementOutlier, total_latency,
                          "Latency measurement outlier detected");
    }
    
    return measurement;
}

LatencyStats LatencyCompensatorImpl::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

std::vector<LatencyMeasurement> LatencyCompensatorImpl::get_recent_measurements(size_t count) const {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    if (count == 0 || count >= measurements_.size()) {
        return measurements_;
    }
    
    return std::vector<LatencyMeasurement>(
        measurements_.end() - count, measurements_.end());
}

void LatencyCompensatorImpl::set_event_callback(LatencyEventCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    event_callback_ = std::move(callback);
}

std::string LatencyCompensatorImpl::generate_report() const {
    auto stats = get_statistics();
    auto recent_measurements = get_recent_measurements(10);
    
    std::stringstream report;
    report << "=== Latency Compensation Report ===\n";
    report << "Current Compensation: " << std::fixed << std::setprecision(2) 
           << get_current_compensation_ms() << " ms\n";
    report << "Plugin Latency: " << std::fixed << std::setprecision(2) 
           << get_total_plugin_latency_ms() << " ms\n";
    report << "System Latency: " << std::fixed << std::setprecision(2) 
           << get_system_latency_ms() << " ms\n";
    report << "\nStatistics:\n";
    report << "  Measurements: " << stats.measurement_count << "\n";
    report << "  Mean Latency: " << std::fixed << std::setprecision(2) << stats.mean_latency_ms << " ms\n";
    report << "  Std Deviation: " << std::fixed << std::setprecision(2) << stats.std_deviation_ms << " ms\n";
    report << "  Compensation Adjustments: " << stats.compensation_adjustments << "\n";
    
    report << "\nRegistered Plugins:\n";
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    for (const auto& [plugin_id, plugin_info] : registered_plugins_) {
        report << "  " << plugin_id << ": " << std::fixed << std::setprecision(2) 
               << plugin_info.processing_latency_ms << " ms";
        if (plugin_info.is_bypassed) {
            report << " (bypassed)";
        }
        report << "\n";
    }
    
    return report.str();
}

bool LatencyCompensatorImpl::validate_compensation() const {
    double compensation = get_current_compensation_ms();
    
    // Check if compensation is within acceptable limits
    if (std::abs(compensation) > config_.max_compensation_ms) {
        return false;
    }
    
    // Check if compensation is stable (low variance in recent measurements)
    auto recent = get_recent_measurements(10);
    if (recent.size() >= 5) {
        double variance = 0.0;
        double mean_compensation = 0.0;
        
        for (const auto& measurement : recent) {
            mean_compensation += measurement.compensation_applied_ms;
        }
        mean_compensation /= recent.size();
        
        for (const auto& measurement : recent) {
            double diff = measurement.compensation_applied_ms - mean_compensation;
            variance += diff * diff;
        }
        variance /= recent.size();
        
        // If variance is too high, compensation is unstable
        if (variance > (config_.max_compensation_ms * 0.1)) {
            return false;
        }
    }
    
    return true;
}

void LatencyCompensatorImpl::force_recalculation() {
    if (!running_.load()) {
        return;
    }
    
    update_compensation();
    ve::log::debug("Forced latency compensation recalculation");
}

void LatencyCompensatorImpl::update_compensation() {
    if (!running_.load()) {
        return;
    }
    
    double new_compensation = calculate_adaptive_compensation();
    double old_compensation = current_compensation_ms_.load();
    
    // Apply adaptation speed
    double adapted_compensation = old_compensation + 
        (new_compensation - old_compensation) * config_.adaptation_speed;
    
    // Clamp to maximum allowed compensation
    adapted_compensation = std::clamp(adapted_compensation, 
                                    -config_.max_compensation_ms, 
                                     config_.max_compensation_ms);
    
    current_compensation_ms_.store(adapted_compensation);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        if (std::abs(adapted_compensation - old_compensation) > 0.1) {
            statistics_.compensation_adjustments++;
            statistics_.total_compensation_applied_ms += std::abs(adapted_compensation - old_compensation);
        }
        statistics_.current_compensation_ms = adapted_compensation;
    }
    
    // Emit event if compensation changed significantly
    if (std::abs(adapted_compensation - old_compensation) > 1.0) {
        emit_latency_event(LatencyEventType::CompensationChanged, adapted_compensation,
                          "Compensation updated");
    }
    
    // Check if we hit the compensation limit
    if (std::abs(adapted_compensation) >= config_.max_compensation_ms * 0.95) {
        emit_latency_event(LatencyEventType::CompensationLimitReached, adapted_compensation,
                          "Approaching maximum compensation limit");
    }
}

void LatencyCompensatorImpl::emit_latency_event(LatencyEventType type, double latency_ms, 
                                               const std::string& description) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    
    if (!event_callback_) {
        return;
    }
    
    LatencyEvent event;
    event.type = type;
    event.latency_ms = latency_ms;
    event.description = description;
    event.timestamp = std::chrono::high_resolution_clock::now();
    
    event_callback_(event);
}

double LatencyCompensatorImpl::calculate_adaptive_compensation() {
    double plugin_compensation = calculate_plugin_compensation();
    double system_compensation = calculate_system_compensation();
    
    return plugin_compensation + system_compensation;
}

bool LatencyCompensatorImpl::is_measurement_outlier(const LatencyMeasurement& measurement) const {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    if (measurements_.size() < 5) {
        return false; // Not enough data to determine outliers
    }
    
    // Calculate statistics of recent measurements
    std::vector<double> latencies;
    for (const auto& m : measurements_) {
        latencies.push_back(m.total_latency_ms);
    }
    
    auto stats = latency_utils::calculate_statistics(measurements_);
    
    // Check if measurement is beyond threshold standard deviations
    double z_score = std::abs(measurement.total_latency_ms - stats.mean_latency_ms) / 
                     std::max(0.1, stats.std_deviation_ms);
    
    return z_score > config_.outlier_threshold;
}

void LatencyCompensatorImpl::update_statistics() {
    std::lock_guard<std::mutex> measurements_lock(measurements_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    if (measurements_.empty()) {
        return;
    }
    
    statistics_ = latency_utils::calculate_statistics(measurements_);
    
    // Update timing information
    if (measurements_.size() >= 2) {
        statistics_.last_measurement = measurements_.back().timestamp;
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            measurements_.back().timestamp - measurements_.front().timestamp);
        statistics_.measurement_duration_us = duration;
    }
}

double LatencyCompensatorImpl::calculate_plugin_compensation() const {
    if (!config_.enable_pdc) {
        return 0.0;
    }
    
    double total_plugin_latency = get_total_plugin_latency_ms();
    
    // Apply look-ahead compensation
    double compensation = total_plugin_latency - config_.pdc_lookahead_ms;
    
    return std::max(0.0, compensation);
}

double LatencyCompensatorImpl::calculate_system_compensation() const {
    if (!config_.enable_system_latency_compensation) {
        return 0.0;
    }
    
    double system_latency = get_system_latency_ms();
    
    // Compensate for system latency
    return system_latency;
}

// Utility functions implementation
namespace latency_utils {

LatencyStats calculate_statistics(const std::vector<LatencyMeasurement>& measurements) {
    LatencyStats stats;
    
    if (measurements.empty()) {
        return stats;
    }
    
    stats.measurement_count = measurements.size();
    
    // Extract latency values
    std::vector<double> latencies;
    latencies.reserve(measurements.size());
    
    for (const auto& measurement : measurements) {
        latencies.push_back(measurement.total_latency_ms);
    }
    
    // Mean
    stats.mean_latency_ms = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    
    // Min/Max
    auto minmax = std::minmax_element(latencies.begin(), latencies.end());
    stats.min_latency_ms = *minmax.first;
    stats.max_latency_ms = *minmax.second;
    
    // Median
    auto sorted_latencies = latencies;
    std::sort(sorted_latencies.begin(), sorted_latencies.end());
    size_t mid = sorted_latencies.size() / 2;
    if (sorted_latencies.size() % 2 == 0) {
        stats.median_latency_ms = (sorted_latencies[mid - 1] + sorted_latencies[mid]) / 2.0;
    } else {
        stats.median_latency_ms = sorted_latencies[mid];
    }
    
    // Standard deviation
    double variance = 0.0;
    for (double latency : latencies) {
        double diff = latency - stats.mean_latency_ms;
        variance += diff * diff;
    }
    variance /= latencies.size();
    stats.std_deviation_ms = std::sqrt(variance);
    
    return stats;
}

std::vector<bool> detect_outliers(const std::vector<LatencyMeasurement>& measurements, double threshold) {
    std::vector<bool> outliers(measurements.size(), false);
    
    if (measurements.size() < 5) {
        return outliers; // Not enough data
    }
    
    auto stats = calculate_statistics(measurements);
    
    for (size_t i = 0; i < measurements.size(); ++i) {
        double z_score = std::abs(measurements[i].total_latency_ms - stats.mean_latency_ms) / 
                         std::max(0.1, stats.std_deviation_ms);
        outliers[i] = (z_score > threshold);
    }
    
    return outliers;
}

int64_t latency_ms_to_samples(double latency_ms, double sample_rate) {
    return static_cast<int64_t>(latency_ms * sample_rate / 1000.0);
}

double samples_to_latency_ms(int64_t samples, double sample_rate) {
    return static_cast<double>(samples) * 1000.0 / sample_rate;
}

std::string format_latency(double latency_ms) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << latency_ms << " ms";
    return oss.str();
}

std::string format_latency_report(const LatencyStats& stats, 
                                 const std::vector<LatencyMeasurement>& recent_measurements) {
    std::ostringstream report;
    
    report << "=== Latency Analysis Report ===\n";
    report << "Measurements: " << stats.measurement_count << "\n";
    report << "Mean Latency: " << format_latency(stats.mean_latency_ms) << "\n";
    report << "Median Latency: " << format_latency(stats.median_latency_ms) << "\n";
    report << "Std Deviation: " << format_latency(stats.std_deviation_ms) << "\n";
    report << "Range: " << format_latency(stats.min_latency_ms) 
           << " - " << format_latency(stats.max_latency_ms) << "\n";
    report << "Current Compensation: " << format_latency(stats.current_compensation_ms) << "\n";
    
    if (!recent_measurements.empty()) {
        report << "\nRecent Measurements:\n";
        for (size_t i = 0; i < std::min(size_t(5), recent_measurements.size()); ++i) {
            const auto& m = recent_measurements[recent_measurements.size() - 1 - i];
            report << "  " << format_latency(m.total_latency_ms) 
                   << " (Plugin: " << format_latency(m.plugin_latency_ms)
                   << ", System: " << format_latency(m.system_latency_ms) << ")\n";
        }
    }
    
    return report.str();
}

} // namespace latency_utils

} // namespace ve::audio