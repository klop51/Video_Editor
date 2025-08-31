// Week 10: GPU Histogram and Temporal Effects
// Advanced histogram analysis and temporal processing using compute shaders

#pragma once

#include "compute_shader_system.hpp"
#include "core/result.hpp"
#include "core/frame.hpp"
#include <memory>
#include <vector>
#include <array>
#include <functional>

namespace VideoEditor::GFX {

// =============================================================================
// Histogram Analysis Types
// =============================================================================

enum class HistogramType {
    LUMINANCE,          // Y channel histogram
    RGB_COMBINED,       // Combined RGB histogram
    RGB_SEPARATE,       // Separate R, G, B histograms
    HSV_HUE,           // Hue histogram
    HSV_SATURATION,    // Saturation histogram
    HSV_VALUE,         // Value histogram
    LAB_LIGHTNESS,     // L* channel histogram
    LAB_A_CHANNEL,     // a* channel histogram
    LAB_B_CHANNEL,     // b* channel histogram
    VECTORSCOPE,       // UV chrominance vectorscope
    WAVEFORM_Y,        // Luminance waveform
    WAVEFORM_RGB,      // RGB waveform
    EXPOSURE_ZONES     // Zone system exposure analysis
};

enum class HistogramResolution {
    LOW = 256,         // 8-bit resolution
    MEDIUM = 1024,     // 10-bit resolution
    HIGH = 4096,       // 12-bit resolution
    ULTRA = 16384      // 14-bit resolution
};

// =============================================================================
// Histogram Data Structures
// =============================================================================

struct HistogramData {
    std::vector<uint32_t> bins;
    uint32_t total_pixels = 0;
    uint32_t resolution = 256;
    float min_value = 0.0f;
    float max_value = 1.0f;
    float mean_value = 0.0f;
    float median_value = 0.0f;
    float std_deviation = 0.0f;
    
    // Statistical percentiles
    float percentile_1 = 0.0f;
    float percentile_5 = 0.0f;
    float percentile_10 = 0.0f;
    float percentile_90 = 0.0f;
    float percentile_95 = 0.0f;
    float percentile_99 = 0.0f;
};

struct VectorscopeData {
    std::vector<std::vector<uint32_t>> bins;  // 2D bins for U/V
    uint32_t width = 256;
    uint32_t height = 256;
    uint32_t total_pixels = 0;
    float max_bin_value = 0.0f;
    float center_u = 0.5f;
    float center_v = 0.5f;
    float gamut_coverage_percent = 0.0f;
};

struct WaveformData {
    std::vector<std::vector<uint32_t>> lines;  // 2D array: [line][value]
    uint32_t width = 0;   // Image width
    uint32_t height = 256; // Value resolution
    uint32_t total_pixels = 0;
    float max_line_value = 0.0f;
};

struct ExposureZoneData {
    std::array<float, 11> zone_percentages;  // Zone 0-X coverage
    float average_zone = 5.0f;
    float dynamic_range_stops = 0.0f;
    bool has_clipped_highlights = false;
    bool has_crushed_shadows = false;
    float highlight_clipping_percent = 0.0f;
    float shadow_clipping_percent = 0.0f;
};

// =============================================================================
// GPU Histogram Analyzer
// =============================================================================

class GPUHistogramAnalyzer {
public:
    GPUHistogramAnalyzer() = default;
    ~GPUHistogramAnalyzer() = default;

    // System initialization
    Core::Result<void> initialize(ComputeShaderSystem* compute_system);
    void shutdown();

    // Histogram generation
    Core::Result<HistogramData> generate_histogram(
        ComputeTexture* input_texture,
        HistogramType type,
        HistogramResolution resolution = HistogramResolution::MEDIUM
    );

    // Vectorscope analysis
    Core::Result<VectorscopeData> generate_vectorscope(
        ComputeTexture* input_texture,
        uint32_t resolution = 256
    );

    // Waveform analysis
    Core::Result<WaveformData> generate_waveform(
        ComputeTexture* input_texture,
        HistogramType waveform_type,
        uint32_t value_resolution = 256
    );

    // Exposure zone analysis
    Core::Result<ExposureZoneData> analyze_exposure_zones(
        ComputeTexture* input_texture
    );

