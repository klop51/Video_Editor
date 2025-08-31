#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>
#include "decode/frame.hpp"
#include "media_io/hdr_infrastructure.hpp"  // Phase 2 Week 5: HDR integration

namespace ve::media_io {

/**
 * Professional format detection and capability validation system
 * Phase 1 Week 1: Core infrastructure for detecting all professional video formats
 */

enum class CodecFamily {
    UNKNOWN,
    
    // Professional acquisition codecs
    PRORES,
    DNXHD,
    DNXHR,
    
    // Modern delivery codecs  
    H264,
    H265_HEVC,
    HEVC,         // Phase 1 Week 4: Enhanced HEVC support
    AV1,          // Phase 1 Week 4: Next-generation codec
    VP9,          // Phase 1 Week 4: Web streaming optimization
    
    // Broadcast legacy
    DV,
    DVCPRO,
    HDV,
    
    // RAW formats (future implementation)
    REDCODE,
    ARRIRAW,
    BLACKMAGIC_RAW,
    PRORES_RAW,
    CINEMA_DNG
};

enum class ContainerType {
    UNKNOWN,
    
    // Professional containers
    MOV,              // QuickTime (ProRes, etc.)
    MXF,              // Material Exchange Format
    AVI,              // Audio Video Interleave
    MP4,              // MPEG-4 container
    MKV,              // Matroska
    WEBM,             // Phase 1 Week 4: WebM for VP9/AV1
    TS,               // Phase 1 Week 4: Transport Stream for HEVC
    
    // Broadcast containers
    GXF,              // General Exchange Format
    LXF,              // Leitch eXchange Format
    
    // RAW containers
    R3D,              // RED files
    ARI,              // ARRI files
    BRAW,             // Blackmagic RAW
    DNG               // Digital Negative
};

struct FormatCapability {
    bool supports_decode = false;
    bool supports_encode = false;
    bool hardware_accelerated = false;
    bool real_time_capable = false;
    
    // Performance characteristics
    uint32_t max_width = 0;
    uint32_t max_height = 0;
    uint32_t max_framerate = 0;
    
    // Quality characteristics
    uint8_t max_bit_depth = 8;
    bool supports_alpha = false;
    bool supports_hdr = false;
    
    // Professional features
    bool supports_timecode = false;
    bool supports_metadata = false;
    bool supports_multitrack_audio = false;
    
    // Phase 1 Week 4: Modern codec features
    float compression_efficiency = 1.0f;  // Relative to H.264
    bool streaming_optimized = false;
    bool supports_variable_framerate = false;
    bool adaptive_streaming_ready = false;
};

struct DetectedFormat {
    CodecFamily codec_family = CodecFamily::UNKNOWN;  // Updated for consistency
    CodecFamily codec = CodecFamily::UNKNOWN;
    ContainerType container = ContainerType::UNKNOWN;
    ve::decode::PixelFormat pixel_format = ve::decode::PixelFormat::Unknown;
    ve::decode::ColorSpace color_space = ve::decode::ColorSpace::Unknown;
    ve::decode::ColorRange color_range = ve::decode::ColorRange::Limited; // Phase 1 Week 4
    
    // Detection confidence (Phase 1 Week 4: Modern codec integration)
    float confidence = 0.0f; // 0.0-1.0 detection confidence
    
    // Stream information
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t framerate_num = 0;
    uint32_t framerate_den = 1;
    uint8_t bit_depth = 8;
    
    // Professional metadata
    std::string profile_name;
    std::optional<std::string> timecode;
    std::vector<std::string> metadata_keys;
    
    // Phase 2 Week 5: HDR metadata integration
    std::optional<HDRMetadata> hdr_metadata;
    bool has_hdr_content = false;
    
    // Capability assessment
    FormatCapability capability;
    
    // Quality assessment
    float professional_score = 0.0f; // 0.0-1.0 scale
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    
    // Phase 1 Week 4: Modern codec features
    uint32_t memory_requirement_mb = 0;
    float decode_complexity = 0.0f;
    bool hardware_acceleration_available = false;
    bool hardware_acceleration_required = false;
    bool streaming_optimized = false;
    bool archival_quality = false;
};

/**
 * Format Detection Engine
 * Auto-detects and validates professional video formats
 */
class FormatDetector {
public:
    FormatDetector();
    ~FormatDetector() = default;
    
    /**
     * Detect format from file path
     * @param file_path Path to video file
     * @return Detected format information or nullopt if unsupported
     */
    std::optional<DetectedFormat> detect_file_format(const std::string& file_path);
    
