#pragma once

#include "../../quality/include/quality/format_validator.hpp"
#include "../../quality/include/quality/quality_metrics.hpp"
#include "../../standards/include/standards/compliance_engine.hpp"
#include <chrono>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>

namespace ve::testing {

/**
 * Performance Testing Framework for Quality & Standards Systems
 * Comprehensive testing and benchmarking for Week 15 implementation
 */

/**
 * Performance Benchmark Suite
 * Tests performance of quality and standards validation systems
 */
class QualityPerformanceBenchmark {
public:
    enum class BenchmarkType {
        FORMAT_VALIDATION,      // Format validation speed
        QUALITY_METRICS,        // Quality measurement performance
        STANDARDS_COMPLIANCE,   // Standards compliance testing
        BATCH_PROCESSING,       // Batch file processing
        MEMORY_USAGE,           // Memory consumption
        CONCURRENT_PROCESSING,  // Multi-threaded performance
        STREAMING_ANALYSIS,     // Real-time streaming analysis
        LARGE_FILE_HANDLING    // Large file processing
    };
    
    struct BenchmarkResult {
        BenchmarkType type;
        std::string test_name;
        
        // Timing results
        std::chrono::milliseconds execution_time{0};
        std::chrono::milliseconds average_time{0};
        std::chrono::milliseconds min_time{0};
        std::chrono::milliseconds max_time{0};
        
        // Throughput results
        double files_per_second = 0.0;
        double megabytes_per_second = 0.0;
        double operations_per_second = 0.0;
        
        // Memory results
        size_t peak_memory_mb = 0;
        size_t average_memory_mb = 0;
        size_t memory_leaks_detected = 0;
        
        // Quality results
        bool all_tests_passed = false;
        uint32_t successful_operations = 0;
        uint32_t failed_operations = 0;
        double success_rate = 0.0;
        
        // Detailed metrics
        std::map<std::string, double> custom_metrics;
        std::vector<std::string> performance_issues;
        std::vector<std::string> recommendations;
    };
    
    struct BenchmarkConfig {
        uint32_t test_iterations = 10;
        uint32_t warmup_iterations = 3;
        bool measure_memory = true;
        bool detect_memory_leaks = true;
        bool concurrent_testing = true;
        uint32_t max_threads = 8;
        std::string test_data_directory = "test_data";
        std::vector<std::string> test_file_patterns = {"*.mp4", "*.mov", "*.mkv"};
    };
    
    // Benchmark execution
    static BenchmarkResult runBenchmark(BenchmarkType type, const BenchmarkConfig& config = {});
    static std::vector<BenchmarkResult> runAllBenchmarks(const BenchmarkConfig& config = {});
    static std::vector<BenchmarkResult> runBenchmarkSuite(const std::vector<BenchmarkType>& types,
                                                         const BenchmarkConfig& config = {});
    
    // Specific benchmark tests
    static BenchmarkResult benchmarkFormatValidation(const BenchmarkConfig& config);
    static BenchmarkResult benchmarkQualityMetrics(const BenchmarkConfig& config);
    static BenchmarkResult benchmarkStandardsCompliance(const BenchmarkConfig& config);
    static BenchmarkResult benchmarkBatchProcessing(const BenchmarkConfig& config);
    static BenchmarkResult benchmarkMemoryUsage(const BenchmarkConfig& config);
    static BenchmarkResult benchmarkConcurrentProcessing(const BenchmarkConfig& config);
    static BenchmarkResult benchmarkStreamingAnalysis(const BenchmarkConfig& config);
    static BenchmarkResult benchmarkLargeFileHandling(const BenchmarkConfig& config);
    
    // Performance analysis
    static std::string generatePerformanceReport(const std::vector<BenchmarkResult>& results);
    static std::vector<std::string> identifyPerformanceBottlenecks(const std::vector<BenchmarkResult>& results);
    static std::vector<std::string> getOptimizationRecommendations(const std::vector<BenchmarkResult>& results);
    
