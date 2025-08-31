#include "media_io/color_management.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace ve {
namespace media_io {

ColorManagement::ColorManagement() {
    initializeColorSpaces();
    initializeWhitePoints();
    
    // Set default display configuration
    display_config_.native_color_space = ColorSpace::BT709;
    display_config_.white_point = WhitePoint::D65;
    display_config_.max_luminance = 100.0;  // Standard SDR
    display_config_.min_luminance = 0.1;
    display_config_.hdr_capable = false;
    display_config_.wide_gamut = false;
}

ColorSpaceInfo ColorManagement::getColorSpaceInfo(ColorSpace space) const {
    auto it = color_space_info_.find(space);
    if (it != color_space_info_.end()) {
        return it->second;
    }
    return ColorSpaceInfo{};
}

std::vector<ColorSpace> ColorManagement::getSupportedColorSpaces() const {
    return {
        ColorSpace::BT601_525,
        ColorSpace::BT601_625,
        ColorSpace::BT709,
        ColorSpace::SRGB,
        ColorSpace::BT2020,
        ColorSpace::BT2020_NCL,
        ColorSpace::DCI_P3,
        ColorSpace::DISPLAY_P3,
        ColorSpace::ADOBE_RGB,
        ColorSpace::LINEAR_BT709,
        ColorSpace::LINEAR_BT2020,
        ColorSpace::ACES_CG
    };
}

bool ColorManagement::isColorSpaceSupported(ColorSpace space) const {
    auto supported = getSupportedColorSpaces();
    return std::find(supported.begin(), supported.end(), space) != supported.end();
}

ColorMatrix ColorManagement::getConversionMatrix(ColorSpace from, ColorSpace to) const {
    if (from == to) {
        // Identity matrix
        return {{
            {{1.0, 0.0, 0.0}},
            {{0.0, 1.0, 0.0}},
            {{0.0, 0.0, 1.0}}
        }};
    }
    
    // Get matrices for each color space
    ColorMatrix from_matrix = getPrimariesMatrix(from);
    ColorMatrix to_matrix = getPrimariesMatrix(to);
    ColorMatrix to_inverse = invertMatrix(to_matrix);
    
    // Combine: RGB_from -> XYZ -> RGB_to
    return multiplyMatrices(to_inverse, from_matrix);
}

ColorMatrix ColorManagement::getChromaticAdaptationMatrix(WhitePoint from, WhitePoint to) const {
    if (from == to) {
        return {{
            {{1.0, 0.0, 0.0}},
            {{0.0, 1.0, 0.0}},
            {{0.0, 0.0, 1.0}}
        }};
    }
    
    return bradfordAdaptation(from, to);
}

ColorMatrix ColorManagement::getPrimariesMatrix(ColorSpace space) const {
    // Simplified matrices for major color spaces
    // In production, these would be calculated from precise primaries
    
    switch (space) {
        case ColorSpace::BT709:
        case ColorSpace::SRGB:
            return {{
                {{0.4124564, 0.3575761, 0.1804375}},
                {{0.2126729, 0.7151522, 0.0721750}},
                {{0.0193339, 0.1191920, 0.9503041}}
            }};
            
        case ColorSpace::BT2020:
            return {{
                {{0.6369580, 0.1446169, 0.1688809}},
                {{0.2627045, 0.6779982, 0.0593017}},
                {{0.0000000, 0.0280727, 1.0609851}}
            }};
            
        case ColorSpace::DCI_P3:
            return {{
                {{0.4451698, 0.2771344, 0.1722826}},
                {{0.2094917, 0.7215952, 0.0689133}},
                {{0.0000000, 0.0470606, 0.9073906}}
            }};
            
        case ColorSpace::ADOBE_RGB:
            return {{
                {{0.5767309, 0.1855540, 0.1881852}},
                {{0.2973769, 0.6273491, 0.0752741}},
                {{0.0270343, 0.0706872, 0.9911085}}
            }};
            
        default:
            // Default to BT.709
            return getPrimariesMatrix(ColorSpace::BT709);
    }
}

ColorConversionResult ColorManagement::convertColorSpace(
    const std::vector<RGBColor>& source_colors,
    const ColorConversionConfig& config) const {
    
    ColorConversionResult result;
    result.converted_colors.reserve(source_colors.size());
    result.out_of_gamut_pixels = 0;
    result.color_accuracy_delta_e = 0.0;
    result.conversion_successful = true;
    
    ColorMatrix conversion_matrix = getConversionMatrix(config.source_space, config.target_space);
    
    double total_delta_e = 0.0;
    
    for (const auto& source_color : source_colors) {
        // Apply conversion matrix
        RGBColor converted;
        converted.R = conversion_matrix[0][0] * source_color.R + 
                     conversion_matrix[0][1] * source_color.G + 
                     conversion_matrix[0][2] * source_color.B;
        converted.G = conversion_matrix[1][0] * source_color.R + 
                     conversion_matrix[1][1] * source_color.G + 
                     conversion_matrix[1][2] * source_color.B;
        converted.B = conversion_matrix[2][0] * source_color.R + 
                     conversion_matrix[2][1] * source_color.G + 
                     conversion_matrix[2][2] * source_color.B;
        
        // Apply gamut mapping
        if (!isInGamut(converted, config.target_space)) {
            converted = applyGamutMapping(converted, config.target_space, config.gamut_method, config.gamut_compression_factor);
            result.out_of_gamut_pixels++;
        }
        
        // Clamp values to valid range
        converted.R = clamp(converted.R, 0.0, 1.0);
        converted.G = clamp(converted.G, 0.0, 1.0);
        converted.B = clamp(converted.B, 0.0, 1.0);
        
        result.converted_colors.push_back(converted);
        
        // Calculate color accuracy
        total_delta_e += calculateDeltaE(source_color, converted);
    }
    
    result.color_accuracy_delta_e = total_delta_e / source_colors.size();
    result.gamut_coverage = 1.0 - (static_cast<double>(result.out_of_gamut_pixels) / source_colors.size());
    
    if (result.out_of_gamut_pixels > source_colors.size() * 0.1) {
        result.warning_message = "High percentage of out-of-gamut pixels detected";
    }
    
    return result;
}

RGBColor ColorManagement::convertSingleColor(
    const RGBColor& source,
    ColorSpace from,
    ColorSpace to,
    GamutMappingMethod method) const {
    
    ColorConversionConfig config;
    config.source_space = from;
    config.target_space = to;
    config.gamut_method = method;
    config.preserve_blacks = true;
    config.chromatic_adaptation = true;
    config.gamut_compression_factor = 0.8;
    
    auto result = convertColorSpace({source}, config);
    
    if (!result.converted_colors.empty()) {
        return result.converted_colors[0];
    }
    
    return source;  // Fallback
}

GamutBoundary ColorManagement::calculateGamutBoundary(ColorSpace space) const {
    GamutBoundary boundary;
    
    // Calculate approximate gamut boundary by sampling RGB cube edges
    std::vector<RGBColor> test_colors;
    
    // Sample RGB cube corners and edges
    for (int r = 0; r <= 1; ++r) {
        for (int g = 0; g <= 1; ++g) {
            for (int b = 0; b <= 1; ++b) {
                test_colors.emplace_back(static_cast<double>(r), static_cast<double>(g), static_cast<double>(b));
            }
        }
    }
    
    // Add edge samples
    for (double t = 0.0; t <= 1.0; t += 0.1) {
        test_colors.emplace_back(t, 0.0, 0.0);  // Red edge
        test_colors.emplace_back(0.0, t, 0.0);  // Green edge
        test_colors.emplace_back(0.0, 0.0, t);  // Blue edge
        test_colors.emplace_back(t, t, 0.0);    // Yellow edge
        test_colors.emplace_back(t, 0.0, t);    // Magenta edge
        test_colors.emplace_back(0.0, t, t);    // Cyan edge
    }
    
    boundary.boundary_points = test_colors;
    
    // Calculate approximate gamut area (simplified)
    boundary.area = 1.0;  // Placeholder - real implementation would calculate precise area
    boundary.volume = 1.0;
    boundary.limits = {0.0, 1.0, 0.0, 1.0, 0.0, 1.0};  // RGB min/max
    
    return boundary;
}

bool ColorManagement::isInGamut(const RGBColor& color, ColorSpace space) const {
    // Simple gamut test - check if RGB values are in valid range
    // Real implementation would use precise gamut boundaries
    
    bool in_range = (color.R >= 0.0 && color.R <= 1.0 &&
                     color.G >= 0.0 && color.G <= 1.0 &&
                     color.B >= 0.0 && color.B <= 1.0);
    
    if (!in_range) {
        return false;
    }
    
    // Additional checks for specific color spaces
    switch (space) {
        case ColorSpace::BT709:
        case ColorSpace::SRGB:
            // Standard gamut - if in range, it's valid
            return true;
            
        case ColorSpace::BT2020:
            // Wider gamut - most RGB values are valid
            return true;
            
        case ColorSpace::DCI_P3:
            // Cinema gamut - check for specific boundaries
            return true;  // Simplified for now
            
        default:
            return true;
    }
}

double ColorManagement::calculateGamutCoverage(ColorSpace source, ColorSpace target) const {
    // Simplified gamut coverage calculation
    // Real implementation would use precise gamut volume calculations
    
    if (source == target) {
        return 1.0;
    }
    
    // Approximate coverage based on gamut relationships
    if (source == ColorSpace::BT709 && target == ColorSpace::BT2020) {
        return 0.76;  // BT.709 covers ~76% of BT.2020
    } else if (source == ColorSpace::BT2020 && target == ColorSpace::BT709) {
        return 1.0;   // BT.2020 fully covers BT.709
    } else if (source == ColorSpace::DCI_P3 && target == ColorSpace::BT709) {
        return 0.89;  // P3 covers ~89% of BT.709
    }
    
    return 0.85;  // Default approximate coverage
}

RGBColor ColorManagement::applyGamutMapping(
    const RGBColor& color,
    ColorSpace target_space,
    GamutMappingMethod method,
    double compression_factor) const {
    
    switch (method) {
        case GamutMappingMethod::CLIP:
            return clipToGamut(color, target_space);
            
        case GamutMappingMethod::PERCEPTUAL:
            return perceptualGamutMap(color, target_space, compression_factor);
            
        case GamutMappingMethod::SATURATION:
            return saturationPreservingMap(color, target_space);
            
        case GamutMappingMethod::RELATIVE_COLORIMETRIC:
            // Maintain color relationships
            return clipToGamut(color, target_space);
            
        default:
            return clipToGamut(color, target_space);
    }
}

void ColorManagement::setDisplayConfig(const DisplayConfig& config) {
    display_config_ = config;
}

DisplayConfig ColorManagement::getDisplayConfig() const {
    return display_config_;
}

RGBColor ColorManagement::adaptForDisplay(const RGBColor& color, ColorSpace source_space) const {
    // Convert to display's native color space
    RGBColor adapted = convertSingleColor(color, source_space, display_config_.native_color_space);
    
    // Apply tone mapping if needed
    if (!display_config_.hdr_capable && source_space == ColorSpace::BT2020) {
        adapted = toneMapForSDR(adapted, display_config_.max_luminance);
    }
    
    return adapted;
}

XYZColor ColorManagement::adaptWhitePoint(const XYZColor& color, WhitePoint from, WhitePoint to) const {
    if (from == to) {
        return color;
    }
    
    ColorMatrix adaptation = bradfordAdaptation(from, to);
    
    XYZColor adapted;
    adapted.X = adaptation[0][0] * color.X + adaptation[0][1] * color.Y + adaptation[0][2] * color.Z;
    adapted.Y = adaptation[1][0] * color.X + adaptation[1][1] * color.Y + adaptation[1][2] * color.Z;
    adapted.Z = adaptation[2][0] * color.X + adaptation[2][1] * color.Y + adaptation[2][2] * color.Z;
    
    return adapted;
}

ColorMatrix ColorManagement::bradfordAdaptation(WhitePoint from, WhitePoint to) const {
    // Bradford chromatic adaptation matrix
    // Simplified implementation - real version would use precise white point coordinates
    
    if (from == to) {
        return {{
            {{1.0, 0.0, 0.0}},
            {{0.0, 1.0, 0.0}},
            {{0.0, 0.0, 1.0}}
        }};
    }
    
    // Approximate adaptation matrix for common transformations
    if (from == WhitePoint::D50 && to == WhitePoint::D65) {
        return {{
            {{0.9555766, -0.0230393, 0.0631967}},
            {{-0.0282895, 1.0099416, 0.0210077}},
            {{0.0122982, -0.0204830, 1.3299098}}
        }};
    } else if (from == WhitePoint::D65 && to == WhitePoint::D50) {
        return {{
            {{1.0478112, 0.0228866, -0.0501270}},
            {{0.0295424, 0.9904844, -0.0170491}},
            {{-0.0092345, 0.0150436, 0.7521316}}
        }};
    }
    
    // Default: identity matrix
    return {{
        {{1.0, 0.0, 0.0}},
        {{0.0, 1.0, 0.0}},
        {{0.0, 0.0, 1.0}}
    }};
}

RGBColor ColorManagement::toneMapForSDR(const RGBColor& hdr_color, double max_nits) const {
    // Simple tone mapping for HDR to SDR conversion
    double scale_factor = max_nits / 1000.0;  // Assume HDR is 1000 nits peak
    
    RGBColor sdr_color;
    sdr_color.R = std::pow(hdr_color.R * scale_factor, 1.0 / 2.4);  // Apply gamma
    sdr_color.G = std::pow(hdr_color.G * scale_factor, 1.0 / 2.4);
    sdr_color.B = std::pow(hdr_color.B * scale_factor, 1.0 / 2.4);
    
    return {clamp(sdr_color.R, 0.0, 1.0), clamp(sdr_color.G, 0.0, 1.0), clamp(sdr_color.B, 0.0, 1.0)};
}

RGBColor ColorManagement::expandToHDR(const RGBColor& sdr_color, double target_nits) const {
    // Simple SDR to HDR expansion
    double scale_factor = target_nits / 100.0;  // Assume SDR is 100 nits
    
    RGBColor hdr_color;
    hdr_color.R = std::pow(sdr_color.R, 2.4) * scale_factor;  // Remove gamma and scale
    hdr_color.G = std::pow(sdr_color.G, 2.4) * scale_factor;
    hdr_color.B = std::pow(sdr_color.B, 2.4) * scale_factor;
    
    return hdr_color;
}

double ColorManagement::calculateDeltaE(const RGBColor& color1, const RGBColor& color2) const {
    // Simplified Delta E calculation using RGB differences
    // Real implementation would use Lab color space for accurate Delta E
    
    double dr = color1.R - color2.R;
    double dg = color1.G - color2.G;
    double db = color1.B - color2.B;
    
    // Approximate Delta E using weighted RGB difference
    return std::sqrt(0.3 * dr * dr + 0.59 * dg * dg + 0.11 * db * db) * 100.0;
}

double ColorManagement::calculateGamutUtilization(const std::vector<RGBColor>& colors, ColorSpace space) const {
    if (colors.empty()) {
        return 0.0;
    }
    
    // Calculate how much of the available gamut is being used
    double min_r = 1.0, max_r = 0.0;
    double min_g = 1.0, max_g = 0.0;
    double min_b = 1.0, max_b = 0.0;
    
    for (const auto& color : colors) {
        min_r = std::min(min_r, color.R);
        max_r = std::max(max_r, color.R);
        min_g = std::min(min_g, color.G);
        max_g = std::max(max_g, color.G);
        min_b = std::min(min_b, color.B);
        max_b = std::max(max_b, color.B);
    }
    
    double utilization = (max_r - min_r) * (max_g - min_g) * (max_b - min_b);
    return std::min(1.0, utilization);
}

bool ColorManagement::validateColorAccuracy(
    const std::vector<RGBColor>& reference,
    const std::vector<RGBColor>& processed,
    double acceptable_delta_e) const {
    
    if (reference.size() != processed.size()) {
        return false;
    }
    
    double total_delta_e = 0.0;
    for (size_t i = 0; i < reference.size(); ++i) {
        double delta_e = calculateDeltaE(reference[i], processed[i]);
        if (delta_e > acceptable_delta_e) {
            return false;
        }
        total_delta_e += delta_e;
    }
    
    double average_delta_e = total_delta_e / reference.size();
    return average_delta_e <= acceptable_delta_e;
}

bool ColorManagement::loadICCProfile(const std::string& profile_path) {
    // Placeholder for ICC profile loading
    // Real implementation would parse ICC profiles
    display_config_.profile_path = profile_path;
    return true;
}

std::string ColorManagement::getRecommendedProfile(ColorSpace space) const {
    switch (space) {
        case ColorSpace::SRGB:
            return "sRGB_IEC61966-2-1.icc";
        case ColorSpace::ADOBE_RGB:
            return "AdobeRGB1998.icc";
        case ColorSpace::DCI_P3:
            return "DCI-P3.icc";
        case ColorSpace::BT2020:
            return "Rec2020.icc";
        default:
            return "sRGB_IEC61966-2-1.icc";
    }
}

// Private implementation methods

void ColorManagement::initializeColorSpaces() {
    // Initialize BT.709 / sRGB
    color_space_info_[ColorSpace::BT709] = {
        ColorSpace::BT709,
        WhitePoint::D65,
        {{{{0.64, 0.33}}, {{0.30, 0.60}}, {{0.15, 0.06}}}},  // RGB primaries
        2.4,
        false,
        false,
        "Rec. 709",
        "ITU-R BT.709 HD television standard"
    };
    
    color_space_info_[ColorSpace::SRGB] = color_space_info_[ColorSpace::BT709];
    color_space_info_[ColorSpace::SRGB].name = "sRGB";
    color_space_info_[ColorSpace::SRGB].description = "Standard RGB color space for computer displays";
    
    // Initialize BT.2020
    color_space_info_[ColorSpace::BT2020] = {
        ColorSpace::BT2020,
        WhitePoint::D65,
        {{{{0.708, 0.292}}, {{0.170, 0.797}}, {{0.131, 0.046}}}},  // RGB primaries
        2.4,
        false,
        true,
        "Rec. 2020",
        "ITU-R BT.2020 UHD television standard"
    };
    
    // Initialize DCI-P3
    color_space_info_[ColorSpace::DCI_P3] = {
        ColorSpace::DCI_P3,
        WhitePoint::DCI,
        {{{{0.680, 0.320}}, {{0.265, 0.690}}, {{0.150, 0.060}}}},  // RGB primaries
        2.6,
        false,
        true,
        "DCI-P3",
        "Digital Cinema Initiative P3 color space"
    };
    
    // Initialize Adobe RGB
    color_space_info_[ColorSpace::ADOBE_RGB] = {
        ColorSpace::ADOBE_RGB,
        WhitePoint::D65,
        {{{{0.64, 0.33}}, {{0.21, 0.71}}, {{0.15, 0.06}}}},  // RGB primaries
        2.2,
        false,
        true,
        "Adobe RGB",
        "Adobe RGB (1998) wide gamut color space"
    };
}

void ColorManagement::initializeWhitePoints() {
    // Initialize standard white points in XYZ coordinates
    white_point_xyz_[WhitePoint::D50] = XYZColor(0.9642, 1.0000, 0.8251);
    white_point_xyz_[WhitePoint::D55] = XYZColor(0.9568, 1.0000, 0.9214);
    white_point_xyz_[WhitePoint::D65] = XYZColor(0.9504, 1.0000, 1.0888);
    white_point_xyz_[WhitePoint::DCI] = XYZColor(0.8951, 1.0000, 0.9543);
    white_point_xyz_[WhitePoint::E] = XYZColor(1.0000, 1.0000, 1.0000);
}

ColorMatrix ColorManagement::multiplyMatrices(const ColorMatrix& a, const ColorMatrix& b) const {
    ColorMatrix result = {{{0.0}}};
    
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    
    return result;
}

ColorMatrix ColorManagement::invertMatrix(const ColorMatrix& matrix) const {
    // 3x3 matrix inversion using cofactor method
    double det = matrixDeterminant(matrix);
    
    if (std::abs(det) < 1e-10) {
        // Return identity matrix if not invertible
        return {{
            {{1.0, 0.0, 0.0}},
            {{0.0, 1.0, 0.0}},
            {{0.0, 0.0, 1.0}}
        }};
    }
    
    ColorMatrix inverse;
    
    // Calculate cofactor matrix and transpose
    inverse[0][0] = (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) / det;
    inverse[0][1] = (matrix[0][2] * matrix[2][1] - matrix[0][1] * matrix[2][2]) / det;
    inverse[0][2] = (matrix[0][1] * matrix[1][2] - matrix[0][2] * matrix[1][1]) / det;
    
    inverse[1][0] = (matrix[1][2] * matrix[2][0] - matrix[1][0] * matrix[2][2]) / det;
    inverse[1][1] = (matrix[0][0] * matrix[2][2] - matrix[0][2] * matrix[2][0]) / det;
    inverse[1][2] = (matrix[0][2] * matrix[1][0] - matrix[0][0] * matrix[1][2]) / det;
    
    inverse[2][0] = (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]) / det;
    inverse[2][1] = (matrix[0][1] * matrix[2][0] - matrix[0][0] * matrix[2][1]) / det;
    inverse[2][2] = (matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0]) / det;
    
    return inverse;
}

