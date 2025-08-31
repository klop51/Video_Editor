// Week 10: Parallel Processing Effects System
// Advanced GPU-accelerated video effects using compute shaders

#pragma once

#include "compute_shader_system.hpp"
#include "core/result.hpp"
#include "core/frame.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace VideoEditor::GFX {

// =============================================================================
// Forward Declarations
// =============================================================================

class ParallelEffectProcessor;
class EffectChain;
class GPUHistogramAnalyzer;
class TemporalEffectProcessor;

// =============================================================================
// Effect Processing Types
// =============================================================================

enum class ParallelEffectType {
    // Basic image processing
    GAUSSIAN_BLUR,
    BOX_BLUR,
    BILATERAL_FILTER,
    EDGE_DETECTION,
    SHARPEN,
    EMBOSS,
    
    // Color processing
    COLOR_CORRECTION,
    HUE_SATURATION,
    BRIGHTNESS_CONTRAST,
    GAMMA_CORRECTION,
    CURVES_ADJUSTMENT,
    WHITE_BALANCE,
    
    // Advanced filters
    NOISE_REDUCTION,
    CHROMATIC_ABERRATION,
    VIGNETTE,
    FILM_GRAIN,
    LENS_DISTORTION,
    MOTION_BLUR,
    
    // Spatial effects
    SCALE_TRANSFORM,
    ROTATE_TRANSFORM,
    PERSPECTIVE_TRANSFORM,
    WARP_DISTORTION,
    
    // Temporal effects
    TEMPORAL_DENOISE,
    MOTION_ESTIMATION,
    FRAME_INTERPOLATION,
    OPTICAL_FLOW,
    
    // Analysis effects
    HISTOGRAM_ANALYSIS,
    LUMINANCE_ANALYSIS,
    COLOR_ANALYSIS,
    MOTION_ANALYSIS,
    
    // Custom compute effects
    CUSTOM_COMPUTE_EFFECT
};

enum class EffectQuality {
    DRAFT = 0,      // Fastest, lowest quality
    PREVIEW = 1,    // Balanced for real-time preview
    GOOD = 2,       // Good quality for review
    HIGH = 3,       // High quality for production
    MAXIMUM = 4     // Maximum quality, may be slow
};

enum class EffectColorSpace {
    RGB,
    YUV420,
    YUV422,
    YUV444,
    HDR10,
    REC2020
};

// =============================================================================
// Effect Parameters
// =============================================================================

struct EffectParameters {
    // Basic parameters
    float intensity = 1.0f;
    float mix_amount = 1.0f;
    bool enabled = true;
    
    // Common effect parameters
    float radius = 1.0f;
    float threshold = 0.0f;
    float strength = 1.0f;
    float softness = 0.5f;
    
    // Color parameters
    float brightness = 0.0f;    // -1.0 to 1.0
    float contrast = 1.0f;      // 0.0 to 2.0
    float saturation = 1.0f;    // 0.0 to 2.0
    float hue_shift = 0.0f;     // -180.0 to 180.0
    float gamma = 1.0f;         // 0.1 to 3.0
    
    // Spatial parameters
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float rotation = 0.0f;      // degrees
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    
    // Custom parameters (for extensibility)
    std::unordered_map<std::string, float> custom_floats;
    std::unordered_map<std::string, int> custom_ints;
    std::unordered_map<std::string, bool> custom_bools;
};

struct EffectRenderInfo {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    EffectColorSpace color_space;
    EffectQuality quality;
    float time_seconds = 0.0f;
    uint32_t frame_number = 0;
    bool is_preview = false;
};

// =============================================================================
// Effect Performance Metrics
// =============================================================================

struct EffectPerformanceMetrics {
    float total_time_ms = 0.0f;
    float gpu_time_ms = 0.0f;
    float cpu_time_ms = 0.0f;
    float memory_bandwidth_gb_s = 0.0f;
    size_t memory_used_bytes = 0;
    uint32_t dispatches_count = 0;
    float gpu_utilization_percent = 0.0f;
    
