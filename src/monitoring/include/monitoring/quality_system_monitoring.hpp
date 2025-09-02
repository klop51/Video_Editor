#pragma once

#include "quality/format_validator.hpp"
#include "quality/quality_metrics.hpp"
#include "standards/compliance_engine.hpp"
#include <chrono>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

namespace ve::monitoring {

/**
 * Quality System Reliability Monitoring
 * Real-time monitoring and health checking for quality systems
 */

/**
 * System Health Monitor
 * Monitors health and performance of quality validation systems
 */
class QualitySystemHealthMonitor {
public:
    enum class HealthStatus {
        HEALTHY,        // All systems operating normally
        WARNING,        // Some issues detected, still functional
        DEGRADED,       // Performance degraded, limited functionality
        CRITICAL,       // Major issues, unreliable operation
        FAILED          // System failure, non-functional
    };
    
    enum class ComponentType {
        FORMAT_VALIDATOR,
        QUALITY_METRICS_ENGINE,
        STANDARDS_COMPLIANCE_ENGINE,
        BROADCAST_STANDARDS,
        MEMORY_MANAGER,
        FILE_IO_SYSTEM,
        THREADING_SYSTEM,
        OVERALL_SYSTEM
    };
    
    struct HealthMetrics {
        ComponentType component;
        HealthStatus status = HealthStatus::HEALTHY;
        std::chrono::steady_clock::time_point last_check;
        
        // Performance metrics
        double cpu_usage_percent = 0.0;
        size_t memory_usage_mb = 0;
        double disk_io_mbps = 0.0;
        double network_io_mbps = 0.0;
        
        // Operational metrics
        uint64_t total_operations = 0;
        uint64_t successful_operations = 0;
        uint64_t failed_operations = 0;
        double success_rate = 0.0;
        std::chrono::milliseconds average_response_time{0};
        
        // Error tracking
        uint32_t errors_last_hour = 0;
        uint32_t warnings_last_hour = 0;
        std::vector<std::string> recent_errors;
        std::vector<std::string> recent_warnings;
        
        // Trend analysis
        std::vector<double> performance_trend;
        std::vector<double> error_rate_trend;
        bool performance_declining = false;
        bool error_rate_increasing = false;
        
        std::string status_message;
        std::vector<std::string> recommendations;
    };
    
    struct SystemHealthReport {
        std::chrono::steady_clock::time_point report_time;
        HealthStatus overall_status = HealthStatus::HEALTHY;
        
        std::map<ComponentType, HealthMetrics> component_health;
        
        // System-wide metrics
        double overall_cpu_usage = 0.0;
        size_t overall_memory_usage_mb = 0;
        double overall_disk_usage = 0.0;
        uint32_t active_threads = 0;
        uint32_t queued_operations = 0;
        
        // Reliability metrics
        std::chrono::milliseconds uptime{0};
        double system_availability = 0.0;
        uint32_t total_restarts = 0;
        std::chrono::steady_clock::time_point last_restart;
        
        // Alerts and issues
        std::vector<std::string> active_alerts;
        std::vector<std::string> critical_issues;
        std::vector<std::string> performance_issues;
        std::vector<std::string> capacity_warnings;
        
        std::string executive_summary;
        std::vector<std::string> immediate_actions_required;
    };
    
    // Health monitoring
    static void startHealthMonitoring(std::chrono::seconds check_interval = std::chrono::seconds(30));
    static void stopHealthMonitoring();
    static SystemHealthReport getCurrentHealthStatus();
    static HealthMetrics getComponentHealth(ComponentType component);
    
    // Health checks
    static HealthStatus performHealthCheck(ComponentType component);
    static HealthStatus performComprehensiveHealthCheck();
    static bool validateSystemIntegrity();
    
    // Monitoring configuration
    static void setHealthThresholds(ComponentType component,
                                   double cpu_warning_threshold,
                                   size_t memory_warning_threshold_mb,
                                   double error_rate_warning_threshold);
    
    static void enableComponent(ComponentType component, bool enabled);
    static void setMonitoringLevel(ComponentType component, uint32_t detail_level);
    
    // Alert management
    static void registerAlertHandler(std::function<void(const std::string&)> handler);
    static void triggerAlert(ComponentType component, const std::string& message, HealthStatus severity);
    static std::vector<std::string> getActiveAlerts();
    static void clearAlert(const std::string& alert_id);
    
