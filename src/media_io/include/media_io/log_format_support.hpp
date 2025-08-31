#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <cstdint>

/**
 * Log Format Support for Phase 2 Week 6
 * FORMAT_SUPPORT_ROADMAP.md - Essential for color grading workflows
 * 
 * This module provides comprehensive support for camera log formats
 * from major manufacturers, enabling professional color grading workflows.
 */

namespace ve::media_io {

/**
 * Camera log formats from major manufacturers
 */
enum class LogFormat {
    NONE = 0,           // Linear/Rec.709
    SLOG3,              // Sony S-Log3
    CLOG3,              // Canon C-Log3  
    LOGC4,              // ARRI Log-C4
    REDLOG,             // RED Log
    BMLOG,              // Blackmagic Log
    VLOG,               // Panasonic V-Log
    FLOG,               // Fujifilm F-Log
    NLOG,               // Nikon N-Log
    PLOG,               // Phantom TMX Log
    DLOG                // DJI D-Log
};

/**
 * Log format characteristics and conversion parameters
 */
struct LogFormatInfo {
    LogFormat format = LogFormat::NONE;
    std::string name;
    std::string manufacturer;
    
    // Log curve parameters
    float black_level = 0.0f;          // Black point in log space
    float white_level = 1.0f;          // White point in log space
    float gamma = 1.0f;                // Log gamma value
    float linear_slope = 1.0f;         // Linear section slope
    float linear_offset = 0.0f;        // Linear section offset
    float log_offset = 0.0f;           // Log section offset
    
    // Exposure characteristics
    float native_iso = 800.0f;         // Native ISO for the log format
    float exposure_range_stops = 14.0f; // Total dynamic range in stops
    float middle_grey_value = 0.18f;   // 18% grey value in linear
    float middle_grey_log = 0.41f;     // 18% grey value in log space
    
    // Color space information
    std::string color_primaries = "BT.2020";
    std::string transfer_function = "Log";
    bool wide_gamut = true;
    
    // Conversion LUT parameters
    bool requires_lut = false;
    std::string lut_path;
    uint32_t lut_size = 1024;
};

/**
 * 3D LUT for accurate color transformations
 */
struct ColorLUT3D {
    uint32_t size = 32;                           // LUT cube size (32x32x32)
    std::vector<std::array<float, 3>> data;      // RGB triplets
    std::string source_space;
    std::string target_space;
    bool is_valid = false;
};

/**
 * 1D LUT for tone curve transformations
 */
struct ToneLUT1D {
    uint32_t size = 1024;                        // Number of LUT entries
    std::vector<float> input_values;             // Input range [0.0, 1.0]
    std::vector<float> output_values;            // Output values
    LogFormat source_format = LogFormat::NONE;
    bool is_linear_output = true;                // True if output is linear
};

/**
 * Log processing configuration
 */
struct LogProcessingConfig {
    LogFormat input_format = LogFormat::NONE;
    LogFormat output_format = LogFormat::NONE;
    
    // Exposure adjustments in log space
    float exposure_offset = 0.0f;               // Exposure adjustment in stops
    float gamma_adjustment = 1.0f;              // Gamma correction
    float lift = 0.0f;                          // Lift (shadows)
    float gain = 1.0f;                          // Gain (highlights)
    
    // Color correction
    float temperature_shift = 0.0f;             // White balance temperature
    float tint_shift = 0.0f;                    // White balance tint
    float saturation = 1.0f;                    // Color saturation
    
    // Processing options
    bool preserve_highlights = true;
    bool use_hardware_acceleration = true;
    bool enable_real_time_preview = true;
};

/**
 * Log Format Support class
 * Provides comprehensive camera log format support for professional workflows
 */
class LogFormatSupport {
public:
    /**
     * Initialize log format support system
     * @param enable_hardware_luts Use GPU-accelerated LUT processing
     * @return True if initialization successful
     */
    static bool initialize(bool enable_hardware_luts = true);
    
    /**
     * Detect log format from image data
     * @param image_data Raw image data
     * @param width Image width
     * @param height Image height
     * @param metadata Optional metadata from camera
     * @return Detected log format
     */
    static LogFormat detect_log_format(const uint8_t* image_data,
                                       uint32_t width, uint32_t height,
                                       const std::string& metadata = "");
    
