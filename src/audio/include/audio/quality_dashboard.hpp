/**
 * Quality Analysis Dashboard - Week 10 Audio Engine Roadmap
 * 
 * Implements a comprehensive real-time quality monitoring interface including:
 * - Real-time quality assessment and reporting
 * - Export quality validation and compliance checking
 * - Performance metrics and system health monitoring
 * - Professional broadcast compliance dashboard
 * - Quality score calculation and trend analysis
 * 
 * This dashboard provides actionable feedback for professional video editing
 * workflows with real-time quality monitoring and compliance validation.
 */

#pragma once

#include "audio/loudness_monitor.hpp"
#include "audio/audio_meters.hpp"
#include "audio/audio_frame.hpp"
#include "core/time.hpp"
#include <memory>
#include <mutex>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <functional>
#include <atomic>

namespace ve::audio {

/**
 * Quality assessment categories
 */
enum class QualityCategory {
    EXCELLENT,      // 90-100% quality score
    GOOD,           // 70-89% quality score
    ACCEPTABLE,     // 50-69% quality score
    POOR,           // 30-49% quality score
    UNACCEPTABLE    // 0-29% quality score
};

/**
 * Quality metrics for comprehensive assessment
 */
struct QualityMetrics {
    // Loudness quality
    double loudness_score = 100.0;          // 0-100%
    bool loudness_compliant = true;
    double target_lufs_deviation = 0.0;     // Deviation from target
    
    // Peak level quality
    double peak_score = 100.0;              // 0-100%
    bool peak_compliant = true;
    double peak_margin_db = 0.0;             // Margin below ceiling
    
    // Phase quality
    double phase_score = 100.0;             // 0-100%
    bool mono_compatible = true;
    double correlation_value = 1.0;
    
    // Dynamic range quality
    double dynamic_range_score = 100.0;     // 0-100%
    double dr_measurement = 0.0;             // Dynamic range in dB
    
    // Frequency response quality
    double frequency_score = 100.0;         // 0-100%
    bool frequency_balanced = true;
    
    // Overall quality
    double overall_score = 100.0;           // Weighted average
    QualityCategory category = QualityCategory::EXCELLENT;
    
    std::chrono::steady_clock::time_point timestamp;
    bool valid = false;
};

/**
 * Performance monitoring data
 */
struct PerformanceMetrics {
    // Processing performance
    double cpu_usage_percent = 0.0;
    double memory_usage_mb = 0.0;
    double real_time_factor = 1.0;          // >1.0 = faster than real-time
    
    // Audio processing metrics
    uint64_t samples_processed = 0;
    uint64_t frames_processed = 0;
    uint64_t buffer_underruns = 0;
    uint64_t buffer_overruns = 0;
    
    // Timing metrics
    double average_processing_time_ms = 0.0;
    double max_processing_time_ms = 0.0;
    double jitter_ms = 0.0;
    
    // Quality processing metrics
    uint64_t quality_assessments = 0;
    double assessment_frequency_hz = 0.0;
    
    std::chrono::steady_clock::time_point timestamp;
    bool valid = false;
};

/**
 * Platform-specific quality targets
 */
struct PlatformQualityTargets {
    std::string platform_name;
    
    // Loudness targets
    double target_lufs = -23.0;
    double lufs_tolerance = 1.0;
    
    // Peak targets
    double peak_ceiling_dbfs = -1.0;
    double peak_margin_db = 3.0;
    
    // Dynamic range targets
    double min_dynamic_range_db = 6.0;
    double target_dynamic_range_db = 12.0;
    
    // Phase targets
    double min_correlation = 0.5;
    
    // Quality thresholds
    double min_acceptable_score = 70.0;
    double target_score = 90.0;
    
    static PlatformQualityTargets ebu_r128_broadcast() {
        PlatformQualityTargets targets;
        targets.platform_name = "EBU R128 Broadcast";
        targets.target_lufs = -23.0;
        targets.lufs_tolerance = 1.0;
        targets.peak_ceiling_dbfs = -1.0;
        targets.min_dynamic_range_db = 6.0;
        targets.target_dynamic_range_db = 12.0;
        return targets;
    }
    