    // Batch analysis
    struct AnalysisRequest {
        ComputeTexture* texture;
        std::vector<HistogramType> histogram_types;
        bool include_vectorscope = false;
        bool include_waveform = false;
        bool include_exposure_zones = false;
        HistogramResolution resolution = HistogramResolution::MEDIUM;
    };

    struct AnalysisResult {
        std::unordered_map<HistogramType, HistogramData> histograms;
        VectorscopeData vectorscope;
        WaveformData waveform;
        ExposureZoneData exposure_zones;
        float total_analysis_time_ms = 0.0f;
    };

    Core::Result<AnalysisResult> perform_full_analysis(const AnalysisRequest& request);

    // Real-time analysis
    void enable_real_time_analysis(bool enabled) { real_time_enabled_ = enabled; }
    void set_analysis_region(float x, float y, float width, float height);
    void clear_analysis_region();

    // Performance monitoring
    float get_last_analysis_time_ms() const { return last_analysis_time_ms_; }
    void enable_profiling(bool enabled) { profiling_enabled_ = enabled; }

private:
    struct HistogramConstants {
        uint32_t image_width;
        uint32_t image_height;
        uint32_t histogram_bins;
        uint32_t histogram_type;
        float min_value;
        float max_value;
        uint32_t region_enabled;
        float region_x;
        float region_y;
        float region_width;
        float region_height;
        float pad[1];
    };

    Core::Result<void> create_histogram_shaders();
    Core::Result<void> create_analysis_buffers(uint32_t max_resolution);
    
    void calculate_histogram_statistics(HistogramData& histogram);
    void calculate_percentiles(HistogramData& histogram);

    ComputeShaderSystem* compute_system_ = nullptr;
    
    // Compute shaders
    std::unique_ptr<ComputeShader> luminance_histogram_shader_;
    std::unique_ptr<ComputeShader> rgb_histogram_shader_;
    std::unique_ptr<ComputeShader> hsv_histogram_shader_;
    std::unique_ptr<ComputeShader> vectorscope_shader_;
    std::unique_ptr<ComputeShader> waveform_shader_;
    std::unique_ptr<ComputeShader> exposure_zone_shader_;
    std::unique_ptr<ComputeShader> statistics_shader_;
    
    // Compute buffers
    std::unique_ptr<ComputeBuffer> histogram_buffer_;
    std::unique_ptr<ComputeBuffer> vectorscope_buffer_;
    std::unique_ptr<ComputeBuffer> waveform_buffer_;
    std::unique_ptr<ComputeBuffer> constants_buffer_;
    std::unique_ptr<ComputeBuffer> statistics_buffer_;
    
    // Analysis region
    bool has_analysis_region_ = false;
    float region_x_ = 0.0f, region_y_ = 0.0f;
    float region_width_ = 1.0f, region_height_ = 1.0f;
    
    // Performance tracking
    bool profiling_enabled_ = false;
    bool real_time_enabled_ = false;
    float last_analysis_time_ms_ = 0.0f;
    uint32_t max_histogram_resolution_ = 4096;
};

// =============================================================================
// Temporal Effect Types
// =============================================================================

enum class TemporalEffectType {
    MOTION_BLUR,
    FRAME_INTERPOLATION,
    TEMPORAL_DENOISE,
    OPTICAL_FLOW,
    MOTION_COMPENSATION,
    FRAME_BLENDING,
    TIME_REMAPPING,
    STABILIZATION,
    TEMPORAL_SHARPEN,
    GHOSTING_REMOVAL
};

enum class MotionEstimationMethod {
    BLOCK_MATCHING,
    OPTICAL_FLOW_LUCAS_KANADE,
    OPTICAL_FLOW_HORN_SCHUNCK,
    PHASE_CORRELATION,
    FEATURE_MATCHING
};

// =============================================================================
// Temporal Data Structures
// =============================================================================

struct MotionVector {
    float x, y;
    float confidence;
    float magnitude;
};

struct MotionField {
    std::vector<MotionVector> vectors;
    uint32_t width;
    uint32_t height;
    uint32_t block_size;
    float average_motion;
    float max_motion;
    uint32_t frame_distance;
};

struct TemporalFrame {
    std::unique_ptr<ComputeTexture> texture;
    MotionField motion_to_next;
    MotionField motion_from_prev;
    float timestamp;
    uint32_t frame_number;
    bool is_keyframe;
};

struct TemporalBuffer {
    std::vector<std::unique_ptr<TemporalFrame>> frames;
    uint32_t capacity;
    uint32_t current_index;
    uint32_t valid_frames;
    
