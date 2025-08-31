// Week 9: HDR Processor
// Professional HDR processing pipeline with wide color gamut support

#pragma once

#include "gfx/graphics_device.hpp"
#include <array>
#include <vector>
#include <memory>
#include <string>

namespace ve::gfx {

/**
 * @brief HDR transfer functions
 */
enum class HDRTransferFunction {
    LINEAR,         // Linear light (no transfer function)
    GAMMA_2_2,      // Standard gamma 2.2
    GAMMA_2_4,      // sRGB-like gamma 2.4
    PQ_2084,        // SMPTE ST 2084 (Dolby Vision, HDR10)
    HLG_ARIB,       // Hybrid Log-Gamma (BBC/NHK standard)
    HLG_BT2100,     // ITU-R BT.2100 HLG variant
    LOG_C,          // ARRI LogC
    LOG_3_G10,      // RED Log3G10
    SONY_S_LOG3     // Sony S-Log3
};

/**
 * @brief Color space primaries
 */
enum class ColorSpace {
    BT_709,         // HD standard (sRGB)
    BT_2020,        // UHD standard (wide gamut)
    DCI_P3,         // Digital cinema
    ADOBE_RGB,      // Adobe RGB (1998)
    PROPHOTO_RGB,   // ProPhoto RGB
    REC_2100,       // HDR broadcast standard
    ACES_CG,        // Academy Color Encoding System
    ACES_CC,        // ACES Color Correction working space
    CUSTOM          // User-defined primaries
};

/**
 * @brief Tone mapping operators
 */
enum class ToneMappingOperator {
    LINEAR,         // No tone mapping (clamp)
    REINHARD,       // Reinhard operator
    REINHARD_EXTENDED, // Extended Reinhard
    HABLE,          // John Hable (Uncharted 2)
    ACES,           // Academy Color Encoding System
    ACES_HILL,      // ACES with Hill tone curve
    FILMIC,         // Generic filmic curve
    GT,             // Gran Turismo tone mapper
    HEJL_BURGESS,   // Hejl-Burgess-Dawson
    CUSTOM          // User-defined curve
};

/**
 * @brief HDR metadata structure
 */
struct HDRMetadata {
    // Static metadata (HDR10)
    uint16_t max_display_mastering_luminance = 10000; // nits
    uint16_t min_display_mastering_luminance = 50;    // 0.005 nits (scaled by 10000)
    uint16_t max_content_light_level = 4000;          // nits
    uint16_t max_frame_average_light_level = 400;     // nits
    
    // Display primaries (chromaticity coordinates x10000)
    struct {
        uint16_t red_x = 6800, red_y = 3200;       // BT.2020 red
        uint16_t green_x = 2650, green_y = 6900;   // BT.2020 green
        uint16_t blue_x = 1500, blue_y = 600;      // BT.2020 blue
        uint16_t white_x = 3127, white_y = 3290;   // D65 white point
    } display_primaries;
    
    // Dynamic metadata (optional)
    bool has_dynamic_metadata = false;
    std::vector<uint8_t> dynamic_metadata_payload;
    
    // Content type
    bool is_hdr10 = false;
    bool is_hdr10_plus = false;
    bool is_dolby_vision = false;
    bool is_hlg = false;
};

/**
 * @brief HDR processing parameters
 */
struct HDRProcessingParams {
    // Input characteristics
    HDRTransferFunction input_transfer_function = HDRTransferFunction::LINEAR;
    ColorSpace input_color_space = ColorSpace::BT_709;
    float input_peak_luminance = 100.0f;    // nits
    float input_paper_white = 100.0f;       // nits
    
    // Output characteristics
    HDRTransferFunction output_transfer_function = HDRTransferFunction::GAMMA_2_2;
    ColorSpace output_color_space = ColorSpace::BT_709;
    float output_peak_luminance = 100.0f;   // nits
    float output_paper_white = 80.0f;       // nits
    
    // Tone mapping
    ToneMappingOperator tone_mapping = ToneMappingOperator::ACES;
    float tone_mapping_exposure = 0.0f;     // EV adjustment
    float tone_mapping_highlights = 1.0f;   // Highlight rolloff
    float tone_mapping_shadows = 1.0f;      // Shadow expansion
    float tone_mapping_contrast = 1.0f;     // Contrast adjustment
    float tone_mapping_saturation = 1.0f;   // Saturation preservation
    
