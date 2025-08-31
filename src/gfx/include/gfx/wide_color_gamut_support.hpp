// Week 9: Wide Color Gamut Support
// Advanced color space handling for professional video workflows

#pragma once

#include "gfx/graphics_device.hpp"
#include <array>
#include <vector>
#include <memory>
#include <string>
#include <map>

namespace ve::gfx {

/**
 * @brief RGB working space definitions
 */
enum class RGBWorkingSpace {
    SRGB,           // Standard sRGB (BT.709 primaries)
    ADOBE_RGB,      // Adobe RGB (1998)
    PROPHOTO_RGB,   // ProPhoto RGB (ROMM RGB)
    BT_2020,        // ITU-R BT.2020 (UHD standard)
    DCI_P3,         // Digital Cinema Initiatives P3
    DISPLAY_P3,     // Apple Display P3 (P3 primaries with sRGB white point)
    ACES_CG,        // ACES Color Grading working space
    ACES_CC,        // ACES Color Correction working space
    ACES_CCT,       // ACES Color Correction and Tone mapping
    ALEXA_WIDE_GAMUT, // ARRI Alexa Wide Gamut
    RED_WIDE_GAMUT, // RED Wide Gamut RGB
    SONY_S_GAMUT3, // Sony S-Gamut3
    PANASONIC_V_GAMUT, // Panasonic V-Gamut
    CUSTOM          // User-defined color space
};

/**
 * @brief Illuminant/White point standards
 */
enum class Illuminant {
    D50,            // Daylight 5000K (printing standard)
    D55,            // Daylight 5500K
    D60,            // Daylight 6000K (ACES standard)
    D65,            // Daylight 6500K (sRGB, BT.709, BT.2020)
    D75,            // Daylight 7500K
    A,              // Tungsten 2856K
    B,              // Daylight 4874K (obsolete)
    C,              // Daylight 6774K (obsolete)
    E,              // Equal energy
    F2,             // Fluorescent (cool white)
    F7,             // Fluorescent (broad-band daylight)
    F11,            // Fluorescent (narrow-band white)
    DCI,            // DCI white point (x=0.314, y=0.351)
    CUSTOM          // User-defined white point
};

/**
 * @brief Chromatic adaptation transforms
 */
enum class ChromaticAdaptation {
    NONE,           // No adaptation
    BRADFORD,       // Bradford transform (most accurate)
    VON_KRIES,      // Von Kries transform
    XYZ_SCALING,    // Simple XYZ scaling
    CAT02,          // CIECAM02 chromatic adaptation
    CAT16,          // CIECAM16 chromatic adaptation
    SHARP,          // Sharp transform
    CMCCAT2000      // CMCCAT2000 transform
};

/**
 * @brief Gamut mapping methods
 */
enum class GamutMapping {
    CLIP,           // Simple clipping (fastest)
    COMPRESS,       // Smooth compression toward gamut boundary
    CUSP_COMPRESS,  // Compress toward cusp (luminance-preserving)
    PERCEPTUAL,     // Perceptual gamut mapping
    RELATIVE_COLORIMETRIC, // Relative colorimetric mapping
    ABSOLUTE_COLORIMETRIC, // Absolute colorimetric mapping
    SATURATION,     // Preserve saturation
    LC_CUSP,        // Lightness-chroma cusp mapping
    HIGHLIGHT_PRESERVING, // Preserve highlight details
    SHADOW_PRESERVING,    // Preserve shadow details
    CUSTOM          // User-defined mapping
};

/**
 * @brief Color space primaries (chromaticity coordinates)
 */
struct ColorPrimaries {
    // Red primary (x, y chromaticity)
    float red_x, red_y;
    
    // Green primary (x, y chromaticity)
    float green_x, green_y;
    
    // Blue primary (x, y chromaticity)
    float blue_x, blue_y;
    
    // White point (x, y chromaticity)
    float white_x, white_y;
    
