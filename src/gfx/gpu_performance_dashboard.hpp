// GPU Performance Monitoring Dashboard
// Week 16: Real-time performance monitoring and optimization recommendations

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <queue>

namespace video_editor::gfx {

// Forward declarations
class GraphicsDevice;

// ============================================================================
// Performance Metrics Types
// ============================================================================

struct FrameTimingMetrics {
    float frame_time_ms;
    float render_time_ms;
    float compute_time_ms;
    float present_time_ms;
    float cpu_wait_time_ms;
    float gpu_idle_time_ms;
    
    uint64_t frame_number;
    std::chrono::steady_clock::time_point timestamp;
    
    // Quality metrics
    bool frame_dropped;
    bool vsync_missed;
    float adaptive_quality_factor;
};

struct MemoryUsageMetrics {
    size_t total_vram_mb;
    size_t used_vram_mb;
    size_t available_vram_mb;
    size_t texture_memory_mb;
    size_t buffer_memory_mb;
    size_t shader_memory_mb;
    size_t system_memory_mb;
    
    float fragmentation_ratio;
    uint32_t allocation_count;
    uint32_t deallocation_count;
    
    std::chrono::steady_clock::time_point timestamp;
};

struct GPUUtilizationMetrics {
    float gpu_utilization_percent;
    float compute_utilization_percent;
    float memory_bandwidth_percent;
    float temperature_celsius;
    float power_usage_watts;
    float clock_speed_mhz;
    
    uint32_t active_compute_units;
    uint32_t total_compute_units;
    
    std::chrono::steady_clock::time_point timestamp;
};

struct PipelinePerformanceMetrics {
    std::string pipeline_name;
    float execution_time_ms;
    float setup_time_ms;
    float teardown_time_ms;
    uint32_t draw_calls;
    uint32_t vertices_processed;
    uint32_t pixels_shaded;
    
    // Efficiency metrics
    float fill_rate_efficiency;
    float vertex_rate_efficiency;
    float memory_efficiency;
    
    std::chrono::steady_clock::time_point timestamp;
};

struct EffectPerformanceMetrics {
    std::string effect_name;
    float processing_time_ms;
    uint32_t input_width;
    uint32_t input_height;
    float megapixels_per_second;
    
    // Quality vs Performance
    float quality_setting;
    float quality_score;
    float performance_score;
    
    std::chrono::steady_clock::time_point timestamp;
};

// ============================================================================
// Performance Thresholds & Targets
// ============================================================================

struct PerformanceTargets {
    // Frame timing targets
    float target_frame_time_ms = 33.33f;     // 30 FPS
    float max_frame_time_ms = 50.0f;         // 20 FPS minimum
    float target_render_time_ms = 25.0f;     // Leave headroom
    
    // Memory targets
    float max_vram_usage_percent = 90.0f;
    float warning_vram_usage_percent = 80.0f;
    size_t max_allocation_size_mb = 512;
    
    // GPU utilization targets
    float target_gpu_utilization_percent = 85.0f;
    float max_temperature_celsius = 85.0f;
    float warning_temperature_celsius = 80.0f;
    
    // Quality targets
    float min_quality_score = 0.8f;
    float target_quality_score = 0.95f;
    
    // Specific effect targets
    std::unordered_map<std::string, float> effect_time_targets;
    
    PerformanceTargets() {
        // Initialize effect-specific targets
        effect_time_targets["ColorGrading"] = 8.0f;
        effect_time_targets["FilmGrain"] = 5.0f;
        effect_time_targets["Vignette"] = 3.0f;
        effect_time_targets["ChromaticAberration"] = 4.0f;
        effect_time_targets["MotionBlur"] = 12.0f;
        effect_time_targets["DepthOfField"] = 15.0f;
        effect_time_targets["Bloom"] = 10.0f;
        effect_time_targets["ToneMapping"] = 6.0f;
        effect_time_targets["Sharpen"] = 4.0f;
        effect_time_targets["Denoise"] = 8.0f;
    }
};

// ============================================================================
// Performance Alert System
// ============================================================================

enum class AlertLevel {
    Info,
    Warning,
    Critical,
    Emergency
};

struct PerformanceAlert {
    AlertLevel level;
    std::string category;
    std::string message;
    std::string recommendation;
    std::chrono::steady_clock::time_point timestamp;
    float severity_score; // 0.0 = low impact, 1.0 = high impact
    
