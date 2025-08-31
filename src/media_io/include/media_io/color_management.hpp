#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <array>
#include <map>

namespace ve {
namespace media_io {

/**
 * @brief Professional Color Management Integration
 * 
 * Provides accurate color space conversion matrices, gamut mapping, and display adaptation
 * for professional color accuracy. Essential for high-end video production workflows.
 */

// Professional color spaces used in video production
enum class ColorSpace {
    // Standard Definition
    BT601_525,          // NTSC (525-line)
    BT601_625,          // PAL (625-line)
    
    // High Definition
    BT709,              // Rec. 709 (HDTV standard)
    SRGB,               // sRGB (computer graphics)
    
    // Ultra High Definition / Wide Gamut
    BT2020,             // Rec. 2020 (UHD standard)
    BT2020_NCL,         // Rec. 2020 Non-Constant Luminance
    BT2020_CL,          // Rec. 2020 Constant Luminance
    
    // Cinema
    DCI_P3,             // DCI-P3 (digital cinema)
    DISPLAY_P3,         // Display P3 (Apple displays)
    
    // Professional/Print
    ADOBE_RGB,          // Adobe RGB (1998)
    PROPHOTO_RGB,       // ProPhoto RGB (wide gamut)
    SMPTE_C,            // SMPTE-C (legacy broadcast)
    
    // Linear working spaces
    LINEAR_BT709,       // Linear Rec. 709
    LINEAR_BT2020,      // Linear Rec. 2020
    ACES_CG,            // ACES Color Grading space
    
    UNKNOWN
};

// White point standards
enum class WhitePoint {
    D50,                // 5000K (print/ICC standard)
    D55,                // 5500K
    D65,                // 6500K (video standard)
    DCI,                // DCI white point (cinema)
    E,                  // Equal energy white point
    CUSTOM
};

// Gamut mapping methods
enum class GamutMappingMethod {
    CLIP,               // Hard clipping (fastest)
    PERCEPTUAL,         // Perceptual compression
    RELATIVE_COLORIMETRIC, // Relative colorimetric
    SATURATION,         // Saturation preservation
    ABSOLUTE_COLORIMETRIC, // Absolute colorimetric
    CUSTOM_LUT          // User-defined 3D LUT
};

// Color space information structure
struct ColorSpaceInfo {
    ColorSpace space;
    WhitePoint white_point;
    std::array<std::array<double, 2>, 3> primaries; // RGB primaries (x,y coordinates)
    double gamma;                                    // Gamma correction value
    bool is_linear;                                  // Linear or gamma-corrected
    bool is_wide_gamut;                             // Wide gamut capability
    std::string name;                               // Human-readable name
    std::string description;                        // Detailed description
};

// 3x3 color transformation matrix
using ColorMatrix = std::array<std::array<double, 3>, 3>;

// Color point in XYZ space
struct XYZColor {
    double X, Y, Z;
    XYZColor(double x = 0.0, double y = 0.0, double z = 0.0) : X(x), Y(y), Z(z) {}
};

// Color point in RGB space
struct RGBColor {
    double R, G, B;
    RGBColor(double r = 0.0, double g = 0.0, double b = 0.0) : R(r), G(g), B(b) {}
};

// Gamut boundary information
struct GamutBoundary {
    std::vector<RGBColor> boundary_points;          // Gamut boundary vertices
    double area;                                    // Gamut area (relative to XYZ)
    double volume;                                  // 3D gamut volume
    std::array<double, 6> limits;                  // Min/max RGB values
};

// Display adaptation configuration
struct DisplayConfig {
    ColorSpace native_color_space;                  // Display's native color space
    WhitePoint white_point;                         // Display white point
    double max_luminance;                           // Peak luminance (nits)
    double min_luminance;                           // Black level (nits)
    bool hdr_capable;                               // HDR display support
    bool wide_gamut;                                // Wide gamut support
    std::string profile_path;                       // ICC profile path (optional)
};

// Color conversion configuration
struct ColorConversionConfig {
    ColorSpace source_space;
    ColorSpace target_space;
    GamutMappingMethod gamut_method;
    bool preserve_blacks;                           // Preserve pure black/white
    bool chromatic_adaptation;                      // Enable white point adaptation
    double gamut_compression_factor;                // 0.0 = no compression, 1.0 = full
    double perceptual_intent_strength;             // Perceptual mapping strength
};

// Color conversion result with metrics
struct ColorConversionResult {
    std::vector<RGBColor> converted_colors;
    uint32_t out_of_gamut_pixels;                  // Count of clipped pixels
    double gamut_coverage;                          // Coverage ratio (0.0-1.0)
    double color_accuracy_delta_e;                 // Average Delta E error
    bool conversion_successful;
    std::string warning_message;                    // Any conversion warnings
};

/**
 * @brief Professional Color Management System
 * 
 * Handles color space conversions, gamut mapping, and display adaptation
 * with professional accuracy for video production workflows.
 */
class ColorManagement {
public:
    ColorManagement();
    ~ColorManagement() = default;
    
    // Color space information
    ColorSpaceInfo getColorSpaceInfo(ColorSpace space) const;
    std::vector<ColorSpace> getSupportedColorSpaces() const;
    bool isColorSpaceSupported(ColorSpace space) const;
    