    // Per-effect breakdown
    std::vector<std::pair<std::string, float>> effect_timings;
};

// =============================================================================
// Base Effect Class
// =============================================================================

class ParallelEffect {
public:
    ParallelEffect(ParallelEffectType type, const std::string& name);
    virtual ~ParallelEffect() = default;

    // Effect lifecycle
    virtual Core::Result<void> initialize(ComputeShaderSystem* compute_system) = 0;
    virtual Core::Result<EffectPerformanceMetrics> process(
        ComputeTexture* input,
        ComputeTexture* output,
        const EffectParameters& params,
        const EffectRenderInfo& render_info
    ) = 0;
    virtual void shutdown() = 0;

    // Effect properties
    ParallelEffectType get_type() const { return type_; }
    const std::string& get_name() const { return name_; }
    const std::string& get_description() const { return description_; }
    
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }
    
    // Parameter validation
    virtual bool validate_parameters(const EffectParameters& params) const { return true; }
    virtual EffectParameters get_default_parameters() const { return EffectParameters{}; }

    // Capabilities
    virtual bool supports_quality(EffectQuality quality) const { return true; }
    virtual bool supports_color_space(EffectColorSpace color_space) const { return true; }
    virtual bool requires_temporal_data() const { return false; }

protected:
    void set_description(const std::string& desc) { description_ = desc; }
    
    ParallelEffectType type_;
    std::string name_;
    std::string description_;
    bool enabled_ = true;
    ComputeShaderSystem* compute_system_ = nullptr;
};

// =============================================================================
// Gaussian Blur Effect
// =============================================================================

class GaussianBlurEffect : public ParallelEffect {
public:
    GaussianBlurEffect();
    
    Core::Result<void> initialize(ComputeShaderSystem* compute_system) override;
    Core::Result<EffectPerformanceMetrics> process(
        ComputeTexture* input,
        ComputeTexture* output,
        const EffectParameters& params,
        const EffectRenderInfo& render_info
    ) override;
    void shutdown() override;
    
    bool validate_parameters(const EffectParameters& params) const override;
    EffectParameters get_default_parameters() const override;

private:
    struct BlurConstants {
        float radius;
        float sigma;
        uint32_t kernel_size;
        uint32_t image_width;
        uint32_t image_height;
        float pad[3];
    };

    std::unique_ptr<ComputeShader> horizontal_blur_shader_;
    std::unique_ptr<ComputeShader> vertical_blur_shader_;
    std::unique_ptr<ComputeBuffer> constants_buffer_;
    std::unique_ptr<ComputeTexture> intermediate_texture_;
    
    Core::Result<void> update_blur_kernel(float radius, float sigma);
    uint32_t calculate_kernel_size(float radius) const;
};

// =============================================================================
// Color Correction Effect
// =============================================================================

class ColorCorrectionEffect : public ParallelEffect {
public:
    ColorCorrectionEffect();
    
    Core::Result<void> initialize(ComputeShaderSystem* compute_system) override;
    Core::Result<EffectPerformanceMetrics> process(
        ComputeTexture* input,
        ComputeTexture* output,
        const EffectParameters& params,
        const EffectRenderInfo& render_info
    ) override;
    void shutdown() override;
    
    bool validate_parameters(const EffectParameters& params) const override;
    EffectParameters get_default_parameters() const override;

private:
    struct ColorConstants {
        float brightness;
        float contrast;
        float saturation;
        float hue_shift;
        float gamma;
        float exposure;
        float highlights;
        float shadows;
        float whites;
        float blacks;
        float vibrance;
        float pad;
    };

    std::unique_ptr<ComputeShader> color_correction_shader_;
    std::unique_ptr<ComputeBuffer> constants_buffer_;
    
    void calculate_color_matrix(const EffectParameters& params, float* matrix) const;
};

// =============================================================================
// Edge Detection Effect
// =============================================================================

class EdgeDetectionEffect : public ParallelEffect {
public:
    EdgeDetectionEffect();
    
