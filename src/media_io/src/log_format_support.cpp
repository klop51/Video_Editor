#include "media_io/log_format_support.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cctype>

/**
 * Log Format Support Implementation for Phase 2 Week 6
 * FORMAT_SUPPORT_ROADMAP.md - Essential for color grading workflows
 * 
 * Implementation of comprehensive camera log format support including:
 * - Sony S-Log3, Canon C-Log3, ARRI Log-C4, RED Log
 * - Blackmagic Log, Panasonic V-Log, and others
 * - Accurate log-to-linear conversion with proper gamma handling
 * - Real-time LUT processing for professional workflows
 */

namespace ve::media_io {

// Static member initialization
std::unordered_map<LogFormat, LogFormatInfo> LogFormatSupport::format_database_;
bool LogFormatSupport::hardware_luts_enabled_ = false;
bool LogFormatSupport::is_initialized_ = false;

bool LogFormatSupport::initialize(bool enable_hardware_luts) {
    if (is_initialized_) {
        return true;
    }
    
    hardware_luts_enabled_ = enable_hardware_luts;
    initialize_format_database();
    is_initialized_ = true;
    
    return true;
}

void LogFormatSupport::initialize_format_database() {
    format_database_.clear();
    
    // Sony S-Log3 characteristics
    LogFormatInfo slog3;
    slog3.format = LogFormat::SLOG3;
    slog3.name = "Sony S-Log3";
    slog3.manufacturer = "Sony";
    slog3.black_level = 0.0929f;
    slog3.white_level = 0.705f;
    slog3.gamma = 1.0f / 2.6f;
    slog3.native_iso = 800.0f;
    slog3.exposure_range_stops = 14.0f;
    slog3.middle_grey_value = 0.18f;
    slog3.middle_grey_log = 0.41f;
    slog3.color_primaries = "S-Gamut3.Cine";
    slog3.wide_gamut = true;
    format_database_[LogFormat::SLOG3] = slog3;
    
    // Canon C-Log3 characteristics  
    LogFormatInfo clog3;
    clog3.format = LogFormat::CLOG3;
    clog3.name = "Canon C-Log3";
    clog3.manufacturer = "Canon";
    clog3.black_level = 0.07329f;
    clog3.white_level = 0.75f;
    clog3.gamma = 1.0f / 2.6f;
    clog3.native_iso = 800.0f;
    clog3.exposure_range_stops = 14.0f;
    clog3.middle_grey_value = 0.18f;
    clog3.middle_grey_log = 0.34679f;
    clog3.color_primaries = "Cinema Gamut";
    clog3.wide_gamut = true;
    format_database_[LogFormat::CLOG3] = clog3;
    
    // ARRI Log-C4 characteristics
    LogFormatInfo logc4;
    logc4.format = LogFormat::LOGC4;
    logc4.name = "ARRI Log-C4";
    logc4.manufacturer = "ARRI";
    logc4.black_level = 0.0f;
    logc4.white_level = 1.0f;
    logc4.gamma = 1.0f / 2.6f;
    logc4.native_iso = 800.0f;
    logc4.exposure_range_stops = 15.0f;
    logc4.middle_grey_value = 0.18f;
    logc4.middle_grey_log = 0.39f;
    logc4.color_primaries = "ALEXA Wide Gamut 4";
    logc4.wide_gamut = true;
    format_database_[LogFormat::LOGC4] = logc4;
    
    // RED Log characteristics
    LogFormatInfo redlog;
    redlog.format = LogFormat::REDLOG;
    redlog.name = "RED Log";
    redlog.manufacturer = "RED Digital Cinema";
    redlog.black_level = 0.0f;
    redlog.white_level = 1.0f;
    redlog.gamma = 1.0f / 2.4f;
    redlog.native_iso = 800.0f;
    redlog.exposure_range_stops = 16.0f;
    redlog.middle_grey_value = 0.18f;
    redlog.middle_grey_log = 0.37f;
    redlog.color_primaries = "REDWideGamutRGB";
    redlog.wide_gamut = true;
    format_database_[LogFormat::REDLOG] = redlog;
    
    // Blackmagic Log characteristics
    LogFormatInfo bmlog;
    bmlog.format = LogFormat::BMLOG;
    bmlog.name = "Blackmagic Film";
    bmlog.manufacturer = "Blackmagic Design";
    bmlog.black_level = 0.0f;
    bmlog.white_level = 1.0f;
    bmlog.gamma = 1.0f / 2.4f;
    bmlog.native_iso = 400.0f;
    bmlog.exposure_range_stops = 12.0f;
    bmlog.middle_grey_value = 0.18f;
    bmlog.middle_grey_log = 0.38f;
    bmlog.color_primaries = "Blackmagic Wide Gamut";
    bmlog.wide_gamut = true;
    format_database_[LogFormat::BMLOG] = bmlog;
    
    // Panasonic V-Log characteristics
    LogFormatInfo vlog;
    vlog.format = LogFormat::VLOG;
    vlog.name = "Panasonic V-Log";
    vlog.manufacturer = "Panasonic";
    vlog.black_level = 0.125f;
    vlog.white_level = 0.75f;
    vlog.gamma = 1.0f / 2.6f;
    vlog.native_iso = 800.0f;
    vlog.exposure_range_stops = 14.0f;
    vlog.middle_grey_value = 0.18f;
    vlog.middle_grey_log = 0.42f;
    vlog.color_primaries = "V-Gamut";
    vlog.wide_gamut = true;
    format_database_[LogFormat::VLOG] = vlog;
}

LogFormat LogFormatSupport::detect_log_format(const uint8_t* image_data,
                                               uint32_t width, uint32_t height,
                                               const std::string& metadata) {
    if (!is_initialized_) {
        initialize();
    }
    
    // First try metadata-based detection
    LogFormat format = detect_from_metadata(metadata);
    if (format != LogFormat::NONE) {
        return format;
    }
    
    // Fallback to histogram-based detection
    return detect_from_histogram(image_data, width * height * 3);
}

LogFormat LogFormatSupport::detect_from_metadata(const std::string& metadata) {
    std::string lower_metadata = metadata;
    std::transform(lower_metadata.begin(), lower_metadata.end(), 
                   lower_metadata.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (lower_metadata.find("s-log3") != std::string::npos || 
        lower_metadata.find("slog3") != std::string::npos) {
        return LogFormat::SLOG3;
    }
    if (lower_metadata.find("c-log3") != std::string::npos || 
        lower_metadata.find("clog3") != std::string::npos) {
        return LogFormat::CLOG3;
    }
    if (lower_metadata.find("log-c4") != std::string::npos || 
        lower_metadata.find("logc4") != std::string::npos) {
        return LogFormat::LOGC4;
    }
    if (lower_metadata.find("redlog") != std::string::npos || 
        lower_metadata.find("red log") != std::string::npos) {
        return LogFormat::REDLOG;
    }
    if (lower_metadata.find("blackmagic") != std::string::npos || 
        lower_metadata.find("bmlog") != std::string::npos) {
        return LogFormat::BMLOG;
    }
    if (lower_metadata.find("v-log") != std::string::npos || 
        lower_metadata.find("vlog") != std::string::npos) {
        return LogFormat::VLOG;
    }
    
    return LogFormat::NONE;
}

LogFormat LogFormatSupport::detect_from_histogram(const uint8_t* data, uint32_t size) {
    // Analyze pixel distribution to detect log characteristics
    constexpr uint32_t histogram_bins = 256;
    std::vector<uint32_t> histogram(histogram_bins, 0);
    
    // Build histogram
    for (uint32_t i = 0; i < size; ++i) {
        histogram[data[i]]++;
    }
    
    // Calculate distribution characteristics
    float low_count = 0, mid_count = 0, high_count = 0;
    for (uint32_t i = 0; i < histogram_bins; ++i) {
        if (i < 85) low_count += static_cast<float>(histogram[i]);
        else if (i < 170) mid_count += static_cast<float>(histogram[i]);
        else high_count += static_cast<float>(histogram[i]);
    }
    
    float total = static_cast<float>(size);
    float low_ratio = low_count / total;
    float mid_ratio = mid_count / total;
    float high_ratio = high_count / total;
    
    // Log formats typically have concentrated midtones
    if (mid_ratio > 0.6f && low_ratio < 0.2f && high_ratio < 0.2f) {
        // This could be a log format, but we can't determine which one
        // without more sophisticated analysis
        return LogFormat::SLOG3; // Default assumption
    }
    
    return LogFormat::NONE;
}

LogFormatInfo LogFormatSupport::get_log_format_info(LogFormat format) {
    if (!is_initialized_) {
        initialize();
    }
    
    auto it = format_database_.find(format);
    if (it != format_database_.end()) {
        return it->second;
    }
    
    return LogFormatInfo{}; // Return default/empty info
}

bool LogFormatSupport::log_to_linear(const float* input_data, float* output_data,
                                      uint32_t width, uint32_t height,
                                      LogFormat format,
                                      const LogProcessingConfig& config) {
    if (!input_data || !output_data || format == LogFormat::NONE) {
        return false;
    }
    
    uint32_t pixel_count = width * height;
    float exposure_multiplier = calculate_exposure_multiplier(config.exposure_offset, format);
    
    for (uint32_t i = 0; i < pixel_count; ++i) {
        uint32_t idx = i * 3; // RGB pixels
        
        // Convert each channel
        for (int channel = 0; channel < 3; ++channel) {
            float log_value = input_data[idx + static_cast<uint32_t>(channel)];
            
            // Apply exposure adjustment in log space
            log_value *= exposure_multiplier;
            
            // Convert to linear based on format
            float linear_value = 0.0f;
            switch (format) {
                case LogFormat::SLOG3:
                    linear_value = slog3_to_linear(log_value);
                    break;
                case LogFormat::CLOG3:
                    linear_value = clog3_to_linear(log_value);
                    break;
                case LogFormat::LOGC4:
                    linear_value = logc4_to_linear(log_value);
                    break;
                case LogFormat::REDLOG:
                    linear_value = redlog_to_linear(log_value);
                    break;
                case LogFormat::BMLOG:
                    linear_value = bmlog_to_linear(log_value);
                    break;
                case LogFormat::VLOG:
                    linear_value = vlog_to_linear(log_value);
                    break;
                default:
                    linear_value = log_value; // Pass through
                    break;
            }
            
            // Apply additional corrections
            linear_value *= config.gain;
            linear_value += config.lift;
            linear_value = std::pow(linear_value, config.gamma_adjustment);
            
            output_data[idx + static_cast<uint32_t>(channel)] = std::max(0.0f, linear_value);
        }
    }
    
    return true;
}

// Sony S-Log3 conversion functions
float LogFormatSupport::slog3_to_linear(float log_value) {
    const float a = 420.0f / 1023.0f;
    const float b = 261.5f / 1023.0f;
    const float c = 0.432699f;
    const float d = 0.037584f;
    
    if (log_value >= c) {
        return (std::pow(10.0f, (log_value - d) / 0.9f) - b) / a;
    } else {
        return (log_value - 0.030001f) / 3.53881f;
    }
}

float LogFormatSupport::linear_to_slog3(float linear_value) {
    const float a = 420.0f / 1023.0f;
    const float b = 261.5f / 1023.0f;
    [[maybe_unused]] const float c = 0.432699f;
    const float d = 0.037584f;
    
    if (linear_value >= 0.0f) {
        return 0.9f * log10f(a * linear_value + b) + d;
    } else {
        return 3.53881f * linear_value + 0.030001f;
    }
}

// Canon C-Log3 conversion functions
float LogFormatSupport::clog3_to_linear(float log_value) {
    const float a = 0.036036f;
    const float b = 0.12783f;
    const float c = 0.30103f;
    
    if (log_value > 0.097465f) {
        return (std::pow(10.0f, (log_value - c) / 0.36f) - 1.0f) / a;
    } else {
        return (log_value - b) / 2.755341f;
    }
}

float LogFormatSupport::linear_to_clog3(float linear_value) {
    const float a = 0.036036f;
    const float b = 0.12783f;
    const float c = 0.30103f;
    
    if (linear_value >= 0.014f) {
        return 0.36f * log10f(a * linear_value + 1.0f) + c;
    } else {
        return 2.755341f * linear_value + b;
    }
}

// ARRI Log-C4 conversion functions
float LogFormatSupport::logc4_to_linear(float log_value) {
    const float a = 0.18f;
    const float b = 0.005561f;
    const float c = 0.247190f;
    const float d = 0.386036f;
    
    return (std::pow(10.0f, (log_value - d) / c) - b) / a;
}

float LogFormatSupport::linear_to_logc4(float linear_value) {
    const float a = 0.18f;
    const float b = 0.005561f;
    const float c = 0.247190f;
    const float d = 0.386036f;
    
    return c * log10f(a * linear_value + b) + d;
}

// RED Log conversion functions
float LogFormatSupport::redlog_to_linear(float log_value) {
    return (std::pow(10.0f, (log_value * 1023.0f - 512.0f) / 154.0f) - 1.0f) / 511.0f;
}

float LogFormatSupport::linear_to_redlog(float linear_value) {
    return (154.0f * log10f(511.0f * linear_value + 1.0f) + 512.0f) / 1023.0f;
}

// Blackmagic Log conversion functions
float LogFormatSupport::bmlog_to_linear(float log_value) {
    if (log_value < 0.005f) {
        return log_value / 6.025f;
    } else {
        return (std::pow(10.0f, (log_value - 0.00692f) / 0.0929f) - 1.0f) / 18.8515625f;
    }
}

float LogFormatSupport::linear_to_bmlog(float linear_value) {
    if (linear_value < 0.005f / 6.025f) {
        return linear_value * 6.025f;
    } else {
        return 0.0929f * log10f(18.8515625f * linear_value + 1.0f) + 0.00692f;
    }
}

// Panasonic V-Log conversion functions
float LogFormatSupport::vlog_to_linear(float log_value) {
    const float cut1 = 0.181f;
    [[maybe_unused]] const float cut2 = 0.0f;
    const float a = 5.6f;
    const float b = 0.125f;
    const float c = 0.241514f;
    const float d = 0.598206f;
    
    if (log_value < cut1) {
        return (log_value - b) / a;
    } else {
        return std::pow(10.0f, (log_value - d) / c) - 0.00873f;
    }
}

float LogFormatSupport::linear_to_vlog(float linear_value) {
    const float cut1 = 0.01f;
    [[maybe_unused]] const float cut2 = 0.0f;
    const float a = 5.6f;
    const float b = 0.125f;
    const float c = 0.241514f;
    const float d = 0.598206f;
    
    if (linear_value < cut1) {
        return a * linear_value + b;
    } else {
        return c * log10f(linear_value + 0.00873f) + d;
    }
}

ToneLUT1D LogFormatSupport::create_tone_lut(LogFormat source_format,
                                             bool target_linear,
                                             float exposure_offset) {
    ToneLUT1D lut;
    lut.source_format = source_format;
    lut.is_linear_output = target_linear;
    lut.size = 1024;
    
    lut.input_values.resize(lut.size);
    lut.output_values.resize(lut.size);
    
    float exposure_multiplier = calculate_exposure_multiplier(exposure_offset, source_format);
    
    for (uint32_t i = 0; i < lut.size; ++i) {
        float input = static_cast<float>(i) / static_cast<float>(lut.size - 1);
        lut.input_values[i] = input;
        
        // Apply exposure adjustment
        float adjusted_input = input * exposure_multiplier;
        
        // Convert to linear
        float linear_output = 0.0f;
        switch (source_format) {
            case LogFormat::SLOG3:
                linear_output = slog3_to_linear(adjusted_input);
                break;
            case LogFormat::CLOG3:
                linear_output = clog3_to_linear(adjusted_input);
                break;
            case LogFormat::LOGC4:
                linear_output = logc4_to_linear(adjusted_input);
                break;
            case LogFormat::REDLOG:
                linear_output = redlog_to_linear(adjusted_input);
                break;
            case LogFormat::BMLOG:
                linear_output = bmlog_to_linear(adjusted_input);
                break;
            case LogFormat::VLOG:
                linear_output = vlog_to_linear(adjusted_input);
                break;
            default:
                linear_output = adjusted_input;
                break;
        }
        
        // Convert to target space if not linear
        if (!target_linear) {
            // Apply Rec.709 gamma curve
            if (linear_output < 0.018f) {
                linear_output = 4.5f * linear_output;
            } else {
                linear_output = 1.09929682680944f * std::pow(linear_output, 0.45f) - 0.09929682680944f;
            }
        }
        
        lut.output_values[i] = std::max(0.0f, std::min(1.0f, linear_output));
    }
    
    return lut;
}

std::string LogFormatSupport::get_log_format_name(LogFormat format) {
    LogFormatInfo info = get_log_format_info(format);
    return info.name.empty() ? "Unknown" : info.name;
}

bool LogFormatSupport::requires_3d_lut(LogFormat format) {
    // Most log formats can be accurately converted with 1D tone curves
    // 3D LUTs are recommended for the most accurate color space conversion
    switch (format) {
        case LogFormat::LOGC4:
        case LogFormat::REDLOG:
            return true; // These benefit significantly from 3D LUTs
        default:
            return false;
    }
}

float LogFormatSupport::calculate_exposure_multiplier(float stops, [[maybe_unused]] LogFormat format) {
    // In log space, exposure adjustment is typically linear
    // Each stop represents a doubling/halving of exposure
    return std::pow(2.0f, stops);
}

bool LogFormatSupport::validate_processing_config(const LogProcessingConfig& config) {
    // Basic validation
    if (config.exposure_offset < -10.0f || config.exposure_offset > 10.0f) {
        return false; // Extreme exposure adjustments
    }
    if (config.gamma_adjustment < 0.1f || config.gamma_adjustment > 10.0f) {
        return false; // Invalid gamma range
    }
    if (config.gain < 0.0f || config.gain > 100.0f) {
        return false; // Invalid gain range
    }
    
    return true;
}

std::vector<LogFormat> LogFormatSupport::get_supported_formats() {
    return {
        LogFormat::SLOG3,
        LogFormat::CLOG3,
        LogFormat::LOGC4,
        LogFormat::REDLOG,
        LogFormat::BMLOG,
        LogFormat::VLOG
    };
}

} // namespace ve::media_io
