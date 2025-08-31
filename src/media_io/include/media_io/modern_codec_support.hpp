#pragma once

#include "decode/frame.hpp"
#include "media_io/format_detector.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

/**
 * Modern Codec Support Implementation for Phase 1 Week 4
 * FORMAT_SUPPORT_ROADMAP.md - Future-proofing for modern workflows
 * 
 * Comprehensive support for AV1, HEVC 10/12-bit, and VP9 codecs
 * Essential for streaming, modern delivery, and next-generation video workflows
 */

namespace ve::media_io {

/**
 * AV1 Profile Support
 * Next-generation codec for efficient streaming and delivery
 */
enum class AV1Profile {
    MAIN,           // 8-bit 4:2:0
    HIGH,           // 8-bit 4:4:4, 10-bit 4:2:0/4:2:2
    PROFESSIONAL    // 12-bit, full feature set
};

/**
 * HEVC (H.265) Profile Support
 * Extended support for 10-bit and 12-bit HDR workflows
 */
enum class HEVCProfile {
    MAIN,           // 8-bit 4:2:0
    MAIN10,         // 10-bit 4:2:0 (HDR)
    MAIN12,         // 12-bit 4:2:0 (cinema)
    MAIN444,        // 8-bit 4:4:4
    MAIN444_10,     // 10-bit 4:4:4 (professional)
    MAIN444_12      // 12-bit 4:4:4 (cinema)
};

/**
 * VP9 Profile Support
 * Google's codec for YouTube and WebM streaming
 */
enum class VP9Profile {
    PROFILE_0,      // 8-bit 4:2:0
    PROFILE_1,      // 8-bit 4:2:2/4:4:4
    PROFILE_2,      // 10-bit 4:2:0
    PROFILE_3       // 10-bit 4:2:2/4:4:4
};

/**
 * Hardware Acceleration Support Detection
 * Critical for real-time playback of modern codecs
 */
enum class HardwareVendor {
    INTEL,          // Intel Quick Sync Video
    AMD,            // AMD VCE/VCN
    NVIDIA,         // NVIDIA NVDEC
    APPLE,          // Apple VideoToolbox
    QUALCOMM,       // Qualcomm Adreno
    SOFTWARE        // Software fallback
};

/**
 * Modern Codec Information Structure
 */
struct ModernCodecInfo {
    // Codec identification
    CodecFamily codec_family = CodecFamily::UNKNOWN;
    
    // Profile information
    AV1Profile av1_profile = static_cast<AV1Profile>(-1);
    HEVCProfile hevc_profile = static_cast<HEVCProfile>(-1);
    VP9Profile vp9_profile = static_cast<VP9Profile>(-1);
    
    // Technical specifications
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t framerate_num = 0;
    uint32_t framerate_den = 1;
    uint8_t bit_depth = 8;
    bool supports_alpha = false;
    bool is_hdr = false;
    
    // Performance characteristics
    uint32_t average_bitrate_kbps = 0;
    uint32_t max_bitrate_kbps = 0;
    float compression_efficiency = 1.0f; // vs H.264 baseline
    
    // Hardware acceleration
    HardwareVendor hw_vendor = HardwareVendor::SOFTWARE;
    bool hw_acceleration_available = false;
    bool hw_acceleration_required = false;
    
    // Color characteristics
    ve::decode::ColorSpace color_space = ve::decode::ColorSpace::BT709;
    ve::decode::ColorRange color_range = ve::decode::ColorRange::Limited;
    
    // Container and stream information
    std::string container_format;
    std::vector<uint8_t> codec_data;
    std::vector<std::string> metadata_tags;
    