    // Alert metadata
    std::unordered_map<std::string, std::string> metadata;
    bool user_visible;
    bool auto_actionable;
};

// ============================================================================
// Performance Dashboard Interface
// ============================================================================

class IPerformanceDashboardUI {
public:
    virtual ~IPerformanceDashboardUI() = default;
    
    // Real-time display updates
    virtual void update_frame_timing(const FrameTimingMetrics& metrics) = 0;
    virtual void update_memory_usage(const MemoryUsageMetrics& metrics) = 0;
    virtual void update_gpu_utilization(const GPUUtilizationMetrics& metrics) = 0;
    virtual void update_pipeline_performance(const PipelinePerformanceMetrics& metrics) = 0;
    virtual void update_effect_performance(const EffectPerformanceMetrics& metrics) = 0;
    
    // Alert notifications
    virtual void show_alert(const PerformanceAlert& alert) = 0;
    virtual void clear_alert(const std::string& alert_id) = 0;
    
    // Dashboard state
    virtual void set_monitoring_enabled(bool enabled) = 0;
    virtual void set_targets(const PerformanceTargets& targets) = 0;
};

// ============================================================================
// Performance Statistics Aggregator
// ============================================================================

class PerformanceStatistics {
public:
    PerformanceStatistics(size_t history_size = 1000);
    
    // Add metrics
    void add_frame_timing(const FrameTimingMetrics& metrics);
    void add_memory_usage(const MemoryUsageMetrics& metrics);
    void add_gpu_utilization(const GPUUtilizationMetrics& metrics);
    void add_pipeline_performance(const PipelinePerformanceMetrics& metrics);
    void add_effect_performance(const EffectPerformanceMetrics& metrics);
    
    // Statistical analysis
    struct TimingStatistics {
        float mean_ms;
        float median_ms;
        float min_ms;
        float max_ms;
        float std_dev_ms;
        float percentile_95_ms;
        float percentile_99_ms;
        uint32_t sample_count;
    };
    
    TimingStatistics get_frame_timing_stats(std::chrono::seconds window = std::chrono::seconds(60)) const;
    TimingStatistics get_effect_timing_stats(const std::string& effect_name, 
                                            std::chrono::seconds window = std::chrono::seconds(60)) const;
    
    // Memory analysis
    struct MemoryStatistics {
        float mean_usage_percent;
        float peak_usage_percent;
        float fragmentation_ratio;
        uint32_t allocation_rate_per_second;
        size_t largest_allocation_mb;
        uint32_t out_of_memory_events;
    };
    
    MemoryStatistics get_memory_stats(std::chrono::seconds window = std::chrono::seconds(300)) const;
    
    // Performance trends
    enum class TrendDirection { Improving, Stable, Degrading, Unknown };
    TrendDirection get_performance_trend(std::chrono::seconds window = std::chrono::seconds(600)) const;
    
    // Bottleneck analysis
    enum class BottleneckType { CPU, GPU_Compute, GPU_Memory, Memory_Bandwidth, Unknown };
    BottleneckType identify_primary_bottleneck() const;
    
    void clear_history();
    void set_history_size(size_t size);
    
private:
    mutable std::mutex data_mutex_;
    size_t max_history_size_;
    
    std::vector<FrameTimingMetrics> frame_timing_history_;
    std::vector<MemoryUsageMetrics> memory_usage_history_;
    std::vector<GPUUtilizationMetrics> gpu_utilization_history_;
    std::vector<PipelinePerformanceMetrics> pipeline_performance_history_;
    std::vector<EffectPerformanceMetrics> effect_performance_history_;
    
    template<typename T>
    TimingStatistics calculate_timing_stats(const std::vector<T>& data,
                                           std::function<float(const T&)> extract_time,
                                           std::chrono::seconds window) const;
    
