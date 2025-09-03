// Week 9: Wide Color Gamut Support Implementation
// Complete implementation of wide color gamut processing for professional video workflows

#include "gfx/wide_color_gamut_support.hpp"
#include "core/logger.hpp"
#include "core/exceptions.hpp"
#include "gfx/device.hpp"
#include <algorithm>
#include <cmath>
#include <array>

namespace VideoEditor::GFX {

// =============================================================================
// Color Space Definitions and Constants
// =============================================================================

namespace {
    // RGB working space primaries (x, y coordinates)
    struct RGBPrimaries {
        ColorPrimaries red, green, blue;
        WhitePoint white_point;
    };
    
    const std::array<RGBPrimaries, 10> RGB_WORKING_SPACES = {{
        // sRGB / Rec.709
        {{{0.64f, 0.33f}, {0.30f, 0.60f}, {0.15f, 0.06f}, {0.3127f, 0.3290f}}},
        
        // Adobe RGB (1998)
        {{{0.64f, 0.33f}, {0.21f, 0.71f}, {0.15f, 0.06f}, {0.3127f, 0.3290f}}},
        
        // Rec.2020
        {{{0.708f, 0.292f}, {0.170f, 0.797f}, {0.131f, 0.046f}, {0.3127f, 0.3290f}}},
        
        // DCI-P3
        {{{0.680f, 0.320f}, {0.265f, 0.690f}, {0.150f, 0.060f}, {0.314f, 0.351f}}},
        
        // ACES CG
        {{{0.713f, 0.293f}, {0.165f, 0.830f}, {0.128f, 0.044f}, {0.32168f, 0.33767f}}},
        
        // ProPhoto RGB
        {{{0.7347f, 0.2653f}, {0.1596f, 0.8404f}, {0.0366f, 0.0001f}, {0.3457f, 0.3585f}}},
        
        // ALEXA Wide Gamut
        {{{0.684f, 0.313f}, {0.221f, 0.848f}, {0.0861f, -0.102f}, {0.3127f, 0.3290f}}},
        
        // RED Wide Gamut RGB
        {{{0.780308f, 0.304253f}, {0.121595f, 1.493994f}, {0.095612f, -0.084589f}, {0.3127f, 0.3290f}}},
        
        // Canon Cinema Gamut
        {{{0.740f, 0.270f}, {0.170f, 1.140f}, {0.080f, -0.100f}, {0.3127f, 0.3290f}}},
        
        // Sony S-Gamut3.Cine
        {{{0.766f, 0.275f}, {0.225f, 0.800f}, {0.089f, -0.087f}, {0.3127f, 0.3290f}}}
    }};
    
    // Chromatic adaptation matrices
    const std::array<std::array<float, 9>, 4> CHROMATIC_ADAPTATION_MATRICES = {{
        // Bradford
        {0.8951f, 0.2664f, -0.1614f, -0.7502f, 1.7135f, 0.0367f, 0.0389f, -0.0685f, 1.0296f},
        
        // Von Kries
        {0.4002f, 0.7075f, -0.0807f, -0.2280f, 1.1500f, 0.0612f, 0.0000f, 0.0000f, 0.9184f},
        
        // CAT02
        {0.7328f, 0.4296f, -0.1624f, -0.7036f, 1.6975f, 0.0061f, 0.0030f, 0.0136f, 0.9834f},
        
        // CAT16
        {0.401288f, 0.650173f, -0.051461f, -0.250268f, 1.204414f, 0.045854f, -0.002079f, 0.048952f, 0.953127f}
    }};
    
    // Standard illuminants (x, y coordinates)
    const std::array<WhitePoint, 6> STANDARD_ILLUMINANTS = {{
        {0.3127f, 0.3290f}, // D65
        {0.3457f, 0.3585f}, // D50
        {0.3324f, 0.3474f}, // C
        {0.3101f, 0.3162f}, // E
        {0.4254f, 0.4044f}, // A (2856K)
        {0.3774f, 0.3875f}  // F2
    }};
}

// =============================================================================
// Color Matrix Operations
// =============================================================================

ColorMatrix3x3 WideColorGamutSupport::multiply_matrices(const ColorMatrix3x3& a, 
                                                       const ColorMatrix3x3& b) const noexcept {
    ColorMatrix3x3 result{};
    
    try {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result.m[i][j] = 0.0f;
                for (int k = 0; k < 3; ++k) {
                    result.m[i][j] += a.m[i][k] * b.m[k][j];
                }
            }
        }
    } catch (...) {
        LOG_ERROR("Matrix multiplication failed");
        // Return identity matrix on error
        result.m[0][0] = result.m[1][1] = result.m[2][2] = 1.0f;
    }
    
    return result;
}