    static PlatformQualityTargets youtube_streaming() {
        PlatformQualityTargets targets;
        targets.platform_name = "YouTube Streaming";
        targets.target_lufs = -14.0;
        targets.lufs_tolerance = 2.0;
        targets.peak_ceiling_dbfs = -1.0;
        targets.min_dynamic_range_db = 4.0;
        targets.target_dynamic_range_db = 8.0;
        return targets;
    }
    
    static PlatformQualityTargets netflix_broadcast() {
        PlatformQualityTargets targets;
        targets.platform_name = "Netflix Broadcast";
        targets.target_lufs = -27.0;
        targets.lufs_tolerance = 0.5;
        targets.peak_ceiling_dbfs = -2.0;
        targets.min_dynamic_range_db = 8.0;
        targets.target_dynamic_range_db = 15.0;
        return targets;
    }
    
    static PlatformQualityTargets spotify_streaming() {
        PlatformQualityTargets targets;
        targets.platform_name = "Spotify Streaming";
        targets.target_lufs = -14.0;
        targets.lufs_tolerance = 2.0;
        targets.peak_ceiling_dbfs = -1.0;
        targets.min_dynamic_range_db = 3.0;
        targets.target_dynamic_range_db = 6.0;
        return targets;
    }
};

/**
 * Quality assessment report
 */
struct QualityReport {
    QualityMetrics metrics;
    PerformanceMetrics performance;
    PlatformQualityTargets targets;
    
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    std::vector<std::string> compliance_issues;
    
    std::string summary_text;
    std::chrono::steady_clock::time_point generation_time;
    bool ready_for_export = false;
};

/**
 * Quality trend tracking for analysis
 */
class QualityTrendTracker {
private:
    std::vector<QualityMetrics> history_;
    size_t max_history_size_ = 1000;  // Keep last 1000 measurements
    std::chrono::duration<double> sample_interval_{1.0}; // 1 second sampling
    std::chrono::steady_clock::time_point last_sample_time_;
    
public:
    explicit QualityTrendTracker(size_t max_size = 1000, double sample_interval_seconds = 1.0)
        : max_history_size_(max_size)
        , sample_interval_(sample_interval_seconds) {
        history_.reserve(max_history_size_);
    }
    
    void add_measurement(const QualityMetrics& metrics) {
        auto now = std::chrono::steady_clock::now();
        
        // Sample at specified interval to avoid too frequent updates
        if (now - last_sample_time_ >= sample_interval_) {
            history_.push_back(metrics);
            
            // Keep history within bounds
            if (history_.size() > max_history_size_) {
                history_.erase(history_.begin());
            }
            
            last_sample_time_ = now;
        }
    }
    
    double get_average_quality_score(std::chrono::duration<double> time_window) const {
        if (history_.empty()) return 0.0;
        
        auto cutoff_time = std::chrono::steady_clock::now() - time_window;
        double sum = 0.0;
        size_t count = 0;
        
        for (const auto& metrics : history_) {
            if (metrics.timestamp >= cutoff_time) {
                sum += metrics.overall_score;
                count++;
            }
        }
        
        return count > 0 ? sum / count : 0.0;
    }
    
    std::vector<double> get_score_trend(size_t sample_count = 100) const {
        std::vector<double> trend;
        
        size_t start_idx = history_.size() > sample_count ? 
                          history_.size() - sample_count : 0;
        
        for (size_t i = start_idx; i < history_.size(); ++i) {
            trend.push_back(history_[i].overall_score);
        }
        
        return trend;
    }
    
    bool is_quality_declining() const {
        if (history_.size() < 10) return false;
        
        // Check if recent quality is significantly lower than earlier quality
        double recent_avg = get_average_quality_score(std::chrono::seconds(30));
        double older_avg = 0.0;
        
        size_t older_count = 0;
        auto cutoff_recent = std::chrono::steady_clock::now() - std::chrono::seconds(30);
        auto cutoff_older = cutoff_recent - std::chrono::seconds(60);
        
        for (const auto& metrics : history_) {
            if (metrics.timestamp >= cutoff_older && metrics.timestamp < cutoff_recent) {
                older_avg += metrics.overall_score;
                older_count++;
            }
        }
        
        if (older_count > 0) {
            older_avg /= older_count;
            return (older_avg - recent_avg) > 10.0; // 10% decline threshold
        }
        
        return false;
    }
    
    void clear_history() {
        history_.clear();
    }
    