double ColorManagement::matrixDeterminant(const ColorMatrix& matrix) const {
    return matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
           matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
           matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
}

XYZColor ColorManagement::rgbToXYZ(const RGBColor& rgb, ColorSpace space) const {
    ColorMatrix matrix = getPrimariesMatrix(space);
    
    XYZColor xyz;
    xyz.X = matrix[0][0] * rgb.R + matrix[0][1] * rgb.G + matrix[0][2] * rgb.B;
    xyz.Y = matrix[1][0] * rgb.R + matrix[1][1] * rgb.G + matrix[1][2] * rgb.B;
    xyz.Z = matrix[2][0] * rgb.R + matrix[2][1] * rgb.G + matrix[2][2] * rgb.B;
    
    return xyz;
}

RGBColor ColorManagement::xyzToRGB(const XYZColor& xyz, ColorSpace space) const {
    ColorMatrix matrix = invertMatrix(getPrimariesMatrix(space));
    
    RGBColor rgb;
    rgb.R = matrix[0][0] * xyz.X + matrix[0][1] * xyz.Y + matrix[0][2] * xyz.Z;
    rgb.G = matrix[1][0] * xyz.X + matrix[1][1] * xyz.Y + matrix[1][2] * xyz.Z;
    rgb.B = matrix[2][0] * xyz.X + matrix[2][1] * xyz.Y + matrix[2][2] * xyz.Z;
    
    return rgb;
}

