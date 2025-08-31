// Week 9: Color Grading LUT Generator
// Professional 3D LUT generation and color grading tools for HDR workflows

#pragma once

#include "core/base_types.hpp"
#include "core/result.hpp"
#include "gfx/hdr_processor.hpp"
#include "gfx/wide_color_gamut_support.hpp"
#include "gfx/color_accuracy_validator.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace VideoEditor::GFX {

// =============================================================================
// LUT Types and Formats
// =============================================================================

enum class LUTFormat : uint8_t {
    CUBE,           // Adobe .cube format
    CSP,            // Rising Sun Research .csp
    HDR,            // Dolby Vision .hdr
    CTF,            // Color Transform Format
    CLF,            // Common LUT Format (ACES)
    CSL,            // Color Space Library
    IRIDAS_LOOK,    // Iridas .look format
    AUTODESK_CTF,   // Autodesk Color Transform Format
    NUCODA_CMS,     // Nucoda Color Management
    RESOLVE_LUT,    // DaVinci Resolve .dctl
    BINARY_3D       // Custom binary format
};

enum class LUTColorSpace : uint8_t {
    LOG_C,          // ARRI LogC
    LOG3G10,        // RED Log3G10
    SLOG3,          // Sony S-Log3
    VLOG,           // Panasonic V-Log
    FLOG,           // Fujifilm F-Log
    LINEAR,         // Linear RGB
    REC709,         // Rec. 709
    REC2020,        // Rec. 2020
    DCI_P3,         // DCI-P3
    ACES_CG,        // ACES CG
    ALEXA_WIDE_GAMUT // ARRI Alexa Wide Gamut
};

enum class LUTDimension : uint8_t {
    LUT_1D = 1,     // 1D LUT (tone curve)
    LUT_3D = 3      // 3D LUT (color transformation)
};

// =============================================================================
// LUT Data Structures
// =============================================================================

struct LUT1D {
    std::vector<float> red_curve;     // Red channel LUT
    std::vector<float> green_curve;   // Green channel LUT
    std::vector<float> blue_curve;    // Blue channel LUT
    uint32_t size = 0;                // Number of entries per channel
    
    float input_min = 0.0f;           // Input range minimum
    float input_max = 1.0f;           // Input range maximum
    float output_min = 0.0f;          // Output range minimum
    float output_max = 1.0f;          // Output range maximum
    
    bool is_valid() const noexcept;
    void apply(float& r, float& g, float& b) const;
    std::string to_string() const;
};

struct LUT3D {
    std::vector<float> data;          // RGB triplets (r,g,b,r,g,b,...)
    uint32_t size = 0;                // LUT dimension (size x size x size)
    
    float input_min = 0.0f;           // Input range minimum
    float input_max = 1.0f;           // Input range maximum
    float output_min = 0.0f;          // Output range minimum
    float output_max = 1.0f;          // Output range maximum
    
    bool is_valid() const noexcept;
    void apply(float& r, float& g, float& b) const;
    void apply_trilinear(float& r, float& g, float& b) const;
    void apply_tetrahedral(float& r, float& g, float& b) const;
    size_t get_data_size() const { return size * size * size * 3; }
    std::string to_string() const;
};

struct ColorGradingParameters {
    // Primary color correction
    float exposure = 0.0f;            // EV stops (-6 to +6)
    float contrast = 1.0f;            // Contrast multiplier (0.1 to 3.0)
    float brightness = 0.0f;          // Brightness offset (-1 to +1)
    float saturation = 1.0f;          // Saturation multiplier (0 to 3.0)
    float vibrance = 0.0f;            // Vibrance adjustment (-1 to +1)
    
    // Gamma correction
    float gamma = 1.0f;               // Gamma curve (0.1 to 3.0)
    float lift = 0.0f;                // Shadow lift (-1 to +1)
    float gain = 1.0f;                // Highlight gain (0.1 to 3.0)
    