    // Helper methods
    static ColorPrimaries srgb();
    static ColorPrimaries bt2020();
    static ColorPrimaries dci_p3();
    static ColorPrimaries display_p3();
    static ColorPrimaries adobe_rgb();
    static ColorPrimaries prophoto_rgb();
    static ColorPrimaries aces_cg();
    static ColorPrimaries alexa_wide_gamut();
    static ColorPrimaries red_wide_gamut();
    
    bool operator==(const ColorPrimaries& other) const;
    bool operator!=(const ColorPrimaries& other) const { return !(*this == other); }
};

/**
 * @brief White point definition
 */
struct WhitePoint {
    float x, y;     // Chromaticity coordinates
    float Y = 1.0f; // Luminance (usually 1.0 for normalized)
    
    static WhitePoint d50();
    static WhitePoint d55();
    static WhitePoint d60();
    static WhitePoint d65();
    static WhitePoint d75();
    static WhitePoint dci();
    static WhitePoint from_temperature(float temperature_kelvin);
    
    bool operator==(const WhitePoint& other) const;
    bool operator!=(const WhitePoint& other) const { return !(*this == other); }
};

/**
 * @brief 3x3 Color transformation matrix
 */
struct ColorMatrix3x3 {
    std::array<std::array<float, 3>, 3> m;
    
    ColorMatrix3x3();
    ColorMatrix3x3(const std::array<float, 9>& values);
    
    static ColorMatrix3x3 identity();
    static ColorMatrix3x3 from_primaries(const ColorPrimaries& primaries);
    static ColorMatrix3x3 rgb_to_xyz(const ColorPrimaries& primaries);
    static ColorMatrix3x3 xyz_to_rgb(const ColorPrimaries& primaries);
    static ColorMatrix3x3 bradford_adaptation(const WhitePoint& src, const WhitePoint& dst);
    static ColorMatrix3x3 von_kries_adaptation(const WhitePoint& src, const WhitePoint& dst);
    
    ColorMatrix3x3 operator*(const ColorMatrix3x3& other) const;
    std::array<float, 3> transform(const std::array<float, 3>& color) const;
    ColorMatrix3x3 inverse() const;
    ColorMatrix3x3 transpose() const;
    float determinant() const;
    
    void normalize_to_y(); // Normalize so that equal RGB maps to white point Y
};

/**
 * @brief Wide color gamut configuration
 */
struct WideColorGamutConfig {
    // Input characteristics
    RGBWorkingSpace input_working_space = RGBWorkingSpace::SRGB;
    Illuminant input_white_point = Illuminant::D65;
    ColorPrimaries custom_input_primaries; // Used if input_working_space is CUSTOM
    
    // Output characteristics  
    RGBWorkingSpace output_working_space = RGBWorkingSpace::SRGB;
    Illuminant output_white_point = Illuminant::D65;
    ColorPrimaries custom_output_primaries; // Used if output_working_space is CUSTOM
    
    // Conversion settings
    ChromaticAdaptation adaptation_method = ChromaticAdaptation::BRADFORD;
    GamutMapping gamut_mapping_method = GamutMapping::PERCEPTUAL;
    
    // Gamut mapping parameters
    float gamut_compression_strength = 1.0f;    // 0.0 = no compression, 1.0 = full compression
    float saturation_preservation = 0.8f;      // How much to preserve saturation
    float luminance_preservation = 0.9f;       // How much to preserve luminance
    bool enable_soft_clipping = true;          // Use soft clipping for out-of-gamut colors
    float soft_clip_threshold = 0.9f;          // Threshold for soft clipping (0.0-1.0)
    
    // Advanced options
    bool enable_black_point_compensation = true; // Adjust for different black points
    bool enable_perceptual_adaptation = true;    // Use perceptual color space for adaptation
    bool preserve_pure_colors = false;           // Try to preserve pure R, G, B colors
    float rendering_intent_weight = 1.0f;       // Blend between colorimetric and perceptual
    