    // Historical data
    static std::vector<SystemHealthReport> getHealthHistory(std::chrono::hours duration = std::chrono::hours(24));
    static std::map<ComponentType, std::vector<HealthMetrics>> getComponentHistory(std::chrono::hours duration);
    static void exportHealthData(const std::string& file_path, std::chrono::hours duration);

private:
    static std::atomic<bool> monitoring_active_;
    static std::mutex health_mutex_;
    static std::map<ComponentType, HealthMetrics> current_health_;
    static std::vector<SystemHealthReport> health_history_;
    static std::vector<std::function<void(const std::string&)>> alert_handlers_;
    
    static void monitoringLoop(std::chrono::seconds interval);
    static HealthMetrics checkFormatValidatorHealth();
    static HealthMetrics checkQualityMetricsHealth();
    static HealthMetrics checkStandardsComplianceHealth();
    static HealthMetrics checkMemoryManagerHealth();
    static HealthMetrics checkFileIOHealth();
    static HealthMetrics checkThreadingHealth();
    
    static void updateTrends(HealthMetrics& metrics);
    static bool detectPerformanceIssues(const HealthMetrics& metrics);
    static std::vector<std::string> generateRecommendations(const HealthMetrics& metrics);
};

/**
 * Error Tracking and Analysis
 * Comprehensive error tracking, analysis, and reporting
 */
class ErrorTracker {
public:
    enum class ErrorSeverity {
        INFO,           // Informational messages
        WARNING,        // Warning conditions
        ERROR,          // Error conditions
        CRITICAL,       // Critical errors
        FATAL          // Fatal system errors
    };
    
    enum class ErrorCategory {
        FORMAT_VALIDATION,
        QUALITY_ANALYSIS,
        STANDARDS_COMPLIANCE,
        MEMORY_MANAGEMENT,
        FILE_IO,
        NETWORK,
        THREADING,
        CONFIGURATION,
        USER_INPUT,
        SYSTEM_INTEGRATION,
        UNKNOWN
    };
    
    struct ErrorEntry {
        std::string error_id;
        std::chrono::steady_clock::time_point timestamp;
        ErrorSeverity severity;
        ErrorCategory category;
        
        std::string component_name;
        std::string function_name;
        std::string error_message;
        std::string detailed_description;
        
        // Context information
        std::string file_being_processed;
        std::map<std::string, std::string> context_parameters;
        std::vector<std::string> stack_trace;
        
        // System state
        size_t memory_usage_mb = 0;
        double cpu_usage_percent = 0.0;
        uint32_t active_threads = 0;
        std::string system_state;
        
        // Resolution tracking
        bool resolved = false;
        std::chrono::steady_clock::time_point resolution_time;
        std::string resolution_action;
        std::string resolution_notes;
        
        uint32_t occurrence_count = 1;
        std::vector<std::chrono::steady_clock::time_point> occurrence_times;
    };
    
    struct ErrorAnalysisReport {
        std::chrono::steady_clock::time_point analysis_time;
        std::chrono::hours analysis_period{24};
        
        // Error statistics
        uint32_t total_errors = 0;
        uint32_t unique_errors = 0;
        std::map<ErrorSeverity, uint32_t> errors_by_severity;
        std::map<ErrorCategory, uint32_t> errors_by_category;
        std::map<std::string, uint32_t> errors_by_component;
        
        // Top issues
        std::vector<ErrorEntry> most_frequent_errors;
        std::vector<ErrorEntry> most_recent_critical_errors;
        std::vector<ErrorEntry> unresolved_errors;
        std::vector<ErrorEntry> recurring_errors;
        
        // Trend analysis
        std::vector<std::pair<std::chrono::steady_clock::time_point, uint32_t>> error_rate_timeline;
        bool error_rate_increasing = false;
        double error_rate_change_percent = 0.0;
        
        // Pattern analysis
        std::vector<std::string> error_patterns;
        std::vector<std::string> correlation_findings;
        std::vector<std::string> system_state_correlations;
        
        // Recommendations
        std::vector<std::string> immediate_actions;
        std::vector<std::string> preventive_measures;
        std::vector<std::string> system_improvements;
        
        std::string executive_summary;
    };
    
    // Error reporting
    static std::string reportError(ErrorSeverity severity,
                                  ErrorCategory category,
                                  const std::string& component_name,
                                  const std::string& error_message,
                                  const std::string& detailed_description = "",
                                  const std::map<std::string, std::string>& context = {});
    
    static void reportException(const std::exception& ex,
                               const std::string& component_name,
                               const std::string& function_name,
                               const std::map<std::string, std::string>& context = {});
    
