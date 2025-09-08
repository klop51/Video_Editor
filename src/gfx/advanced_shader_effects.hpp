#pragma once

#include "../core/types.hpp"
#include "../core/math.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace gfx {

// Forward declarations
class GraphicsDevice;
class Shader;
class Texture;
class Buffer;
class Pipeline;

// Effect parameter types for professional control
struct ColorWheelParams {
    // Lift (shadows), Gamma (midtones), Gain (highlights)
    Vec3 lift = {0.0f, 0.0f, 0.0f};
    Vec3 gamma = {1.0f, 1.0f, 1.0f};
    Vec3 gain = {1.0f, 1.0f, 1.0f};
    
    // Offset (master offset), Power (contrast), Slope (brightness)
    Vec3 offset = {0.0f, 0.0f, 0.0f};
    float power = 1.0f;
    float slope = 1.0f;
    
    // Saturation controls
    float lift_saturation = 1.0f;
    float gamma_saturation = 1.0f;
    float gain_saturation = 1.0f;
    
    void reset_to_defaults();
    bool is_identity() const;
};

struct BezierCurve {
    // Control points for cubic Bezier curve
    Vec2 p0 = {0.0f, 0.0f};   // Start point
    Vec2 p1 = {0.33f, 0.33f}; // First control point
    Vec2 p2 = {0.66f, 0.66f}; // Second control point
    Vec2 p3 = {1.0f, 1.0f};   // End point
    
    // Curve properties
    bool enabled = false;
    float strength = 1.0f;
    
    // Evaluation
    float evaluate(float t) const;
    void set_linear() { p1 = {0.33f, 0.33f}; p2 = {0.66f, 0.66f}; }
    void set_s_curve() { p1 = {0.2f, 0.4f}; p2 = {0.8f, 0.6f}; }
};

struct ColorCurvesParams {
    BezierCurve master_curve;
    BezierCurve red_curve;
    BezierCurve green_curve;
    BezierCurve blue_curve;
    BezierCurve hue_vs_sat_curve;
    BezierCurve hue_vs_lum_curve;
    BezierCurve sat_vs_sat_curve;
    BezierCurve lum_vs_sat_curve;
    
    void reset_to_defaults();
    bool is_identity() const;
};

struct HSLQualifierParams {
    // Hue range selection
    float hue_center = 0.0f;      // 0-360 degrees
    float hue_range = 60.0f;      // Selection range
    float hue_softness = 0.1f;    // Edge softness
    
    // Saturation range
    float sat_min = 0.0f;         // 0-1
    float sat_max = 1.0f;
    float sat_softness = 0.1f;
    
    // Luminance range
    float lum_min = 0.0f;         // 0-1
    float lum_max = 1.0f;
    float lum_softness = 0.1f;
    
    // Selection mode
    bool invert_selection = false;
    float selection_strength = 1.0f;
    
    bool is_identity() const;
};

struct LensDistortionParams {
    float barrel_distortion = 0.0f;    // Negative for pincushion
    float asymmetric_distortion = 0.0f; // Different X/Y distortion
    float chromatic_aberration = 0.0f;  // Color fringing
    Vec2 center_offset = {0.0f, 0.0f};  // Distortion center
    float zoom = 1.0f;                  // Compensating zoom
    
    bool is_identity() const;
};

struct FilmGrainParams {
    float intensity = 0.0f;         // 0-1 grain strength
    float size = 1.0f;              // Grain size multiplier
    float color_amount = 0.5f;      // Color vs monochrome grain
    float response_curve = 1.0f;    // Non-linear response
    bool highlight_desaturate = true; // Reduce grain in highlights
    
    // Advanced grain characteristics
    float red_multiplier = 1.0f;
    float green_multiplier = 1.0f;
    float blue_multiplier = 1.0f;
    uint32_t random_seed = 12345;
    
    bool is_identity() const;
};

struct VignetteParams {
    float radius = 0.8f;            // Vignette size
    float softness = 0.5f;          // Edge falloff
    float strength = 0.5f;          // Effect intensity
    Vec2 center = {0.5f, 0.5f};     // Vignette center
    
    // Shape control
    float roundness = 1.0f;         // 0=rectangular, 1=circular
    float feather = 0.0f;           // Additional edge feathering
    
    // Color tinting
    Vec3 color_tint = {0.0f, 0.0f, 0.0f}; // RGB color bias
    
    bool is_identity() const;
};

struct MotionBlurParams {
    int sample_count = 8;           // Quality vs performance
    float shutter_angle = 180.0f;   // Motion blur amount
    Vec2 global_motion = {0.0f, 0.0f}; // Global motion vector
    float per_pixel_motion_strength = 1.0f; // Motion vector influence
    
    // Advanced controls
    bool use_depth_weighting = false;
    float depth_threshold = 0.01f;
    float max_blur_radius = 32.0f;
    