    // Performance options
    bool use_lut_acceleration = true;           // Pre-compute conversion LUTs
    int lut_resolution = 64;                    // LUT resolution (64³ = 262K entries)
    bool enable_gpu_acceleration = true;       // Use GPU shaders for conversion
};

/**
 * @brief Color gamut analysis results
 */
struct GamutAnalysis {
    // Coverage statistics
    float coverage_srgb = 0.0f;        // Percentage of sRGB gamut covered
    float coverage_dci_p3 = 0.0f;      // Percentage of DCI-P3 gamut covered
    float coverage_bt2020 = 0.0f;      // Percentage of BT.2020 gamut covered
    float coverage_adobe_rgb = 0.0f;   // Percentage of Adobe RGB gamut covered
    
    // Out-of-gamut statistics
    float out_of_gamut_pixels = 0.0f;  // Percentage of pixels outside target gamut
    float max_saturation_error = 0.0f; // Maximum saturation error after mapping
    float average_delta_e = 0.0f;      // Average Delta E error after conversion
    float max_delta_e = 0.0f;          // Maximum Delta E error
    
    // Color distribution
    std::array<float, 3> primary_coverage;     // Coverage of R, G, B primaries
    std::array<float, 3> secondary_coverage;   // Coverage of C, M, Y secondaries
    float white_point_accuracy = 0.0f;         // How well white point is preserved
    float black_point_accuracy = 0.0f;         // How well black point is preserved
    
    // Visual quality metrics
    float color_fidelity_score = 0.0f;         // Overall color fidelity (0-1)
    float saturation_preservation_score = 0.0f; // How well saturation is preserved
    float luminance_preservation_score = 0.0f;  // How well luminance is preserved
    
    void reset() {
        *this = GamutAnalysis{};
    }
};

/**
 * @brief Wide Color Gamut Support
 * 
 * Advanced color space handling system supporting:
 * - Professional RGB working spaces
 * - Accurate chromatic adaptation
 * - Perceptual gamut mapping
 * - Real-time color space conversions
 * - Gamut analysis and visualization
 * - 3D LUT generation for color pipelines
 */
class WideColorGamutSupport {
public:
    /**
     * @brief Create wide color gamut support system
     * @param device Graphics device for GPU acceleration
     */
    explicit WideColorGamutSupport(GraphicsDevice* device);
    
    /**
     * @brief Destructor
     */
    ~WideColorGamutSupport();
    
    // Non-copyable, non-movable
    WideColorGamutSupport(const WideColorGamutSupport&) = delete;
    WideColorGamutSupport& operator=(const WideColorGamutSupport&) = delete;
    
    /**
     * @brief Initialize wide color gamut system
     * @return True if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Convert texture between color spaces
     * @param input_texture Source texture
     * @param config Conversion configuration
     * @return Converted texture
     */
    TextureHandle convert_color_space(TextureHandle input_texture, const WideColorGamutConfig& config);
    
    /**
     * @brief Apply gamut mapping to bring colors into target gamut
     * @param input_texture Source texture (any color space)
     * @param target_gamut Target color space
     * @param mapping_method Gamut mapping algorithm
     * @param parameters Additional mapping parameters
     * @return Gamut-mapped texture
     */
    TextureHandle apply_gamut_mapping(TextureHandle input_texture,
                                     RGBWorkingSpace target_gamut,
                                     GamutMapping mapping_method,
                                     const WideColorGamutConfig& parameters);
    
    /**
     * @brief Apply chromatic adaptation between white points
     * @param input_texture Source texture
     * @param source_white Source white point
     * @param target_white Target white point
     * @param adaptation_method Adaptation algorithm
     * @return Adapted texture
     */
    TextureHandle apply_chromatic_adaptation(TextureHandle input_texture,
                                            const WhitePoint& source_white,
                                            const WhitePoint& target_white,
                                            ChromaticAdaptation adaptation_method);
    