    void add_frame(std::unique_ptr<TemporalFrame> frame);
    TemporalFrame* get_frame(int relative_offset) const;
    void clear();
};

// =============================================================================
// Temporal Effect Processor
// =============================================================================

class TemporalEffectProcessor {
public:
    TemporalEffectProcessor() = default;
    ~TemporalEffectProcessor() = default;

    // System initialization
    Core::Result<void> initialize(ComputeShaderSystem* compute_system, uint32_t max_temporal_frames = 16);
    void shutdown();

    // Motion estimation
    Core::Result<MotionField> estimate_motion(
        ComputeTexture* frame1,
        ComputeTexture* frame2,
        MotionEstimationMethod method = MotionEstimationMethod::BLOCK_MATCHING,
        uint32_t block_size = 16
    );

    // Frame interpolation
    Core::Result<ComputeTexture*> interpolate_frame(
        ComputeTexture* frame1,
        ComputeTexture* frame2,
        float interpolation_factor,  // 0.0 = frame1, 1.0 = frame2
        const MotionField& motion_field
    );

    // Temporal effects
    Core::Result<ComputeTexture*> apply_motion_blur(
        const std::vector<ComputeTexture*>& frame_sequence,
        const std::vector<MotionField>& motion_fields,
        float blur_amount = 1.0f,
        uint32_t blur_samples = 8
    );

    Core::Result<ComputeTexture*> apply_temporal_denoise(
        ComputeTexture* current_frame,
        const TemporalBuffer& temporal_history,
        float noise_threshold = 0.1f,
        float temporal_strength = 0.8f
    );

    Core::Result<ComputeTexture*> apply_temporal_sharpen(
        ComputeTexture* current_frame,
        const TemporalBuffer& temporal_history,
        float sharpen_amount = 1.0f,
        float temporal_weight = 0.5f
    );

    // Stabilization
    struct StabilizationResult {
        std::unique_ptr<ComputeTexture> stabilized_frame;
        float transform_matrix[9];  // 3x3 homography matrix
        float confidence;
        bool success;
    };

    Core::Result<StabilizationResult> stabilize_frame(
        ComputeTexture* current_frame,
        const TemporalBuffer& temporal_history,
        float stabilization_strength = 1.0f
    );

    // Temporal buffer management
    TemporalBuffer* get_temporal_buffer() { return &temporal_buffer_; }
    void add_frame_to_history(std::unique_ptr<ComputeTexture> frame, float timestamp, uint32_t frame_number);
    void clear_temporal_history();

    // Performance monitoring
    float get_last_processing_time_ms() const { return last_processing_time_ms_; }
    void enable_profiling(bool enabled) { profiling_enabled_ = enabled; }

private:
    struct MotionEstimationConstants {
        uint32_t image_width;
        uint32_t image_height;
        uint32_t block_size;
        uint32_t search_range;
        uint32_t method;
        float threshold;
        float confidence_threshold;
        uint32_t pad[1];
    };

    struct InterpolationConstants {
        uint32_t image_width;
        uint32_t image_height;
        float interpolation_factor;
        uint32_t use_motion_compensation;
        float motion_threshold;
        float occlusion_threshold;
        uint32_t pad[2];
    };

    Core::Result<void> create_temporal_shaders();
    Core::Result<void> create_temporal_buffers();
    
    Core::Result<MotionField> compute_block_matching(ComputeTexture* frame1, ComputeTexture* frame2, uint32_t block_size);
    Core::Result<MotionField> compute_optical_flow(ComputeTexture* frame1, ComputeTexture* frame2);
    
    void detect_occlusions(const MotionField& forward_motion, const MotionField& backward_motion, 
                          std::vector<bool>& occlusion_mask);

    ComputeShaderSystem* compute_system_ = nullptr;
    
    // Temporal data
    TemporalBuffer temporal_buffer_;
    uint32_t max_temporal_frames_;
    
