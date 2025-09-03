// Week 9: HDR Processor Implementation
// Complete implementation of HDR processing system for professional video workflows

#include "gfx/hdr_processor.hpp"
#include "core/logger.hpp"
#include "core/exceptions.hpp"
#include "gfx/device.hpp"
#include "gfx/texture.hpp"
#include "gfx/shader.hpp"
#include <algorithm>
#include <cmath>
#include <array>

namespace VideoEditor::GFX {

// =============================================================================
// Transfer Function Implementations
// =============================================================================

namespace {
    // PQ (SMPTE ST 2084) constants
    constexpr float PQ_M1 = 0.1593017578125f;    // 2610.0 / 16384.0
    constexpr float PQ_M2 = 78.84375f;           // 2523.0 / 32.0
    constexpr float PQ_C1 = 0.8359375f;          // 3424.0 / 4096.0
    constexpr float PQ_C2 = 18.8515625f;         // 2413.0 / 128.0
    constexpr float PQ_C3 = 18.6875f;            // 2392.0 / 128.0
    
    // HLG constants
    constexpr float HLG_A = 0.17883277f;
    constexpr float HLG_B = 0.28466892f;
    constexpr float HLG_C = 0.55991073f;
    constexpr float HLG_BETA = 0.04f;
    
    // Log C constants (ARRI)
    constexpr float LOGC_A = 5.555556f;
    constexpr float LOGC_B = 0.047996f;
    constexpr float LOGC_C = 0.244161f;
    constexpr float LOGC_D = 0.386036f;
    constexpr float LOGC_CUT = 0.010591f;
    constexpr float LOGC_E = 5.367655f;
    constexpr float LOGC_F = 0.092809f;
}

float HDRProcessor::apply_pq_oetf(float linear_value, float max_luminance) noexcept {
    try {
        // Normalize to 0-1 range
        float normalized = std::clamp(linear_value / max_luminance, 0.0f, 1.0f);
        
        // Apply PQ curve
        float powed = std::pow(normalized, PQ_M1);
        float numerator = PQ_C1 + PQ_C2 * powed;
        float denominator = 1.0f + PQ_C3 * powed;
        
        return std::pow(numerator / denominator, PQ_M2);
    } catch (...) {
        LOG_ERROR("PQ OETF application failed");
        return 0.0f;
    }
}

float HDRProcessor::apply_pq_eotf(float pq_value, float max_luminance) noexcept {
    try {
        // Inverse PQ curve
        float powed = std::pow(std::clamp(pq_value, 0.0f, 1.0f), 1.0f / PQ_M2);
        float numerator = std::max(powed - PQ_C1, 0.0f);
        float denominator = PQ_C2 - PQ_C3 * powed;
        
        if (denominator <= 0.0f) return 0.0f;
        
        float linear_norm = std::pow(numerator / denominator, 1.0f / PQ_M1);
        return linear_norm * max_luminance;
    } catch (...) {
        LOG_ERROR("PQ EOTF application failed");
        return 0.0f;
    }
}

float HDRProcessor::apply_hlg_oetf(float linear_value) noexcept {
    try {
        float E = std::clamp(linear_value, 0.0f, 1.0f);
        
        if (E <= 1.0f / 12.0f) {
            return std::sqrt(3.0f * E);
        } else {
            return HLG_A * std::log(12.0f * E - HLG_B) + HLG_C;
        }
    } catch (...) {
        LOG_ERROR("HLG OETF application failed");
        return 0.0f;
    }
}

float HDRProcessor::apply_hlg_eotf(float hlg_value) noexcept {
    try {
        float E_prime = std::clamp(hlg_value, 0.0f, 1.0f);
        
        if (E_prime <= 0.5f) {
            return (E_prime * E_prime) / 3.0f;
        } else {
            return (std::exp((E_prime - HLG_C) / HLG_A) + HLG_B) / 12.0f;
        }
    } catch (...) {
        LOG_ERROR("HLG EOTF application failed");
        return 0.0f;
    }
}

float HDRProcessor::apply_log_c_encoding(float linear_value) noexcept {
    try {
        float E = std::clamp(linear_value, 0.0f, 100.0f); // Typical LogC range
        
        if (E > LOGC_CUT) {
            return LOGC_C * std::log10(LOGC_A * E + LOGC_B) + LOGC_D;
        } else {
            return LOGC_E * E + LOGC_F;
        }
    } catch (...) {
        LOG_ERROR("LogC encoding failed");
        return 0.0f;
    }
}

float HDRProcessor::apply_log_c_decoding(float log_value) noexcept {
    try {
        float threshold = LOGC_E * LOGC_CUT + LOGC_F;
        
        if (log_value > threshold) {
            return (std::pow(10.0f, (log_value - LOGC_D) / LOGC_C) - LOGC_B) / LOGC_A;
        } else {
            return (log_value - LOGC_F) / LOGC_E;
        }
    } catch (...) {
        LOG_ERROR("LogC decoding failed");
        return 0.0f;
    }
}

// =============================================================================
// Color Space Conversion Matrices
// =============================================================================

namespace {
    // Color space conversion matrices (row-major)
    const std::array<std::array<float, 9>, 8> COLOR_SPACE_MATRICES = {{
        // BT.709 to BT.2020
        {0.6274f, 0.3293f, 0.0433f, 0.0691f, 0.9195f, 0.0114f, 0.0164f, 0.0880f, 0.8956f},
        
        // BT.2020 to BT.709
        {1.7166511f, -0.3556708f, -0.2533663f, -0.6666844f, 1.6164812f, 0.0157685f, 0.0176399f, -0.0427706f, 0.9421031f},
        
        // BT.709 to DCI-P3
        {0.8224621f, 0.1775380f, 0.0000000f, 0.0331941f, 0.9668058f, 0.0000000f, 0.0170827f, 0.0723974f, 0.9105199f},
        
        // DCI-P3 to BT.709
        {1.2249401f, -0.2249404f, 0.0000000f, -0.0420569f, 1.0420571f, 0.0000000f, -0.0196376f, -0.0786361f, 1.0982735f},
        
        // BT.2020 to DCI-P3
        {1.3459433f, -0.2556075f, -0.0511118f, -0.5445989f, 1.5081673f, 0.0202050f, 0.0000000f, -0.0118732f, 1.0118732f},
        
        // DCI-P3 to BT.2020
        {0.7347000f, 0.2653000f, 0.0000000f, 0.0000000f, 1.0000000f, 0.0000000f, 0.0000000f, 0.0000000f, 1.0000000f},
        
        // ACES CG to BT.709
        {1.7050515f, -0.6217923f, -0.0832593f, -0.1302597f, 1.1408027f, -0.0105430f, -0.0240003f, -0.1289687f, 1.1529691f},
        
        // BT.709 to ACES CG
        {0.6131325f, 0.3395209f, 0.0473466f, 0.0701871f, 0.9160283f, 0.0137846f, 0.0206393f, 0.1095902f, 0.8697705f}
    }};
    