    size_t get_history_size() const {
        return history_.size();
    }
};

/**
 * Real-time quality analysis dashboard
 */
class QualityAnalysisDashboard {
private:
    // Core monitoring components
    std::unique_ptr<RealTimeLoudnessMonitor> loudness_monitor_;
    std::unique_ptr<MeterGroup> meter_group_;
    std::unique_ptr<QualityTrendTracker> trend_tracker_;
    
    // Configuration
    PlatformQualityTargets current_targets_;
    double sample_rate_ = 48000.0;
    uint16_t channels_ = 2;
    
    // Current state
    QualityMetrics current_metrics_;
    PerformanceMetrics current_performance_;
    QualityReport latest_report_;
    
    // Analysis timing
    std::chrono::steady_clock::time_point last_analysis_time_;
    std::chrono::duration<double> analysis_interval_{0.1}; // 100ms analysis rate
    
    // Performance tracking
    std::chrono::steady_clock::time_point processing_start_time_;
    std::vector<double> processing_times_;
    std::atomic<uint64_t> total_samples_processed_{0};
    std::atomic<uint64_t> total_frames_processed_{0};
    
    // Thread safety
    mutable std::mutex dashboard_mutex_;
    
public:
    explicit QualityAnalysisDashboard(const PlatformQualityTargets& targets = PlatformQualityTargets::ebu_r128_broadcast(),
                                     double sample_rate = 48000.0,
                                     uint16_t channels = 2)
        : current_targets_(targets)
        , sample_rate_(sample_rate)
        , channels_(channels) {
        initialize();
    }
    
    void initialize() {
        // Create monitoring components
        loudness_monitor_ = std::make_unique<RealTimeLoudnessMonitor>(sample_rate_, channels_);
        meter_group_ = std::make_unique<MeterGroup>(channels_, sample_rate_);
        trend_tracker_ = std::make_unique<QualityTrendTracker>(1000, 1.0);
        
        // Configure for current platform
        configure_for_platform(current_targets_);
        
        reset();
    }
    
    void configure_for_platform(const PlatformQualityTargets& targets) {
        std::lock_guard<std::mutex> lock(dashboard_mutex_);
        
        current_targets_ = targets;
        
        // Note: Platform-specific configuration would be implemented
        // when broadcast vs streaming specific features are needed
    }
    
    void reset() {
        std::lock_guard<std::mutex> lock(dashboard_mutex_);
        
        loudness_monitor_->reset();
        meter_group_->reset_all();
        trend_tracker_->clear_history();
        
        current_metrics_ = QualityMetrics{};
        current_performance_ = PerformanceMetrics{};
        latest_report_ = QualityReport{};
        
        total_samples_processed_ = 0;
        total_frames_processed_ = 0;
        processing_times_.clear();
        processing_times_.reserve(1000);
    }
    
    void process_audio_frame(const AudioFrame& frame) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Process through monitoring components
        loudness_monitor_->process_samples(frame);
        meter_group_->process_frame(frame);
        
        // Update counters
        total_samples_processed_ += frame.sample_count() * frame.channel_count();
        total_frames_processed_++;
        
        // Track processing time
        auto end_time = std::chrono::high_resolution_clock::now();
        double processing_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        processing_times_.push_back(processing_time_ms);
        if (processing_times_.size() > 1000) {
            processing_times_.erase(processing_times_.begin());
        }
        
        // Perform quality analysis at specified interval
        auto now = std::chrono::steady_clock::now();
        if (now - last_analysis_time_ >= analysis_interval_) {
            perform_quality_analysis();
            last_analysis_time_ = now;
        }
    }
    
    QualityReport get_current_report() const {
        std::lock_guard<std::mutex> lock(dashboard_mutex_);
        return latest_report_;
    }
    
    QualityMetrics get_current_metrics() const {
        std::lock_guard<std::mutex> lock(dashboard_mutex_);
        return current_metrics_;
    }
    
    PerformanceMetrics get_performance_metrics() const {
        std::lock_guard<std::mutex> lock(dashboard_mutex_);
        return current_performance_;
    }
    
    bool is_export_ready() const {
        auto report = get_current_report();
        return report.ready_for_export;
    }
    
    std::vector<std::string> get_quality_warnings() const {
        auto report = get_current_report();
        return report.warnings;
    }
    