    /**
     * Get log format information
     * @param format Log format to query
     * @return Format characteristics and parameters
     */
    static LogFormatInfo get_log_format_info(LogFormat format);
    
    /**
     * Convert log to linear RGB
     * @param input_data Log-encoded image data
     * @param output_data Linear RGB output buffer
     * @param width Image width
     * @param height Image height
     * @param format Source log format
     * @param config Processing configuration
     * @return True if conversion successful
     */
    static bool log_to_linear(const float* input_data, float* output_data,
                              uint32_t width, uint32_t height,
                              LogFormat format,
                              const LogProcessingConfig& config = {});
    
    /**
     * Convert linear RGB to log
     * @param input_data Linear RGB image data
     * @param output_data Log-encoded output buffer
     * @param width Image width
     * @param height Image height
     * @param format Target log format
     * @param config Processing configuration
     * @return True if conversion successful
     */
    static bool linear_to_log(const float* input_data, float* output_data,
                              uint32_t width, uint32_t height,
                              LogFormat format,
                              const LogProcessingConfig& config = {});
    
    /**
     * Create 1D tone LUT for log conversion
     * @param source_format Source log format
     * @param target_linear True for linear output, false for Rec.709
     * @param exposure_offset Exposure adjustment in stops
     * @return Generated tone LUT
     */
    static ToneLUT1D create_tone_lut(LogFormat source_format,
                                     bool target_linear = true,
                                     float exposure_offset = 0.0f);
    
    /**
     * Apply 1D LUT transformation
     * @param input_data Source image data
     * @param output_data Transformed output buffer
     * @param width Image width
     * @param height Image height
     * @param lut 1D LUT to apply
     * @return True if transformation successful
     */
    static bool apply_1d_lut(const float* input_data, float* output_data,
                             uint32_t width, uint32_t height,
                             const ToneLUT1D& lut);
    
    /**
     * Load 3D LUT from file
     * @param lut_path Path to LUT file (.cube, .3dl, .lut)
     * @return Loaded 3D LUT
     */
    static ColorLUT3D load_3d_lut(const std::string& lut_path);
    
    /**
     * Apply 3D LUT transformation
     * @param input_data Source RGB image data
     * @param output_data Transformed RGB output buffer
     * @param width Image width
     * @param height Image height
     * @param lut 3D LUT to apply
     * @return True if transformation successful
     */
    static bool apply_3d_lut(const float* input_data, float* output_data,
                             uint32_t width, uint32_t height,
                             const ColorLUT3D& lut);
    
    /**
     * Get log format name string
     * @param format Log format enum
     * @return Human-readable format name
     */
    static std::string get_log_format_name(LogFormat format);
    
    /**
     * Check if format requires 3D LUT for accurate conversion
     * @param format Log format to check
     * @return True if 3D LUT recommended
     */
    static bool requires_3d_lut(LogFormat format);
    
    /**
     * Calculate exposure adjustment for log space
     * @param stops Exposure adjustment in stops
     * @param format Log format being processed
     * @return Multiplier for log values
     */
    static float calculate_exposure_multiplier(float stops, LogFormat format);
    
    /**
     * Validate log processing configuration
     * @param config Configuration to validate
     * @return True if configuration is valid
     */
    static bool validate_processing_config(const LogProcessingConfig& config);
    
    /**
     * Get supported log formats
     * @return Vector of all supported log formats
     */
    static std::vector<LogFormat> get_supported_formats();

private:
    // Internal implementation details
    static std::unordered_map<LogFormat, LogFormatInfo> format_database_;
    static bool hardware_luts_enabled_;
    static bool is_initialized_;
    
    // Log curve mathematics
    static float slog3_to_linear(float log_value);
    static float linear_to_slog3(float linear_value);
    static float clog3_to_linear(float log_value);
    static float linear_to_clog3(float linear_value);
    static float logc4_to_linear(float log_value);
    static float linear_to_logc4(float linear_value);
    static float redlog_to_linear(float log_value);
    static float linear_to_redlog(float linear_value);
    static float bmlog_to_linear(float log_value);
    static float linear_to_bmlog(float linear_value);
    static float vlog_to_linear(float log_value);
    static float linear_to_vlog(float linear_value);
    
    // Helper functions
    static void initialize_format_database();
    static LogFormat detect_from_metadata(const std::string& metadata);
    static LogFormat detect_from_histogram(const uint8_t* data, uint32_t size);
};

} // namespace ve::media_io