    void trim_history();
};

// ============================================================================
// Performance Optimizer Recommendations
// ============================================================================

class PerformanceOptimizer {
public:
    PerformanceOptimizer(const PerformanceTargets& targets);
    
    struct OptimizationRecommendation {
        std::string category;
        std::string description;
        std::string action;
        float expected_improvement_percent;
        float confidence_score;
        bool requires_user_action;
        bool auto_applicable;
        
        std::unordered_map<std::string, std::string> parameters;
    };
    
    // Analyze current performance and generate recommendations
    std::vector<OptimizationRecommendation> analyze_performance(
        const PerformanceStatistics& stats) const;
    
    // Specific analysis functions
    std::vector<OptimizationRecommendation> analyze_frame_timing(
        const PerformanceStatistics::TimingStatistics& timing) const;
    
    std::vector<OptimizationRecommendation> analyze_memory_usage(
        const PerformanceStatistics::MemoryStatistics& memory) const;
    
    std::vector<OptimizationRecommendation> analyze_gpu_utilization(
        const GPUUtilizationMetrics& gpu) const;
    
    std::vector<OptimizationRecommendation> analyze_effect_performance(
        const std::unordered_map<std::string, PerformanceStatistics::TimingStatistics>& effects) const;
    
    // Auto-optimization
    bool can_auto_optimize(const OptimizationRecommendation& recommendation) const;
    bool apply_auto_optimization(const OptimizationRecommendation& recommendation);
    
private:
    PerformanceTargets targets_;
    
    OptimizationRecommendation create_quality_reduction_recommendation(
        const std::string& effect_name, float current_time, float target_time) const;
    
    OptimizationRecommendation create_memory_optimization_recommendation(
        const PerformanceStatistics::MemoryStatistics& memory) const;
    
    OptimizationRecommendation create_gpu_utilization_recommendation(
        const GPUUtilizationMetrics& gpu) const;
};

// ============================================================================
// Main Performance Dashboard
// ============================================================================

class PerformanceDashboard {
public:
    PerformanceDashboard(GraphicsDevice* device, const PerformanceTargets& targets = {});
    ~PerformanceDashboard();
    
    // Monitoring control
    void start_monitoring();
    void stop_monitoring();
    bool is_monitoring() const { return monitoring_active_; }
    
    // UI integration
    void register_ui(std::shared_ptr<IPerformanceDashboardUI> ui);
    void unregister_ui(std::shared_ptr<IPerformanceDashboardUI> ui);
    
    // Manual metric recording
    void record_frame_timing(const FrameTimingMetrics& metrics);
    void record_memory_usage(const MemoryUsageMetrics& metrics);
    void record_gpu_utilization(const GPUUtilizationMetrics& metrics);
    void record_pipeline_performance(const PipelinePerformanceMetrics& metrics);
    void record_effect_performance(const EffectPerformanceMetrics& metrics);
    
    // Performance analysis
    const PerformanceStatistics& get_statistics() const { return statistics_; }
    std::vector<PerformanceOptimizer::OptimizationRecommendation> get_recommendations() const;
    
    // Alert management
    void register_alert_callback(std::function<void(const PerformanceAlert&)> callback);
    std::vector<PerformanceAlert> get_active_alerts() const;
    void acknowledge_alert(const std::string& alert_id);
    
    // Configuration
    void set_targets(const PerformanceTargets& targets);
    const PerformanceTargets& get_targets() const { return targets_; }
    
    void set_monitoring_frequency(std::chrono::milliseconds frequency);
    void set_ui_update_frequency(std::chrono::milliseconds frequency);
    
    // Export/Import
    bool export_statistics(const std::string& file_path) const;
    bool import_statistics(const std::string& file_path);
    
    // System integration
    void integrate_with_error_handler(class GPUErrorHandler* error_handler);
    void integrate_with_memory_optimizer(class GPUMemoryOptimizer* memory_optimizer);
    
private:
    GraphicsDevice* device_;
    PerformanceTargets targets_;
    PerformanceStatistics statistics_;
    PerformanceOptimizer optimizer_;
    