    // Color grading
    bool enable_color_grading = false;
    float temperature_shift = 0.0f;         // Kelvin shift
    float tint_shift = 0.0f;                // Green-magenta shift
    float hue_shift = 0.0f;                 // Hue rotation (degrees)
    float saturation_adjustment = 1.0f;     // Global saturation
    float luminance_adjustment = 1.0f;      // Global luminance
    
    // Advanced options
    bool enable_gamut_mapping = true;       // Smart gamut compression
    bool preserve_chromaticity = true;      // Maintain color relationships
    bool use_bt2390_mapping = false;        // ITU-R BT.2390 tone mapping
    float adaptation_strength = 1.0f;       // Chromatic adaptation strength
};

/**
 * @brief Color space conversion matrices (3x3 in row-major order)
 */
struct ColorSpaceMatrix {
    std::array<float, 9> matrix;
    
    static ColorSpaceMatrix identity();
    static ColorSpaceMatrix bt709_to_bt2020();
    static ColorSpaceMatrix bt2020_to_bt709();
    static ColorSpaceMatrix bt709_to_dci_p3();
    static ColorSpaceMatrix dci_p3_to_bt709();
    static ColorSpaceMatrix bt2020_to_dci_p3();
    static ColorSpaceMatrix dci_p3_to_bt2020();
    static ColorSpaceMatrix aces_cg_to_bt709();
    static ColorSpaceMatrix bt709_to_aces_cg();
    static ColorSpaceMatrix create_bradford_adaptation(const float* src_white, const float* dst_white);
    
    ColorSpaceMatrix operator*(const ColorSpaceMatrix& other) const;
    std::array<float, 3> transform(const std::array<float, 3>& color) const;
};

/**
 * @brief HDR processing statistics
 */
struct HDRProcessingStats {
    // Processing performance
    float processing_time_ms = 0.0f;
    size_t pixels_processed = 0;
    float megapixels_per_second = 0.0f;
    
    // Content analysis
    float peak_luminance_detected = 0.0f;   // nits
    float average_luminance = 0.0f;         // nits
    float histogram_99_percentile = 0.0f;   // nits
    float histogram_95_percentile = 0.0f;   // nits
    
    // Color gamut analysis
    float gamut_coverage_bt709 = 0.0f;      // Percentage
    float gamut_coverage_dci_p3 = 0.0f;     // Percentage
    float gamut_coverage_bt2020 = 0.0f;     // Percentage
    bool has_out_of_gamut_colors = false;
    
    // Tone mapping effectiveness
    float contrast_preserved = 0.0f;        // 0-1
    float detail_preserved = 0.0f;          // 0-1
    float color_accuracy = 0.0f;            // Delta E average
    
    void reset() {
        *this = HDRProcessingStats{};
    }
};

/**
 * @brief HDR Processor
 * 
 * Comprehensive HDR processing pipeline supporting:
 * - Multiple HDR transfer functions (PQ, HLG, Log curves)
 * - Wide color gamut conversions (BT.2020, DCI-P3, etc.)
 * - Professional tone mapping operators
 * - Real-time HDR to SDR conversion
 * - Color space conversions with chromatic adaptation
 * - HDR metadata processing
 */
class HDRProcessor {
public:
    /**
     * @brief Configuration for HDR processor
     */
    struct Config {
        bool enable_gpu_acceleration = true;
        bool enable_compute_shaders = true;
        bool enable_10bit_processing = true;    // Use 10-bit intermediate format
        bool enable_16bit_processing = false;   // Use 16-bit float for extreme precision
        bool enable_histogram_analysis = true;  // Real-time content analysis
        bool enable_gamut_warning = false;      // Highlight out-of-gamut colors
        bool enable_performance_monitoring = true;
        
        // Processing quality
        int tone_mapping_quality = 2;           // 0=fast, 1=balanced, 2=quality
        bool use_lut_acceleration = true;       // Pre-computed LUTs for speed
        size_t lut_size = 64;                   // LUT resolution (64³ = 262K entries)
        
        // Memory management
        size_t max_texture_cache_mb = 512;      // Cache for intermediate results
        bool enable_streaming_processing = true; // Process large images in tiles
        size_t streaming_tile_size = 1024;     // Tile size for streaming
    };
    
