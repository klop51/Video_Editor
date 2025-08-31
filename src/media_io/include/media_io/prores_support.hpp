#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "decode/frame.hpp"

namespace ve::media_io {

/**
 * ProRes Support Implementation
 * Phase 1 Week 2: Professional ProRes codec support for video editor
 */

enum class ProResProfile {
    UNKNOWN,
    PROXY,      // ~45 Mbps - Proxy workflows, offline editing
    LT,         // ~102 Mbps - Lower bandwidth, streaming
    STANDARD,   // ~147 Mbps - Standard quality, most common
    HQ,         // ~220 Mbps - High quality, finishing work
    FOUR444,    // ~330 Mbps - 4:4:4 sampling with alpha
    FOUR444_XQ  // ~500 Mbps - Extreme quality, highest fidelity
};

enum class ProResColorSpace {
    UNKNOWN,
    REC709,     // Standard broadcast color space
    REC2020,    // Wide color gamut for HDR
    P3_D65,     // DCI-P3 with D65 white point
    LOG,        // Log encoding for grading workflows
    LINEAR      // Linear light for VFX work
};

struct ProResInfo {
    ProResProfile profile = ProResProfile::UNKNOWN;
    ProResColorSpace color_space = ProResColorSpace::UNKNOWN;
    
    // Technical specifications
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t framerate_num = 0;
    uint32_t framerate_den = 1;
    uint8_t bit_depth = 10;          // Usually 10-bit, 12-bit for 4444 XQ
    bool has_alpha = false;          // True for ProRes 4444 variants
    
    // Bitrate information
    uint32_t target_bitrate_mbps = 0;
    uint32_t actual_bitrate_mbps = 0;
    
    // Quality metrics
    float compression_ratio = 0.0f;   // Actual compression achieved
    std::string profile_name;         // Human-readable profile name
    std::string fourcc;               // ProRes FourCC identifier
    
    // Metadata
    std::vector<std::string> camera_metadata;
    std::optional<std::string> creation_software;
    std::optional<std::string> camera_model;
};

/**
 * ProRes Profile Detector and Validator
 * Detects ProRes variants and validates compatibility
 */
class ProResDetector {
public:
    ProResDetector() = default;
    ~ProResDetector() = default;
    
    /**
     * Detect ProRes profile from FourCC and codec private data
     * @param fourcc ProRes FourCC identifier
     * @param codec_data Additional codec-specific data
     * @return Detected ProRes information
     */
    std::optional<ProResInfo> detect_prores_profile(
        const std::string& fourcc,
        const std::vector<uint8_t>& codec_data = {}
    );
    
    /**
     * Validate ProRes file compatibility
     * @param prores_info Detected ProRes information
     * @return true if supported, false otherwise
     */
    bool validate_prores_compatibility(const ProResInfo& prores_info);
    
    /**
     * Get optimal decode settings for ProRes profile
     * @param profile ProRes profile
     * @return Recommended decode parameters
     */
    struct DecodeSettings {
        bool use_hardware_acceleration = true;
        bool enable_alpha_channel = false;
        ve::decode::PixelFormat target_pixel_format = ve::decode::PixelFormat::YUV422P10LE;
        ve::decode::ColorSpace target_color_space = ve::decode::ColorSpace::BT709;
        uint32_t decode_threads = 4;
        bool preserve_metadata = true;
    };
    
    DecodeSettings get_optimal_decode_settings(ProResProfile profile);
    
    /**
     * Estimate performance requirements for ProRes profile
     * @param prores_info ProRes file information
     * @return Performance requirements
     */
    struct PerformanceRequirements {
        uint64_t memory_mb_per_frame = 0;
        uint32_t cpu_threads_recommended = 4;
        float gpu_memory_mb = 0.0f;
        bool requires_hardware_decode = false;
        float real_time_factor = 1.0f;  // 1.0 = real-time, >1.0 = faster than real-time
    };
    