    /**
     * @brief Analyze color gamut of texture
     * @param input_texture Texture to analyze
     * @param reference_gamut Reference color space for analysis
     * @return Gamut analysis results
     */
    GamutAnalysis analyze_color_gamut(TextureHandle input_texture, RGBWorkingSpace reference_gamut);
    
    /**
     * @brief Create gamut visualization overlay
     * @param input_texture Source texture
     * @param reference_gamut Reference color space
     * @param visualization_type 0=out-of-gamut highlight, 1=gamut coverage, 2=saturation error
     * @return Visualization overlay texture
     */
    TextureHandle create_gamut_visualization(TextureHandle input_texture,
                                            RGBWorkingSpace reference_gamut,
                                            int visualization_type = 0);
    
    /**
     * @brief Generate 3D LUT for color space conversion
     * @param config Conversion configuration
     * @param lut_size LUT resolution (e.g., 64 for 64³ LUT)
     * @return 3D LUT texture
     */
    TextureHandle generate_color_space_lut(const WideColorGamutConfig& config, int lut_size = 64);
    
    /**
     * @brief Apply pre-computed 3D LUT for color conversion
     * @param input_texture Source texture
     * @param lut_texture 3D LUT texture
     * @param interpolation_method 0=nearest, 1=trilinear, 2=tetrahedral
     * @return LUT-converted texture
     */
    TextureHandle apply_color_lut(TextureHandle input_texture, 
                                 TextureHandle lut_texture,
                                 int interpolation_method = 2);
    
    /**
     * @brief Get conversion matrix between color spaces
     * @param source_space Source RGB working space
     * @param target_space Target RGB working space
     * @param adaptation_method Chromatic adaptation method
     * @return 3x3 conversion matrix
     */
    ColorMatrix3x3 get_conversion_matrix(RGBWorkingSpace source_space,
                                        RGBWorkingSpace target_space,
                                        ChromaticAdaptation adaptation_method = ChromaticAdaptation::BRADFORD);
    
    /**
     * @brief Get color primaries for RGB working space
     * @param working_space RGB working space
     * @return Color primaries structure
     */
    ColorPrimaries get_color_primaries(RGBWorkingSpace working_space) const;
    
    /**
     * @brief Get white point for illuminant
     * @param illuminant Standard illuminant
     * @return White point structure
     */
    WhitePoint get_white_point(Illuminant illuminant) const;
    
    /**
     * @brief Check if color is within gamut
     * @param rgb_color RGB color (0-1 range)
     * @param gamut Target color space
     * @return True if color is within gamut
     */
    bool is_within_gamut(const std::array<float, 3>& rgb_color, RGBWorkingSpace gamut) const;
    
    /**
     * @brief Calculate Delta E color difference
     * @param color1 First color (RGB 0-1)
     * @param color2 Second color (RGB 0-1)
     * @param working_space Color space for comparison
     * @param delta_e_formula 0=CIE76, 1=CIE94, 2=CIEDE2000
     * @return Delta E value
     */
    float calculate_delta_e(const std::array<float, 3>& color1,
                           const std::array<float, 3>& color2,
                           RGBWorkingSpace working_space,
                           int delta_e_formula = 2) const;
    
    /**
     * @brief Get supported working spaces
     * @return Vector of supported RGB working spaces
     */
    std::vector<RGBWorkingSpace> get_supported_working_spaces() const;
    
    /**
     * @brief Get working space name
     * @param working_space RGB working space enum
     * @return Human-readable name
     */
    std::string get_working_space_name(RGBWorkingSpace working_space) const;
    
    /**
     * @brief Get illuminant name
     * @param illuminant Illuminant enum
     * @return Human-readable name
     */
    std::string get_illuminant_name(Illuminant illuminant) const;
    
    /**
     * @brief Validate color space conversion configuration
     * @param config Configuration to validate
     * @return Vector of validation warnings/errors
     */
    std::vector<std::string> validate_conversion_config(const WideColorGamutConfig& config) const;

private:
    // Initialization helpers
    bool initialize_color_space_data();
    bool initialize_shaders();
    bool initialize_lut_resources();
    
