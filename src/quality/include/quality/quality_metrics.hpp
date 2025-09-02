#pragma once

#include "core/frame.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace ve::quality {

/**
 * Comprehensive Quality Metrics System
 * Professional-grade quality measurement and analysis tools
 */

enum class QualityMetricType {
    OBJECTIVE,              // Mathematically calculated metrics (PSNR, SSIM)
    PERCEPTUAL,            // Perceptually-based metrics (VMAF, BUTTERAUGLI)
    TECHNICAL,             // Technical compliance metrics
    SUBJECTIVE,            // Human-evaluated metrics
    TEMPORAL,              // Time-based quality metrics
    SPATIAL                // Spatial quality metrics
};

enum class QualityDomain {
    VIDEO,                 // Video quality metrics
    AUDIO,                 // Audio quality metrics
    CONTAINER,             // Container/format quality
    METADATA,              // Metadata quality
    OVERALL                // Overall content quality
};

struct QualityScore {
    double value = 0.0;             // Metric value (scale depends on metric)
    double normalized_value = 0.0;  // Normalized value (0.0-1.0 or 0-100)
    std::string unit;               // Unit of measurement
    std::string interpretation;     // Quality interpretation ("excellent", "good", etc.)
    double confidence = 1.0;        // Confidence in measurement (0.0-1.0)
    bool is_valid = false;          // Whether metric calculation succeeded
    std::string error_message;      // Error message if calculation failed
};

struct QualityMetricDefinition {
    std::string name;
    std::string description;
    QualityMetricType type;
    QualityDomain domain;
    std::string reference_standard;  // Reference standard (if applicable)
    double min_value = 0.0;         // Minimum possible value
    double max_value = 100.0;       // Maximum possible value
    std::string unit;
    bool requires_reference = false; // Whether metric needs reference content
    std::vector<std::string> parameters; // Required parameters
};

struct QualityAnalysisReport {
    std::string content_id;
    std::string content_type;       // "video", "audio", "image", "sequence"
    std::chrono::system_clock::time_point analysis_time;
    
    // Content information
    uint32_t width = 0;
    uint32_t height = 0;
    double frame_rate = 0.0;
    double duration_seconds = 0.0;
    std::string codec;
    std::string container;
    
    // Quality scores by domain
    std::map<QualityDomain, std::map<std::string, QualityScore>> quality_scores;
    
    // Overall quality assessment
    QualityScore overall_video_quality;
    QualityScore overall_audio_quality;
    QualityScore overall_technical_quality;
    QualityScore overall_perceptual_quality;
    
    // Detailed analysis
    std::vector<QualityScore> frame_quality_scores;  // Per-frame quality
    std::vector<QualityScore> temporal_quality_scores; // Temporal consistency
    std::map<std::string, std::vector<double>> quality_timeseries; // Time-based metrics
    
    // Quality issues and recommendations
    struct QualityIssue {
        std::string category;
        std::string description;
        std::string severity;      // "low", "medium", "high", "critical"
        double timestamp = -1.0;   // Timestamp where issue occurs (-1 = global)
        std::string recommendation;
    };
    std::vector<QualityIssue> quality_issues;
    
    // Statistical analysis
    struct QualityStatistics {
        double mean = 0.0;
        double median = 0.0;
        double std_deviation = 0.0;
        double min_value = 0.0;
        double max_value = 0.0;
        double percentile_5 = 0.0;
        double percentile_95 = 0.0;
    };
    std::map<std::string, QualityStatistics> metric_statistics;
    
    // Performance information
    double analysis_duration_seconds = 0.0;
    uint64_t memory_usage_bytes = 0;
};

class QualityMetricsEngine {
public:
    QualityMetricsEngine();
    ~QualityMetricsEngine();
    
    // Main analysis interface
    QualityAnalysisReport analyzeContent(const std::string& file_path,
                                       const std::string& reference_path = "");
    
    QualityAnalysisReport analyzeFrame(const Frame& frame,
                                     const Frame& reference_frame = Frame{});
    
    QualityAnalysisReport analyzeSequence(const std::vector<Frame>& frames,
                                        const std::vector<Frame>& reference_frames = {});
    
    // Specific metric calculations
    QualityScore calculatePSNR(const Frame& test_frame, const Frame& reference_frame);
    QualityScore calculateSSIM(const Frame& test_frame, const Frame& reference_frame);
    QualityScore calculateVMAF(const Frame& test_frame, const Frame& reference_frame);
    QualityScore calculateButteraugli(const Frame& test_frame, const Frame& reference_frame);
    QualityScore calculateLPIPS(const Frame& test_frame, const Frame& reference_frame);
    
    // No-reference quality metrics
    QualityScore calculateBRISQUE(const Frame& frame);
    QualityScore calculateNIQE(const Frame& frame);
    QualityScore calculateILNIQE(const Frame& frame);
    QualityScore calculateBlur(const Frame& frame);
    QualityScore calculateNoise(const Frame& frame);
    QualityScore calculateBlockiness(const Frame& frame);
    
