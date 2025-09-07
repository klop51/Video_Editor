// GPU System Comprehensive Test Suite
// Production-ready testing framework for all GPU system components
// Validates functionality, performance, and stability across all implemented weeks

#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

// Include all GPU system components  
#include "gfx/graphics_device.hpp"
#include "gfx/vk_instance.hpp"
#include "gfx/vk_device.hpp"

namespace video_editor::gfx::test {

// Test result tracking
struct TestResult {
    std::string test_name;
    bool passed = false;
    double execution_time_ms = 0.0;
    std::string error_message;
    size_t memory_used_mb = 0;
    float gpu_utilization = 0.0f;
};

// Performance benchmarking data
struct PerformanceBenchmark {
    std::string operation_name;
    double min_time_ms = std::numeric_limits<double>::max();
    double max_time_ms = 0.0;
    double avg_time_ms = 0.0;
    double std_dev_ms = 0.0;
    size_t sample_count = 0;
    bool meets_target = false;
    double target_time_ms = 0.0;
};

// Comprehensive GPU Test Suite
class GPUTestSuite {
public:
    struct TestConfig {
        bool enable_memory_leak_detection = true;
        bool enable_performance_regression = true;
        bool enable_cross_platform_testing = true;
        bool enable_stress_testing = true;
        bool enable_error_recovery_testing = true;
        bool enable_shader_validation = true;
        uint32_t stress_test_duration_minutes = 10;
        uint32_t performance_sample_count = 100;
        bool verbose_output = true;
    };

private:
    TestConfig config_;
    std::vector<TestResult> test_results_;
    std::unordered_map<std::string, PerformanceBenchmark> benchmarks_;
    std::unique_ptr<GraphicsDevice> test_device_;
    std::chrono::steady_clock::time_point suite_start_time_;
    
    // Test tracking
    size_t tests_passed_ = 0;
    size_t tests_failed_ = 0;
    size_t tests_skipped_ = 0;

public:
    explicit GPUTestSuite(const TestConfig& config = TestConfig{});
    ~GPUTestSuite();

    // Main test execution
    bool run_all_tests();
    bool run_test_category(const std::string& category);
    void generate_test_report();
    void export_performance_data(const std::string& filename);

    // Week 1-4: Foundation Testing (Re-validation)
    bool test_graphics_device_creation();
    bool test_graphics_device_destruction();
    bool test_basic_resource_management();
    bool test_command_buffer_operations();

    // Week 5-7: Compute Pipeline Testing
    bool test_compute_shader_compilation();
    bool test_compute_pipeline_execution();
    bool test_parallel_compute_operations();
    bool test_compute_memory_management();

    // Week 8-11: Effects Pipeline Testing
    bool test_all_shader_effects();
    bool test_effect_parameter_validation();
    bool test_effect_performance_benchmarks();
    bool test_effect_quality_validation();

    // Week 12-13: Cross-Platform Testing
    bool test_vulkan_d3d11_parity();
    bool test_cross_platform_shader_compatibility();
    bool test_device_feature_detection();
    bool test_backend_switching();

    // Week 14: Advanced Effects Testing
    bool test_cinematic_effects_quality();
    bool test_color_grading_accuracy();
    bool test_spatial_effects_precision();
    bool test_temporal_effects_stability();

    // Week 15: Memory Optimization Testing
    bool test_intelligent_cache_performance();
    bool test_8k_video_processing();
    bool test_memory_pressure_handling();
    bool test_streaming_optimization();

    // Comprehensive Integration Testing
    bool test_complete_video_workflow();
    bool test_realtime_playback_pipeline();
    bool test_export_rendering_pipeline();
    bool test_multi_effect_combinations();

    // Error Handling & Recovery Testing
    bool test_device_lost_recovery();
    bool test_out_of_memory_handling();
    bool test_shader_compilation_failure_recovery();
    bool test_graceful_degradation();

    // Performance Regression Testing
    bool test_frame_timing_consistency();
    bool test_memory_usage_stability();
    bool test_gpu_utilization_efficiency();
    bool test_thermal_throttling_handling();

    // Memory Leak Detection
    bool test_memory_leak_detection();
    bool test_resource_cleanup_verification();
    bool test_long_running_stability();

    // Quality Assurance Testing
    bool test_color_accuracy_validation();
    bool test_effect_visual_quality();
    bool test_temporal_stability();
    bool test_precision_validation();

private:
    // Test utilities
    TestResult run_test(const std::string& test_name, std::function<bool()> test_func);
    void record_benchmark(const std::string& operation, double time_ms, double target_ms = 0.0);
    bool validate_performance_target(const std::string& operation, double target_ms);
    void setup_test_environment();
    void cleanup_test_environment();
    
    // Memory monitoring
    size_t get_current_memory_usage();
    bool detect_memory_leaks();
    void track_resource_allocations();
    
    // Performance monitoring
    void start_performance_monitoring();
    void stop_performance_monitoring();
    float get_gpu_utilization();
    
