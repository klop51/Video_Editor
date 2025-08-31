#pragma once

#include "detailed_profiler.hpp"
#include <memory>
#include <functional>
#include <atomic>

namespace gfx {

// Forward declarations
class GraphicsDevice;
class TextureHandle;
class BufferHandle;

// Quality settings for dynamic performance scaling
struct QualitySettings {
    // Rendering quality
    float render_scale = 1.0f;              // 0.25-1.0 for lower resolution rendering
    float ui_scale = 1.0f;                  // UI can render at higher resolution than content
    int shadow_quality = 3;                 // 0-4: off, low, medium, high, ultra
    int reflection_quality = 2;             // 0-3: off, low, medium, high
    int particle_quality = 2;               // 0-3: off, low, medium, high
    
    // Effect quality levels
    int effect_quality = 3;                 // 0-4: LOD for expensive effects
    bool temporal_upscaling = false;        // DLSS-like technique
    bool adaptive_quality = true;           // Allow automatic quality adjustments
    bool motion_blur = true;                // Expensive effect that can be disabled
    bool depth_of_field = true;             // Another expensive effect
    
    // Performance targets
    float target_fps = 60.0f;               // Desired frame rate
    float min_acceptable_fps = 30.0f;       // Minimum before quality reduction
    float quality_scale_speed = 0.1f;       // How quickly to adjust quality (0.01-1.0)
    
    // Memory management
    size_t max_texture_memory_mb = 2048;    // Maximum texture memory usage
    size_t max_buffer_memory_mb = 512;      // Maximum buffer memory usage
    bool aggressive_culling = false;        // Enable aggressive frustum/occlusion culling
    
    // Advanced settings
    bool prefer_compute_shaders = true;     // Use compute for suitable operations
    bool enable_async_compute = false;      // Overlap compute with graphics
    int max_worker_threads = 0;             // 0 = auto-detect
    
    // Validation and utility
    void clamp_to_valid_ranges();
    void reset_to_defaults();
    bool is_valid() const;
    float get_overall_quality_score() const;  // 0.0-1.0 overall quality metric
};

// Performance bottleneck analysis
enum class BottleneckType {
    NONE,
    CPU_BOUND,
    GPU_BOUND,
    MEMORY_BOUND,
    IO_BOUND,
    SYNCHRONIZATION,
    OVERDRAW,
    VERTEX_PROCESSING,
    FRAGMENT_PROCESSING,
    TEXTURE_BANDWIDTH
};

struct BottleneckAnalysis {
    BottleneckType primary_bottleneck = BottleneckType::NONE;
    std::vector<BottleneckType> secondary_bottlenecks;
    float confidence_score = 0.0f;          // 0.0-1.0 how confident we are
    std::string detailed_description;
    std::vector<std::string> suggested_fixes;
    
    // Performance metrics that led to this analysis
    float avg_frame_time_ms = 0.0f;
    float cpu_utilization = 0.0f;
    float gpu_utilization = 0.0f;
    size_t memory_usage_mb = 0;
    float memory_bandwidth_usage = 0.0f;
};

// Automatic performance optimization system
class PerformanceOptimizer {
public:
    PerformanceOptimizer(DetailedProfiler& profiler, GraphicsDevice& device);
    ~PerformanceOptimizer();

    // Main optimization interface
    void update(float delta_time);
    void analyze_bottlenecks();
    void apply_automatic_optimizations();
    void suggest_quality_settings();
    void enable_adaptive_quality(bool enabled) { adaptive_quality_enabled_ = enabled; }
    
    // Quality management
    void set_quality_settings(const QualitySettings& settings);
    const QualitySettings& get_quality_settings() const { return current_quality_; }
    void reset_quality_to_defaults();
    
    // Performance targets
    void set_target_fps(float fps) { target_fps_ = fps; }
    void set_min_acceptable_fps(float fps) { min_acceptable_fps_ = fps; }
    float get_current_fps() const;
    bool is_meeting_performance_targets() const;
    
    // Analysis results
    const BottleneckAnalysis& get_last_analysis() const { return last_analysis_; }
    std::vector<std::string> get_optimization_suggestions() const;
    std::vector<std::string> get_detailed_recommendations() const;
    
    // Advanced features
    void enable_detailed_logging(bool enabled) { detailed_logging_ = enabled; }
    void set_optimization_aggressiveness(float level);  // 0.0-1.0
    void force_quality_adjustment(float scale_factor);  // Emergency quality reduction
    
    // Performance monitoring callbacks
    using PerformanceCallback = std::function<void(float fps, const BottleneckAnalysis&)>;
    void set_performance_callback(PerformanceCallback callback) { performance_callback_ = callback; }
    
    // GPU-specific optimizations
    void optimize_for_gpu_architecture();
    void enable_gpu_scheduling_optimizations(bool enabled);
    void set_memory_pressure_threshold(float threshold);  // 0.0-1.0

private:
    DetailedProfiler& profiler_;
    GraphicsDevice& device_;
    
    // Current state
    QualitySettings current_quality_;
    QualitySettings default_quality_;
    BottleneckAnalysis last_analysis_;
    
    // Performance tracking
    std::vector<float> recent_frame_times_;
    std::vector<float> recent_cpu_usage_;
    std::vector<float> recent_gpu_usage_;
    std::vector<size_t> recent_memory_usage_;
    
    // Configuration
    bool adaptive_quality_enabled_ = true;
    bool detailed_logging_ = false;
    float target_fps_ = 60.0f;
    float min_acceptable_fps_ = 30.0f;
    float optimization_aggressiveness_ = 0.5f;
    float memory_pressure_threshold_ = 0.8f;
    