ColorMatrix3x3 WideColorGamutSupport::invert_matrix(const ColorMatrix3x3& matrix) const noexcept {
    ColorMatrix3x3 result{};
    
    try {
        // Calculate determinant
        float det = matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1]) -
                   matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0]) +
                   matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]);
        
        if (std::abs(det) < 1e-8f) {
            LOG_ERROR("Matrix is not invertible (determinant near zero)");
            result.m[0][0] = result.m[1][1] = result.m[2][2] = 1.0f;
            return result;
        }
        
        float inv_det = 1.0f / det;
        
        // Calculate adjugate matrix elements
        result.m[0][0] = (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1]) * inv_det;
        result.m[0][1] = (matrix.m[0][2] * matrix.m[2][1] - matrix.m[0][1] * matrix.m[2][2]) * inv_det;
        result.m[0][2] = (matrix.m[0][1] * matrix.m[1][2] - matrix.m[0][2] * matrix.m[1][1]) * inv_det;
        
        result.m[1][0] = (matrix.m[1][2] * matrix.m[2][0] - matrix.m[1][0] * matrix.m[2][2]) * inv_det;
        result.m[1][1] = (matrix.m[0][0] * matrix.m[2][2] - matrix.m[0][2] * matrix.m[2][0]) * inv_det;
        result.m[1][2] = (matrix.m[0][2] * matrix.m[1][0] - matrix.m[0][0] * matrix.m[1][2]) * inv_det;
        
        result.m[2][0] = (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]) * inv_det;
        result.m[2][1] = (matrix.m[0][1] * matrix.m[2][0] - matrix.m[0][0] * matrix.m[2][1]) * inv_det;
        result.m[2][2] = (matrix.m[0][0] * matrix.m[1][1] - matrix.m[0][1] * matrix.m[1][0]) * inv_det;
        
    } catch (...) {
        LOG_ERROR("Matrix inversion failed");
        result.m[0][0] = result.m[1][1] = result.m[2][2] = 1.0f;
    }
    
    return result;
}

void WideColorGamutSupport::apply_matrix(const ColorMatrix3x3& matrix, 
                                        float& r, float& g, float& b) const noexcept {
    try {
        float new_r = matrix.m[0][0] * r + matrix.m[0][1] * g + matrix.m[0][2] * b;
        float new_g = matrix.m[1][0] * r + matrix.m[1][1] * g + matrix.m[1][2] * b;
        float new_b = matrix.m[2][0] * r + matrix.m[2][1] * g + matrix.m[2][2] * b;
        
        r = new_r;
        g = new_g;
        b = new_b;
    } catch (...) {
        LOG_ERROR("Matrix application failed");
    }
}

// =============================================================================
// Color Space Conversion
// =============================================================================