RGBColor ColorManagement::clipToGamut(const RGBColor& color, ColorSpace space) const {
    // Simple hard clipping to [0, 1] range
    return {
        clamp(color.R, 0.0, 1.0),
        clamp(color.G, 0.0, 1.0),
        clamp(color.B, 0.0, 1.0)
    };
}

RGBColor ColorManagement::perceptualGamutMap(const RGBColor& color, ColorSpace space, double factor) const {
    // Simplified perceptual gamut mapping
    // Real implementation would use more sophisticated algorithms
    
    // First check if any component is negative or too large
    double min_component = std::min({color.R, color.G, color.B});
    double max_component = std::max({color.R, color.G, color.B});
    
    if (min_component >= 0.0 && max_component <= 1.0) {
        return color;  // Already in gamut
    }
    
    // Handle negative values by clipping first, then scaling
    RGBColor clipped = {
        std::max(0.0, color.R),
        std::max(0.0, color.G),
        std::max(0.0, color.B)
    };
    
    // Now handle values > 1.0
    max_component = std::max({clipped.R, clipped.G, clipped.B});
    if (max_component > 1.0) {
        // Scale down while preserving hue
        double scale = (1.0 - factor) + factor / max_component;
        
        clipped.R *= scale;
        clipped.G *= scale;
        clipped.B *= scale;
    }
    
    // Final clamp to ensure valid range
    return {
        clamp(clipped.R, 0.0, 1.0),
        clamp(clipped.G, 0.0, 1.0),
        clamp(clipped.B, 0.0, 1.0)
    };
}