    bool is_identity() const;
};

struct ChromaticAberrationParams {
    float strength = 0.0f;          // Overall aberration amount
    Vec2 red_offset = {0.0f, 0.0f}; // Red channel displacement
    Vec2 blue_offset = {0.0f, 0.0f}; // Blue channel displacement
    float edge_falloff = 1.0f;      // Stronger at edges
    
    bool is_identity() const;
};

// Professional color grading effect processor
class ColorGradingProcessor {
public:
    ColorGradingProcessor(GraphicsDevice& device);
    ~ColorGradingProcessor();

    // Primary color correction tools
    void set_color_wheels(const ColorWheelParams& params) { color_wheels_ = params; }
    void set_color_curves(const ColorCurvesParams& params) { color_curves_ = params; }
    void set_hsl_qualifier(const HSLQualifierParams& params) { hsl_qualifier_ = params; }
    
    // Basic color adjustments
    void set_exposure(float exposure) { exposure_ = exposure; }
    void set_contrast(float contrast) { contrast_ = contrast; }
    void set_highlights(float highlights) { highlights_ = highlights; }
    void set_shadows(float shadows) { shadows_ = shadows; }
    void set_whites(float whites) { whites_ = whites; }
    void set_blacks(float blacks) { blacks_ = blacks; }
    void set_saturation(float saturation) { saturation_ = saturation; }
    void set_vibrance(float vibrance) { vibrance_ = vibrance; }
    
    // Temperature and tint
    void set_temperature(float temperature) { temperature_ = temperature; } // 2000K-12000K
    void set_tint(float tint) { tint_ = tint; } // Magenta-Green bias
    
    // Advanced controls
    void set_clarity(float clarity) { clarity_ = clarity; }
    void set_dehaze(float dehaze) { dehaze_ = dehaze; }
    void set_texture(float texture) { texture_ = texture; }
    
    // LUT support
    void set_lookup_table(std::shared_ptr<Texture> lut_texture) { lut_texture_ = lut_texture; }
    void set_lut_strength(float strength) { lut_strength_ = strength; }
    
    // Processing
    void apply(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    void compile_shaders();
    
    // Presets and serialization
    void save_preset(const std::string& name) const;
    void load_preset(const std::string& name);
    std::vector<std::string> get_available_presets() const;
    
    // Real-time preview
    void enable_real_time_preview(bool enabled) { real_time_preview_ = enabled; }
    bool is_real_time_preview_enabled() const { return real_time_preview_; }

private:
    GraphicsDevice& device_;
    
    // Effect parameters
    ColorWheelParams color_wheels_;
    ColorCurvesParams color_curves_;
    HSLQualifierParams hsl_qualifier_;
    
    // Basic adjustments
    float exposure_ = 0.0f;
    float contrast_ = 0.0f;
    float highlights_ = 0.0f;
    float shadows_ = 0.0f;
    float whites_ = 0.0f;
    float blacks_ = 0.0f;
    float saturation_ = 0.0f;
    float vibrance_ = 0.0f;
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
    float clarity_ = 0.0f;
    float dehaze_ = 0.0f;
    float texture_ = 0.0f;
    
    // LUT processing
    std::shared_ptr<Texture> lut_texture_;
    float lut_strength_ = 1.0f;
    
    // GPU resources
    std::shared_ptr<Shader> color_grading_shader_;
    std::shared_ptr<Buffer> constants_buffer_;
    std::shared_ptr<Pipeline> grading_pipeline_;
    
    // Configuration
    bool real_time_preview_ = true;
    
    // Internal methods
    void update_constants_buffer();
    void create_gpu_resources();
    Vec3 temperature_to_rgb(float temperature) const;
    Vec3 apply_lift_gamma_gain(const Vec3& color, const ColorWheelParams& params) const;
};

// Cinematic effects processor for film-style post-processing
class CinematicEffectsProcessor {
public:
    CinematicEffectsProcessor(GraphicsDevice& device);
    ~CinematicEffectsProcessor();

    // Film emulation effects
    void set_film_grain(const FilmGrainParams& params) { film_grain_ = params; }
    void set_vignette(const VignetteParams& params) { vignette_ = params; }
    void set_chromatic_aberration(const ChromaticAberrationParams& params) { chromatic_aberration_ = params; }
    
    // Lens effects
    void set_lens_distortion(const LensDistortionParams& params) { lens_distortion_ = params; }
    void set_bokeh_quality(int quality) { bokeh_quality_ = quality; }
    void set_bloom_threshold(float threshold) { bloom_threshold_ = threshold; }
    void set_bloom_intensity(float intensity) { bloom_intensity_ = intensity; }
    
    // Film look presets
    void apply_film_look_preset(const std::string& preset_name);
    void create_custom_film_look(const std::string& name);
    