    // Color balance
    float temperature = 0.0f;         // Color temperature shift (-100 to +100)
    float tint = 0.0f;                // Magenta/Green tint (-100 to +100)
    
    // Selective color adjustments
    struct ColorWheels {
        float shadows_r = 0.0f, shadows_g = 0.0f, shadows_b = 0.0f;
        float midtones_r = 0.0f, midtones_g = 0.0f, midtones_b = 0.0f;
        float highlights_r = 0.0f, highlights_g = 0.0f, highlights_b = 0.0f;
    } color_wheels;
    
    // Advanced adjustments
    float hue_shift = 0.0f;           // Global hue shift (-180 to +180 degrees)
    float shadow_detail = 0.0f;       // Shadow detail enhancement (-1 to +1)
    float highlight_recovery = 0.0f;  // Highlight recovery (-1 to +1)
    float clarity = 0.0f;             // Local contrast enhancement (-1 to +1)
    
    // HDR specific
    float hdr_strength = 1.0f;        // HDR processing strength (0 to 2.0)
    float tone_mapping_strength = 1.0f; // Tone mapping intensity (0 to 2.0)
    
    bool is_identity() const noexcept;
    std::string to_string() const;
};

struct LUTMetadata {
    std::string title;
    std::string creator;
    std::string description;
    std::string version = "1.0";
    
    LUTColorSpace input_color_space = LUTColorSpace::LINEAR;
    LUTColorSpace output_color_space = LUTColorSpace::LINEAR;
    
    float input_min = 0.0f;
    float input_max = 1.0f;
    float output_min = 0.0f;
    float output_max = 1.0f;
    
    std::string creation_date;
    std::map<std::string, std::string> custom_attributes;
    
    std::string to_string() const;
};

// =============================================================================
// Color Grading LUT Generator
// =============================================================================

class ColorGradingLUTGenerator {
public:
    explicit ColorGradingLUTGenerator() = default;
    ~ColorGradingLUTGenerator() = default;
    
    // 1D LUT generation
    Core::Result<LUT1D> generate_1d_lut(
        const ColorGradingParameters& params,
        uint32_t lut_size = 1024) const;
    
    Core::Result<LUT1D> generate_tone_curve(
        ToneMappingOperator tone_operator,
        float white_point = 100.0f,
        uint32_t lut_size = 1024) const;
    
    Core::Result<LUT1D> generate_gamma_curve(
        float gamma = 2.2f,
        uint32_t lut_size = 1024) const;
    
    // 3D LUT generation
    Core::Result<LUT3D> generate_3d_lut(
        const ColorGradingParameters& params,
        uint32_t lut_size = 33) const;
    
    Core::Result<LUT3D> generate_color_space_conversion_lut(
        ColorSpace input_space,
        ColorSpace output_space,
        uint32_t lut_size = 33) const;
    
    Core::Result<LUT3D> generate_hdr_tone_mapping_lut(
        ToneMappingOperator tone_operator,
        HDRTransferFunction input_transfer,
        HDRTransferFunction output_transfer,
        float peak_luminance = 1000.0f,
        uint32_t lut_size = 33) const;
    
    // Creative LUT generation
    Core::Result<LUT3D> generate_film_emulation_lut(
        const std::string& film_stock_name,
        uint32_t lut_size = 33) const;
    
    Core::Result<LUT3D> generate_vintage_look_lut(
        float vintage_strength = 0.5f,
        float warmth = 0.3f,
        uint32_t lut_size = 33) const;
    
    Core::Result<LUT3D> generate_cinematic_lut(
        float contrast_enhancement = 0.3f,
        float color_grade_strength = 0.5f,
        uint32_t lut_size = 33) const;
    
    // Calibration LUTs
    Core::Result<LUT3D> generate_display_calibration_lut(
        const CalibrationCorrection& calibration,
        uint32_t lut_size = 33) const;
    