RGBColor ColorManagement::saturationPreservingMap(const RGBColor& color, ColorSpace space) const {
    // Preserve saturation while mapping to gamut
    
    // First handle negative values
    RGBColor clipped = {
        std::max(0.0, color.R),
        std::max(0.0, color.G),
        std::max(0.0, color.B)
    };
    
    double luma = luminance(clipped, space);
    
    RGBColor mapped = clipped;
    
    // If out of gamut, reduce saturation while preserving luminance
    if (!isInGamut(clipped, space)) {
        double saturation_scale = 0.9;  // Reduce saturation by 10%
        
        mapped.R = luma + (clipped.R - luma) * saturation_scale;
        mapped.G = luma + (clipped.G - luma) * saturation_scale;
        mapped.B = luma + (clipped.B - luma) * saturation_scale;
        
        mapped = clipToGamut(mapped, space);
    }
    
    return mapped;
}

double ColorManagement::applyGamma(double linear_value, double gamma) const {
    if (linear_value <= 0.0) return 0.0;
    return std::pow(linear_value, 1.0 / gamma);
}

double ColorManagement::removeGamma(double gamma_value, double gamma) const {
    if (gamma_value <= 0.0) return 0.0;
    return std::pow(gamma_value, gamma);
}

std::array<double, 3> ColorManagement::rgbToLab(const RGBColor& rgb, ColorSpace space) const {
    // Simplified RGB to Lab conversion
    // Real implementation would use precise XYZ intermediate step
    
    XYZColor xyz = rgbToXYZ(rgb, space);
    
    // Convert XYZ to Lab (simplified)
    double L = xyz.Y * 100.0;
    double a = (xyz.X - xyz.Y) * 128.0;
    double b = (xyz.Y - xyz.Z) * 128.0;
    
    return {L, a, b};
}