ColorMatrix3x3 WideColorGamutSupport::calculate_rgb_to_xyz_matrix(RGBWorkingSpace space) const {
    ColorMatrix3x3 result{};
    
    try {
        if (space == RGBWorkingSpace::INVALID || 
            static_cast<size_t>(space) >= RGB_WORKING_SPACES.size()) {
            LOG_ERROR("Invalid RGB working space");
            result.m[0][0] = result.m[1][1] = result.m[2][2] = 1.0f;
            return result;
        }
        
        const RGBPrimaries& primaries = RGB_WORKING_SPACES[static_cast<size_t>(space)];
        
        // Calculate XYZ coordinates from xy chromaticity coordinates
        auto xy_to_xyz = [](const ColorPrimaries& primary) -> std::array<float, 3> {
            float Y = 1.0f;
            float X = (primary.x / primary.y) * Y;
            float Z = ((1.0f - primary.x - primary.y) / primary.y) * Y;
            return {X, Y, Z};
        };
        
        auto red_xyz = xy_to_xyz(primaries.red);
        auto green_xyz = xy_to_xyz(primaries.green);
        auto blue_xyz = xy_to_xyz(primaries.blue);
        
        // White point XYZ
        float white_Y = 1.0f;
        float white_X = (primaries.white_point.x / primaries.white_point.y) * white_Y;
        float white_Z = ((1.0f - primaries.white_point.x - primaries.white_point.y) / primaries.white_point.y) * white_Y;
        
        // Create matrix from primaries
        ColorMatrix3x3 primaries_matrix{};
        primaries_matrix.m[0][0] = red_xyz[0];   primaries_matrix.m[0][1] = green_xyz[0]; primaries_matrix.m[0][2] = blue_xyz[0];
        primaries_matrix.m[1][0] = red_xyz[1];   primaries_matrix.m[1][1] = green_xyz[1]; primaries_matrix.m[1][2] = blue_xyz[1];
        primaries_matrix.m[2][0] = red_xyz[2];   primaries_matrix.m[2][1] = green_xyz[2]; primaries_matrix.m[2][2] = blue_xyz[2];
        
        // Calculate scaling factors
        ColorMatrix3x3 inv_primaries = invert_matrix(primaries_matrix);
        float scale_r = inv_primaries.m[0][0] * white_X + inv_primaries.m[0][1] * white_Y + inv_primaries.m[0][2] * white_Z;
        float scale_g = inv_primaries.m[1][0] * white_X + inv_primaries.m[1][1] * white_Y + inv_primaries.m[1][2] * white_Z;
        float scale_b = inv_primaries.m[2][0] * white_X + inv_primaries.m[2][1] * white_Y + inv_primaries.m[2][2] * white_Z;
        
        // Apply scaling
        result.m[0][0] = primaries_matrix.m[0][0] * scale_r;
        result.m[0][1] = primaries_matrix.m[0][1] * scale_g;
        result.m[0][2] = primaries_matrix.m[0][2] * scale_b;
        
        result.m[1][0] = primaries_matrix.m[1][0] * scale_r;
        result.m[1][1] = primaries_matrix.m[1][1] * scale_g;
        result.m[1][2] = primaries_matrix.m[1][2] * scale_b;
        
        result.m[2][0] = primaries_matrix.m[2][0] * scale_r;
        result.m[2][1] = primaries_matrix.m[2][1] * scale_g;
        result.m[2][2] = primaries_matrix.m[2][2] * scale_b;
        
    } catch (const std::exception& e) {
        LOG_ERROR("RGB to XYZ matrix calculation failed: {}", e.what());
        result.m[0][0] = result.m[1][1] = result.m[2][2] = 1.0f;
    }
    
    return result;
}

ColorMatrix3x3 WideColorGamutSupport::get_chromatic_adaptation_matrix(ChromaticAdaptation method) const {
    ColorMatrix3x3 result{};
    
    try {
        size_t matrix_index = static_cast<size_t>(method);
        if (matrix_index >= CHROMATIC_ADAPTATION_MATRICES.size()) {
            LOG_ERROR("Invalid chromatic adaptation method");
            result.m[0][0] = result.m[1][1] = result.m[2][2] = 1.0f;
            return result;
        }
        
        const auto& matrix_data = CHROMATIC_ADAPTATION_MATRICES[matrix_index];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result.m[i][j] = matrix_data[i * 3 + j];
            }
        }
        
    } catch (...) {
        LOG_ERROR("Chromatic adaptation matrix retrieval failed");
        result.m[0][0] = result.m[1][1] = result.m[2][2] = 1.0f;
    }
    
    return result;
}

// =============================================================================
// Gamut Mapping Functions
// =============================================================================

void WideColorGamutSupport::apply_clipping_gamut_mapping(float& r, float& g, float& b) const noexcept {
    try {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
    } catch (...) {
        LOG_ERROR("Clipping gamut mapping failed");
    }
}