    /**
     * Detect format from file header/metadata
     * @param header_data First bytes of file for format detection
     * @param file_extension File extension hint
     * @return Detected format information
     */
    std::optional<DetectedFormat> detect_format_from_header(
        const std::vector<uint8_t>& header_data,
        const std::string& file_extension = ""
    );
    
    /**
     * Get capability matrix for specific format combination
     * @param codec Codec family
     * @param container Container type  
     * @return Capability information
     */
    FormatCapability get_format_capability(CodecFamily codec, ContainerType container);
    
    /**
     * Validate if format combination is supported
     * @param format Detected format to validate
     * @return true if fully supported, false otherwise
     */
    bool is_format_supported(const DetectedFormat& format);
    
    /**
     * Get recommended settings for optimal playback
     * @param format Detected format
     * @return Optimization recommendations
     */
    std::vector<std::string> get_optimization_recommendations(const DetectedFormat& format);
    
    /**
     * Get professional score for format
     * Rates format suitability for professional workflows (0.0-1.0)
     * @param format Detected format
     * @return Professional quality score
     */
    float calculate_professional_score(const DetectedFormat& format);
    
    /**
     * Register custom format capability
     * Allows runtime capability updates
     */
    void register_format_capability(CodecFamily codec, ContainerType container, FormatCapability capability);
    
    /**
     * Register custom codec detector (Phase 1 Week 4: Modern codec integration)
     * @param codec_name Name of the codec
     * @param detector_func Function that returns confidence (0.0-1.0) for data
     */
    using CodecDetectorFunction = std::function<float(const uint8_t*, size_t)>;
    void register_codec_detector(const std::string& codec_name, CodecDetectorFunction detector_func);
    
    /**
     * Detect HDR metadata from stream data (Phase 2 Week 5)
     * @param stream_data Stream metadata/SEI messages
     * @return Detected HDR metadata or nullopt
     */
    std::optional<HDRMetadata> detect_hdr_metadata(const std::vector<uint8_t>& stream_data);
    
    /**
     * Assess HDR capability for detected format (Phase 2 Week 5)
     * @param format Format to assess for HDR support
     */
    void assess_hdr_capability(DetectedFormat& format);
    
    /**
     * Get list of all supported professional formats
     * @return Vector of supported codec/container combinations
     */
    std::vector<std::pair<CodecFamily, ContainerType>> get_supported_formats();
    
private:
    // Capability matrix for format validation
    std::unordered_map<CodecFamily, std::unordered_map<ContainerType, FormatCapability>> capability_matrix_;
    
    // Modern codec detector functions (Phase 1 Week 4)
    std::unordered_map<std::string, CodecDetectorFunction> codec_detectors_;
    
    // HDR infrastructure integration (Phase 2 Week 5)
    std::unique_ptr<HDRInfrastructure> hdr_infrastructure_;
    
    // Format detection helpers
    CodecFamily detect_codec_from_fourcc(uint32_t fourcc);
    ContainerType detect_container_from_signature(const std::vector<uint8_t>& header);
    std::string extract_profile_name(CodecFamily codec, const std::vector<uint8_t>& codec_private_data);
    
    // Professional format validators
    bool validate_prores_profile(const std::string& profile);
    bool validate_dnx_profile(const std::string& profile);
    bool validate_professional_resolution(uint32_t width, uint32_t height);
    
    // Capability assessment
    void assess_hardware_acceleration(DetectedFormat& format);
    void assess_real_time_capability(DetectedFormat& format);
    void generate_format_warnings(DetectedFormat& format);
    
    // Initialize capability matrix with professional formats
    void initialize_professional_capabilities();
    void initialize_broadcast_capabilities();
    void initialize_modern_codec_capabilities();
};

/**
 * Format Utility Functions
 */
namespace format_utils {
    
    /**
     * Convert codec family to human-readable string
     */
    std::string codec_family_to_string(CodecFamily codec);
    
    /**
     * Convert container type to human-readable string  
     */
    std::string container_type_to_string(ContainerType container);
    
    /**
     * Get file extension for container type
     */
    std::string get_extension_for_container(ContainerType container);
    
    /**
     * Check if codec is professional acquisition format
     */
    bool is_professional_acquisition_codec(CodecFamily codec);
    
    /**
     * Check if codec supports HDR workflows
     */
    bool supports_hdr_workflow(CodecFamily codec);
    
    /**
     * Get typical bitrate range for codec/resolution combination
     */
    std::pair<uint32_t, uint32_t> get_bitrate_range_mbps(
        CodecFamily codec, 
        uint32_t width, 
        uint32_t height, 
        uint32_t framerate
    );
}

} // namespace ve::media_io