    std::vector<std::string> get_recommendations() const {
        auto report = get_current_report();
        return report.recommendations;
    }
    
    double get_overall_quality_score() const {
        auto metrics = get_current_metrics();
        return metrics.overall_score;
    }
    
    std::string get_quality_summary() const {
        auto report = get_current_report();
        return report.summary_text;
    }
    
    // Trend analysis
    bool is_quality_declining() const {
        return trend_tracker_->is_quality_declining();
    }
    
    double get_average_quality(std::chrono::duration<double> time_window = std::chrono::minutes(5)) const {
        return trend_tracker_->get_average_quality_score(time_window);
    }
    
    std::vector<double> get_quality_trend(size_t sample_count = 100) const {
        return trend_tracker_->get_score_trend(sample_count);
    }
    
    // Performance monitoring
    double get_real_time_factor() const {
        auto perf = get_performance_metrics();
        return perf.real_time_factor;
    }
    
    double get_cpu_usage() const {
        auto perf = get_performance_metrics();
        return perf.cpu_usage_percent;
    }
    
    void set_analysis_rate(double frequency_hz) {
        analysis_interval_ = std::chrono::duration<double>(1.0 / frequency_hz);
    }
    
    PlatformQualityTargets get_current_targets() const {
        std::lock_guard<std::mutex> lock(dashboard_mutex_);
        return current_targets_;
    }
    
private:
    void perform_quality_analysis() {
        std::lock_guard<std::mutex> lock(dashboard_mutex_);
        
        // Get current measurements
        auto loudness_measurement = loudness_monitor_->get_current_measurement();
        
        // Calculate quality metrics
        calculate_quality_metrics(loudness_measurement);
        calculate_performance_metrics();
        
        // Generate quality report
        generate_quality_report();
        
        // Add to trend tracking
        trend_tracker_->add_measurement(current_metrics_);
    }
    
    void calculate_quality_metrics(const LoudnessMeasurement& loudness) {
        current_metrics_.timestamp = std::chrono::steady_clock::now();
        
        // Loudness quality assessment
        if (loudness.valid) {
            double lufs_deviation = std::abs(loudness.integrated_lufs - current_targets_.target_lufs);
            current_metrics_.target_lufs_deviation = lufs_deviation;
            current_metrics_.loudness_compliant = lufs_deviation <= current_targets_.lufs_tolerance;
            
            // Score based on deviation from target
            current_metrics_.loudness_score = std::clamp(
                100.0 - (lufs_deviation / current_targets_.lufs_tolerance) * 20.0, 0.0, 100.0);
        }
        
        // Peak level quality assessment
        double max_peak = std::max(loudness.peak_left_dbfs, loudness.peak_right_dbfs);
        current_metrics_.peak_margin_db = current_targets_.peak_ceiling_dbfs - max_peak;
        current_metrics_.peak_compliant = max_peak <= current_targets_.peak_ceiling_dbfs;
        
        current_metrics_.peak_score = current_metrics_.peak_compliant ? 100.0 :
            std::clamp(100.0 - (max_peak - current_targets_.peak_ceiling_dbfs) * 10.0, 0.0, 100.0);
        
        // Phase quality assessment
        if (auto* correlation_meter = meter_group_->get_correlation_meter()) {
            current_metrics_.correlation_value = correlation_meter->get_correlation();
            current_metrics_.mono_compatible = correlation_meter->is_mono_compatible();
            
            current_metrics_.phase_score = current_metrics_.mono_compatible ? 100.0 :
                std::clamp(current_metrics_.correlation_value * 100.0 + 50.0, 0.0, 100.0);
        }
        
        // Dynamic range assessment (simplified)
        double peak_rms_difference = max_peak - std::max(loudness.rms_left_dbfs, loudness.rms_right_dbfs);
        current_metrics_.dr_measurement = peak_rms_difference;
        current_metrics_.dynamic_range_score = std::clamp(
            (peak_rms_difference / current_targets_.target_dynamic_range_db) * 100.0, 0.0, 100.0);
        
        // Overall quality score (weighted average)
        current_metrics_.overall_score = 
            current_metrics_.loudness_score * 0.35 +     // 35% weight on loudness
            current_metrics_.peak_score * 0.25 +         // 25% weight on peaks
            current_metrics_.phase_score * 0.20 +        // 20% weight on phase
            current_metrics_.dynamic_range_score * 0.20; // 20% weight on dynamic range
        
        // Determine quality category
        if (current_metrics_.overall_score >= 90.0) {
            current_metrics_.category = QualityCategory::EXCELLENT;
        } else if (current_metrics_.overall_score >= 70.0) {
            current_metrics_.category = QualityCategory::GOOD;
        } else if (current_metrics_.overall_score >= 50.0) {
            current_metrics_.category = QualityCategory::ACCEPTABLE;
        } else if (current_metrics_.overall_score >= 30.0) {
            current_metrics_.category = QualityCategory::POOR;
        } else {
            current_metrics_.category = QualityCategory::UNACCEPTABLE;
        }
        
        current_metrics_.valid = true;
    }
    
