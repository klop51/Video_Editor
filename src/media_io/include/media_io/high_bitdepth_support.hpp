#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <array>

namespace ve {
namespace media_io {

/**
 * @brief High bit depth pixel format support for professional workflows
 * 
 * Supports 10-bit, 12-bit, and 16-bit processing pipelines with quality preservation.
 * Essential for professional color grading and high-end production workflows.
 */

// Extended pixel formats for high bit depth processing
enum class HighBitDepthFormat {
    // 10-bit formats
    YUV420P10LE,     // 4:2:0 10-bit planar (most common)
    YUV422P10LE,     // 4:2:2 10-bit planar (broadcast)
    YUV444P10LE,     // 4:4:4 10-bit planar (highest quality)
    
    // 12-bit formats
    YUV420P12LE,     // 4:2:0 12-bit planar (cinema cameras)
    YUV422P12LE,     // 4:2:2 12-bit planar (professional)
    YUV444P12LE,     // 4:4:4 12-bit planar (mastering)
    
    // 16-bit formats
    YUV420P16LE,     // 4:2:0 16-bit planar (internal processing)
    YUV422P16LE,     // 4:2:2 16-bit planar (internal processing)
    YUV444P16LE,     // 4:4:4 16-bit planar (internal processing)
    
    // RGB high bit depth
    RGB48LE,         // 16-bit per channel RGB (48-bit total)
    RGBA64LE,        // 16-bit per channel RGBA (64-bit total)
    
    // Alpha variants
    YUVA420P10LE,    // 4:2:0:4 10-bit with alpha
    YUVA422P10LE,    // 4:2:2:4 10-bit with alpha
    YUVA444P10LE,    // 4:4:4:4 10-bit with alpha
    YUVA420P12LE,    // 4:2:0:4 12-bit with alpha
    YUVA422P12LE,    // 4:2:2:4 12-bit with alpha
    YUVA444P12LE,    // 4:4:4:4 12-bit with alpha
    
    // Professional packed formats
    V210,            // 10-bit 4:2:2 packed (broadcast standard)
    V410,            // 10-bit 4:4:4 packed (post-production)
    
    UNKNOWN
};

// Bit depth information
struct BitDepthInfo {
    uint8_t bits_per_component;      // 8, 10, 12, or 16
    uint8_t components_per_pixel;    // 3 for YUV, 4 for YUVA
    bool has_alpha;                  // Alpha channel present
    bool is_planar;                  // Planar vs packed layout
    bool is_signed;                  // Signed vs unsigned values
    uint32_t max_value;              // Maximum value for bit depth
    double range_scale;              // Scale factor (max_value / 255.0)
    std::string description;         // Human-readable description
};

// Processing precision configuration
struct ProcessingPrecision {
    enum class PrecisionMode {
        PRESERVE_SOURCE,    // Match source bit depth
        FORCE_16BIT,       // Always use 16-bit internal
        ADAPTIVE,          // Choose based on operations
        MEMORY_OPTIMIZED   // Balance quality vs memory
    };
    
    PrecisionMode mode = PrecisionMode::ADAPTIVE;
    bool enable_dithering = true;           // Dither when reducing bit depth
    bool detect_clipping = true;            // Warn about value clipping
    bool preserve_range = true;             // Maintain legal vs full range
    double quality_threshold = 0.98;       // Quality preservation target
};

// Quality metrics for bit depth conversion
struct QualityMetrics {
    double psnr = 0.0;                     // Peak Signal-to-Noise Ratio
    double ssim = 0.0;                     // Structural Similarity Index
    double delta_e = 0.0;                  // Color difference (CIE Delta E)
    uint32_t clipped_pixels = 0;           // Number of clipped values
    uint32_t out_of_range_pixels = 0;      // Out of legal range pixels
    bool quality_acceptable = false;        // Overall quality assessment
};

// Dithering algorithm options
enum class DitheringMethod {
    NONE,              // No dithering
    ORDERED,           // Ordered dithering (Bayer matrix)
    ERROR_DIFFUSION,   // Floyd-Steinberg error diffusion
    BLUE_NOISE,        // Blue noise dithering (high quality)
    TRIANGULAR_PDF     // Triangular PDF dithering (simple)
};

// High bit depth frame data structure
struct HighBitDepthFrame {
    HighBitDepthFormat format;
    uint32_t width;
    uint32_t height;
    std::vector<std::vector<uint8_t>> planes;  // Separate planes for planar formats
    std::vector<uint32_t> linesize;            // Bytes per line for each plane
    BitDepthInfo bit_depth_info;
    
    // Metadata
    bool is_limited_range = true;              // Limited vs full range
    double gamma = 2.4;                        // Gamma correction value
    uint32_t frame_number = 0;                 // Frame sequence number
    
    // Quality tracking
    QualityMetrics quality_metrics;
};

/**
 * @brief High Bit Depth Processing Pipeline
 * 
 * Handles conversion between different bit depths while preserving quality.
 * Implements professional dithering and quality assessment.
 */
class HighBitDepthSupport {
public:
    HighBitDepthSupport();
    ~HighBitDepthSupport() = default;
    