    // Matrix calculations
    ColorMatrix3x3 calculate_rgb_to_xyz_matrix(const ColorPrimaries& primaries) const;
    ColorMatrix3x3 calculate_xyz_to_rgb_matrix(const ColorPrimaries& primaries) const;
    ColorMatrix3x3 calculate_chromatic_adaptation_matrix(const WhitePoint& source, 
                                                        const WhitePoint& target,
                                                        ChromaticAdaptation method) const;
    
    // Gamut mapping implementations
    TextureHandle gamut_map_clip(TextureHandle input, const WideColorGamutConfig& config);
    TextureHandle gamut_map_compress(TextureHandle input, const WideColorGamutConfig& config);
    TextureHandle gamut_map_perceptual(TextureHandle input, const WideColorGamutConfig& config);
    TextureHandle gamut_map_cusp_compress(TextureHandle input, const WideColorGamutConfig& config);
    
    // Color space conversions
    std::array<float, 3> rgb_to_lab(const std::array<float, 3>& rgb, const ColorPrimaries& primaries) const;
    std::array<float, 3> lab_to_rgb(const std::array<float, 3>& lab, const ColorPrimaries& primaries) const;
    std::array<float, 3> rgb_to_lch(const std::array<float, 3>& rgb, const ColorPrimaries& primaries) const;
    std::array<float, 3> lch_to_rgb(const std::array<float, 3>& lch, const ColorPrimaries& primaries) const;
    
    // Utility functions
    float get_color_temperature(const WhitePoint& white_point) const;
    WhitePoint calculate_white_point_from_temperature(float temperature_kelvin) const;
    float calculate_gamut_volume(const ColorPrimaries& primaries) const;
    bool point_in_gamut_triangle(const std::array<float, 2>& point, const ColorPrimaries& primaries) const;
    
    // LUT generation
    std::vector<float> generate_conversion_lut_data(const WideColorGamutConfig& config, int lut_size);
    void apply_gamut_mapping_to_lut(std::vector<float>& lut_data, const WideColorGamutConfig& config, int lut_size);
    
    // Shader management
    ShaderHandle get_conversion_shader(RGBWorkingSpace source, RGBWorkingSpace target);
    ShaderHandle get_gamut_mapping_shader(GamutMapping method);
    ShaderHandle get_adaptation_shader(ChromaticAdaptation method);
    
    // Configuration and state
    GraphicsDevice* graphics_device_;
    bool initialized_;
    
    // Color space data
    std::map<RGBWorkingSpace, ColorPrimaries> working_space_primaries_;
    std::map<Illuminant, WhitePoint> illuminant_white_points_;
    std::map<std::string, ColorMatrix3x3> cached_matrices_;
    
    // Shader resources
    std::map<std::pair<RGBWorkingSpace, RGBWorkingSpace>, ShaderHandle> conversion_shaders_;
    std::map<GamutMapping, ShaderHandle> gamut_mapping_shaders_;
    std::map<ChromaticAdaptation, ShaderHandle> adaptation_shaders_;
    ShaderHandle lut_application_shader_;
    ShaderHandle gamut_analysis_shader_;
    ShaderHandle visualization_shader_;
    
    // Resource management
    std::map<uint64_t, TextureHandle> lut_cache_;
    BufferHandle constant_buffer_;
    BufferHandle analysis_buffer_;
    
    // Constants and lookup tables
    static constexpr float D65_WHITE_X = 0.31271f;
    static constexpr float D65_WHITE_Y = 0.32902f;
    static constexpr float D50_WHITE_X = 0.34567f;
    static constexpr float D50_WHITE_Y = 0.35850f;
    
    // Color science constants
    static constexpr float CIE_E = 216.0f / 24389.0f;
    static constexpr float CIE_K = 24389.0f / 27.0f;
    static constexpr float LAB_DELTA = 6.0f / 29.0f;
};

} // namespace ve::gfx