    // Error injection for testing
    void inject_device_lost_error();
    void inject_out_of_memory_error();
    void inject_shader_compilation_error();
};

// Specialized Test Classes for Different Components

// Shader Effects Test Suite
class ShaderEffectsTestSuite {
public:
    bool test_film_grain_quality();
    bool test_vignette_accuracy();
    bool test_chromatic_aberration_precision();
    bool test_lens_distortion_correction();
    bool test_color_wheel_accuracy();
    bool test_bezier_curve_interpolation();
    bool test_hsl_qualifier_precision();
    bool test_motion_blur_quality();
    bool test_temporal_denoising_effectiveness();
    
private:
    bool compare_with_reference_implementation(const std::string& effect_name);
    double calculate_psnr(const TextureHandle& result, const TextureHandle& reference);
    bool validate_color_accuracy(const TextureHandle& result, float tolerance_delta_e);
};

// Memory Optimization Test Suite
class MemoryOptimizationTestSuite {
public:
    bool test_cache_hit_ratio_targets();
    bool test_memory_pressure_response();
    bool test_compression_efficiency();
    bool test_streaming_buffer_management();
    bool test_8k_video_smooth_playback();
    bool test_vram_exhaustion_prevention();
    
private:
    bool simulate_8k_video_workflow();
    bool measure_cache_performance();
    bool validate_compression_quality();
};

// Cross-Platform Compatibility Test Suite
class CrossPlatformTestSuite {
public:
    bool test_vulkan_backend_functionality();
    bool test_d3d11_backend_functionality();
    bool test_backend_feature_parity();
    bool test_shader_cross_compilation();
    bool test_performance_parity();
    
private:
    bool compare_backend_outputs(GraphicsAPI api1, GraphicsAPI api2);
    bool validate_shader_compilation_across_apis();
};

// Performance Benchmark Suite
class PerformanceBenchmarkSuite {
public:
    struct BenchmarkTargets {
        // 4K Performance Targets
        double k4_30fps_basic_effects_ms = 33.33;      // 30fps = 33.33ms per frame
        double k4_60fps_optimized_effects_ms = 16.67;  // 60fps = 16.67ms per frame
        
        // 8K Performance Targets
        double k8_30fps_quality_scaling_ms = 33.33;    // 30fps with quality scaling
        
        // Effect-specific targets
        double color_grading_ms = 2.0;                 // Color grading should be <2ms
        double motion_blur_ms = 8.0;                   // Motion blur should be <8ms
        double compute_effect_ms = 5.0;                // Compute effects should be <5ms
        
        // Memory targets
        size_t max_vram_usage_mb = 7000;               // Max 7GB VRAM usage
        size_t max_system_ram_mb = 16000;              // Max 16GB system RAM
        
        // Glass-to-glass latency
        double live_preview_latency_ms = 50.0;         // <50ms for live preview
    };
    
    bool run_all_benchmarks();
    bool test_4k_30fps_performance();
    bool test_4k_60fps_performance();
    bool test_8k_30fps_performance();
    bool test_effect_performance_individual();
    bool test_memory_usage_benchmarks();
    bool test_live_preview_latency();
    
    void generate_performance_report();
    BenchmarkTargets get_targets() const { return targets_; }
    
private:
    BenchmarkTargets targets_;
    std::vector<PerformanceBenchmark> benchmark_results_;
    
    bool benchmark_video_processing(uint32_t width, uint32_t height, uint32_t fps, 
                                   const std::vector<std::string>& effects);
    double measure_effect_execution_time(const std::string& effect_name);
    size_t measure_memory_usage_during_operation(std::function<void()> operation);
};

// Error Recovery Test Suite
class ErrorRecoveryTestSuite {
public:
    bool test_device_lost_scenarios();
    bool test_out_of_memory_scenarios();
    bool test_shader_compilation_failures();
    bool test_invalid_parameter_handling();
    bool test_resource_cleanup_on_error();
    bool test_fallback_mechanism_activation();
    
private:
    bool simulate_device_lost();
    bool simulate_memory_exhaustion();
    bool simulate_shader_errors();
    bool verify_system_recovery();
};

// Quality Assurance Test Suite
class QualityAssuranceTestSuite {
public:
    struct QualityTargets {
        double color_accuracy_delta_e = 2.0;          // Delta E <2.0 for professional workflows
        double temporal_stability_threshold = 0.01;    // <1% temporal variation
        double precision_tolerance = 1e-6;             // Numerical precision tolerance
        uint32_t visual_validation_frames = 1000;      // Frames to validate visually
    };
    
    bool test_color_accuracy_professional();
    bool test_effect_visual_quality();
    bool test_temporal_stability_validation();
    bool test_numerical_precision();
    bool test_visual_artifact_detection();
    
private:
    QualityTargets targets_;
    
    double calculate_delta_e(const float* color1, const float* color2);
    bool detect_temporal_artifacts(const std::vector<TextureHandle>& frames);
    bool validate_visual_quality_against_reference();
};

// Main Test Runner
class ProductionTestRunner {
public:
    struct TestSummary {
        size_t total_tests = 0;
        size_t passed_tests = 0;
        size_t failed_tests = 0;
        size_t skipped_tests = 0;
        double total_execution_time_ms = 0.0;
        bool all_critical_tests_passed = false;
        bool performance_targets_met = false;
        bool quality_targets_met = false;
        bool stability_validated = false;
    };
    
    TestSummary run_production_validation();
    void generate_comprehensive_report();
    bool is_production_ready() const;
    
private:
    std::unique_ptr<GPUTestSuite> main_test_suite_;
    std::unique_ptr<ShaderEffectsTestSuite> effects_test_suite_;
    std::unique_ptr<MemoryOptimizationTestSuite> memory_test_suite_;
    std::unique_ptr<CrossPlatformTestSuite> cross_platform_test_suite_;
    std::unique_ptr<PerformanceBenchmarkSuite> performance_test_suite_;
    std::unique_ptr<ErrorRecoveryTestSuite> error_recovery_test_suite_;
    std::unique_ptr<QualityAssuranceTestSuite> qa_test_suite_;
    
    TestSummary test_summary_;
    
    bool validate_critical_functionality();
    bool validate_performance_requirements();
    bool validate_quality_requirements();
    bool validate_stability_requirements();
};

} // namespace video_editor::gfx::test