    // Monitoring
    std::atomic<bool> monitoring_active_;
    std::atomic<bool> monitoring_enabled_;
    std::thread monitoring_thread_;
    std::chrono::milliseconds monitoring_frequency_;
    std::chrono::milliseconds ui_update_frequency_;
    
    // Integration with other components
    class GPUErrorHandler* error_handler_;
    class GPUMemoryOptimizer* memory_optimizer_;
    
    // UI management
    std::vector<std::weak_ptr<IPerformanceDashboardUI>> ui_instances_;
    mutable std::mutex ui_mutex_;
    
    // Alert system
    std::vector<PerformanceAlert> active_alerts_;
    std::vector<std::function<void(const PerformanceAlert&)>> alert_callbacks_;
    mutable std::mutex alerts_mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> alert_timestamps_;
    
    // Monitoring implementation
    void monitoring_loop();
    void collect_frame_timing_metrics();
    void collect_memory_usage_metrics();
    void collect_gpu_utilization_metrics();
    void update_ui_instances();
    
    // Alert generation
    void check_performance_thresholds();
    void generate_alert(AlertLevel level, const std::string& category,
                       const std::string& message, const std::string& recommendation,
                       float severity = 0.5f);
    void clear_expired_alerts();
    
    // Utility functions
    FrameTimingMetrics collect_current_frame_timing();
    MemoryUsageMetrics collect_current_memory_usage();
    GPUUtilizationMetrics collect_current_gpu_utilization();
    
    std::string generate_alert_id(const std::string& category, const std::string& message) const;
    void notify_alert_callbacks(const PerformanceAlert& alert);
};

// ============================================================================
// Performance Profiler Integration
// ============================================================================

class PerformanceProfiler {
public:
    PerformanceProfiler(PerformanceDashboard* dashboard);
    ~PerformanceProfiler();
    
    // RAII profiling scopes
    class ProfileScope {
    public:
        ProfileScope(PerformanceProfiler* profiler, const std::string& name);
        ~ProfileScope();
        
        void add_metadata(const std::string& key, const std::string& value);
        
    private:
        PerformanceProfiler* profiler_;
        std::string scope_name_;
        std::chrono::high_resolution_clock::time_point start_time_;
        std::unordered_map<std::string, std::string> metadata_;
    };
    
    // Manual profiling
    void begin_scope(const std::string& name);
    void end_scope(const std::string& name);
    
    // Effect profiling
    void begin_effect(const std::string& effect_name, uint32_t width, uint32_t height);
    void end_effect(const std::string& effect_name);
    
    // Pipeline profiling
    void begin_pipeline(const std::string& pipeline_name);
    void end_pipeline(const std::string& pipeline_name, uint32_t draw_calls, uint32_t vertices);
    
private:
    PerformanceDashboard* dashboard_;
    
    struct ActiveScope {
        std::string name;
        std::chrono::high_resolution_clock::time_point start_time;
        std::unordered_map<std::string, std::string> metadata;
    };
    
    std::unordered_map<std::string, ActiveScope> active_scopes_;
    std::mutex scopes_mutex_;
};

// ============================================================================
// Utility Macros
// ============================================================================

#define PERF_SCOPE(profiler, name) \
    video_editor::gfx::PerformanceProfiler::ProfileScope _prof_scope(profiler, name)

#define PERF_EFFECT(profiler, effect_name, width, height) \
    profiler->begin_effect(effect_name, width, height); \
    auto _effect_cleanup = [&]() { profiler->end_effect(effect_name); }; \
    std::unique_ptr<void, decltype(_effect_cleanup)> _effect_guard(nullptr, _effect_cleanup)

#define PERF_PIPELINE(profiler, pipeline_name) \
    profiler->begin_pipeline(pipeline_name); \
    auto _pipeline_cleanup = [&](uint32_t draws, uint32_t verts) { \
        profiler->end_pipeline(pipeline_name, draws, verts); \
    }

} // namespace video_editor::gfx
