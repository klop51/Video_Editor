#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace ve {
namespace media_io {

/**
 * @brief RAW video format enumeration for professional cinematography
 * Supports major camera manufacturer RAW formats
 */
enum class RAWFormat {
    UNKNOWN = 0,
    REDCODE,        // RED cameras (.r3d)
    ARRIRAW,        // ARRI cameras (.ari) 
    BLACKMAGIC_RAW, // BMD cameras (.braw)
    CINEMA_DNG,     // Adobe standard (.dng sequence)
    PRORES_RAW,     // Apple RAW (.mov)
    CANON_RAW,      // Canon Cinema RAW (.rmf)
    SONY_RAW,       // Sony RAW (.srw, .mxf)
    PANASONIC_RAW   // Panasonic RAW (.raw)
};

/**
 * @brief Bayer pattern types for sensor debayering
 * Common sensor arrangements in digital cameras
 */
enum class BayerPattern {
    UNKNOWN = 0,
    RGGB,           // Red-Green-Green-Blue
    BGGR,           // Blue-Green-Green-Red  
    GRBG,           // Green-Red-Blue-Green
    GBRG,           // Green-Blue-Red-Green
    XTRANS,         // Fujifilm X-Trans pattern
    MONOCHROME      // Single channel sensor
};

/**
 * @brief Debayer algorithm quality levels
 * Trade-off between speed and quality
 */
enum class DebayerQuality {
    FAST = 0,       // Nearest neighbor - fastest
    BILINEAR,       // Bilinear interpolation - balanced  
    ADAPTIVE,       // Edge-aware interpolation - high quality
    PROFESSIONAL   // Advanced algorithms - best quality
};

/**
 * @brief Camera metadata structure
 * Essential information from camera sensors
 */
struct CameraMetadata {
    std::string camera_make;
    std::string camera_model; 
    std::string lens_model;
    uint32_t iso_speed = 0;
    float shutter_speed = 0.0f;
    float aperture = 0.0f;
    float focal_length = 0.0f;
    uint32_t color_temperature = 0;
    float tint = 0.0f;
    float exposure_compensation = 0.0f;
    std::string timestamp;
    std::string firmware_version;
};

/**
 * @brief Color matrix for camera color space conversion
 * 3x3 matrix for RGB color space transformation
 */
struct ColorMatrix {
    float matrix[3][3];
    std::string color_space_name;
    float white_balance[3] = {1.0f, 1.0f, 1.0f};
    
    ColorMatrix() {
        // Initialize to identity matrix
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                matrix[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        color_space_name = "sRGB";
    }
};

/**
 * @brief RAW frame information
 * Complete description of a RAW video frame
 */
struct RAWFrameInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bit_depth = 12;        // Typical RAW bit depth
    BayerPattern bayer_pattern = BayerPattern::UNKNOWN;
    RAWFormat format = RAWFormat::UNKNOWN;
    CameraMetadata metadata;
    ColorMatrix color_matrix;
    bool has_lens_correction = false;
    std::vector<uint8_t> lens_correction_data;
    size_t frame_size_bytes = 0;
    uint64_t timestamp_us = 0;
};

/**
 * @brief Debayer processing parameters
 * Configuration for RAW sensor data processing
 */
struct DebayerParams {
    DebayerQuality quality = DebayerQuality::BILINEAR;
    bool apply_color_matrix = true;
    bool apply_white_balance = true;
    bool apply_gamma_correction = true;
    float gamma_value = 2.2f;
    bool apply_lens_correction = false;
    float exposure_adjustment = 0.0f;
    float highlight_recovery = 0.0f;
    float shadow_lift = 0.0f;
};

/**
 * @brief RAW format detection and processing class
 * Foundation for professional RAW video workflow support
 */
class RAWFormatSupport {
public:
    RAWFormatSupport();
    ~RAWFormatSupport();

    // Format detection and identification
    RAWFormat detectRAWFormat(const std::string& file_path) const;
    RAWFormat detectRAWFormat(const uint8_t* header_data, size_t header_size) const;
    bool isRAWFormat(const std::string& file_path) const;
    std::string getFormatName(RAWFormat format) const;
    std::vector<std::string> getSupportedExtensions(RAWFormat format) const;

    // RAW frame analysis
    bool analyzeRAWFrame(const std::string& file_path, RAWFrameInfo& frame_info) const;
    bool extractCameraMetadata(const std::string& file_path, CameraMetadata& metadata) const;
    bool extractColorMatrix(const std::string& file_path, ColorMatrix& color_matrix) const;
    BayerPattern detectBayerPattern(const uint8_t* raw_data, uint32_t width, uint32_t height) const;