    // Timing and state
    float time_since_last_analysis_ = 0.0f;
    float time_since_quality_change_ = 0.0f;
    bool quality_recently_changed_ = false;
    std::chrono::high_resolution_clock::time_point last_update_time_;
    
    // Callbacks
    PerformanceCallback performance_callback_;
    
    // Analysis methods
    void update_performance_history();
    BottleneckType analyze_primary_bottleneck() const;
    std::vector<BottleneckType> analyze_secondary_bottlenecks() const;
    float calculate_confidence_score(BottleneckType bottleneck) const;
    
    // Optimization strategies
    void apply_cpu_optimizations();
    void apply_gpu_optimizations();
    void apply_memory_optimizations();
    void apply_quality_optimizations();
    
    // Quality adjustment
    void adjust_render_scale(float target_change);
    void adjust_effect_quality(int target_change);
    void adjust_shadow_quality(int target_change);
    void toggle_expensive_effects(bool enable);
    
    // GPU-specific analysis
    bool is_gpu_memory_limited() const;
    bool is_gpu_compute_limited() const;
    bool is_gpu_bandwidth_limited() const;
    float estimate_gpu_utilization() const;
    
    // CPU analysis
    bool is_cpu_limited() const;
    float estimate_cpu_utilization() const;
    bool is_synchronization_bottleneck() const;
    
    // Memory analysis
    bool is_memory_pressure_high() const;
    size_t get_available_gpu_memory() const;
    float get_memory_bandwidth_utilization() const;
    
    // Suggestion generation
    std::vector<std::string> generate_cpu_suggestions() const;
    std::vector<std::string> generate_gpu_suggestions() const;
    std::vector<std::string> generate_memory_suggestions() const;
    std::vector<std::string> generate_quality_suggestions() const;
    
    // Utility methods
    void log_performance_data() const;
    void log_bottleneck_analysis() const;
    void notify_performance_change() const;
    bool should_adjust_quality() const;
    float calculate_quality_adjustment_factor() const;
};

// Adaptive quality system that automatically adjusts settings
class AdaptiveQualityManager {
public:
    AdaptiveQualityManager(PerformanceOptimizer& optimizer);
    
    void update(float delta_time);
    void set_enabled(bool enabled) { enabled_ = enabled; }
    void set_response_speed(float speed) { response_speed_ = speed; }  // 0.1-2.0
    
    // Quality presets
    void apply_quality_preset(const std::string& preset_name);
    void save_current_as_preset(const std::string& preset_name);
    std::vector<std::string> get_available_presets() const;
    
    // Machine learning-like adaptation
    void learn_from_user_preferences();
    void reset_learning_data();

private:
    PerformanceOptimizer& optimizer_;
    bool enabled_ = true;
    float response_speed_ = 0.5f;
    
    // Learning system
    struct QualityEvent {
        QualitySettings settings;
        float resulting_fps;
        float user_satisfaction;  // If user manually adjusted
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    std::vector<QualityEvent> quality_history_;
    std::unordered_map<std::string, QualitySettings> quality_presets_;
    
    // Adaptation logic
    void analyze_performance_trends();
    void predict_optimal_settings();
    void apply_gradual_quality_changes();
    
    void initialize_default_presets();
    QualitySettings interpolate_quality_settings(const QualitySettings& a, const QualitySettings& b, float t);
};

// GPU architecture-specific optimizations
class GPUOptimizer {
public:
    enum class GPUVendor {
        UNKNOWN,
        NVIDIA,
        AMD,
        INTEL,
        QUALCOMM,
        ARM
    };
    
    enum class GPUArchitecture {
        UNKNOWN,
        NVIDIA_PASCAL,
        NVIDIA_TURING,
        NVIDIA_AMPERE,
        NVIDIA_ADA_LOVELACE,
        AMD_GCN,
        AMD_RDNA,
        AMD_RDNA2,
        AMD_RDNA3,
        INTEL_GEN9,
        INTEL_GEN11,
        INTEL_GEN12,
        INTEL_ARC
    };
    
    GPUOptimizer(GraphicsDevice& device);
    
    void detect_gpu_capabilities();
    void apply_vendor_specific_optimizations(QualitySettings& settings);
    void apply_architecture_specific_optimizations(QualitySettings& settings);
    
    GPUVendor get_vendor() const { return vendor_; }
    GPUArchitecture get_architecture() const { return architecture_; }
    bool supports_async_compute() const { return supports_async_compute_; }
    bool supports_variable_rate_shading() const { return supports_vrs_; }
    
private:
    GraphicsDevice& device_;
    GPUVendor vendor_ = GPUVendor::UNKNOWN;
    GPUArchitecture architecture_ = GPUArchitecture::UNKNOWN;
    
    // GPU capabilities
    bool supports_async_compute_ = false;
    bool supports_vrs_ = false;
    bool supports_mesh_shaders_ = false;
    bool supports_raytracing_ = false;
    size_t gpu_memory_mb_ = 0;
    float memory_bandwidth_gbps_ = 0.0f;
    
    void detect_nvidia_optimizations(QualitySettings& settings);
    void detect_amd_optimizations(QualitySettings& settings);
    void detect_intel_optimizations(QualitySettings& settings);
    
    void apply_memory_optimizations(QualitySettings& settings);
    void apply_compute_optimizations(QualitySettings& settings);
    void apply_bandwidth_optimizations(QualitySettings& settings);
};

} // namespace gfx