    // Error management
    static std::vector<ErrorEntry> getErrors(ErrorSeverity min_severity = ErrorSeverity::WARNING,
                                            std::chrono::hours duration = std::chrono::hours(24));
    
    static std::vector<ErrorEntry> getErrorsByCategory(ErrorCategory category,
                                                      std::chrono::hours duration = std::chrono::hours(24));
    
    static std::vector<ErrorEntry> getErrorsByComponent(const std::string& component_name,
                                                       std::chrono::hours duration = std::chrono::hours(24));
    
    static ErrorEntry getError(const std::string& error_id);
    static void markErrorResolved(const std::string& error_id,
                                 const std::string& resolution_action,
                                 const std::string& notes = "");
    
    // Analysis and reporting
    static ErrorAnalysisReport generateErrorAnalysis(std::chrono::hours period = std::chrono::hours(24));
    static std::vector<std::string> identifyErrorPatterns(std::chrono::hours period = std::chrono::hours(168));
    static std::vector<std::string> findErrorCorrelations(std::chrono::hours period = std::chrono::hours(24));
    
    // Configuration
    static void setErrorRetentionPeriod(std::chrono::hours period);
    static void setMaxErrorEntries(uint32_t max_entries);
    static void enableErrorCategory(ErrorCategory category, bool enabled);
    static void setMinimumSeverityLevel(ErrorSeverity min_severity);
    
    // Export and import
    static void exportErrors(const std::string& file_path,
                           std::chrono::hours duration = std::chrono::hours(168));
    static void importErrors(const std::string& file_path);
    static void clearErrorHistory(std::chrono::hours older_than = std::chrono::hours(168));

private:
    static std::mutex error_mutex_;
    static std::vector<ErrorEntry> error_history_;
    static std::map<std::string, ErrorEntry> error_index_;
    static std::atomic<uint32_t> next_error_id_;
    static std::chrono::hours retention_period_;
    static uint32_t max_error_entries_;
    
    static std::string generateErrorId();
    static void captureSystemState(ErrorEntry& entry);
    static void captureStackTrace(ErrorEntry& entry);
    static void cleanupOldErrors();
    static void updateErrorStatistics(const ErrorEntry& entry);
};

/**
 * Performance Metrics Collector
 * Collects and analyzes performance metrics for quality systems
 */
class PerformanceMetricsCollector {
public:
    enum class MetricType {
        LATENCY,            // Operation latency
        THROUGHPUT,         // Operations per second
        RESOURCE_USAGE,     // CPU, memory, disk usage
        QUEUE_LENGTH,       // Processing queue lengths
        ERROR_RATE,         // Error occurrence rate
        CACHE_HIT_RATE,     // Cache effectiveness
        CONCURRENT_OPS,     // Concurrent operations
        CUSTOM             // Custom application metrics
    };
    
    struct MetricValue {
        MetricType type;
        std::string metric_name;
        std::chrono::steady_clock::time_point timestamp;
        
        double value = 0.0;
        std::string unit;
        std::string component_name;
        std::map<std::string, std::string> tags;
        
        // Statistical data
        double min_value = 0.0;
        double max_value = 0.0;
        double average_value = 0.0;
        uint32_t sample_count = 1;
    };
    
    struct PerformanceReport {
        std::chrono::steady_clock::time_point report_time;
        std::chrono::hours report_period{1};
        
        // Aggregated metrics
        std::map<std::string, MetricValue> current_metrics;
        std::map<std::string, std::vector<MetricValue>> metric_history;
        
        // Performance summary
        double overall_latency_ms = 0.0;
        double overall_throughput_ops_sec = 0.0;
        double overall_error_rate = 0.0;
        double overall_resource_utilization = 0.0;
        
        // Performance trends
        std::map<std::string, double> metric_trends;  // percent change
        std::vector<std::string> improving_metrics;
        std::vector<std::string> degrading_metrics;
        
        // Anomaly detection
        std::vector<std::string> performance_anomalies;
        std::vector<std::string> capacity_warnings;
        std::vector<std::string> efficiency_opportunities;
        
        // Recommendations
        std::vector<std::string> optimization_recommendations;
        std::vector<std::string> scaling_recommendations;
        
        std::string performance_summary;
    };
    
    // Metric collection
    static void recordMetric(MetricType type,
                           const std::string& metric_name,
                           double value,
                           const std::string& unit = "",
                           const std::string& component_name = "",
                           const std::map<std::string, std::string>& tags = {});
    
    static void recordLatency(const std::string& operation_name,
                            std::chrono::milliseconds latency,
                            const std::string& component_name = "");
    