    // Format information
    BitDepthInfo getBitDepthInfo(HighBitDepthFormat format) const;
    std::vector<HighBitDepthFormat> getSupportedFormats() const;
    bool isFormatSupported(HighBitDepthFormat format) const;
    
    // Format detection
    HighBitDepthFormat detectFormat(const uint8_t* data, size_t size) const;
    bool requiresHighBitDepthProcessing(HighBitDepthFormat format) const;
    
    // Processing configuration
    void setProcessingPrecision(const ProcessingPrecision& precision);
    ProcessingPrecision getProcessingPrecision() const;
    
    // Bit depth conversion
    bool convertBitDepth(
        const HighBitDepthFrame& source,
        HighBitDepthFrame& destination,
        HighBitDepthFormat target_format,
        DitheringMethod dithering = DitheringMethod::ERROR_DIFFUSION
    ) const;
    
    // Quality assessment
    QualityMetrics assessQuality(
        const HighBitDepthFrame& reference,
        const HighBitDepthFrame& processed
    ) const;
    
    // Precision analysis
    uint8_t recommendInternalBitDepth(
        HighBitDepthFormat input_format,
        const std::vector<std::string>& processing_operations
    ) const;
    
    // Memory optimization
    size_t calculateMemoryRequirement(
        HighBitDepthFormat format,
        uint32_t width,
        uint32_t height
    ) const;
    
    // Dithering methods
    void applyDithering(
        const std::vector<uint16_t>& source,
        std::vector<uint8_t>& destination,
        DitheringMethod method,
        uint32_t width,
        uint32_t height
    ) const;
    
    // Range conversion
    void convertRange(
        HighBitDepthFrame& frame,
        bool to_full_range
    ) const;
    
    // Clipping detection
    std::vector<std::pair<uint32_t, uint32_t>> detectClipping(
        const HighBitDepthFrame& frame
    ) const;
    
    // Format validation
    bool validateFormat(const HighBitDepthFrame& frame) const;
    std::string getFormatDescription(HighBitDepthFormat format) const;

private:
    ProcessingPrecision processing_precision_;
    std::array<BitDepthInfo, static_cast<size_t>(HighBitDepthFormat::UNKNOWN)> format_info_;
    
    // Internal processing methods
    void initializeFormatInfo();
    std::vector<uint8_t> generateBayerMatrix(uint32_t size) const;
    void floydSteinbergDither(
        const std::vector<uint16_t>& source,
        std::vector<uint8_t>& destination,
        uint32_t width,
        uint32_t height,
        uint32_t source_max,
        uint32_t dest_max
    ) const;
    
    double calculatePSNR(
        const std::vector<uint16_t>& reference,
        const std::vector<uint16_t>& processed
    ) const;
    
    double calculateSSIM(
        const std::vector<uint16_t>& reference,
        const std::vector<uint16_t>& processed,
        uint32_t width,
        uint32_t height
    ) const;
    
    uint32_t countOutOfRangePixels(
        const HighBitDepthFrame& frame
    ) const;
    
    // Additional helper methods for conversion
    bool convertWithDithering(
        const HighBitDepthFrame& source,
        HighBitDepthFrame& destination,
        DitheringMethod dithering
    ) const;
    
    bool convertWithUpscaling(
        const HighBitDepthFrame& source,
        HighBitDepthFrame& destination
    ) const;
    
    bool convertSameBitDepth(
        const HighBitDepthFrame& source,
        HighBitDepthFrame& destination
    ) const;
    
    void extractLumaData(
        const HighBitDepthFrame& frame,
        std::vector<uint16_t>& luma_data
    ) const;
    
    uint32_t countClippedPixels(const HighBitDepthFrame& frame) const;
    
    void applyOrderedDithering(
        const std::vector<uint16_t>& source,
        std::vector<uint8_t>& destination,
        uint32_t width,
        uint32_t height
    ) const;
    
    void applyTriangularDithering(
        const std::vector<uint16_t>& source,
        std::vector<uint8_t>& destination
    ) const;
};

// Utility functions
namespace high_bitdepth_utils {
    
    // Convert between bit depths with scaling
    uint16_t scaleBitDepth(uint16_t value, uint8_t from_bits, uint8_t to_bits);
    
    // Check if value is in legal range
    bool isLegalRange(uint16_t value, uint8_t bit_depth, bool limited_range);
    
    // Professional format detection from metadata
    HighBitDepthFormat detectFromCodecName(const std::string& codec_name);
    
    // Calculate optimal internal precision
    uint8_t calculateOptimalPrecision(
        const std::vector<HighBitDepthFormat>& input_formats,
        const std::vector<std::string>& operations
    );
    
    // Memory-efficient processing recommendations
    struct ProcessingRecommendation {
        uint8_t internal_bit_depth;
        bool use_streaming;
        size_t recommended_buffer_size;
        DitheringMethod dithering_method;
    };
    
    ProcessingRecommendation getProcessingRecommendation(
        HighBitDepthFormat input_format,
        uint32_t width,
        uint32_t height,
        size_t available_memory
    );
}

} // namespace media_io
} // namespace ve