    // Motion estimation shaders
    std::unique_ptr<ComputeShader> block_matching_shader_;
    std::unique_ptr<ComputeShader> optical_flow_lk_shader_;
    std::unique_ptr<ComputeShader> optical_flow_hs_shader_;
    std::unique_ptr<ComputeShader> phase_correlation_shader_;
    
    // Temporal effect shaders
    std::unique_ptr<ComputeShader> frame_interpolation_shader_;
    std::unique_ptr<ComputeShader> motion_blur_shader_;
    std::unique_ptr<ComputeShader> temporal_denoise_shader_;
    std::unique_ptr<ComputeShader> temporal_sharpen_shader_;
    std::unique_ptr<ComputeShader> stabilization_shader_;
    
    // Compute buffers
    std::unique_ptr<ComputeBuffer> motion_estimation_constants_;
    std::unique_ptr<ComputeBuffer> interpolation_constants_;
    std::unique_ptr<ComputeBuffer> motion_vectors_buffer_;
    std::unique_ptr<ComputeBuffer> confidence_buffer_;
    std::unique_ptr<ComputeBuffer> occlusion_buffer_;
    
    // Intermediate textures
    std::unique_ptr<ComputeTexture> interpolated_frame_texture_;
    std::unique_ptr<ComputeTexture> motion_blur_texture_;
    std::unique_ptr<ComputeTexture> denoised_frame_texture_;
    std::unique_ptr<ComputeTexture> stabilized_frame_texture_;
    
    // Performance tracking
    bool profiling_enabled_ = false;
    float last_processing_time_ms_ = 0.0f;
};

// =============================================================================
// Temporal Analysis System
// =============================================================================

class TemporalAnalysisSystem {
public:
    TemporalAnalysisSystem() = default;
    ~TemporalAnalysisSystem() = default;

    // System initialization
    Core::Result<void> initialize(ComputeShaderSystem* compute_system);
    void shutdown();

    // Scene analysis
    struct SceneChangeInfo {
        bool is_scene_change;
        float confidence;
        float similarity_score;
        std::string change_type;  // "cut", "fade", "dissolve", etc.
    };

    Core::Result<SceneChangeInfo> detect_scene_change(
        ComputeTexture* frame1,
        ComputeTexture* frame2,
        float threshold = 0.3f
    );

    // Motion analysis
    struct MotionStatistics {
        float average_motion;
        float max_motion;
        float motion_variance;
        uint32_t static_regions_percent;
        uint32_t high_motion_regions_percent;
        std::vector<std::pair<float, float>> motion_hotspots;
    };

    Core::Result<MotionStatistics> analyze_motion_statistics(const MotionField& motion_field);

    // Temporal consistency analysis
    struct ConsistencyAnalysis {
        float temporal_coherence_score;
        float flicker_metric;
        float ghosting_metric;
        std::vector<std::pair<uint32_t, uint32_t>> inconsistent_regions;
    };

    Core::Result<ConsistencyAnalysis> analyze_temporal_consistency(
        const std::vector<ComputeTexture*>& frame_sequence
    );

    // Quality assessment
    struct TemporalQualityMetrics {
        float motion_smoothness;
        float temporal_sharpness;
        float artifacts_score;
        float overall_quality;
        std::unordered_map<std::string, float> detailed_metrics;
    };

    Core::Result<TemporalQualityMetrics> assess_temporal_quality(
        const std::vector<ComputeTexture*>& frame_sequence,
        const std::vector<MotionField>& motion_fields
    );

private:
    Core::Result<void> create_analysis_shaders();

    ComputeShaderSystem* compute_system_ = nullptr;
    
    // Analysis shaders
    std::unique_ptr<ComputeShader> scene_change_shader_;
    std::unique_ptr<ComputeShader> motion_statistics_shader_;
    std::unique_ptr<ComputeShader> consistency_analysis_shader_;
    std::unique_ptr<ComputeShader> quality_assessment_shader_;
    
    // Analysis buffers
    std::unique_ptr<ComputeBuffer> analysis_results_buffer_;
    std::unique_ptr<ComputeBuffer> statistics_buffer_;
    std::unique_ptr<ComputeBuffer> consistency_buffer_;
};

} // namespace VideoEditor::GFX