    // Processing pipeline
    void apply(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    void apply_film_grain_only(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    void apply_lens_effects_only(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    
    // Quality and performance
    void set_quality_preset(const std::string& preset); // "Low", "Medium", "High", "Ultra"
    void enable_temporal_optimization(bool enabled) { temporal_optimization_ = enabled; }

private:
    GraphicsDevice& device_;
    
    // Effect parameters
    FilmGrainParams film_grain_;
    VignetteParams vignette_;
    ChromaticAberrationParams chromatic_aberration_;
    LensDistortionParams lens_distortion_;
    
    // Additional effects
    int bokeh_quality_ = 3;
    float bloom_threshold_ = 1.0f;
    float bloom_intensity_ = 0.2f;
    
    // GPU resources
    std::shared_ptr<Shader> film_grain_shader_;
    std::shared_ptr<Shader> vignette_shader_;
    std::shared_ptr<Shader> chromatic_aberration_shader_;
    std::shared_ptr<Shader> lens_distortion_shader_;
    std::shared_ptr<Shader> composite_shader_;
    
    std::shared_ptr<Buffer> constants_buffer_;
    std::vector<std::shared_ptr<Pipeline>> effect_pipelines_;
    
    // Temporary textures for multi-pass effects
    std::shared_ptr<Texture> temp_texture_1_;
    std::shared_ptr<Texture> temp_texture_2_;
    
    // Configuration
    bool temporal_optimization_ = true;
    std::string current_quality_preset_ = "High";
    
    // Film look presets
    std::unordered_map<std::string, std::function<void()>> film_look_presets_;
    
    void initialize_film_look_presets();
    void create_gpu_resources();
    void update_constants_buffer();
    void ensure_temp_textures(uint32_t width, uint32_t height);
};

// Spatial effects processor for geometric corrections
class SpatialEffectsProcessor {
public:
    SpatialEffectsProcessor(GraphicsDevice& device);
    ~SpatialEffectsProcessor();

    // Geometric corrections
    void set_lens_distortion(const LensDistortionParams& params) { lens_distortion_ = params; }
    void set_keystone_correction(const Mat4& correction_matrix) { keystone_matrix_ = correction_matrix; }
    void set_perspective_correction(const Mat4& perspective_matrix) { perspective_matrix_ = perspective_matrix; }
    
    // Stabilization
    void set_image_stabilization(const Mat4& stabilization_matrix) { stabilization_matrix_ = stabilization_matrix; }
    void enable_rolling_shutter_correction(bool enabled) { rolling_shutter_correction_ = enabled; }
    
    // Cropping and scaling
    void set_crop_parameters(const Vec4& crop_rect) { crop_rect_ = crop_rect; } // left, top, right, bottom
    void set_scale_parameters(float scale_x, float scale_y) { scale_x_ = scale_x; scale_y_ = scale_y; }
    
    // Processing
    void apply(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    void apply_lens_correction_only(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    void apply_geometric_correction_only(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    
    // Calibration and analysis
    void analyze_lens_distortion(std::shared_ptr<Texture> calibration_image);
    void save_lens_profile(const std::string& lens_name) const;
    void load_lens_profile(const std::string& lens_name);

private:
    GraphicsDevice& device_;
    
    // Effect parameters
    LensDistortionParams lens_distortion_;
    Mat4 keystone_matrix_ = Mat4::identity();
    Mat4 perspective_matrix_ = Mat4::identity();
    Mat4 stabilization_matrix_ = Mat4::identity();
    
    Vec4 crop_rect_ = {0.0f, 0.0f, 1.0f, 1.0f};
    float scale_x_ = 1.0f;
    float scale_y_ = 1.0f;
    bool rolling_shutter_correction_ = false;
    
    // GPU resources
    std::shared_ptr<Shader> distortion_correction_shader_;
    std::shared_ptr<Shader> geometric_transform_shader_;
    std::shared_ptr<Shader> composite_shader_;
    std::shared_ptr<Buffer> constants_buffer_;
    std::shared_ptr<Pipeline> correction_pipeline_;
    
    void create_gpu_resources();
    void update_constants_buffer();
};

// Temporal effects processor for motion-based effects
class TemporalEffectsProcessor {
public:
    TemporalEffectsProcessor(GraphicsDevice& device);
    ~TemporalEffectsProcessor();

    // Motion blur effects
    void set_motion_blur(const MotionBlurParams& params) { motion_blur_ = params; }
    void set_motion_vectors(std::shared_ptr<Texture> motion_vectors) { motion_vectors_ = motion_vectors; }
    void set_depth_buffer(std::shared_ptr<Texture> depth_buffer) { depth_buffer_ = depth_buffer; }
    
    // Frame blending and temporal filtering
    void enable_frame_blending(bool enabled, float blend_factor = 0.5f);
    void enable_temporal_denoising(bool enabled) { temporal_denoising_ = enabled; }
    void set_temporal_accumulation_factor(float factor) { temporal_accumulation_factor_ = factor; }
    
    // Processing
    void apply_motion_blur(std::shared_ptr<Texture> current_frame, std::shared_ptr<Texture> output);
    void apply_temporal_effects(std::shared_ptr<Texture> current_frame, 
                               std::shared_ptr<Texture> previous_frame,
                               std::shared_ptr<Texture> output);
    
    // Frame history management
    void update_frame_history(std::shared_ptr<Texture> current_frame);
    void clear_frame_history();
    
    // Quality settings
    void set_motion_blur_quality(int quality) { motion_blur_quality_ = quality; }
    void enable_adaptive_quality(bool enabled) { adaptive_quality_ = enabled; }

private:
    GraphicsDevice& device_;
    
    // Effect parameters
    MotionBlurParams motion_blur_;
    std::shared_ptr<Texture> motion_vectors_;
    std::shared_ptr<Texture> depth_buffer_;
    
    // Temporal settings
    bool frame_blending_enabled_ = false;
    float frame_blend_factor_ = 0.5f;
    bool temporal_denoising_ = false;
    float temporal_accumulation_factor_ = 0.9f;
    
    // Frame history
    std::vector<std::shared_ptr<Texture>> frame_history_;
    size_t max_frame_history_ = 4;
    size_t current_frame_index_ = 0;
    
    // GPU resources
    std::shared_ptr<Shader> motion_blur_shader_;
    std::shared_ptr<Shader> temporal_blend_shader_;
    std::shared_ptr<Shader> temporal_denoise_shader_;
    std::shared_ptr<Buffer> constants_buffer_;
    
    // Quality settings
    int motion_blur_quality_ = 2; // 0=Low, 1=Medium, 2=High, 3=Ultra
    bool adaptive_quality_ = true;
    
    void create_gpu_resources();
    void update_constants_buffer();
    void ensure_frame_history_size(uint32_t width, uint32_t height);
};

// Master effects manager that coordinates all effect processors
class AdvancedShaderEffectsManager {
public:
    AdvancedShaderEffectsManager(GraphicsDevice& device);
    ~AdvancedShaderEffectsManager();

    // Processor access
    ColorGradingProcessor& get_color_grading() { return *color_grading_; }
    CinematicEffectsProcessor& get_cinematic_effects() { return *cinematic_effects_; }
    SpatialEffectsProcessor& get_spatial_effects() { return *spatial_effects_; }
    TemporalEffectsProcessor& get_temporal_effects() { return *temporal_effects_; }
    
    // Master processing pipeline
    void apply_all_effects(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    void apply_color_grading_only(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    void apply_cinematic_only(std::shared_ptr<Texture> input, std::shared_ptr<Texture> output);
    
    // Effect ordering and management
    void set_effect_order(const std::vector<std::string>& effect_names);
    void enable_effect(const std::string& effect_name, bool enabled);
    bool is_effect_enabled(const std::string& effect_name) const;
    
    // Performance and quality
    void set_global_quality_preset(const std::string& preset);
    void enable_gpu_optimization(bool enabled) { gpu_optimization_ = enabled; }
    void set_target_framerate(float fps) { target_framerate_ = fps; }
    
    // Presets and workflows
    void save_effects_preset(const std::string& name) const;
    void load_effects_preset(const std::string& name);
    void apply_workflow_preset(const std::string& workflow); // "Cinematic", "Documentary", "Music Video"
    
    // Real-time monitoring
    float get_last_processing_time_ms() const { return last_processing_time_ms_; }
    bool is_meeting_framerate_target() const;

private:
    GraphicsDevice& device_;
    
    // Effect processors
    std::unique_ptr<ColorGradingProcessor> color_grading_;
    std::unique_ptr<CinematicEffectsProcessor> cinematic_effects_;
    std::unique_ptr<SpatialEffectsProcessor> spatial_effects_;
    std::unique_ptr<TemporalEffectsProcessor> temporal_effects_;
    
    // Effect management
    std::vector<std::string> effect_order_;
    std::unordered_map<std::string, bool> effect_enabled_;
    
    // Temporary textures for effect chain
    std::shared_ptr<Texture> temp_texture_1_;
    std::shared_ptr<Texture> temp_texture_2_;
    std::shared_ptr<Texture> temp_texture_3_;
    
    // Performance tracking
    float last_processing_time_ms_ = 0.0f;
    float target_framerate_ = 60.0f;
    bool gpu_optimization_ = true;
    
    void initialize_default_effect_order();
    void ensure_temp_textures(uint32_t width, uint32_t height);
    void initialize_workflow_presets();
};

} // namespace gfx