    // Quality assessment
    float streaming_suitability = 0.0f;   // 0.0-1.0 for streaming
    float archival_quality = 0.0f;        // 0.0-1.0 for archival
    bool real_time_capable = false;
};

/**
 * Modern Codec Decode Settings
 * Optimized for hardware acceleration and streaming workflows
 */
struct ModernCodecDecodeSettings {
    // Hardware acceleration preference
    bool prefer_hardware_acceleration = true;
    bool fallback_to_software = true;
    HardwareVendor preferred_hw_vendor = HardwareVendor::SOFTWARE;
    
    // Performance settings
    uint32_t decode_threads = 4;
    bool enable_parallel_processing = true;
    bool enable_frame_threading = true;
    
    // Quality settings
    bool enable_error_concealment = true;
    bool enable_deblocking_filter = true;
    bool enable_adaptive_quantization = false;
    
    // Memory optimization
    uint32_t frame_buffer_count = 4;
    bool enable_zero_copy = true;
    bool enable_memory_pooling = true;
    
    // Streaming optimization
    bool enable_low_latency_mode = false;
    bool enable_temporal_layers = false;
    uint32_t max_decode_delay_frames = 8;
    
    // HDR and color settings
    bool preserve_hdr_metadata = true;
    bool enable_tone_mapping = false;
    ve::decode::PixelFormat target_pixel_format = ve::decode::PixelFormat::YUV420P;
};

/**
 * Modern Codec Performance Requirements
 * Detailed requirements for real-time playback
 */
struct ModernCodecPerformanceRequirements {
    // Memory requirements (MB)
    uint64_t decode_memory_mb = 0;
    uint64_t frame_memory_mb = 0;
    uint64_t total_memory_mb = 0;
    
    // CPU requirements
    uint32_t recommended_cores = 4;
    uint32_t recommended_threads = 8;
    float cpu_usage_estimate = 0.5f;
    
    // GPU requirements (if hardware acceleration used)
    uint64_t gpu_memory_mb = 0;
    float gpu_usage_estimate = 0.3f;
    bool requires_modern_gpu = false;
    
    // Real-time capability
    float real_time_factor = 1.0f;
    bool hardware_acceleration_required = false;
    bool software_fallback_viable = true;
    
    // Bandwidth requirements
    uint64_t bandwidth_kbps = 0;
    bool adaptive_streaming_capable = false;
};

/**
 * Modern Codec Detector and Analyzer
 * Core implementation for AV1, HEVC, and VP9 detection and optimization
 */
class ModernCodecDetector {
public:
    /**
     * Detect modern codec from container data
     * @param container_data Raw container data
     * @param data_size Size of container data
     * @param codec_hint Hint about expected codec family
     * @return ModernCodecInfo structure with detected information
     */
    static ModernCodecInfo detect_modern_codec(const uint8_t* container_data, 
                                               size_t data_size, 
                                               CodecFamily codec_hint = CodecFamily::UNKNOWN);
    
    /**
     * Detect hardware acceleration capabilities
     * @param codec_info Detected codec information
     * @return Updated codec info with hardware acceleration details
     */
    static ModernCodecInfo detect_hardware_acceleration(const ModernCodecInfo& codec_info);
    
    /**
     * Get optimized decode settings for modern codec
     * @param codec_info Detected codec information
     * @return Optimized settings for decoding
     */
    static ModernCodecDecodeSettings get_decode_settings(const ModernCodecInfo& codec_info);
    
    /**
     * Estimate performance requirements
     * @param codec_info Modern codec information
     * @return Performance requirements structure
     */
    static ModernCodecPerformanceRequirements estimate_performance_requirements(
        const ModernCodecInfo& codec_info);
    
    /**
     * Validate modern codec compatibility for streaming
     * @param codec_info Codec information
     * @param target_bandwidth_kbps Target streaming bandwidth
     * @return True if suitable for streaming at target bandwidth
     */
    static bool validate_streaming_compatibility(const ModernCodecInfo& codec_info,
                                                uint32_t target_bandwidth_kbps);
    