    Core::Result<LUT3D> generate_camera_calibration_lut(
        const std::string& camera_model,
        const std::string& picture_profile,
        uint32_t lut_size = 33) const;
    
    // LUT operations
    Core::Result<LUT3D> combine_luts(const LUT3D& lut1, const LUT3D& lut2) const;
    Core::Result<LUT3D> invert_lut(const LUT3D& input_lut, uint32_t output_size = 33) const;
    Core::Result<LUT3D> resize_lut(const LUT3D& input_lut, uint32_t new_size) const;
    Core::Result<LUT3D> interpolate_luts(const LUT3D& lut1, const LUT3D& lut2, float blend_factor) const;
    
    // LUT validation and analysis
    bool validate_lut_1d(const LUT1D& lut) const;
    bool validate_lut_3d(const LUT3D& lut) const;
    
    struct LUTAnalysis {
        float dynamic_range = 0.0f;        // Output dynamic range
        float color_shift_magnitude = 0.0f; // Average color shift
        float contrast_change = 0.0f;       // Contrast modification
        float saturation_change = 0.0f;     // Saturation modification
        bool has_clipping = false;          // True if output is clipped
        bool is_monotonic = true;           // True if tone curve is monotonic
        float smoothness_metric = 0.0f;     // Measure of curve smoothness
    };
    
    LUTAnalysis analyze_lut_1d(const LUT1D& lut) const;
    LUTAnalysis analyze_lut_3d(const LUT3D& lut) const;
    
    // File I/O
    Core::Result<void> save_lut_1d(const LUT1D& lut, const std::string& filename,
                                  LUTFormat format = LUTFormat::CUBE,
                                  const LUTMetadata& metadata = {}) const;
    
    Core::Result<void> save_lut_3d(const LUT3D& lut, const std::string& filename,
                                  LUTFormat format = LUTFormat::CUBE,
                                  const LUTMetadata& metadata = {}) const;
    
    Core::Result<LUT1D> load_lut_1d(const std::string& filename) const;
    Core::Result<LUT3D> load_lut_3d(const std::string& filename) const;
    
    // Preview and visualization
    Core::Result<FrameData> apply_lut_to_frame(const FrameData& input,
                                              const LUT3D& lut) const;
    
    Core::Result<FrameData> generate_lut_preview_image(const LUT3D& lut,
                                                      uint32_t width = 512,
                                                      uint32_t height = 512) const;
    
    // GPU acceleration support
    Core::Result<std::vector<uint8_t>> generate_gpu_lut_texture(const LUT3D& lut) const;
    Core::Result<void> upload_lut_to_gpu(const LUT3D& lut, uint32_t texture_id) const;
    
private:
    // Color grading implementation helpers
    void apply_exposure_adjustment(float& r, float& g, float& b, float exposure) const;
    void apply_contrast_adjustment(float& r, float& g, float& b, float contrast) const;
    void apply_saturation_adjustment(float& r, float& g, float& b, float saturation) const;
    void apply_color_balance(float& r, float& g, float& b, float temperature, float tint) const;
    void apply_color_wheels(float& r, float& g, float& b, const ColorGradingParameters::ColorWheels& wheels) const;
    