    /**
     * @brief Create HDR processor
     * @param device Graphics device for GPU acceleration
     * @param config Configuration options
     */
    HDRProcessor(GraphicsDevice* device, const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~HDRProcessor();
    
    // Non-copyable, non-movable
    HDRProcessor(const HDRProcessor&) = delete;
    HDRProcessor& operator=(const HDRProcessor&) = delete;
    
    /**
     * @brief Initialize HDR processor
     * @return True if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Process HDR frame
     * @param input_texture Source HDR texture
     * @param params Processing parameters
     * @param metadata Optional HDR metadata
     * @return Processed output texture
     */
    TextureHandle process_hdr_frame(TextureHandle input_texture, 
                                   const HDRProcessingParams& params,
                                   const HDRMetadata* metadata = nullptr);
    
    /**
     * @brief Convert HDR to SDR with tone mapping
     * @param hdr_texture Source HDR texture
     * @param tone_mapping Tone mapping operator
     * @param params Additional tone mapping parameters
     * @return SDR output texture
     */
    TextureHandle convert_hdr_to_sdr(TextureHandle hdr_texture,
                                    ToneMappingOperator tone_mapping,
                                    const HDRProcessingParams& params);
    
    /**
     * @brief Convert between color spaces
     * @param input_texture Source texture
     * @param source_space Source color space
     * @param target_space Target color space
     * @param chromatic_adaptation Whether to apply chromatic adaptation
     * @return Converted texture
     */
    TextureHandle convert_color_space(TextureHandle input_texture,
                                     ColorSpace source_space,
                                     ColorSpace target_space,
                                     bool chromatic_adaptation = true);
    
    /**
     * @brief Apply transfer function conversion
     * @param input_texture Source texture
     * @param source_function Source transfer function
     * @param target_function Target transfer function
     * @param params Processing parameters for tone curve
     * @return Converted texture
     */
    TextureHandle convert_transfer_function(TextureHandle input_texture,
                                           HDRTransferFunction source_function,
                                           HDRTransferFunction target_function,
                                           const HDRProcessingParams& params);
    
    /**
     * @brief Analyze HDR content characteristics
     * @param hdr_texture Input HDR texture
     * @return Content analysis statistics
     */
    HDRProcessingStats analyze_hdr_content(TextureHandle hdr_texture);
    
    /**
     * @brief Create 3D LUT for HDR processing
     * @param params Processing parameters to bake into LUT
     * @param lut_size LUT resolution (e.g., 64 for 64³ LUT)
     * @return 3D LUT texture handle
     */
    TextureHandle create_hdr_lut(const HDRProcessingParams& params, int lut_size = 64);
    
    /**
     * @brief Apply pre-computed 3D LUT
     * @param input_texture Source texture
     * @param lut_texture 3D LUT texture
     * @param interpolation_quality 0=nearest, 1=linear, 2=tetrahedral
     * @return LUT-processed texture
     */
    TextureHandle apply_hdr_lut(TextureHandle input_texture, 
                               TextureHandle lut_texture,
                               int interpolation_quality = 2);
    
    /**
     * @brief Get supported HDR formats
     * @return Vector of supported TextureFormat values for HDR
     */
    std::vector<TextureFormat> get_supported_hdr_formats() const;
    
    /**
     * @brief Check if format supports HDR
     * @param format Texture format to check
     * @return True if format can represent HDR values
     */
    bool is_hdr_format(TextureFormat format) const;
    
    /**
     * @brief Get processing statistics
     * @return Current processing statistics
     */
    HDRProcessingStats get_processing_stats() const;
    
    /**
     * @brief Reset processing statistics
     */
    void reset_processing_stats();
    
    /**
     * @brief Update configuration
     * @param new_config New configuration options
     */
    void update_config(const Config& new_config);
    
    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }
    
    /**
     * @brief Validate HDR processing chain
     * @param params Processing parameters to validate
     * @return Vector of validation warnings/errors
     */
    std::vector<std::string> validate_processing_chain(const HDRProcessingParams& params) const;
    
    /**
     * @brief Get recommended parameters for display
     * @param display_capabilities Target display characteristics
     * @return Optimized processing parameters
     */
    HDRProcessingParams get_recommended_params_for_display(const HDRMetadata& display_capabilities) const;

private:
    // Initialization helpers
    bool initialize_shaders();
    bool initialize_compute_shaders();
    bool initialize_lut_resources();
    bool initialize_analysis_resources();
    
    // Shader resource management
    bool load_hdr_shaders();
    ShaderHandle get_transfer_function_shader(HDRTransferFunction function);
    ShaderHandle get_tone_mapping_shader(ToneMappingOperator op);
    ShaderHandle get_color_space_shader(ColorSpace source, ColorSpace target);
    
    // Processing pipeline stages
    TextureHandle apply_input_transform(TextureHandle input, const HDRProcessingParams& params);
    TextureHandle apply_color_space_conversion(TextureHandle input, ColorSpace source, ColorSpace target);
    TextureHandle apply_tone_mapping(TextureHandle input, const HDRProcessingParams& params);
    TextureHandle apply_output_transform(TextureHandle input, const HDRProcessingParams& params);
    