    // Temporal quality metrics
    QualityScore calculateTemporalConsistency(const std::vector<Frame>& frames);
    QualityScore calculateFlicker(const std::vector<Frame>& frames);
    QualityScore calculateJudder(const std::vector<Frame>& frames);
    QualityScore calculateMotionSmoothness(const std::vector<Frame>& frames);
    
    // Audio quality metrics
    QualityScore calculateAudioSNR(const std::vector<float>& test_audio,
                                  const std::vector<float>& reference_audio);
    QualityScore calculateTHD(const std::vector<float>& audio_data);
    QualityScore calculatePESQ(const std::vector<float>& test_audio,
                              const std::vector<float>& reference_audio);
    QualityScore calculateSTOI(const std::vector<float>& test_audio,
                              const std::vector<float>& reference_audio);
    
    // Configuration
    void enableMetric(const std::string& metric_name, bool enabled = true);
    void setMetricParameters(const std::string& metric_name,
                           const std::map<std::string, std::string>& parameters);
    void setQualityTarget(QualityDomain domain, const std::string& target_level);
    
    // Custom metrics
    void registerCustomMetric(const std::string& metric_name,
                            const QualityMetricDefinition& definition,
                            std::function<QualityScore(const Frame&, const Frame&)> calculator);
    
    // Metric information
    std::vector<QualityMetricDefinition> getAvailableMetrics() const;
    QualityMetricDefinition getMetricDefinition(const std::string& metric_name) const;
    std::vector<std::string> getMetricsByDomain(QualityDomain domain) const;
    std::vector<std::string> getMetricsByType(QualityMetricType type) const;
    
    // Batch processing
    std::vector<QualityAnalysisReport> analyzeDirectory(const std::string& directory_path,
                                                       bool recursive = false);
    QualityAnalysisReport compareFiles(const std::vector<std::string>& file_paths);
    
    // Reporting
    std::string generateQualityReport(const QualityAnalysisReport& report,
                                    const std::string& format = "html");
    bool exportReport(const QualityAnalysisReport& report,
                     const std::string& output_path,
                     const std::string& format = "html");

private:
    struct MetricsEngineImpl;
    std::unique_ptr<MetricsEngineImpl> impl_;
    
    // Core metric implementations
    double calculatePSNRInternal(const uint8_t* test_data, const uint8_t* ref_data,
                                uint32_t width, uint32_t height, uint32_t channels);
    double calculateSSIMInternal(const uint8_t* test_data, const uint8_t* ref_data,
                               uint32_t width, uint32_t height);
    double calculateMSSSIMInternal(const uint8_t* test_data, const uint8_t* ref_data,
                                 uint32_t width, uint32_t height);
    
    // No-reference implementations
    double calculateBRISQUEInternal(const uint8_t* image_data, uint32_t width, uint32_t height);
    double calculateBlurInternal(const uint8_t* image_data, uint32_t width, uint32_t height);
    double calculateNoiseInternal(const uint8_t* image_data, uint32_t width, uint32_t height);
    
    // Temporal analysis
    double calculateOpticalFlow(const Frame& frame1, const Frame& frame2);
    std::vector<double> calculateLuminanceTimeseries(const std::vector<Frame>& frames);
    double calculateTemporalVariation(const std::vector<double>& timeseries);
    
    // Statistical helpers
    QualityAnalysisReport::QualityStatistics calculateStatistics(const std::vector<double>& values);
    double calculatePercentile(const std::vector<double>& values, double percentile);
    
    // Quality interpretation
    std::string interpretQualityScore(const std::string& metric_name, double score);
    std::string getQualityLevel(double normalized_score);
    
    // Performance optimization
    Frame downsampleForAnalysis(const Frame& frame, uint32_t max_dimension = 1920);
    bool shouldUseGPUAcceleration(const std::string& metric_name, uint32_t data_size);
};

/**
 * Professional Quality Standards Manager
 * Manages quality standards for different professional workflows
 */
class QualityStandardsManager {
public:
    enum class QualityStandard {
        BROADCAST_SD,           // Standard definition broadcast
        BROADCAST_HD,           // High definition broadcast
        BROADCAST_4K,           // 4K broadcast
        STREAMING_HD,           // HD streaming services
        STREAMING_4K,           // 4K streaming services
        CINEMA_2K,              // 2K digital cinema
        CINEMA_4K,              // 4K digital cinema
        WEB_DELIVERY,           // Web delivery standards
        MOBILE_DELIVERY,        // Mobile device delivery
        ARCHIVE_MASTER,         // Archive/master standards
        PROSUMER,               // Prosumer/enthusiast standards
        CUSTOM                  // Custom standards
    };
    
    struct QualityRequirement {
        std::string metric_name;
        double minimum_value = 0.0;
        double target_value = 0.0;
        double maximum_value = 100.0;
        bool is_mandatory = true;
        std::string description;
    };
    
    struct QualityStandardDefinition {
        QualityStandard standard;
        std::string name;
        std::string description;
        std::string organization;       // Standards organization
        std::string reference_document; // Reference document/spec
        std::vector<QualityRequirement> requirements;
        std::map<std::string, std::string> technical_specs;
    };
    