    // Comparison and regression testing
    static std::map<std::string, double> comparePerformance(const BenchmarkResult& baseline,
                                                           const BenchmarkResult& current);
    static bool detectPerformanceRegression(const std::vector<BenchmarkResult>& historical_results,
                                           const BenchmarkResult& current_result,
                                           double regression_threshold = 0.15);

private:
    static std::vector<std::string> getTestFiles(const std::string& directory,
                                                const std::vector<std::string>& patterns);
    static void measureMemoryUsage(std::function<void()> operation, BenchmarkResult& result);
    static std::chrono::milliseconds measureExecutionTime(std::function<void()> operation);
    static void runWarmupIterations(std::function<void()> operation, uint32_t iterations);
};

/**
 * Memory Analysis Tools
 * Detailed memory usage analysis for quality systems
 */
class MemoryAnalyzer {
public:
    struct MemorySnapshot {
        std::chrono::steady_clock::time_point timestamp;
        size_t total_memory_mb = 0;
        size_t heap_memory_mb = 0;
        size_t stack_memory_mb = 0;
        size_t gpu_memory_mb = 0;
        std::map<std::string, size_t> component_memory_mb;
        std::vector<std::string> active_operations;
    };
    
    struct MemoryProfile {
        std::vector<MemorySnapshot> snapshots;
        size_t peak_memory_mb = 0;
        size_t average_memory_mb = 0;
        size_t min_memory_mb = 0;
        std::chrono::milliseconds peak_time{0};
        
        // Memory leak detection
        bool memory_leaks_detected = false;
        std::vector<std::string> potential_leaks;
        size_t leaked_memory_estimate_mb = 0;
        
        // Allocation patterns
        std::map<std::string, size_t> allocation_counts;
        std::map<std::string, size_t> allocation_sizes_mb;
        std::vector<std::string> large_allocations;
        
        // Performance impact
        bool memory_pressure_detected = false;
        std::vector<std::string> memory_pressure_events;
        double memory_efficiency_score = 0.0;
    };
    
    // Memory monitoring
    static void startMemoryProfiling(const std::string& session_name);
    static void stopMemoryProfiling(const std::string& session_name);
    static MemoryProfile getMemoryProfile(const std::string& session_name);
    
    static MemorySnapshot takeMemorySnapshot(const std::string& operation_name = "");
    static MemoryProfile analyzeMemoryUsage(std::function<void()> operation,
                                           const std::string& operation_name);
    
    // Memory analysis
    static std::vector<std::string> detectMemoryLeaks(const MemoryProfile& profile);
    static std::vector<std::string> identifyMemoryBottlenecks(const MemoryProfile& profile);
    static std::vector<std::string> getMemoryOptimizationRecommendations(const MemoryProfile& profile);
    
    // Memory comparison
    static std::map<std::string, double> compareMemoryProfiles(const MemoryProfile& baseline,
                                                              const MemoryProfile& current);

private:
    static std::map<std::string, std::vector<MemorySnapshot>> active_profiles_;
    static MemorySnapshot getCurrentMemoryUsage();
    static size_t getComponentMemoryUsage(const std::string& component_name);
    static bool detectMemoryPressure();
};

/**
 * Quality System Stress Testing
 * Stress testing for quality and standards validation systems
 */
class QualityStressTester {
public:
    enum class StressTestType {
        HIGH_VOLUME,            // Large number of files
        LARGE_FILES,            // Very large individual files
        CONCURRENT_LOAD,        // Multiple simultaneous operations
        MEMORY_PRESSURE,        // Limited memory conditions
        NETWORK_LATENCY,        // Simulated network delays
        CORRUPTED_INPUT,        // Corrupted or invalid files
        EXTREME_PARAMETERS,     // Edge case parameters
        SUSTAINED_LOAD         // Long-duration testing
    };
    
    struct StressTestConfig {
        StressTestType type;
        uint32_t concurrent_threads = 8;
        uint32_t total_operations = 1000;
        std::chrono::minutes test_duration{30};
        size_t memory_limit_mb = 4096;
        std::chrono::milliseconds network_latency{0};
        double corruption_rate = 0.0;
        bool enable_failure_injection = false;
        std::string test_data_path = "stress_test_data";
    };
    
    struct StressTestResult {
        StressTestType type;
        std::string test_name;
        bool test_passed = false;
        
