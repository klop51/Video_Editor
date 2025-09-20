/**
 * @file sync_validator.cpp
 * @brief A/V Synchronization Validation Framework Implementation
 */

#include "audio/sync_validator.h"
#include "core/log.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <fstream>
#include <iomanip>

namespace ve::audio {

// Helper function to convert TimePoint to seconds
static double to_seconds(const ve::TimePoint& time_point) {
    const auto& rational = time_point.to_rational();
    if (rational.den == 0) {
        return 0.0;
    }
    return static_cast<double>(rational.num) / static_cast<double>(rational.den);
}

std::unique_ptr<SyncValidator> SyncValidator::create(const SyncValidatorConfig& config) {
    return std::make_unique<SyncValidatorImpl>(config);
}

SyncValidatorImpl::SyncValidatorImpl(const SyncValidatorConfig& config)
    : config_(config) {
    std::ostringstream oss;
    oss << "Sync validator created with tolerance: " << config_.sync_tolerance_ms 
        << "ms, interval: " << config_.measurement_interval_ms << "ms";
    ve::log::info(oss.str());
}

bool SyncValidatorImpl::start() {
    std::lock_guard<std::mutex> measurements_lock(measurements_mutex_);
    std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
    
    if (running_.load()) {
        ve::log::warn("Sync validator already running");
        return false;
    }
    
    // Reset all state
    measurements_.clear();
    measurements_.reserve(config_.max_measurement_history);
    quality_metrics_ = SyncQualityMetrics{};
    was_in_sync_.store(true);
    
    running_.store(true);
    ve::log::info("Sync validator started");
    return true;
}

void SyncValidatorImpl::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    std::ostringstream oss;
    oss << "Sync validator stopped - total measurements: " << quality_metrics_.measurement_count;
    ve::log::info(oss.str());
}

void SyncValidatorImpl::reset() {
    std::lock_guard<std::mutex> measurements_lock(measurements_mutex_);
    std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
    
    measurements_.clear();
    quality_metrics_ = SyncQualityMetrics{};
    was_in_sync_.store(true);
    
    ve::log::debug("Sync validator reset");
}

SyncMeasurement SyncValidatorImpl::record_measurement(
    const ve::TimePoint& audio_position,
    const ve::TimePoint& video_position,
    std::chrono::high_resolution_clock::time_point timestamp) {
    
    if (!running_.load()) {
        return SyncMeasurement{};
    }
    
    // Calculate A/V offset
    double audio_sec = to_seconds(audio_position);
    double video_sec = to_seconds(video_position);
    double offset_ms = (video_sec - audio_sec) * 1000.0;
    
    // Create measurement
    auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        timestamp.time_since_epoch()).count();
    
    SyncMeasurement measurement(timestamp_us, offset_ms, 0.0, audio_position, video_position);
    measurement.confidence_score = calculate_confidence_score(measurement);
    
    // Store measurement
    {
        std::lock_guard<std::mutex> lock(measurements_mutex_);
        
        measurements_.push_back(measurement);
        latest_measurement_ = measurement;
        
        // Limit history size
        if (measurements_.size() > config_.max_measurement_history) {
            measurements_.erase(measurements_.begin(), 
                              measurements_.begin() + (measurements_.size() - config_.max_measurement_history));
        }
    }
    
    // Update metrics and check for events
    update_quality_metrics();
    check_sync_events(measurement);
    
    return measurement;
}

double SyncValidatorImpl::get_current_offset_ms() const {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    return latest_measurement_.av_offset_ms;
}

bool SyncValidatorImpl::is_in_sync() const {
    double offset = std::abs(get_current_offset_ms());
    return offset <= config_.sync_tolerance_ms;
}

SyncQualityMetrics SyncValidatorImpl::get_quality_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return quality_metrics_;
}

std::vector<SyncMeasurement> SyncValidatorImpl::get_recent_measurements(size_t count) const {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    if (count == 0 || count >= measurements_.size()) {
        return measurements_;
    }
    
    return std::vector<SyncMeasurement>(
        measurements_.end() - count, measurements_.end());
}

double SyncValidatorImpl::calculate_correction_recommendation() const {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    if (measurements_.size() < MIN_SAMPLES_FOR_STATS) {
        return 0.0;
    }
    
    // Use weighted average of recent measurements
    size_t sample_count = std::min(measurements_.size(), size_t(50));
    auto recent = std::vector<SyncMeasurement>(
        measurements_.end() - sample_count, measurements_.end());
    
    double weighted_sum = 0.0;
    double weight_sum = 0.0;
    
    for (size_t i = 0; i < recent.size(); ++i) {
        // Give more weight to recent measurements
        double weight = recent[i].confidence_score * (i + 1);
        weighted_sum += recent[i].av_offset_ms * weight;
        weight_sum += weight;
    }
    
    if (weight_sum == 0.0) {
        return 0.0;
    }
    
    double correction = -(weighted_sum / weight_sum) * config_.correction_aggression;
    
    std::ostringstream oss;
    oss << "Correction recommendation: " << correction << "ms (from " << sample_count << " samples)";
    ve::log::debug(oss.str());
    return correction;
}