    // Mathematical helpers
    float calculate_luminance_weight(float r, float g, float b, ColorSpace color_space) const;
    void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) const;
    void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) const;
    void rgb_to_hsl(float r, float g, float b, float& h, float& s, float& l) const;
    void hsl_to_rgb(float h, float s, float l, float& r, float& g, float& b) const;
    
    // Interpolation methods
    float linear_interpolate(float a, float b, float t) const;
    float cubic_interpolate(float a, float b, float c, float d, float t) const;
    void trilinear_interpolate(const LUT3D& lut, float r, float g, float b,
                              float& out_r, float& out_g, float& out_b) const;
    void tetrahedral_interpolate(const LUT3D& lut, float r, float g, float b,
                                float& out_r, float& out_g, float& out_b) const;
    
    // Film emulation data
    struct FilmStockData {
        std::string name;
        std::vector<float> red_curve;
        std::vector<float> green_curve;
        std::vector<float> blue_curve;
        float contrast_adjustment;
        float saturation_adjustment;
        float color_temperature_shift;
    };
    
    std::vector<FilmStockData> get_film_stock_database() const;
    FilmStockData get_film_stock_by_name(const std::string& name) const;
    
    // File format handlers
    Core::Result<void> save_cube_format(const LUT3D& lut, const std::string& filename,
                                       const LUTMetadata& metadata) const;
    Core::Result<void> save_csp_format(const LUT3D& lut, const std::string& filename,
                                      const LUTMetadata& metadata) const;
    Core::Result<void> save_clf_format(const LUT3D& lut, const std::string& filename,
                                      const LUTMetadata& metadata) const;
    
    Core::Result<LUT3D> load_cube_format(const std::string& filename) const;
    Core::Result<LUT3D> load_csp_format(const std::string& filename) const;
    Core::Result<LUT3D> load_clf_format(const std::string& filename) const;
    
    // Utility functions
    std::string generate_cube_header(const LUTMetadata& metadata, uint32_t lut_size) const;
    std::string generate_timestamp() const;
    bool is_power_of_two(uint32_t value) const;
    uint32_t next_power_of_two(uint32_t value) const;
};

// =============================================================================
// Real-time LUT Processor
// =============================================================================

class RealtimeLUTProcessor {
public:
    explicit RealtimeLUTProcessor(std::shared_ptr<Device> device);
    ~RealtimeLUTProcessor() = default;
    
    // LUT management
    Core::Result<uint32_t> upload_lut_3d(const LUT3D& lut);
    Core::Result<uint32_t> upload_lut_1d(const LUT1D& lut);
    Core::Result<void> remove_lut(uint32_t lut_id);
    void clear_all_luts();
    
    // Real-time processing
    Core::Result<void> apply_lut_3d(const FrameData& input, FrameData& output, uint32_t lut_id) const;
    Core::Result<void> apply_lut_1d(const FrameData& input, FrameData& output, uint32_t lut_id) const;
    
    // Batch processing
    Core::Result<void> apply_lut_to_sequence(const std::vector<FrameData>& input_frames,
                                            std::vector<FrameData>& output_frames,
                                            uint32_t lut_id) const;
    
    // Live preview support
    Core::Result<void> set_preview_lut(uint32_t lut_id);
    Core::Result<void> enable_live_preview(bool enable);
    Core::Result<FrameData> process_preview_frame(const FrameData& input) const;
    
    // Performance monitoring
    struct ProcessingStats {
        uint64_t frames_processed = 0;
        uint64_t total_processing_time_ms = 0;
        float average_fps = 0.0f;
        float peak_fps = 0.0f;
        uint32_t active_luts = 0;
        uint64_t gpu_memory_used = 0;
    };
    
    ProcessingStats get_processing_stats() const;
    void reset_stats();
    
private:
    std::shared_ptr<Device> device_;
    
    struct LUTResource {
        uint32_t texture_id = 0;
        LUTDimension dimension = LUTDimension::LUT_3D;
        uint32_t size = 0;
        bool is_uploaded = false;
    };
    
    std::map<uint32_t, LUTResource> lut_resources_;
    uint32_t next_lut_id_ = 1;
    uint32_t preview_lut_id_ = 0;
    bool live_preview_enabled_ = false;
    
    mutable ProcessingStats stats_;
    mutable std::chrono::high_resolution_clock::time_point last_stats_update_;
    
    // GPU processing helpers
    Core::Result<uint32_t> create_3d_texture(const LUT3D& lut);
    Core::Result<uint32_t> create_1d_texture(const LUT1D& lut);
    void update_processing_stats() const;
};

} // namespace VideoEditor::GFX