    PerformanceRequirements estimate_performance_requirements(const ProResInfo& prores_info);
    
    /**
     * Get list of supported ProRes profiles
     * @return Vector of supported profiles with capabilities
     */
    static std::vector<std::pair<ProResProfile, bool>> get_supported_profiles(); // bool = encode support
    
    /**
     * Convert ProRes profile to human-readable string
     */
    static std::string profile_to_string(ProResProfile profile);
    
    /**
     * Get target bitrate for ProRes profile at given resolution/framerate
     */
    static uint32_t get_target_bitrate_mbps(
        ProResProfile profile,
        uint32_t width,
        uint32_t height,
        uint32_t framerate
    );
    
    /**
     * Check if ProRes profile supports alpha channel
     */
    static bool supports_alpha_channel(ProResProfile profile);
    
    /**
     * Get recommended pixel format for ProRes profile
     */
    static ve::decode::PixelFormat get_recommended_pixel_format(ProResProfile profile);

private:
    // ProRes FourCC to profile mapping
    ProResProfile fourcc_to_profile(const std::string& fourcc);
    
    // Validate technical specifications
    bool validate_resolution(uint32_t width, uint32_t height);
    bool validate_framerate(uint32_t num, uint32_t den);
    bool validate_bitrate(uint32_t bitrate_mbps, ProResProfile profile);
    
    // Color space detection from metadata
    ProResColorSpace detect_color_space(const std::vector<uint8_t>& metadata);
    
    // Performance estimation helpers
    uint64_t calculate_frame_memory_mb(uint32_t width, uint32_t height, uint8_t bit_depth, bool has_alpha);
    float estimate_decode_complexity(ProResProfile profile);
};

/**
 * ProRes Integration with Format Detection System
 * Extends the format detector with ProRes-specific capabilities
 */
class ProResFormatIntegration {
public:
    /**
     * Register ProRes capabilities with format detector
     * @param detector Format detector instance to enhance
     */
    static void register_prores_capabilities(class FormatDetector& detector);
    
    /**
     * Create ProRes-specific format detection result
     * @param prores_info Detected ProRes information
     * @return DetectedFormat with ProRes-specific data
     */
    static struct DetectedFormat create_prores_detected_format(const ProResInfo& prores_info);
    
    /**
     * Validate ProRes workflow compatibility
     * @param detected_format Format detection result
     * @return Workflow recommendations and warnings
     */
    struct WorkflowRecommendations {
        std::vector<std::string> recommendations;
        std::vector<std::string> warnings;
        float professional_score = 0.0f;
        bool real_time_capable = false;
    };
    
    static WorkflowRecommendations validate_prores_workflow(const struct DetectedFormat& detected_format);
};

/**
 * ProRes Utility Functions
 */
namespace prores_utils {
    
    /**
     * Check if file extension suggests ProRes content
     */
    bool is_prores_extension(const std::string& extension);
    
    /**
     * Get common ProRes FourCC identifiers
     */
    std::vector<std::string> get_prores_fourccs();
    
    /**
     * Estimate file size for ProRes encoding
     */
    uint64_t estimate_file_size_mb(
        ProResProfile profile,
        uint32_t width,
        uint32_t height,
        uint32_t framerate,
        uint32_t duration_seconds
    );
    
    /**
     * Get camera compatibility information
     */
    struct CameraCompatibility {
        std::string camera_brand;
        std::vector<std::string> supported_profiles;
        std::string notes;
    };
    
    std::vector<CameraCompatibility> get_camera_compatibility_matrix();
    
    /**
     * Convert ProRes to other professional formats
     */
    struct ConversionRecommendation {
        std::string target_codec;
        std::string reason;
        float quality_retention = 1.0f;
        float size_factor = 1.0f;
    };
    
    std::vector<ConversionRecommendation> get_conversion_recommendations(ProResProfile source_profile);
}

} // namespace ve::media_io