double SyncValidatorImpl::validate_lip_sync(const void* audio_content, const void* video_content) const {
    // Basic lip-sync validation based on offset magnitude
    double current_offset = std::abs(get_current_offset_ms());
    
    if (!config_.enable_lip_sync_detection) {
        return 1.0; // Always pass if disabled
    }
    
    if (current_offset <= config_.lip_sync_threshold_ms) {
        return 1.0 - (current_offset / config_.lip_sync_threshold_ms) * 0.2;
    } else {
        // Degraded quality beyond threshold
        double excess = current_offset - config_.lip_sync_threshold_ms;
        return std::max(0.0, 0.8 - (excess / config_.lip_sync_threshold_ms) * 0.8);
    }
}

void SyncValidatorImpl::set_event_callback(SyncEventCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    event_callback_ = std::move(callback);
}

void SyncValidatorImpl::update_config(const SyncValidatorConfig& config) {
    config_ = config;
    ve::log::debug("Sync validator config updated");
}

SyncValidatorConfig SyncValidatorImpl::get_config() const {
    return config_;
}

bool SyncValidatorImpl::export_measurements(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::ostringstream oss;
        oss << "Failed to open file for export: " << filename;
        ve::log::error(oss.str());
        return false;
    }
    
    // Write CSV header
    file << "Timestamp_us,Offset_ms,Confidence,Audio_Position_s,Video_Position_s\n";
    
    // Write measurements
    for (const auto& measurement : measurements_) {
        file << measurement.timestamp_us << ","
             << std::fixed << std::setprecision(3) << measurement.av_offset_ms << ","
             << std::fixed << std::setprecision(2) << measurement.confidence_score << ","
             << std::fixed << std::setprecision(6) << to_seconds(measurement.audio_pos) << ","
             << std::fixed << std::setprecision(6) << to_seconds(measurement.video_pos) << "\n";
    }
    
    file.close();
    std::ostringstream oss;
    oss << "Exported " << measurements_.size() << " measurements to " << filename;
    ve::log::info(oss.str());
    return true;
}

std::string SyncValidatorImpl::generate_quality_report() const {
    auto metrics = get_quality_metrics();
    
    std::stringstream report;
    report << "=== A/V Sync Quality Report ===\n";
    report << "Measurement Count: " << metrics.measurement_count << "\n";
    report << "Sync Percentage: " << std::fixed << std::setprecision(1) << metrics.sync_percentage << "%\n";
    report << "Mean Offset: " << std::fixed << std::setprecision(2) << metrics.mean_offset_ms << " ms\n";
    report << "Median Offset: " << std::fixed << std::setprecision(2) << metrics.median_offset_ms << " ms\n";
    report << "Std Deviation: " << std::fixed << std::setprecision(2) << metrics.std_deviation_ms << " ms\n";
    report << "Max Offset: " << std::fixed << std::setprecision(2) << metrics.max_offset_ms << " ms\n";
    report << "Min Offset: " << std::fixed << std::setprecision(2) << metrics.min_offset_ms << " ms\n";
    report << "Drift Rate: " << std::fixed << std::setprecision(3) << metrics.drift_rate_ms_per_min << " ms/min\n";
    report << "Stability Score: " << std::fixed << std::setprecision(2) << metrics.sync_stability_score << "\n";
    report << "Overall Quality: " << std::fixed << std::setprecision(2) << metrics.overall_quality_score << "\n";
    
    if (metrics.measurement_count > 0) {
        auto duration_sec = metrics.measurement_duration_us / 1000000.0;
        report << "Duration: " << std::fixed << std::setprecision(1) << duration_sec << " seconds\n";
    }
    
    return report.str();
}