void WideColorGamutSupport::apply_compression_gamut_mapping(float& r, float& g, float& b, 
                                                           float compression_factor) const noexcept {
    try {
        // Find the maximum component
        float max_component = std::max({r, g, b});
        
        if (max_component > 1.0f) {
            // Apply compression to out-of-gamut colors
            float compression_ratio = 1.0f + (max_component - 1.0f) * compression_factor;
            float scale = compression_ratio / max_component;
            
            r *= scale;
            g *= scale;
            b *= scale;
        }
    } catch (...) {
        LOG_ERROR("Compression gamut mapping failed");
    }
}

void WideColorGamutSupport::apply_perceptual_gamut_mapping(float& r, float& g, float& b, 
                                                          float saturation_preservation) const noexcept {
    try {
        // Convert to luminance-chromaticity representation
        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        
        // Calculate chromaticity components
        float chroma_r = r - luminance;
        float chroma_g = g - luminance;
        float chroma_b = b - luminance;
        
        // Calculate chroma magnitude
        float chroma_magnitude = std::sqrt(chroma_r * chroma_r + chroma_g * chroma_g + chroma_b * chroma_b);
        
        // Estimate maximum allowable chroma for this luminance
        float max_chroma = std::min(1.0f - luminance, luminance); // Simplified boundary
        
        if (chroma_magnitude > max_chroma && chroma_magnitude > 0.0f) {
            // Apply perceptual compression
            float compression = std::lerp(max_chroma / chroma_magnitude, 1.0f, saturation_preservation);
            
            chroma_r *= compression;
            chroma_g *= compression;
            chroma_b *= compression;
            
            // Reconstruct RGB
            r = luminance + chroma_r;
            g = luminance + chroma_g;
            b = luminance + chroma_b;
        }
    } catch (...) {
        LOG_ERROR("Perceptual gamut mapping failed");
    }
}

void WideColorGamutSupport::apply_relative_colorimetric_mapping(float& r, float& g, float& b) const noexcept {
    try {
        // Simple relative colorimetric mapping - preserve white point and clip out-of-gamut colors
        // In practice, this would involve more sophisticated boundary detection
        
        // Check if the color is within the target gamut
        bool in_gamut = (r >= 0.0f && r <= 1.0f && g >= 0.0f && g <= 1.0f && b >= 0.0f && b <= 1.0f);
        
        if (!in_gamut) {
            // Find the intersection with the gamut boundary
            float scale = 1.0f;
            
            if (r > 1.0f) scale = std::min(scale, 1.0f / r);
            if (g > 1.0f) scale = std::min(scale, 1.0f / g);
            if (b > 1.0f) scale = std::min(scale, 1.0f / b);
            if (r < 0.0f) scale = std::min(scale, -r / (r - 0.5f));
            if (g < 0.0f) scale = std::min(scale, -g / (g - 0.5f));
            if (b < 0.0f) scale = std::min(scale, -b / (b - 0.5f));
            
            r *= scale;
            g *= scale;
            b *= scale;
        }
    } catch (...) {
        LOG_ERROR("Relative colorimetric mapping failed");
    }
}

// =============================================================================
// Color Analysis Functions
// =============================================================================