    static void recordThroughput(const std::string& operation_name,
                               double operations_per_second,
                               const std::string& component_name = "");
    
    static void recordResourceUsage(const std::string& resource_name,
                                   double usage_percent,
                                   const std::string& component_name = "");
    
    // Metric queries
    static std::vector<MetricValue> getMetrics(const std::string& metric_name,
                                             std::chrono::hours duration = std::chrono::hours(1));
    
    static std::vector<MetricValue> getMetricsByComponent(const std::string& component_name,
                                                        std::chrono::hours duration = std::chrono::hours(1));
    
    static std::vector<MetricValue> getMetricsByType(MetricType type,
                                                   std::chrono::hours duration = std::chrono::hours(1));
    
    // Analysis and reporting
    static PerformanceReport generatePerformanceReport(std::chrono::hours period = std::chrono::hours(1));
    static std::vector<std::string> detectPerformanceAnomalies(std::chrono::hours analysis_period = std::chrono::hours(24));
    static std::map<std::string, double> calculateMetricTrends(std::chrono::hours period = std::chrono::hours(24));
    
    // Configuration
    static void setMetricRetentionPeriod(std::chrono::hours period);
    static void setMetricSamplingRate(const std::string& metric_name, std::chrono::seconds interval);
    static void enableMetricType(MetricType type, bool enabled);
    
    // Export and visualization
    static void exportMetrics(const std::string& file_path,
                            std::chrono::hours duration = std::chrono::hours(24));
    static void exportPerformanceReport(const PerformanceReport& report,
                                       const std::string& file_path);

private:
    static std::mutex metrics_mutex_;
    static std::map<std::string, std::vector<MetricValue>> metric_data_;
    static std::chrono::hours retention_period_;
    static std::map<std::string, std::chrono::seconds> sampling_rates_;
    static std::set<MetricType> enabled_metric_types_;
    
    static void cleanupOldMetrics();
    static double calculateTrend(const std::vector<MetricValue>& values);
    static bool isAnomalousValue(const MetricValue& value, const std::vector<MetricValue>& history);
    static std::vector<std::string> generateOptimizationRecommendations(const PerformanceReport& report);
};

/**
 * Quality System Dashboard
 * Real-time dashboard for monitoring quality system status
 */
class QualitySystemDashboard {
public:
    struct DashboardData {
        std::chrono::steady_clock::time_point last_update;
        
        // System status
        QualitySystemHealthMonitor::SystemHealthReport health_status;
        ErrorTracker::ErrorAnalysisReport error_analysis;
        PerformanceMetricsCollector::PerformanceReport performance_report;
        
        // Real-time metrics
        uint32_t active_operations = 0;
        uint32_t queued_operations = 0;
        uint32_t completed_operations_today = 0;
        double current_throughput_ops_sec = 0.0;
        double current_cpu_usage = 0.0;
        size_t current_memory_usage_mb = 0;
        
        // Quality insights
        double average_quality_score_today = 0.0;
        uint32_t compliance_violations_today = 0;
        uint32_t format_validation_failures_today = 0;
        
        // Alerts and notifications
        std::vector<std::string> active_alerts;
        std::vector<std::string> recent_notifications;
        uint32_t unresolved_critical_issues = 0;
        
        std::string system_summary;
        std::vector<std::string> trending_issues;
        std::vector<std::string> recommended_actions;
    };
    
    // Dashboard management
    static DashboardData getCurrentDashboardData();
    static void updateDashboard();
    static void startDashboardUpdates(std::chrono::seconds update_interval = std::chrono::seconds(10));
    static void stopDashboardUpdates();
    
    // Dashboard configuration
    static void setUpdateInterval(std::chrono::seconds interval);
    static void enableDashboardComponent(const std::string& component_name, bool enabled);
    static void setDashboardTheme(const std::string& theme_name);
    
    // Export and sharing
    static void exportDashboardData(const std::string& file_path);
    static std::string generateDashboardHTML(const DashboardData& data);
    static std::string generateDashboardJSON(const DashboardData& data);

private:
    static std::atomic<bool> dashboard_active_;
    static DashboardData current_dashboard_data_;
    static std::mutex dashboard_mutex_;
    static std::chrono::seconds update_interval_;
    
    static void dashboardUpdateLoop();
    static void aggregateDashboardData(DashboardData& data);
    static std::string generateSystemSummary(const DashboardData& data);
    static std::vector<std::string> identifyTrendingIssues(const DashboardData& data);
    static std::vector<std::string> generateRecommendedActions(const DashboardData& data);
};

} // namespace ve::monitoring