void SyncValidatorImpl::update_quality_metrics() {
    std::lock_guard<std::mutex> measurements_lock(measurements_mutex_);
    std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
    
    if (measurements_.empty()) {
        return;
    }
    
    // Extract offset values for statistics
    std::vector<double> offsets;
    offsets.reserve(measurements_.size());
    
    int64_t in_sync_count = 0;
    
    for (const auto& measurement : measurements_) {
        offsets.push_back(measurement.av_offset_ms);
        
        if (std::abs(measurement.av_offset_ms) <= config_.sync_tolerance_ms) {
            in_sync_count++;
        }
    }
    
    // Calculate basic statistics
    auto stats = sync_utils::calculate_statistics(offsets);
    
    quality_metrics_.measurement_count = measurements_.size();
    quality_metrics_.mean_offset_ms = stats.mean;
    quality_metrics_.median_offset_ms = stats.median;
    quality_metrics_.std_deviation_ms = stats.std_deviation;
    quality_metrics_.max_offset_ms = stats.max_value;
    quality_metrics_.min_offset_ms = stats.min_value;
    
    quality_metrics_.in_sync_count = in_sync_count;
    quality_metrics_.out_of_sync_count = measurements_.size() - in_sync_count;
    quality_metrics_.sync_percentage = (static_cast<double>(in_sync_count) / measurements_.size()) * 100.0;
    
    // Calculate advanced metrics
    quality_metrics_.drift_rate_ms_per_min = calculate_drift_rate();
    quality_metrics_.sync_stability_score = calculate_stability_score();
    
    // Overall quality score (weighted combination)
    quality_metrics_.overall_quality_score = 
        0.4 * (quality_metrics_.sync_percentage / 100.0) +
        0.3 * quality_metrics_.sync_stability_score +
        0.2 * std::max(0.0, 1.0 - std::abs(quality_metrics_.mean_offset_ms) / config_.sync_tolerance_ms) +
        0.1 * std::max(0.0, 1.0 - quality_metrics_.std_deviation_ms / config_.sync_tolerance_ms);
    
    // Update timing information
    if (measurements_.size() >= 2) {
        quality_metrics_.first_measurement = 
            std::chrono::high_resolution_clock::time_point(
                std::chrono::microseconds(measurements_.front().timestamp_us));
        quality_metrics_.last_measurement = 
            std::chrono::high_resolution_clock::time_point(
                std::chrono::microseconds(measurements_.back().timestamp_us));
        
        quality_metrics_.measurement_duration_us = 
            measurements_.back().timestamp_us - measurements_.front().timestamp_us;
    }
}

void SyncValidatorImpl::check_sync_events(const SyncMeasurement& measurement) {
    bool currently_in_sync = std::abs(measurement.av_offset_ms) <= config_.sync_tolerance_ms;
    bool was_in_sync = was_in_sync_.load();
    
    auto now = std::chrono::high_resolution_clock::now();
    
    // Check for sync state changes
    if (currently_in_sync != was_in_sync) {
        if (currently_in_sync) {
            emit_sync_event(SyncEventType::InSync, measurement.av_offset_ms, 
                          "A/V sync restored within tolerance");
        } else {
            emit_sync_event(SyncEventType::OutOfSync, measurement.av_offset_ms, 
                          "A/V sync outside tolerance");
        }
        
        was_in_sync_.store(currently_in_sync);
        last_event_time_ = now;
    }
    
    // Check for lip-sync issues
    if (config_.enable_lip_sync_detection && 
        std::abs(measurement.av_offset_ms) > config_.lip_sync_threshold_ms) {
        
        // Only emit lip-sync events occasionally to avoid spam
        auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_event_time_).count();
        
        if (time_since_last > 5000) { // 5 seconds minimum between lip-sync events
            emit_sync_event(SyncEventType::LipSyncIssue, measurement.av_offset_ms, 
                          "Lip-sync quality degraded");
            last_event_time_ = now;
        }
    }
}

void SyncValidatorImpl::emit_sync_event(SyncEventType type, double offset, const std::string& description) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    
    if (!event_callback_) {
        return;
    }
    
    SyncEvent event;
    event.type = type;
    event.offset_ms = offset;
    event.confidence = latest_measurement_.confidence_score;
    event.timestamp = std::chrono::high_resolution_clock::now();
    event.description = description;
    
    event_callback_(event);
}

double SyncValidatorImpl::calculate_confidence_score(const SyncMeasurement& measurement) const {
    // Basic confidence calculation based on measurement stability
    double base_confidence = 0.8;
    
    // Reduce confidence for extreme offsets
    double offset_penalty = std::min(0.3, std::abs(measurement.av_offset_ms) / 100.0);
    
    return std::max(0.1, base_confidence - offset_penalty);
}