    // Basic debayer processing  
    bool debayerFrame(const uint8_t* raw_data, uint8_t* rgb_output, 
                     const RAWFrameInfo& frame_info, const DebayerParams& params) const;
    
    // Preview generation
    bool generatePreview(const std::string& file_path, uint8_t* preview_buffer,
                        uint32_t preview_width, uint32_t preview_height) const;

    // Format-specific support queries
    bool supportsFormat(RAWFormat format) const;
    std::vector<RAWFormat> getSupportedFormats() const;
    bool requiresExternalLibrary(RAWFormat format) const;
    std::string getFormatDescription(RAWFormat format) const;

    // Performance and capability queries
    bool canProcessRealtime(RAWFormat format, uint32_t width, uint32_t height) const;
    size_t getRecommendedBufferSize(const RAWFrameInfo& frame_info) const;
    uint32_t getMaxSupportedResolution(RAWFormat format) const;

private:
    // Format detection helpers
    RAWFormat detectREDCodeFormat(const uint8_t* header) const;
    RAWFormat detectARRIFormat(const uint8_t* header) const; 
    RAWFormat detectBlackmagicFormat(const uint8_t* header) const;
    RAWFormat detectCinemaDNGFormat(const std::string& file_path) const;
    RAWFormat detectProResRAWFormat(const uint8_t* header) const;

    // Format-specific frame analyzers
    bool analyzeREDFrame(const uint8_t* header, RAWFrameInfo& frame_info) const;
    bool analyzeARRIFrame(const uint8_t* header, RAWFrameInfo& frame_info) const;
    bool analyzeBRAWFrame(const uint8_t* header, RAWFrameInfo& frame_info) const;
    bool analyzeCinemaDNGFrame(const std::string& file_path, RAWFrameInfo& frame_info) const;
    bool analyzeProResRAWFrame(const uint8_t* header, RAWFrameInfo& frame_info) const;

    // Debayer algorithm implementations
    void debayerNearest(const uint8_t* raw_data, uint8_t* rgb_output,
                       const RAWFrameInfo& frame_info) const;
    void debayerBilinear(const uint8_t* raw_data, uint8_t* rgb_output,
                        const RAWFrameInfo& frame_info) const;
    void debayerAdaptive(const uint8_t* raw_data, uint8_t* rgb_output, 
                        const RAWFrameInfo& frame_info) const;

    // Color processing helpers
    void applyColorMatrix(uint8_t* rgb_data, uint32_t pixel_count, 
                         const ColorMatrix& matrix) const;
    void applyWhiteBalance(uint8_t* rgb_data, uint32_t pixel_count,
                          const float white_balance[3]) const;
    void applyGammaCorrection(uint8_t* rgb_data, uint32_t pixel_count, float gamma) const;

    // Format support database
    std::unordered_map<RAWFormat, std::vector<std::string>> format_extensions_;
    std::unordered_map<RAWFormat, std::string> format_descriptions_;
    std::unordered_map<RAWFormat, bool> realtime_capable_;

    // RAW format magic numbers for detection
    static const std::vector<uint8_t> REDCODE_MAGIC;
    static const std::vector<uint8_t> ARRIRAW_MAGIC;
    static const std::vector<uint8_t> BRAW_MAGIC;
    static const std::vector<uint8_t> PRORES_RAW_MAGIC;

    void initializeFormatDatabase();
    void initializeMagicNumbers();
};

/**
 * @brief RAW format utility functions
 * Helper functions for RAW format workflows
 */
namespace raw_utils {
    
    // Format conversion utilities
    std::string rawFormatToString(RAWFormat format);
    RAWFormat stringToRAWFormat(const std::string& format_str);
    
    // Bayer pattern utilities  
    std::string bayerPatternToString(BayerPattern pattern);
    BayerPattern stringToBayerPattern(const std::string& pattern_str);
    
    // File extension utilities
    RAWFormat getRAWFormatFromExtension(const std::string& file_path);
    bool isRAWExtension(const std::string& extension);
    
    // Metadata utilities
    bool validateCameraMetadata(const CameraMetadata& metadata);
    void printCameraMetadata(const CameraMetadata& metadata);
    
    // Performance utilities
    size_t calculateRAWFrameSize(uint32_t width, uint32_t height, uint32_t bit_depth);
    uint32_t estimateDebayerProcessingTime(uint32_t width, uint32_t height, DebayerQuality quality);
    
    // Color space utilities
    ColorMatrix getStandardColorMatrix(const std::string& color_space);
    bool isValidColorMatrix(const ColorMatrix& matrix);
    void normalizeColorMatrix(ColorMatrix& matrix);

} // namespace raw_utils

} // namespace media_io
} // namespace ve