        // Performance under stress
        std::chrono::milliseconds total_duration{0};
        double operations_per_second = 0.0;
        double success_rate = 0.0;
        uint32_t successful_operations = 0;
        uint32_t failed_operations = 0;
        uint32_t timeout_operations = 0;
        
        // Resource usage under stress
        size_t peak_memory_mb = 0;
        double peak_cpu_usage = 0.0;
        double peak_disk_usage = 0.0;
        double peak_network_usage = 0.0;
        
        // Stability metrics
        uint32_t crashes_detected = 0;
        uint32_t hangs_detected = 0;
        uint32_t memory_leaks_detected = 0;
        uint32_t resource_exhaustion_events = 0;
        
        // Error analysis
        std::map<std::string, uint32_t> error_types;
        std::vector<std::string> critical_errors;
        std::vector<std::string> performance_degradation_events;
        std::vector<std::string> recovery_actions_taken;
    };
    
    // Stress test execution
    static StressTestResult runStressTest(const StressTestConfig& config);
    static std::vector<StressTestResult> runAllStressTests(const StressTestConfig& base_config = {});
    
    // Specific stress tests
    static StressTestResult runHighVolumeTest(const StressTestConfig& config);
    static StressTestResult runLargeFileTest(const StressTestConfig& config);
    static StressTestResult runConcurrentLoadTest(const StressTestConfig& config);
    static StressTestResult runMemoryPressureTest(const StressTestConfig& config);
    static StressTestResult runNetworkLatencyTest(const StressTestConfig& config);
    static StressTestResult runCorruptedInputTest(const StressTestConfig& config);
    static StressTestResult runExtremeParametersTest(const StressTestConfig& config);
    static StressTestResult runSustainedLoadTest(const StressTestConfig& config);
    
    // Analysis and reporting
    static std::string generateStressTestReport(const std::vector<StressTestResult>& results);
    static std::vector<std::string> identifyStabilityIssues(const std::vector<StressTestResult>& results);
    static std::vector<std::string> getReliabilityRecommendations(const std::vector<StressTestResult>& results);

private:
    static void simulateMemoryPressure(size_t limit_mb);
    static void simulateNetworkLatency(std::chrono::milliseconds latency);
    static void injectRandomFailures(double failure_rate);
    static std::vector<std::string> generateCorruptedTestFiles(const std::string& base_path, uint32_t count);
    static void monitorSystemResources(StressTestResult& result);
};

/**
 * Regression Testing Framework
 * Automated regression testing for quality and standards systems
 */
class RegressionTester {
public:
    struct RegressionTest {
        std::string test_name;
        std::string test_description;
        std::string input_file_path;
        std::string expected_output_path;
        std::map<std::string, std::string> test_parameters;
        
        // Expected results
        quality::ValidationLevel expected_validation_level;
        bool expected_compliance = true;
        std::vector<std::string> expected_issues;
        std::map<std::string, double> expected_metrics;
        
        // Tolerance settings
        double metric_tolerance = 0.01;
        bool strict_issue_matching = false;
        std::chrono::milliseconds timeout{30000};
    };
    
    struct RegressionTestResult {
        std::string test_name;
        bool test_passed = false;
        std::chrono::milliseconds execution_time{0};
        
        // Result comparison
        bool validation_level_matches = false;
        bool compliance_matches = false;
        bool issues_match = false;
        bool metrics_match = false;
        
        // Detailed differences
        std::vector<std::string> validation_differences;
        std::vector<std::string> compliance_differences;
        std::vector<std::string> issue_differences;
        std::map<std::string, std::pair<double, double>> metric_differences;  // expected, actual
        
        // Performance comparison
        bool performance_regression = false;
        double performance_change_percent = 0.0;
        
        std::vector<std::string> failure_reasons;
    };
    
    struct RegressionTestSuite {
        std::string suite_name;
        std::string suite_version;
        std::vector<RegressionTest> tests;
        std::string baseline_results_path;
        
        // Suite configuration
        bool parallel_execution = true;
        uint32_t max_parallel_tests = 4;
        bool stop_on_first_failure = false;
        bool update_baseline_on_success = false;
    };
    