double SyncValidatorImpl::calculate_drift_rate() const {
    // Calculate drift rate over the last window of measurements
    if (measurements_.size() < MIN_SAMPLES_FOR_STATS) {
        return 0.0;
    }
    
    // Find measurements within the drift calculation window
    auto now_us = measurements_.back().timestamp_us;
    auto window_start_us = now_us - static_cast<int64_t>(DRIFT_CALCULATION_WINDOW_MS * 1000.0);
    
    std::vector<SyncMeasurement> window_measurements;
    for (const auto& measurement : measurements_) {
        if (measurement.timestamp_us >= window_start_us) {
            window_measurements.push_back(measurement);
        }
    }
    
    if (window_measurements.size() < 2) {
        return 0.0;
    }
    
    // Simple linear regression to estimate drift rate
    double sum_t = 0.0, sum_offset = 0.0, sum_t_offset = 0.0, sum_t_sq = 0.0;
    double n = static_cast<double>(window_measurements.size());
    
    for (const auto& measurement : window_measurements) {
        double t = (measurement.timestamp_us - window_start_us) / 1000.0; // Convert to ms
        double offset = measurement.av_offset_ms;
        
        sum_t += t;
        sum_offset += offset;
        sum_t_offset += t * offset;
        sum_t_sq += t * t;
    }
    
    // Calculate slope (drift rate in ms per ms, convert to ms per minute)
    double denominator = n * sum_t_sq - sum_t * sum_t;
    if (std::abs(denominator) < 1e-6) {
        return 0.0;
    }
    
    double slope = (n * sum_t_offset - sum_t * sum_offset) / denominator;
    return slope * 60.0; // Convert to ms per minute
}

double SyncValidatorImpl::calculate_stability_score() const {
    if (measurements_.size() < MIN_SAMPLES_FOR_STATS) {
        return 1.0;
    }
    
    // Calculate coefficient of variation as a stability measure
    double mean = std::abs(quality_metrics_.mean_offset_ms);
    double std_dev = quality_metrics_.std_deviation_ms;
    
    if (mean < 0.1) {
        return 1.0; // Very stable if mean is near zero
    }
    
    double cv = std_dev / mean;
    
    // Convert to a 0-1 score (lower CV = higher stability)
    return std::max(0.0, 1.0 - std::min(1.0, cv));
}

// Utility functions implementation
namespace sync_utils {

SyncStatistics calculate_statistics(const std::vector<double>& values) {
    if (values.empty()) {
        return SyncStatistics{0.0, 0.0, 0.0, 0.0, 0.0};
    }
    
    SyncStatistics stats;
    
    // Mean
    stats.mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    
    // Min/Max
    auto minmax = std::minmax_element(values.begin(), values.end());
    stats.min_value = *minmax.first;
    stats.max_value = *minmax.second;
    
    // Median
    auto sorted_values = values;
    std::sort(sorted_values.begin(), sorted_values.end());
    size_t mid = sorted_values.size() / 2;
    if (sorted_values.size() % 2 == 0) {
        stats.median = (sorted_values[mid - 1] + sorted_values[mid]) / 2.0;
    } else {
        stats.median = sorted_values[mid];
    }
    
    // Standard deviation
    double variance = 0.0;
    for (double value : values) {
        double diff = value - stats.mean;
        variance += diff * diff;
    }
    variance /= values.size();
    stats.std_deviation = std::sqrt(variance);
    
    return stats;
}

std::vector<SyncPattern> detect_sync_patterns(const std::vector<SyncMeasurement>& measurements) {
    // Simple pattern detection - can be enhanced with more sophisticated algorithms
    std::vector<SyncPattern> patterns;
    
    if (measurements.size() < 10) {
        return patterns;
    }
    
    // Extract offsets and look for periodic behavior
    std::vector<double> offsets;
    for (const auto& measurement : measurements) {
        offsets.push_back(measurement.av_offset_ms);
    }
    
    // Simple oscillation detection
    int sign_changes = 0;
    for (size_t i = 1; i < offsets.size(); ++i) {
        if ((offsets[i] > 0) != (offsets[i-1] > 0)) {
            sign_changes++;
        }
    }
    
    if (sign_changes > offsets.size() / 4) {
        SyncPattern pattern;
        pattern.description = "Oscillating sync pattern detected";
        pattern.confidence = 0.7;
        patterns.push_back(pattern);
    }
    
    return patterns;
}

std::string format_measurement(const SyncMeasurement& measurement) {
    std::stringstream ss;
    ss << "Offset: " << std::fixed << std::setprecision(2) << measurement.av_offset_ms 
       << "ms, Confidence: " << std::fixed << std::setprecision(2) << measurement.confidence_score;
    return ss.str();
}

std::string format_quality_metrics(const SyncQualityMetrics& metrics) {
    std::stringstream ss;
    ss << "Quality: " << std::fixed << std::setprecision(1) << metrics.overall_quality_score * 100 << "%, "
       << "Sync: " << std::fixed << std::setprecision(1) << metrics.sync_percentage << "%, "
       << "Mean: " << std::fixed << std::setprecision(2) << metrics.mean_offset_ms << "ms";
    return ss.str();
}

} // namespace sync_utils

} // namespace ve::audio