    /**
     * Get compression efficiency compared to H.264
     * @param codec_info Codec information
     * @return Compression ratio (higher = more efficient)
     */
    static float get_compression_efficiency(const ModernCodecInfo& codec_info);
    
    /**
     * Check if codec supports HDR workflows
     * @param codec_info Codec information
     * @return True if HDR capable
     */
    static bool supports_hdr_workflows(const ModernCodecInfo& codec_info);
    
    /**
     * Get recommended pixel format for codec
     * @param codec_info Codec information
     * @return Recommended pixel format for optimal performance
     */
    static ve::decode::PixelFormat get_recommended_pixel_format(const ModernCodecInfo& codec_info);
    
    /**
     * Get list of supported modern codecs with capabilities
     * @return Vector of supported codecs with hardware acceleration info
     */
    static std::vector<std::pair<std::string, bool>> get_supported_modern_codecs(); // bool = hw accel

private:
    // Codec-specific detection methods
    static AV1Profile detect_av1_profile(const std::vector<uint8_t>& codec_data);
    static HEVCProfile detect_hevc_profile(const std::vector<uint8_t>& codec_data);
    static VP9Profile detect_vp9_profile(const std::vector<uint8_t>& codec_data);
    
    // Hardware acceleration detection
    static HardwareVendor detect_available_hardware();
    static bool check_codec_hw_support(CodecFamily codec, HardwareVendor vendor);
    
    // Performance estimation helpers
    static uint64_t estimate_decode_complexity(const ModernCodecInfo& codec_info);
    static float calculate_bandwidth_efficiency(const ModernCodecInfo& codec_info);
    static bool requires_modern_hardware(const ModernCodecInfo& codec_info);
};

/**
 * Modern Codec Integration with Format Detection System
 * Extends the format detector with modern codec capabilities
 */
class ModernCodecFormatIntegration {
public:
    /**
     * Register modern codec capabilities with format detector
     * @param detector Format detector instance to enhance
     */
    static void register_modern_codec_capabilities(class FormatDetector& detector);
    
    /**
     * Create modern codec-specific format detection result
     * @param codec_info Detected modern codec information
     * @return DetectedFormat with modern codec-specific data
     */
    static struct DetectedFormat create_modern_codec_detected_format(const ModernCodecInfo& codec_info);
    
    /**
     * Validate modern codec workflow compatibility
     * @param detected_format Format detection result
     * @return Workflow recommendations and warnings
     */
    struct ModernCodecWorkflowRecommendations {
        std::vector<std::string> recommendations;
        std::vector<std::string> warnings;
        float streaming_score = 0.0f;
        float future_compatibility_score = 0.0f;
        bool hardware_acceleration_recommended = false;
    };
    
    static ModernCodecWorkflowRecommendations validate_modern_codec_workflow(
        const struct DetectedFormat& detected_format);
    
    /**
     * Get streaming platform compatibility matrix
     * @return Compatibility information for major streaming platforms
     */
    struct StreamingPlatformCompatibility {
        std::string platform_name;
        bool supports_av1 = false;
        bool supports_hevc_10bit = false;
        bool supports_vp9 = false;
        std::vector<std::string> recommended_profiles;
        uint32_t max_bitrate_kbps = 0;
        bool hdr_support = false;
    };
    
    static std::vector<StreamingPlatformCompatibility> get_streaming_platform_compatibility();
    
    /**
     * Get hardware vendor support matrix
     * @return Hardware acceleration support across vendors
     */
    struct HardwareVendorSupport {
        HardwareVendor vendor;
        std::string vendor_name;
        bool av1_decode = false;
        bool av1_encode = false;
        bool hevc_10bit_decode = false;
        bool hevc_10bit_encode = false;
        bool vp9_decode = false;
        bool vp9_encode = false;
        std::vector<std::string> supported_resolutions;
    };
    
    static std::vector<HardwareVendorSupport> get_hardware_vendor_support();
};

} // namespace ve::media_io