    // Test execution
    static std::vector<RegressionTestResult> runRegressionSuite(const RegressionTestSuite& suite);
    static RegressionTestResult runRegressionTest(const RegressionTest& test,
                                                 const std::string& baseline_path = "");
    
    // Test management
    static RegressionTestSuite loadTestSuite(const std::string& suite_file_path);
    static void saveTestSuite(const RegressionTestSuite& suite, const std::string& file_path);
    static void generateTestSuiteFromSamples(const std::string& sample_directory,
                                            const std::string& output_suite_path);
    
    // Baseline management
    static void generateBaseline(const RegressionTestSuite& suite, const std::string& baseline_path);
    static void updateBaseline(const RegressionTestSuite& suite, const std::string& baseline_path);
    static bool compareWithBaseline(const RegressionTestResult& result, const std::string& baseline_path);
    
    // Reporting
    static std::string generateRegressionReport(const std::vector<RegressionTestResult>& results);
    static std::vector<std::string> identifyRegressions(const std::vector<RegressionTestResult>& results);
    static void exportRegressionResults(const std::vector<RegressionTestResult>& results,
                                       const std::string& output_path);

private:
    static RegressionTestResult compareResults(const quality::FormatValidationReport& actual,
                                             const quality::FormatValidationReport& expected,
                                             const RegressionTest& test);
    static bool compareMetrics(const std::map<std::string, double>& actual,
                             const std::map<std::string, double>& expected,
                             double tolerance);
    static bool compareIssues(const std::vector<std::string>& actual,
                             const std::vector<std::string>& expected,
                             bool strict_matching);
};

/**
 * Integration Testing Framework
 * End-to-end integration testing for quality systems
 */
class IntegrationTester {
public:
    struct IntegrationTestScenario {
        std::string scenario_name;
        std::string description;
        
        // Workflow steps
        std::vector<std::string> workflow_steps;
        std::vector<std::string> input_files;
        std::vector<std::string> expected_outputs;
        
        // Integration points
        bool test_format_validation = true;
        bool test_quality_metrics = true;
        bool test_standards_compliance = true;
        bool test_cross_component_integration = true;
        
        // Validation criteria
        std::map<std::string, std::string> expected_results;
        std::vector<std::string> required_capabilities;
        std::chrono::milliseconds timeout{60000};
    };
    
    struct IntegrationTestResult {
        std::string scenario_name;
        bool test_passed = false;
        std::chrono::milliseconds total_execution_time{0};
        
        // Step-by-step results
        std::vector<std::string> completed_steps;
        std::vector<std::string> failed_steps;
        std::map<std::string, std::chrono::milliseconds> step_timings;
        
        // Integration validation
        bool format_validation_integration = false;
        bool quality_metrics_integration = false;
        bool standards_compliance_integration = false;
        bool cross_component_integration = false;
        
        // Data flow validation
        bool data_consistency = false;
        bool error_propagation_correct = false;
        bool resource_cleanup_successful = false;
        
        std::vector<std::string> integration_issues;
        std::vector<std::string> data_flow_issues;
    };
    
    // Integration test execution
    static IntegrationTestResult runIntegrationTest(const IntegrationTestScenario& scenario);
    static std::vector<IntegrationTestResult> runAllIntegrationTests();
    static std::vector<IntegrationTestResult> runCriticalIntegrationTests();
    
    // Predefined test scenarios
    static IntegrationTestScenario createFullWorkflowTest();
    static IntegrationTestScenario createBatchProcessingTest();
    static IntegrationTestScenario createStreamingWorkflowTest();
    static IntegrationTestScenario createErrorHandlingTest();
    static IntegrationTestScenario createResourceManagementTest();
    
    // Reporting
    static std::string generateIntegrationReport(const std::vector<IntegrationTestResult>& results);
    static std::vector<std::string> identifyIntegrationIssues(const std::vector<IntegrationTestResult>& results);

private:
    static bool validateDataFlow(const std::string& step1, const std::string& step2);
    static bool validateResourceCleanup();
    static bool validateErrorPropagation();
};

} // namespace ve::testing