    void calculate_performance_metrics() {
        current_performance_.timestamp = std::chrono::steady_clock::now();
        
        // Processing time statistics
        if (!processing_times_.empty()) {
            double sum = 0.0;
            double max_time = 0.0;
            
            for (double time : processing_times_) {
                sum += time;
                max_time = std::max(max_time, time);
            }
            
            current_performance_.average_processing_time_ms = sum / processing_times_.size();
            current_performance_.max_processing_time_ms = max_time;
        }
        
        // Sample and frame counts
        current_performance_.samples_processed = total_samples_processed_.load();
        current_performance_.frames_processed = total_frames_processed_.load();
        
        // Real-time factor estimation
        if (current_performance_.average_processing_time_ms > 0.0) {
            double frame_duration_ms = (1000.0 / sample_rate_) * 1024.0; // Assuming 1024 sample frames
            current_performance_.real_time_factor = frame_duration_ms / current_performance_.average_processing_time_ms;
        }
        
        current_performance_.valid = true;
    }
    
    void generate_quality_report() {
        latest_report_.metrics = current_metrics_;
        latest_report_.performance = current_performance_;
        latest_report_.targets = current_targets_;
        latest_report_.generation_time = std::chrono::steady_clock::now();
        
        // Clear previous warnings and recommendations
        latest_report_.warnings.clear();
        latest_report_.recommendations.clear();
        latest_report_.compliance_issues.clear();
        
        // Generate warnings
        if (!current_metrics_.loudness_compliant) {
            latest_report_.warnings.push_back("Loudness not compliant with " + current_targets_.platform_name);
        }
        if (!current_metrics_.peak_compliant) {
            latest_report_.warnings.push_back("Peak levels exceed ceiling for " + current_targets_.platform_name);
        }
        if (!current_metrics_.mono_compatible) {
            latest_report_.warnings.push_back("Stereo correlation indicates mono compatibility issues");
        }
        
        // Generate recommendations
        if (current_metrics_.loudness_score < 80.0) {
            latest_report_.recommendations.push_back("Adjust master gain to target " + 
                std::to_string(current_targets_.target_lufs) + " LUFS");
        }
        if (current_metrics_.dynamic_range_score < 60.0) {
            latest_report_.recommendations.push_back("Consider reducing compression to improve dynamic range");
        }
        if (current_metrics_.phase_score < 70.0) {
            latest_report_.recommendations.push_back("Check for phase cancellation issues in stereo content");
        }
        
        // Generate summary
        std::string category_str;
        switch (current_metrics_.category) {
            case QualityCategory::EXCELLENT: category_str = "Excellent"; break;
            case QualityCategory::GOOD: category_str = "Good"; break;
            case QualityCategory::ACCEPTABLE: category_str = "Acceptable"; break;
            case QualityCategory::POOR: category_str = "Poor"; break;
            case QualityCategory::UNACCEPTABLE: category_str = "Unacceptable"; break;
        }
        
        latest_report_.summary_text = 
            "Quality: " + category_str + " (" + std::to_string(static_cast<int>(current_metrics_.overall_score)) + "%) " +
            "for " + current_targets_.platform_name + " standards";
        
        // Determine if ready for export
        latest_report_.ready_for_export = 
            current_metrics_.overall_score >= current_targets_.min_acceptable_score &&
            current_metrics_.loudness_compliant &&
            current_metrics_.peak_compliant;
    }
};

} // namespace ve::audio