    Core::Result<void> initialize(ComputeShaderSystem* compute_system) override;
    Core::Result<EffectPerformanceMetrics> process(
        ComputeTexture* input,
        ComputeTexture* output,
        const EffectParameters& params,
        const EffectRenderInfo& render_info
    ) override;
    void shutdown() override;
    
    bool validate_parameters(const EffectParameters& params) const override;
    EffectParameters get_default_parameters() const override;

private:
    enum class EdgeDetectionMethod {
        SOBEL,
        PREWITT,
        ROBERTS,
        LAPLACIAN,
        CANNY
    };

    struct EdgeConstants {
        float threshold_low;
        float threshold_high;
        float strength;
        uint32_t method;
        uint32_t image_width;
        uint32_t image_height;
        float pad[2];
    };

    std::unique_ptr<ComputeShader> edge_detection_shader_;
    std::unique_ptr<ComputeBuffer> constants_buffer_;
    EdgeDetectionMethod current_method_ = EdgeDetectionMethod::SOBEL;
};

// =============================================================================
// Noise Reduction Effect
// =============================================================================

class NoiseReductionEffect : public ParallelEffect {
public:
    NoiseReductionEffect();
    
    Core::Result<void> initialize(ComputeShaderSystem* compute_system) override;
    Core::Result<EffectPerformanceMetrics> process(
        ComputeTexture* input,
        ComputeTexture* output,
        const EffectParameters& params,
        const EffectRenderInfo& render_info
    ) override;
    void shutdown() override;
    
    bool validate_parameters(const EffectParameters& params) const override;
    EffectParameters get_default_parameters() const override;
    bool requires_temporal_data() const override { return true; }

private:
    struct NoiseReductionConstants {
        float spatial_strength;
        float temporal_strength;
        float luminance_threshold;
        float chroma_threshold;
        uint32_t search_window_size;
        uint32_t patch_size;
        uint32_t temporal_radius;
        uint32_t image_width;
        uint32_t image_height;
        float pad[3];
    };

    std::unique_ptr<ComputeShader> spatial_denoise_shader_;
    std::unique_ptr<ComputeShader> temporal_denoise_shader_;
    std::unique_ptr<ComputeBuffer> constants_buffer_;
    std::vector<std::unique_ptr<ComputeTexture>> temporal_history_;
    uint32_t current_history_index_ = 0;
};

// =============================================================================
// Effect Chain Class
// =============================================================================

class EffectChain {
public:
    EffectChain() = default;
    ~EffectChain() = default;

    // Chain management
    Core::Result<void> initialize(ComputeShaderSystem* compute_system);
    void shutdown();

    // Effect management
    Core::Result<void> add_effect(std::unique_ptr<ParallelEffect> effect);
    Core::Result<void> remove_effect(size_t index);
    Core::Result<void> move_effect(size_t from_index, size_t to_index);
    void clear_effects();

    // Processing
    Core::Result<EffectPerformanceMetrics> process_chain(
        ComputeTexture* input,
        ComputeTexture* output,
        const EffectRenderInfo& render_info
    );

    // Effect parameter management
    Core::Result<void> set_effect_parameters(size_t effect_index, const EffectParameters& params);
    Core::Result<EffectParameters> get_effect_parameters(size_t effect_index) const;
    Core::Result<void> set_effect_enabled(size_t effect_index, bool enabled);

    // Chain properties
    size_t get_effect_count() const { return effects_.size(); }
    ParallelEffect* get_effect(size_t index) const;
    
    // Performance monitoring
    EffectPerformanceMetrics get_last_performance_metrics() const { return last_metrics_; }
    void enable_profiling(bool enabled) { profiling_enabled_ = enabled; }

private:
    struct EffectInstance {
        std::unique_ptr<ParallelEffect> effect;
        EffectParameters parameters;
        bool enabled = true;
    };

    Core::Result<void> allocate_intermediate_textures(uint32_t width, uint32_t height, 
                                                     EffectColorSpace color_space);
    DXGI_FORMAT get_texture_format(EffectColorSpace color_space) const;