    // Bradford chromatic adaptation matrix
    const std::array<float, 9> BRADFORD_MATRIX = {
        0.8951000f, 0.2664000f, -0.1614000f,
        -0.7502000f, 1.7135000f, 0.0367000f,
        0.0389000f, -0.0685000f, 1.0296000f
    };
    
    const std::array<float, 9> BRADFORD_INVERSE = {
        0.9869929f, -0.1470543f, 0.1599627f,
        0.4323053f, 0.5183603f, 0.0492912f,
        -0.0085287f, 0.0400428f, 0.9684867f
    };
}

ColorSpaceMatrix HDRProcessor::get_color_space_matrix(ColorSpace from, ColorSpace to) const {
    ColorSpaceMatrix matrix{};
    
    try {
        if (from == to) {
            // Identity matrix
            matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 1.0f;
            return matrix;
        }
        
        // Get appropriate conversion matrix
        size_t matrix_index = 0;
        
        if (from == ColorSpace::BT_709 && to == ColorSpace::BT_2020) {
            matrix_index = 0;
        } else if (from == ColorSpace::BT_2020 && to == ColorSpace::BT_709) {
            matrix_index = 1;
        } else if (from == ColorSpace::BT_709 && to == ColorSpace::DCI_P3) {
            matrix_index = 2;
        } else if (from == ColorSpace::DCI_P3 && to == ColorSpace::BT_709) {
            matrix_index = 3;
        } else if (from == ColorSpace::BT_2020 && to == ColorSpace::DCI_P3) {
            matrix_index = 4;
        } else if (from == ColorSpace::DCI_P3 && to == ColorSpace::BT_2020) {
            matrix_index = 5;
        } else if (from == ColorSpace::ACES_CG && to == ColorSpace::BT_709) {
            matrix_index = 6;
        } else if (from == ColorSpace::BT_709 && to == ColorSpace::ACES_CG) {
            matrix_index = 7;
        } else {
            LOG_WARNING("Unsupported color space conversion: {} -> {}", 
                       static_cast<int>(from), static_cast<int>(to));
            matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 1.0f;
            return matrix;
        }
        
        const auto& src_matrix = COLOR_SPACE_MATRICES[matrix_index];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                matrix.m[i][j] = src_matrix[i * 3 + j];
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Color space matrix calculation failed: {}", e.what());
        matrix.m[0][0] = matrix.m[1][1] = matrix.m[2][2] = 1.0f;
    }
    
    return matrix;
}

void HDRProcessor::apply_color_space_matrix(const ColorSpaceMatrix& matrix, 
                                           float& r, float& g, float& b) const noexcept {
    try {
        float new_r = matrix.m[0][0] * r + matrix.m[0][1] * g + matrix.m[0][2] * b;
        float new_g = matrix.m[1][0] * r + matrix.m[1][1] * g + matrix.m[1][2] * b;
        float new_b = matrix.m[2][0] * r + matrix.m[2][1] * g + matrix.m[2][2] * b;
        
        r = new_r;
        g = new_g;
        b = new_b;
    } catch (...) {
        LOG_ERROR("Color space matrix application failed");
    }
}

// =============================================================================
// Tone Mapping Operators
// =============================================================================

void HDRProcessor::apply_reinhard_tone_mapping(float& r, float& g, float& b, 
                                              float white_point) const noexcept {
    try {
        auto tone_map = [white_point](float value) -> float {
            float white_sq = white_point * white_point;
            return value * (1.0f + value / white_sq) / (1.0f + value);
        };
        
        r = tone_map(r);
        g = tone_map(g);
        b = tone_map(b);
    } catch (...) {
        LOG_ERROR("Reinhard tone mapping failed");
    }
}

void HDRProcessor::apply_hable_tone_mapping(float& r, float& g, float& b, 
                                           float exposure_bias) const noexcept {
    try {
        auto hable_partial = [](float x) -> float {
            constexpr float A = 0.15f; // Shoulder strength
            constexpr float B = 0.50f; // Linear strength
            constexpr float C = 0.10f; // Linear angle
            constexpr float D = 0.20f; // Toe strength
            constexpr float E = 0.02f; // Toe numerator
            constexpr float F = 0.30f; // Toe denominator
            
            return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
        };
        
        float curr_r = hable_partial(r * exposure_bias);
        float curr_g = hable_partial(g * exposure_bias);
        float curr_b = hable_partial(b * exposure_bias);
        
        float white_scale = 1.0f / hable_partial(11.2f);
        
        r = curr_r * white_scale;
        g = curr_g * white_scale;
        b = curr_b * white_scale;
    } catch (...) {
        LOG_ERROR("Hable tone mapping failed");
    }
}

void HDRProcessor::apply_aces_tone_mapping(float& r, float& g, float& b) const noexcept {
    try {
        auto aces_curve = [](float x) -> float {
            constexpr float a = 2.51f;
            constexpr float b = 0.03f;
            constexpr float c = 2.43f;
            constexpr float d = 0.59f;
            constexpr float e = 0.14f;
            
            return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
        };
        
        r = aces_curve(r);
        g = aces_curve(g);
        b = aces_curve(b);
    } catch (...) {
        LOG_ERROR("ACES tone mapping failed");
    }
}

void HDRProcessor::apply_filmic_tone_mapping(float& r, float& g, float& b, 
                                            float shoulder, float linear_start, 
                                            float linear_length, float black_tightness) const noexcept {
    try {
        auto filmic_curve = [shoulder, linear_start, linear_length, black_tightness](float x) -> float {
            float adjusted = std::max(0.0f, x - black_tightness);
            float linear_part = linear_start + linear_length * adjusted;
            float shoulder_part = std::pow(adjusted, shoulder);
            
            float t = std::clamp((adjusted - linear_start) / linear_length, 0.0f, 1.0f);
            return linear_part * (1.0f - t) + shoulder_part * t;
        };
        
        r = filmic_curve(r);
        g = filmic_curve(g);
        b = filmic_curve(b);
    } catch (...) {
        LOG_ERROR("Filmic tone mapping failed");
    }
}

// =============================================================================
// HDR Analysis Functions
// =============================================================================

float HDRProcessor::calculate_luminance(float r, float g, float b, ColorSpace color_space) const noexcept {
    try {
        switch (color_space) {
            case ColorSpace::BT_709:
            case ColorSpace::SRGB:
                return 0.2126f * r + 0.7152f * g + 0.0722f * b;
                
            case ColorSpace::BT_2020:
                return 0.2627f * r + 0.6780f * g + 0.0593f * b;
                
            case ColorSpace::DCI_P3:
                return 0.209f * r + 0.721f * g + 0.070f * b;
                
            case ColorSpace::ACES_CG:
                return 0.272f * r + 0.674f * g + 0.054f * b;
                
            default:
                return 0.2126f * r + 0.7152f * g + 0.0722f * b; // Fallback to BT.709
        }
    } catch (...) {
        LOG_ERROR("Luminance calculation failed");
        return 0.0f;
    }
}

HDRProcessor::HDRContentAnalysis HDRProcessor::analyze_hdr_content(const std::vector<float>& rgb_data, 
                                                                   size_t width, size_t height, 
                                                                   ColorSpace color_space) const {
    HDRContentAnalysis analysis{};
    
    try {
        if (rgb_data.size() < width * height * 3) {
            LOG_ERROR("Insufficient RGB data for analysis");
            return analysis;
        }
        
        float min_luminance = std::numeric_limits<float>::max();
        float max_luminance = 0.0f;
        float total_luminance = 0.0f;
        
        size_t gamut_exceeding_pixels = 0;
        const size_t total_pixels = width * height;
        
        for (size_t i = 0; i < total_pixels; ++i) {
            const size_t base_idx = i * 3;
            float r = rgb_data[base_idx];
            float g = rgb_data[base_idx + 1];
            float b = rgb_data[base_idx + 2];
            
            // Calculate luminance
            float luminance = calculate_luminance(r, g, b, color_space);
            
            min_luminance = std::min(min_luminance, luminance);
            max_luminance = std::max(max_luminance, luminance);
            total_luminance += luminance;
            
            // Check gamut exceedance (simplified check for values > 1.0)
            if (r > 1.0f || g > 1.0f || b > 1.0f) {
                ++gamut_exceeding_pixels;
            }
        }
        
        analysis.min_luminance = min_luminance;
        analysis.max_luminance = max_luminance;
        analysis.average_luminance = total_luminance / static_cast<float>(total_pixels);
        analysis.peak_luminance = max_luminance;
        analysis.gamut_coverage = 1.0f - (static_cast<float>(gamut_exceeding_pixels) / static_cast<float>(total_pixels));
        analysis.is_hdr_content = max_luminance > 1.0f;
        analysis.dynamic_range = max_luminance / std::max(min_luminance, 0.001f);
        
        LOG_INFO("HDR analysis complete: Peak={:.2f}, Avg={:.2f}, DR={:.2f}", 
                max_luminance, analysis.average_luminance, analysis.dynamic_range);
        
    } catch (const std::exception& e) {
        LOG_ERROR("HDR content analysis failed: {}", e.what());
    }
    
    return analysis;
}

bool HDRProcessor::is_valid_hdr_metadata(const HDRMetadata& metadata) const noexcept {
    try {
        // Check static metadata
        if (metadata.max_luminance <= 0.0f || metadata.max_luminance > 10000.0f) {
            return false;
        }
        
        if (metadata.min_luminance < 0.0f || metadata.min_luminance >= metadata.max_luminance) {
            return false;
        }
        
        if (metadata.max_cll > metadata.max_luminance || metadata.max_fall > metadata.max_cll) {
            return false;
        }
        
        // Check mastering display primaries
        for (int i = 0; i < 3; ++i) {
            if (metadata.primaries[i].x < 0.0f || metadata.primaries[i].x > 1.0f ||
                metadata.primaries[i].y < 0.0f || metadata.primaries[i].y > 1.0f) {
                return false;
            }
        }
        
        // Check white point
        if (metadata.white_point.x < 0.0f || metadata.white_point.x > 1.0f ||
            metadata.white_point.y < 0.0f || metadata.white_point.y > 1.0f) {
            return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

// =============================================================================
// Constructor and Core Methods
// =============================================================================

HDRProcessor::HDRProcessor(std::shared_ptr<Device> device) 
    : device_(device) {
    try {
        if (!device_) {
            // EXCEPTION_POLICY_OK: Core performance module must remain exception-free per DR-0003
            LOG_ERROR("Device cannot be null in HDRProcessor constructor");
            device_ = nullptr; // Ensure invalid state is detectable
            return; // Constructor cannot return error code, caller must check isValid()
        }
        
        LOG_INFO("HDRProcessor initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("HDRProcessor initialization failed: {}", e.what());
        // EXCEPTION_POLICY_OK: Core performance module must remain exception-free per DR-0003
        device_ = nullptr; // Mark as invalid, caller must check isValid()
    }
}

HDRProcessor::~HDRProcessor() = default;

bool HDRProcessor::process_hdr_frame(const FrameData& input, FrameData& output, 
                                    const HDRProcessingParams& params) {
    try {
        if (!validate_processing_params(params)) {
            LOG_ERROR("Invalid HDR processing parameters");
            return false;
        }
        
        // Prepare output frame
        output = input; // Copy metadata and structure
        
        // Process each pixel
        const size_t pixel_count = input.width * input.height;
        if (input.rgb_data.size() < pixel_count * 3 || output.rgb_data.size() < pixel_count * 3) {
            LOG_ERROR("Insufficient frame data for HDR processing");
            return false;
        }
        
        for (size_t i = 0; i < pixel_count; ++i) {
            const size_t base_idx = i * 3;
            
            float r = input.rgb_data[base_idx];
            float g = input.rgb_data[base_idx + 1];
            float b = input.rgb_data[base_idx + 2];
            
            // Apply input transfer function
            if (params.input_transfer_function != HDRTransferFunction::LINEAR) {
                apply_transfer_function_inverse(r, g, b, params.input_transfer_function);
            }
            
            // Convert color space if needed
            if (params.input_color_space != params.output_color_space) {
                ColorSpaceMatrix matrix = get_color_space_matrix(params.input_color_space, 
                                                                params.output_color_space);
                apply_color_space_matrix(matrix, r, g, b);
            }
            
            // Apply tone mapping
            apply_tone_mapping(r, g, b, params.tone_mapping_operator, params.tone_mapping_params);
            
            // Apply output transfer function
            if (params.output_transfer_function != HDRTransferFunction::LINEAR) {
                apply_transfer_function(r, g, b, params.output_transfer_function);
            }
            
            // Store processed values
            output.rgb_data[base_idx] = r;
            output.rgb_data[base_idx + 1] = g;
            output.rgb_data[base_idx + 2] = b;
        }
        
        LOG_DEBUG("HDR frame processing completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("HDR frame processing failed: {}", e.what());
        return false;
    }
}

bool HDRProcessor::validate_processing_params(const HDRProcessingParams& params) const noexcept {
    try {
        // Check transfer functions
        if (params.input_transfer_function == HDRTransferFunction::INVALID ||
            params.output_transfer_function == HDRTransferFunction::INVALID) {
            return false;
        }
        
        // Check color spaces
        if (params.input_color_space == ColorSpace::INVALID ||
            params.output_color_space == ColorSpace::INVALID) {
            return false;
        }
        
        // Check tone mapping operator
        if (params.tone_mapping_operator == ToneMappingOperator::INVALID) {
            return false;
        }
        
        // Validate tone mapping parameters
        if (params.tone_mapping_params.white_point <= 0.0f ||
            params.tone_mapping_params.exposure_bias < 0.1f ||
            params.tone_mapping_params.exposure_bias > 10.0f) {
            return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

void HDRProcessor::apply_transfer_function(float& r, float& g, float& b, 
                                          HDRTransferFunction function) const noexcept {
    try {
        switch (function) {
            case HDRTransferFunction::LINEAR:
                // No transformation needed
                break;
                
            case HDRTransferFunction::GAMMA_2_2:
                r = std::pow(std::max(r, 0.0f), 1.0f / 2.2f);
                g = std::pow(std::max(g, 0.0f), 1.0f / 2.2f);
                b = std::pow(std::max(b, 0.0f), 1.0f / 2.2f);
                break;
                
            case HDRTransferFunction::PQ_2084:
                r = apply_pq_oetf(r, 10000.0f);
                g = apply_pq_oetf(g, 10000.0f);
                b = apply_pq_oetf(b, 10000.0f);
                break;
                
            case HDRTransferFunction::HLG_ARIB:
                r = apply_hlg_oetf(r);
                g = apply_hlg_oetf(g);
                b = apply_hlg_oetf(b);
                break;
                
            case HDRTransferFunction::LOG_C:
                r = apply_log_c_encoding(r);
                g = apply_log_c_encoding(g);
                b = apply_log_c_encoding(b);
                break;
                
            default:
                LOG_WARNING("Unsupported transfer function: {}", static_cast<int>(function));
                break;
        }
    } catch (...) {
        LOG_ERROR("Transfer function application failed");
    }
}

void HDRProcessor::apply_transfer_function_inverse(float& r, float& g, float& b, 
                                                  HDRTransferFunction function) const noexcept {
    try {
        switch (function) {
            case HDRTransferFunction::LINEAR:
                // No transformation needed
                break;
                
            case HDRTransferFunction::GAMMA_2_2:
                r = std::pow(std::max(r, 0.0f), 2.2f);
                g = std::pow(std::max(g, 0.0f), 2.2f);
                b = std::pow(std::max(b, 0.0f), 2.2f);
                break;
                
            case HDRTransferFunction::PQ_2084:
                r = apply_pq_eotf(r, 10000.0f);
                g = apply_pq_eotf(g, 10000.0f);
                b = apply_pq_eotf(b, 10000.0f);
                break;
                
            case HDRTransferFunction::HLG_ARIB:
                r = apply_hlg_eotf(r);
                g = apply_hlg_eotf(g);
                b = apply_hlg_eotf(b);
                break;
                
            case HDRTransferFunction::LOG_C:
                r = apply_log_c_decoding(r);
                g = apply_log_c_decoding(g);
                b = apply_log_c_decoding(b);
                break;
                
            default:
                LOG_WARNING("Unsupported inverse transfer function: {}", static_cast<int>(function));
                break;
        }
    } catch (...) {
        LOG_ERROR("Inverse transfer function application failed");
    }
}

void HDRProcessor::apply_tone_mapping(float& r, float& g, float& b, 
                                     ToneMappingOperator op, 
                                     const ToneMappingParams& params) const noexcept {
    try {
        switch (op) {
            case ToneMappingOperator::NONE:
                // No tone mapping
                break;
                
            case ToneMappingOperator::REINHARD:
                apply_reinhard_tone_mapping(r, g, b, params.white_point);
                break;
                
            case ToneMappingOperator::HABLE:
                apply_hable_tone_mapping(r, g, b, params.exposure_bias);
                break;
                
            case ToneMappingOperator::ACES:
                apply_aces_tone_mapping(r, g, b);
                break;
                
            case ToneMappingOperator::FILMIC:
                apply_filmic_tone_mapping(r, g, b, params.shoulder_strength, 
                                        params.linear_start, params.linear_length, 
                                        params.black_tightness);
                break;
                
            default:
                LOG_WARNING("Unsupported tone mapping operator: {}", static_cast<int>(op));
                break;
        }
    } catch (...) {
        LOG_ERROR("Tone mapping application failed");
    }
}

} // namespace VideoEditor::GFX