WideColorGamutSupport::ColorGamutAnalysis 
WideColorGamutSupport::analyze_color_gamut(const std::vector<float>& rgb_data, 
                                          size_t width, size_t height, 
                                          RGBWorkingSpace target_space) const {
    ColorGamutAnalysis analysis{};
    
    try {
        const size_t total_pixels = width * height;
        if (rgb_data.size() < total_pixels * 3) {
            LOG_ERROR("Insufficient RGB data for gamut analysis");
            return analysis;
        }
        
        size_t out_of_gamut_pixels = 0;
        float max_saturation = 0.0f;
        float total_saturation = 0.0f;
        
        // Analyze each pixel
        for (size_t i = 0; i < total_pixels; ++i) {
            const size_t base_idx = i * 3;
            float r = rgb_data[base_idx];
            float g = rgb_data[base_idx + 1];
            float b = rgb_data[base_idx + 2];
            
            // Check if pixel is out of target gamut
            bool out_of_gamut = (r < 0.0f || r > 1.0f || g < 0.0f || g > 1.0f || b < 0.0f || b > 1.0f);
            if (out_of_gamut) {
                ++out_of_gamut_pixels;
            }
            
            // Calculate saturation (simplified)
            float max_component = std::max({r, g, b});
            float min_component = std::min({r, g, b});
            float saturation = (max_component > 0.0f) ? (max_component - min_component) / max_component : 0.0f;
            
            max_saturation = std::max(max_saturation, saturation);
            total_saturation += saturation;
        }
        
        analysis.gamut_coverage = 1.0f - (static_cast<float>(out_of_gamut_pixels) / static_cast<float>(total_pixels));
        analysis.peak_saturation = max_saturation;
        analysis.average_saturation = total_saturation / static_cast<float>(total_pixels);
        analysis.out_of_gamut_percentage = static_cast<float>(out_of_gamut_pixels) / static_cast<float>(total_pixels) * 100.0f;
        analysis.requires_gamut_mapping = (out_of_gamut_pixels > 0);
        analysis.color_space_utilization = std::min(max_saturation * 100.0f, 100.0f);
        
        LOG_INFO("Gamut analysis complete: Coverage={:.2f}%, Peak Sat={:.2f}, Out of gamut={:.2f}%", 
                analysis.gamut_coverage * 100.0f, analysis.peak_saturation, analysis.out_of_gamut_percentage);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Color gamut analysis failed: {}", e.what());
    }
    
    return analysis;
}

float WideColorGamutSupport::calculate_color_difference_delta_e(float r1, float g1, float b1, 
                                                               float r2, float g2, float b2) const noexcept {
    try {
        // Simplified Delta E calculation using RGB (in practice, should use LAB color space)
        float delta_r = r1 - r2;
        float delta_g = g1 - g2;
        float delta_b = b1 - b2;
        
        // Weighted Euclidean distance (approximation of Delta E)
        float delta_e = std::sqrt(2.0f * delta_r * delta_r + 
                                 4.0f * delta_g * delta_g + 
                                 3.0f * delta_b * delta_b);
        
        return delta_e * 100.0f; // Scale to typical Delta E range
    } catch (...) {
        LOG_ERROR("Delta E calculation failed");
        return 0.0f;
    }
}

bool WideColorGamutSupport::is_color_within_gamut(float r, float g, float b, 
                                                 RGBWorkingSpace gamut) const noexcept {
    try {
        // For most RGB working spaces, check if values are within [0, 1] range
        // More sophisticated gamut checking would require 3D gamut boundary calculations
        
        switch (gamut) {
            case RGBWorkingSpace::SRGB:
            case RGBWorkingSpace::BT_709:
                return (r >= 0.0f && r <= 1.0f && g >= 0.0f && g <= 1.0f && b >= 0.0f && b <= 1.0f);
                
            case RGBWorkingSpace::ADOBE_RGB:
            case RGBWorkingSpace::BT_2020:
            case RGBWorkingSpace::DCI_P3:
                // These have larger gamuts, so allow for extended range
                return (r >= -0.1f && r <= 1.1f && g >= -0.1f && g <= 1.1f && b >= -0.1f && b <= 1.1f);
                
            case RGBWorkingSpace::ACES_CG:
            case RGBWorkingSpace::PROPHOTO_RGB:
                // Very wide gamuts
                return (r >= -0.5f && r <= 2.0f && g >= -0.5f && g <= 2.0f && b >= -0.5f && b <= 2.0f);
                
            default:
                return (r >= 0.0f && r <= 1.0f && g >= 0.0f && g <= 1.0f && b >= 0.0f && b <= 1.0f);
        }
    } catch (...) {
        return false;
    }
}

// =============================================================================
// Constructor and Core Methods
// =============================================================================