    // Matrix generation
    ColorMatrix getConversionMatrix(ColorSpace from, ColorSpace to) const;
    ColorMatrix getChromaticAdaptationMatrix(WhitePoint from, WhitePoint to) const;
    ColorMatrix getPrimariesMatrix(ColorSpace space) const;
    
    // Color space conversion
    ColorConversionResult convertColorSpace(
        const std::vector<RGBColor>& source_colors,
        const ColorConversionConfig& config
    ) const;
    
    RGBColor convertSingleColor(
        const RGBColor& source,
        ColorSpace from,
        ColorSpace to,
        GamutMappingMethod method = GamutMappingMethod::PERCEPTUAL
    ) const;
    
    // Gamut analysis
    GamutBoundary calculateGamutBoundary(ColorSpace space) const;
    bool isInGamut(const RGBColor& color, ColorSpace space) const;
    double calculateGamutCoverage(ColorSpace source, ColorSpace target) const;
    
    // Gamut mapping
    RGBColor applyGamutMapping(
        const RGBColor& color,
        ColorSpace target_space,
        GamutMappingMethod method,
        double compression_factor = 0.8
    ) const;
    
    // Display adaptation
    void setDisplayConfig(const DisplayConfig& config);
    DisplayConfig getDisplayConfig() const;
    RGBColor adaptForDisplay(const RGBColor& color, ColorSpace source_space) const;
    
    // White point adaptation
    XYZColor adaptWhitePoint(const XYZColor& color, WhitePoint from, WhitePoint to) const;
    ColorMatrix bradfordAdaptation(WhitePoint from, WhitePoint to) const;
    
    // Tone mapping for HDR/SDR
    RGBColor toneMapForSDR(const RGBColor& hdr_color, double max_nits = 100.0) const;
    RGBColor expandToHDR(const RGBColor& sdr_color, double target_nits = 1000.0) const;
    
    // Quality assessment
    double calculateDeltaE(const RGBColor& color1, const RGBColor& color2) const;
    double calculateGamutUtilization(const std::vector<RGBColor>& colors, ColorSpace space) const;
    
    // Professional workflows
    bool validateColorAccuracy(
        const std::vector<RGBColor>& reference,
        const std::vector<RGBColor>& processed,
        double acceptable_delta_e = 2.0
    ) const;
    
    // ICC profile integration (preparation for future)
    bool loadICCProfile(const std::string& profile_path);
    std::string getRecommendedProfile(ColorSpace space) const;

private:
    DisplayConfig display_config_;
    std::map<ColorSpace, ColorSpaceInfo> color_space_info_;
    std::map<WhitePoint, XYZColor> white_point_xyz_;
    
    // Internal calculation methods
    void initializeColorSpaces();
    void initializeWhitePoints();
    
    ColorMatrix multiplyMatrices(const ColorMatrix& a, const ColorMatrix& b) const;
    ColorMatrix invertMatrix(const ColorMatrix& matrix) const;
    double matrixDeterminant(const ColorMatrix& matrix) const;
    
    XYZColor rgbToXYZ(const RGBColor& rgb, ColorSpace space) const;
    RGBColor xyzToRGB(const XYZColor& xyz, ColorSpace space) const;
    
    // Gamut mapping algorithms
    RGBColor clipToGamut(const RGBColor& color, ColorSpace space) const;
    RGBColor perceptualGamutMap(const RGBColor& color, ColorSpace space, double factor) const;
    RGBColor saturationPreservingMap(const RGBColor& color, ColorSpace space) const;
    
    // Gamma correction
    double applyGamma(double linear_value, double gamma) const;
    double removeGamma(double gamma_value, double gamma) const;
    
    // Advanced color science
    std::array<double, 3> rgbToLab(const RGBColor& rgb, ColorSpace space) const;
    RGBColor labToRGB(const std::array<double, 3>& lab, ColorSpace space) const;
    
    // Utility functions
    double clamp(double value, double min_val = 0.0, double max_val = 1.0) const;
    bool isValidRGB(const RGBColor& color) const;
    double luminance(const RGBColor& color, ColorSpace space) const;
};

// Utility functions for color management
namespace color_utils {
    
    // Color space detection from metadata
    ColorSpace detectFromCodecInfo(const std::string& codec_name, const std::string& color_space_str);
    
    // Common conversion shortcuts
    RGBColor bt709ToBt2020(const RGBColor& bt709_color);
    RGBColor bt2020ToBt709(const RGBColor& bt2020_color);
    RGBColor dciP3ToDisplayP3(const RGBColor& dci_color);
    RGBColor srgbToBt709(const RGBColor& srgb_color);
    
    // Professional workflow helpers
    struct WorkflowRecommendation {
        ColorSpace working_space;
        ColorSpace output_space;
        GamutMappingMethod mapping_method;
        bool requires_tone_mapping;
        std::string reasoning;
    };
    
    WorkflowRecommendation getWorkflowRecommendation(
        ColorSpace source_space,
        const std::string& target_delivery,
        bool hdr_workflow = false
    );
    
    // Color accuracy validation
    struct ColorAccuracyReport {
        double average_delta_e;
        double max_delta_e;
        uint32_t failing_pixels;
        double color_fidelity_score;  // 0.0-1.0
        std::vector<std::string> recommendations;
    };
    
    ColorAccuracyReport validateWorkflow(
        const std::vector<RGBColor>& source_colors,
        ColorSpace source_space,
        ColorSpace target_space,
        GamutMappingMethod method
    );
}

} // namespace media_io
} // namespace ve