    ComputeShaderSystem* compute_system_ = nullptr;
    std::vector<EffectInstance> effects_;
    std::vector<std::unique_ptr<ComputeTexture>> intermediate_textures_;
    
    EffectPerformanceMetrics last_metrics_;
    bool profiling_enabled_ = false;
    uint32_t last_width_ = 0;
    uint32_t last_height_ = 0;
    EffectColorSpace last_color_space_ = EffectColorSpace::RGB;
};

// =============================================================================
// Parallel Effect Processor Class
// =============================================================================

class ParallelEffectProcessor {
public:
    ParallelEffectProcessor() = default;
    ~ParallelEffectProcessor() = default;

    // System initialization
    Core::Result<void> initialize(ComputeShaderSystem* compute_system);
    void shutdown();

    // Effect creation factory
    std::unique_ptr<ParallelEffect> create_effect(ParallelEffectType type) const;
    std::vector<ParallelEffectType> get_supported_effects() const;

    // Chain management
    std::unique_ptr<EffectChain> create_effect_chain();
    
    // Batch processing
    struct BatchProcessingJob {
        std::string job_id;
        std::vector<Core::Frame> input_frames;
        std::shared_ptr<EffectChain> effect_chain;
        EffectRenderInfo render_info;
        std::function<void(const std::string&, const std::vector<Core::Frame>&, const EffectPerformanceMetrics&)> completion_callback;
    };

    Core::Result<void> submit_batch_job(const BatchProcessingJob& job);
    void cancel_batch_job(const std::string& job_id);
    void wait_for_all_jobs();

    // Performance monitoring
    EffectPerformanceMetrics get_accumulated_metrics() const { return accumulated_metrics_; }
    void reset_performance_metrics();
    void enable_system_profiling(bool enabled) { system_profiling_enabled_ = enabled; }

    // Memory management
    void flush_gpu_cache();
    size_t get_gpu_memory_usage() const;
    void cleanup_temporary_resources();

private:
    void process_batch_jobs();
    Core::Result<std::vector<Core::Frame>> process_frame_sequence(
        const std::vector<Core::Frame>& input_frames,
        EffectChain* effect_chain,
        const EffectRenderInfo& render_info
    );

    ComputeShaderSystem* compute_system_ = nullptr;
    
    // Batch processing
    std::queue<BatchProcessingJob> pending_jobs_;
    std::unordered_map<std::string, BatchProcessingJob> active_jobs_;
    std::mutex jobs_mutex_;
    std::thread batch_processing_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Performance tracking
    EffectPerformanceMetrics accumulated_metrics_;
    bool system_profiling_enabled_ = false;
    
    // Resource management
    std::vector<std::unique_ptr<ComputeTexture>> texture_pool_;
    std::mutex texture_pool_mutex_;
};

// =============================================================================
// Effect Preset System
// =============================================================================

struct EffectPreset {
    std::string name;
    std::string description;
    std::vector<std::pair<ParallelEffectType, EffectParameters>> effects;
    std::unordered_map<std::string, std::string> metadata;
};

class EffectPresetManager {
public:
    EffectPresetManager() = default;
    ~EffectPresetManager() = default;

    // Preset management
    Core::Result<void> save_preset(const std::string& name, const EffectChain& chain);
    Core::Result<EffectPreset> load_preset(const std::string& name) const;
    Core::Result<void> delete_preset(const std::string& name);
    std::vector<std::string> get_preset_names() const;

    // Apply presets
    Core::Result<void> apply_preset_to_chain(const std::string& preset_name, EffectChain& chain,
                                            ParallelEffectProcessor& processor) const;

    // Import/Export
    Core::Result<void> export_presets_to_file(const std::string& file_path) const;
    Core::Result<void> import_presets_from_file(const std::string& file_path);

    // Built-in presets
    void load_built_in_presets();

private:
    std::unordered_map<std::string, EffectPreset> presets_;
    std::mutex presets_mutex_;
};

} // namespace VideoEditor::GFX