    // Transfer function implementations
    TextureHandle linear_to_pq(TextureHandle linear_texture, float max_luminance);
    TextureHandle pq_to_linear(TextureHandle pq_texture, float max_luminance);
    TextureHandle linear_to_hlg(TextureHandle linear_texture, float system_gamma);
    TextureHandle hlg_to_linear(TextureHandle hlg_texture, float system_gamma);
    TextureHandle linear_to_log(TextureHandle linear_texture, HDRTransferFunction log_type);
    TextureHandle log_to_linear(TextureHandle log_texture, HDRTransferFunction log_type);
    
    // Tone mapping implementations
    TextureHandle tone_map_reinhard(TextureHandle hdr_texture, const HDRProcessingParams& params);
    TextureHandle tone_map_reinhard_extended(TextureHandle hdr_texture, const HDRProcessingParams& params);
    TextureHandle tone_map_hable(TextureHandle hdr_texture, const HDRProcessingParams& params);
    TextureHandle tone_map_aces(TextureHandle hdr_texture, const HDRProcessingParams& params);
    TextureHandle tone_map_filmic(TextureHandle hdr_texture, const HDRProcessingParams& params);
    
    // Content analysis
    TextureHandle generate_luminance_histogram(TextureHandle input);
    HDRProcessingStats analyze_histogram(TextureHandle histogram_texture);
    void analyze_color_gamut(TextureHandle input, HDRProcessingStats& stats);
    
    // LUT generation and management
    std::vector<float> generate_lut_data(const HDRProcessingParams& params, int lut_size);
    void update_lut_cache(const HDRProcessingParams& params);
    TextureHandle get_cached_lut(const HDRProcessingParams& params);
    
    // Matrix calculations
    ColorSpaceMatrix calculate_conversion_matrix(ColorSpace source, ColorSpace target);
    ColorSpaceMatrix calculate_chromatic_adaptation(ColorSpace source, ColorSpace target);
    std::array<float, 3> get_color_space_primaries(ColorSpace space) const;
    std::array<float, 3> get_white_point(ColorSpace space) const;
    
    // Utility functions
    uint64_t calculate_params_hash(const HDRProcessingParams& params) const;
    TextureFormat choose_intermediate_format() const;
    bool needs_compute_shader(const HDRProcessingParams& params) const;
    void update_processing_stats(const HDRProcessingStats& frame_stats);
    
    // Configuration and state
    Config config_;
    GraphicsDevice* graphics_device_;
    bool initialized_;
    
    // Shader resources
    std::map<HDRTransferFunction, ShaderHandle> transfer_function_shaders_;
    std::map<ToneMappingOperator, ShaderHandle> tone_mapping_shaders_;
    std::map<std::pair<ColorSpace, ColorSpace>, ShaderHandle> color_space_shaders_;
    ShaderHandle lut_application_shader_;
    ShaderHandle histogram_shader_;
    ShaderHandle gamut_analysis_shader_;
    
    // Compute shader resources
    std::map<HDRTransferFunction, ShaderHandle> compute_transfer_shaders_;
    std::map<ToneMappingOperator, ShaderHandle> compute_tone_mapping_shaders_;
    ShaderHandle compute_lut_generation_shader_;
    ShaderHandle compute_histogram_shader_;
    
    // Resource management
    std::map<uint64_t, TextureHandle> lut_cache_;
    std::vector<TextureHandle> intermediate_textures_;
    BufferHandle histogram_buffer_;
    BufferHandle constant_buffer_;
    
    // Statistics and monitoring
    mutable std::mutex stats_mutex_;
    HDRProcessingStats processing_stats_;
    std::chrono::steady_clock::time_point last_stats_update_;
    
    // Performance monitoring
    std::vector<float> frame_times_;
    static constexpr size_t MAX_FRAME_TIME_SAMPLES = 60;
    
    // Constants for HDR processing
    static constexpr float PQ_M1 = 0.1593017578125f;
    static constexpr float PQ_M2 = 78.84375f;
    static constexpr float PQ_C1 = 0.8359375f;
    static constexpr float PQ_C2 = 18.8515625f;
    static constexpr float PQ_C3 = 18.6875f;
    static constexpr float HLG_A = 0.17883277f;
    static constexpr float HLG_B = 0.28466892f;
    static constexpr float HLG_C = 0.55991073f;
};

} // namespace ve::gfx