    // Standard management
    static QualityStandardDefinition getStandard(QualityStandard standard);
    static std::vector<QualityStandard> getApplicableStandards(const std::string& content_type);
    static bool validateAgainstStandard(const QualityAnalysisReport& report,
                                       QualityStandard standard);
    
    // Custom standards
    static void defineCustomStandard(const std::string& name,
                                   const QualityStandardDefinition& definition);
    static QualityStandardDefinition getCustomStandard(const std::string& name);
    
    // Compliance checking
    struct ComplianceResult {
        QualityStandard standard;
        bool is_compliant = false;
        std::vector<QualityRequirement> failed_requirements;
        std::vector<QualityRequirement> warning_requirements;
        double compliance_score = 0.0;  // 0.0-1.0
        std::string compliance_level;   // "full", "partial", "minimal", "non-compliant"
    };
    
    static ComplianceResult checkCompliance(const QualityAnalysisReport& report,
                                          QualityStandard standard);
    static std::vector<ComplianceResult> checkAllApplicableStandards(
        const QualityAnalysisReport& report);
    
    // Recommendations
    static std::vector<std::string> getQualityRecommendations(const QualityAnalysisReport& report,
                                                            QualityStandard target_standard);
    static std::map<std::string, double> getTargetMetrics(QualityStandard standard);

private:
    static std::map<QualityStandard, QualityStandardDefinition> standards_database_;
    static std::map<std::string, QualityStandardDefinition> custom_standards_;
    static void initializeStandardsDatabase();
};

/**
 * Quality Benchmark Suite
 * Standardized benchmarks for quality metric validation
 */
class QualityBenchmarkSuite {
public:
    struct BenchmarkTest {
        std::string test_name;
        std::string description;
        std::string test_content_path;
        std::string reference_content_path;
        std::map<std::string, double> expected_scores;
        double tolerance = 0.05;        // Acceptable variance in scores
    };
    
    struct BenchmarkResult {
        std::string test_name;
        bool passed = false;
        std::map<std::string, bool> metric_results;
        std::map<std::string, double> actual_scores;
        std::map<std::string, double> expected_scores;
        std::map<std::string, double> score_differences;
        double overall_accuracy = 0.0;
    };
    
    // Benchmark execution
    std::vector<BenchmarkResult> runBenchmarkSuite(const std::string& suite_name = "full");
    BenchmarkResult runSingleBenchmark(const std::string& test_name);
    
    // Benchmark management
    void addBenchmarkTest(const BenchmarkTest& test);
    void removeBenchmarkTest(const std::string& test_name);
    std::vector<std::string> getAvailableBenchmarks() const;
    
    // Performance benchmarking
    struct PerformanceBenchmark {
        std::string metric_name;
        uint32_t content_width;
        uint32_t content_height;
        uint32_t frame_count;
        double processing_time_seconds;
        uint64_t memory_usage_bytes;
        double throughput_fps;
    };
    
    std::vector<PerformanceBenchmark> runPerformanceBenchmarks();
    PerformanceBenchmark benchmarkMetric(const std::string& metric_name,
                                        uint32_t width, uint32_t height,
                                        uint32_t frame_count = 100);
    
    // Validation
    bool validateQualityEngine(QualityMetricsEngine& engine);
    std::vector<std::string> getValidationIssues() const;

private:
    std::vector<BenchmarkTest> benchmark_tests_;
    std::vector<std::string> validation_issues_;
    
    void loadStandardBenchmarks();
    bool compareScores(double actual, double expected, double tolerance);
    void generateBenchmarkContent(const std::string& content_type,
                                uint32_t width, uint32_t height,
                                const std::string& output_path);
};

/**
 * Real-time Quality Monitor
 * Continuous quality monitoring for live workflows
 */
class RealTimeQualityMonitor {
public:
    struct QualityAlert {
        std::chrono::system_clock::time_point timestamp;
        std::string metric_name;
        double current_value;
        double threshold_value;
        std::string alert_level;    // "warning", "critical"
        std::string description;
    };
    
    // Monitoring control
    void startMonitoring(const std::string& content_source);
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Threshold management
    void setQualityThreshold(const std::string& metric_name, double threshold,
                           const std::string& alert_level = "warning");
    void removeQualityThreshold(const std::string& metric_name);
    
    // Real-time analysis
    void processFrame(const Frame& frame);
    std::vector<QualityScore> getCurrentQualityScores() const;
    std::vector<QualityAlert> getActiveAlerts() const;
    
    // Alert handling
    void setAlertCallback(std::function<void(const QualityAlert&)> callback);
    void setQualityReportCallback(std::function<void(const QualityAnalysisReport&)> callback);
    
    // Statistics
    QualityAnalysisReport getSessionReport() const;
    void resetStatistics();

private:
    struct MonitorImpl;
    std::unique_ptr<MonitorImpl> monitor_impl_;
    
    void checkThresholds(const std::map<std::string, QualityScore>& current_scores);
    void triggerAlert(const QualityAlert& alert);
    void updateStatistics(const std::map<std::string, QualityScore>& scores);
};

} // namespace ve::quality