WideColorGamutSupport::WideColorGamutSupport(std::shared_ptr<Device> device) 
    : device_(device) {
    try {
        if (!device_) {
            // EXCEPTION_POLICY_OK: Core performance module must remain exception-free per DR-0003
            LOG_ERROR("Device cannot be null in WideColorGamutSupport constructor");
            device_ = nullptr; // Ensure invalid state is detectable
            return; // Constructor cannot return error code, caller must check isValid()
        }
        
        LOG_INFO("WideColorGamutSupport initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("WideColorGamutSupport initialization failed: {}", e.what());
        // EXCEPTION_POLICY_OK: Core performance module must remain exception-free per DR-0003
        device_ = nullptr; // Mark as invalid, caller must check isValid()
    }
}

WideColorGamutSupport::~WideColorGamutSupport() = default;

bool WideColorGamutSupport::convert_color_space(const FrameData& input, FrameData& output, 
                                               RGBWorkingSpace from_space, 
                                               RGBWorkingSpace to_space, 
                                               ChromaticAdaptation adaptation_method) {
    try {
        if (from_space == to_space) {
            output = input;
            return true;
        }
        
        // Calculate conversion matrices
        ColorMatrix3x3 from_to_xyz = calculate_rgb_to_xyz_matrix(from_space);
        ColorMatrix3x3 to_from_xyz = invert_matrix(calculate_rgb_to_xyz_matrix(to_space));
        
        // Apply chromatic adaptation if needed
        ColorMatrix3x3 adaptation_matrix = get_chromatic_adaptation_matrix(adaptation_method);
        
        // Combine matrices
        ColorMatrix3x3 conversion_matrix = multiply_matrices(to_from_xyz, 
                                                           multiply_matrices(adaptation_matrix, from_to_xyz));
        
        // Prepare output frame
        output = input;
        
        const size_t pixel_count = input.width * input.height;
        if (input.rgb_data.size() < pixel_count * 3 || output.rgb_data.size() < pixel_count * 3) {
            LOG_ERROR("Insufficient frame data for color space conversion");
            return false;
        }
        
        // Convert each pixel
        for (size_t i = 0; i < pixel_count; ++i) {
            const size_t base_idx = i * 3;
            
            float r = input.rgb_data[base_idx];
            float g = input.rgb_data[base_idx + 1];
            float b = input.rgb_data[base_idx + 2];
            
            apply_matrix(conversion_matrix, r, g, b);
            
            output.rgb_data[base_idx] = r;
            output.rgb_data[base_idx + 1] = g;
            output.rgb_data[base_idx + 2] = b;
        }
        
        LOG_DEBUG("Color space conversion completed: {} -> {}", 
                 static_cast<int>(from_space), static_cast<int>(to_space));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Color space conversion failed: {}", e.what());
        return false;
    }
}

bool WideColorGamutSupport::apply_gamut_mapping(FrameData& frame_data, 
                                               RGBWorkingSpace target_gamut, 
                                               GamutMapping mapping_method, 
                                               float mapping_strength) {
    try {
        const size_t pixel_count = frame_data.width * frame_data.height;
        if (frame_data.rgb_data.size() < pixel_count * 3) {
            LOG_ERROR("Insufficient frame data for gamut mapping");
            return false;
        }
        
        for (size_t i = 0; i < pixel_count; ++i) {
            const size_t base_idx = i * 3;
            
            float r = frame_data.rgb_data[base_idx];
            float g = frame_data.rgb_data[base_idx + 1];
            float b = frame_data.rgb_data[base_idx + 2];
            
            // Apply gamut mapping based on method
            switch (mapping_method) {
                case GamutMapping::CLIP:
                    apply_clipping_gamut_mapping(r, g, b);
                    break;
                    
                case GamutMapping::COMPRESS:
                    apply_compression_gamut_mapping(r, g, b, mapping_strength);
                    break;
                    
                case GamutMapping::PERCEPTUAL:
                    apply_perceptual_gamut_mapping(r, g, b, mapping_strength);
                    break;
                    
                case GamutMapping::RELATIVE_COLORIMETRIC:
                    apply_relative_colorimetric_mapping(r, g, b);
                    break;
                    
                default:
                    LOG_WARNING("Unsupported gamut mapping method: {}", static_cast<int>(mapping_method));
                    break;
            }
            
            frame_data.rgb_data[base_idx] = r;
            frame_data.rgb_data[base_idx + 1] = g;
            frame_data.rgb_data[base_idx + 2] = b;
        }
        
        LOG_DEBUG("Gamut mapping completed using method {}", static_cast<int>(mapping_method));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Gamut mapping failed: {}", e.what());
        return false;
    }
}

} // namespace VideoEditor::GFX