RGBColor ColorManagement::labToRGB(const std::array<double, 3>& lab, ColorSpace space) const {
    // Simplified Lab to RGB conversion
    double L = lab[0], a = lab[1], b = lab[2];
    
    // Convert Lab to XYZ (simplified)
    XYZColor xyz;
    xyz.Y = L / 100.0;
    xyz.X = xyz.Y + a / 128.0;
    xyz.Z = xyz.Y - b / 128.0;
    
    return xyzToRGB(xyz, space);
}

double ColorManagement::clamp(double value, double min_val, double max_val) const {
    return std::max(min_val, std::min(max_val, value));
}

bool ColorManagement::isValidRGB(const RGBColor& color) const {
    return (color.R >= 0.0 && color.R <= 1.0 &&
            color.G >= 0.0 && color.G <= 1.0 &&
            color.B >= 0.0 && color.B <= 1.0);
}

double ColorManagement::luminance(const RGBColor& color, ColorSpace space) const {
    // BT.709 luminance coefficients (approximate for all spaces)
    return 0.2126 * color.R + 0.7152 * color.G + 0.0722 * color.B;
}

// Utility functions implementation
namespace color_utils {

ColorSpace detectFromCodecInfo(const std::string& codec_name, const std::string& color_space_str) {
    std::string lower_codec = codec_name;
    std::transform(lower_codec.begin(), lower_codec.end(), lower_codec.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    std::string lower_space = color_space_str;
    std::transform(lower_space.begin(), lower_space.end(), lower_space.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (lower_space.find("bt2020") != std::string::npos || lower_space.find("2020") != std::string::npos) {
        return ColorSpace::BT2020;
    } else if (lower_space.find("dci") != std::string::npos || lower_space.find("p3") != std::string::npos) {
        return ColorSpace::DCI_P3;
    } else if (lower_space.find("adobe") != std::string::npos) {
        return ColorSpace::ADOBE_RGB;
    } else if (lower_space.find("srgb") != std::string::npos) {
        return ColorSpace::SRGB;
    } else if (lower_space.find("709") != std::string::npos || lower_space.find("bt709") != std::string::npos) {
        return ColorSpace::BT709;
    }
    
    // Default based on codec
    if (lower_codec.find("hevc") != std::string::npos || lower_codec.find("h265") != std::string::npos) {
        return ColorSpace::BT2020;  // Assume UHD content
    }
    
    return ColorSpace::BT709;  // Default
}

RGBColor bt709ToBt2020(const RGBColor& bt709_color) {
    ColorManagement cm;
    return cm.convertSingleColor(bt709_color, ColorSpace::BT709, ColorSpace::BT2020);
}

RGBColor bt2020ToBt709(const RGBColor& bt2020_color) {
    ColorManagement cm;
    return cm.convertSingleColor(bt2020_color, ColorSpace::BT2020, ColorSpace::BT709);
}

RGBColor dciP3ToDisplayP3(const RGBColor& dci_color) {
    ColorManagement cm;
    return cm.convertSingleColor(dci_color, ColorSpace::DCI_P3, ColorSpace::DISPLAY_P3);
}

RGBColor srgbToBt709(const RGBColor& srgb_color) {
    ColorManagement cm;
    return cm.convertSingleColor(srgb_color, ColorSpace::SRGB, ColorSpace::BT709);
}

WorkflowRecommendation getWorkflowRecommendation(
    ColorSpace source_space,
    const std::string& target_delivery,
    bool hdr_workflow) {
    
    WorkflowRecommendation rec;
    
    if (target_delivery.find("netflix") != std::string::npos || 
        target_delivery.find("streaming") != std::string::npos) {
        rec.working_space = ColorSpace::BT2020;
        rec.output_space = hdr_workflow ? ColorSpace::BT2020 : ColorSpace::BT709;
        rec.mapping_method = GamutMappingMethod::PERCEPTUAL;
        rec.requires_tone_mapping = hdr_workflow;
        rec.reasoning = "Streaming platforms prefer BT.2020 working space for future-proofing";
    } else if (target_delivery.find("cinema") != std::string::npos ||
               target_delivery.find("theatrical") != std::string::npos) {
        rec.working_space = ColorSpace::DCI_P3;
        rec.output_space = ColorSpace::DCI_P3;
        rec.mapping_method = GamutMappingMethod::RELATIVE_COLORIMETRIC;
        rec.requires_tone_mapping = false;
        rec.reasoning = "Cinema delivery requires DCI-P3 color space";
    } else {
        rec.working_space = ColorSpace::BT709;
        rec.output_space = ColorSpace::BT709;
        rec.mapping_method = GamutMappingMethod::PERCEPTUAL;
        rec.requires_tone_mapping = false;
        rec.reasoning = "Standard broadcast delivery using BT.709";
    }
    
    return rec;
}

ColorAccuracyReport validateWorkflow(
    const std::vector<RGBColor>& source_colors,
    ColorSpace source_space,
    ColorSpace target_space,
    GamutMappingMethod method) {
    
    ColorAccuracyReport report;
    
    ColorManagement cm;
    ColorConversionConfig config;
    config.source_space = source_space;
    config.target_space = target_space;
    config.gamut_method = method;
    
    auto result = cm.convertColorSpace(source_colors, config);
    
    report.average_delta_e = result.color_accuracy_delta_e;
    report.max_delta_e = 0.0;
    report.failing_pixels = result.out_of_gamut_pixels;
    
    // Calculate max Delta E
    for (size_t i = 0; i < source_colors.size() && i < result.converted_colors.size(); ++i) {
        double delta_e = cm.calculateDeltaE(source_colors[i], result.converted_colors[i]);
        report.max_delta_e = std::max(report.max_delta_e, delta_e);
    }
    
    // Calculate fidelity score
    report.color_fidelity_score = std::max(0.0, 1.0 - report.average_delta_e / 10.0);
    
    // Generate recommendations
    if (report.average_delta_e > 3.0) {
        report.recommendations.push_back("Consider using perceptual gamut mapping for better color preservation");
    }
    if (report.failing_pixels > source_colors.size() * 0.05) {
        report.recommendations.push_back("High number of out-of-gamut pixels - consider wider working color space");
    }
    if (report.color_fidelity_score < 0.8) {
        report.recommendations.push_back("Color accuracy below professional standards - review workflow");
    }
    
    return report;
}

} // namespace color_utils

} // namespace media_io
} // namespace ve
