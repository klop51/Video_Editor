#pragma once

#include "decode/frame.hpp"
#include "format_detector.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

/**
 * DNxHD/DNxHR Support Implementation for Phase 1 Week 3
 * FORMAT_SUPPORT_ROADMAP.md - Critical for broadcast workflows
 * 
 * Comprehensive support for Avid DNxHD (legacy) and DNxHR (modern) codecs
 * Essential for professional video editing and broadcast infrastructure
 */

namespace ve::media_io {

/**
 * DNxHD Legacy Profiles - Fixed resolution 1920x1080
 * Used in traditional broadcast and Avid Media Composer workflows
 */
enum class DNxHDProfile {
    DNxHD_120,  // 1920x1080, 120 Mbps, 8-bit 4:2:2
    DNxHD_145,  // 1920x1080, 145 Mbps, 8-bit 4:2:2
    DNxHD_220,  // 1920x1080, 220 Mbps, 8-bit 4:2:2
    DNxHD_440   // 1920x1080, 440 Mbps, 8-bit 4:4:4 (rare)
};

/**
 * DNxHR Modern Profiles - Resolution independent
 * Supports any resolution from SD to 8K+, critical for modern workflows
 */
enum class DNxHRProfile {
    DNxHR_LB,   // Low Bandwidth - proxy/offline editing
    DNxHR_SQ,   // Standard Quality - online editing
    DNxHR_HQ,   // High Quality - broadcast/delivery  
    DNxHR_HQX,  // High Quality 10-bit - HDR workflows
    DNxHR_444   // 444 12-bit - cinema/archival
};

/**
 * Unified DNx codec information structure
 */
struct DNxInfo {
    // Profile identification
    DNxHDProfile dnxhd_profile = static_cast<DNxHDProfile>(-1);
    DNxHRProfile dnxhr_profile = static_cast<DNxHRProfile>(-1);
    bool is_dnxhr = false;  // false = DNxHD, true = DNxHR
    
    // Technical specifications
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t target_bitrate_mbps = 0;
    uint8_t bit_depth = 8;
    bool supports_alpha = false;
    
    // Video specifications
    uint32_t framerate_num = 0;
    uint32_t framerate_den = 1;
    
    // Color characteristics
    ve::decode::ColorSpace color_space = ve::decode::ColorSpace::BT709;
    ve::decode::ColorRange color_range = ve::decode::ColorRange::Limited;
    
    // Container information
    std::string container_format = "MOV"; // Usually MOV or MXF
    std::vector<uint8_t> codec_data;
    
    // Quality and workflow metadata
    float quality_factor = 1.0f;
    bool broadcast_legal = true;
    bool edit_friendly = true;
};

/**
 * DNx Codec Decoder Settings
 * Optimized for different DNx profiles and workflow requirements
 */
struct DNxDecodeSettings {
    // Pixel format optimization
    ve::decode::PixelFormat target_pixel_format = ve::decode::PixelFormat::YUV422P10LE;
    
    // Performance settings
    bool use_hardware_acceleration = true;
    uint32_t decode_threads = 4;
    bool enable_threading = true;
    
    // Quality settings
    bool enable_error_recovery = true;
    bool enable_quality_enhancement = false;
    
    // Memory optimization
    uint32_t frame_buffer_count = 3;
    bool enable_frame_reordering = true;
    
    // Color space handling
    bool preserve_color_metadata = true;
    bool enable_color_conversion = true;
    
    // Professional workflow features
    bool enable_timecode_extraction = true;
    bool preserve_metadata = true;
    bool enable_alpha_channel = false;
};

/**
 * DNx Performance Requirements Estimation
 * Critical for real-time playback and system resource planning
 */
struct DNxPerformanceRequirements {
    // Memory requirements (MB)
    uint64_t decode_memory_mb = 0;
    uint64_t frame_memory_mb = 0;
    uint64_t total_memory_mb = 0;
    
    // CPU requirements
    uint32_t recommended_cores = 4;
    uint32_t recommended_threads = 8;
    float cpu_usage_estimate = 0.3f;
    
    // Real-time capability
    float real_time_factor = 1.0f;  // 1.0 = real-time, >1.0 = faster than real-time
    bool hardware_acceleration_required = false;
    bool real_time_capable = true;
    
    // I/O requirements
    uint64_t bandwidth_mbps = 0;
    bool ssd_recommended = false;
};

/**
 * DNx Codec Detector and Analyzer
 * Core implementation for DNxHD/DNxHR detection and optimization
 */
class DNxDetector {
public:
    /**
     * Detect DNx profile from container data
     * @param container_data Raw container data (MOV/MXF)
     * @param data_size Size of container data
     * @return DNxInfo structure with detected information
     */
    static DNxInfo detect_dnx_profile(const uint8_t* container_data, size_t data_size);
    
    /**
     * Validate DNx compatibility for resolution/framerate
     * @param width Video width
     * @param height Video height
     * @param fps_num Framerate numerator
     * @param fps_den Framerate denominator
     * @return True if compatible with DNx standards
     */
    static bool validate_dnx_compatibility(uint32_t width, uint32_t height, 
                                          uint32_t fps_num, uint32_t fps_den);
    
    /**
     * Get optimized decode settings for DNx profile
     * @param dnx_info Detected DNx information
     * @return Optimized settings for decoding
     */
    static DNxDecodeSettings get_decode_settings(const DNxInfo& dnx_info);
    
    /**
     * Estimate performance requirements
     * @param dnx_info DNx codec information
     * @return Performance requirements structure
     */
    static DNxPerformanceRequirements estimate_performance_requirements(const DNxInfo& dnx_info);
    
    /**
     * Get target bitrate for DNx profile at specific resolution
     * @param profile DNxHR profile (DNxHD has fixed bitrates)
     * @param width Video width
     * @param height Video height
     * @param fps_num Framerate numerator
     * @param fps_den Framerate denominator
     * @return Target bitrate in Mbps
     */
    static uint32_t calculate_target_bitrate(DNxHRProfile profile, uint32_t width, uint32_t height,
                                           uint32_t fps_num, uint32_t fps_den);
    
    /**
     * Check if DNx profile supports alpha channel
     * @param dnx_info DNx information structure
     * @return True if alpha channel supported
     */
    static bool supports_alpha_channel(const DNxInfo& dnx_info);
    
    /**
     * Get recommended pixel format for DNx profile
     * @param dnx_info DNx information structure
     * @return Recommended pixel format for optimal performance
     */
    static ve::decode::PixelFormat get_recommended_pixel_format(const DNxInfo& dnx_info);
    
    /**
     * Get list of supported DNx profiles with capabilities
     * @return Vector of supported profiles with encode/decode capabilities
     */
    static std::vector<std::pair<std::string, bool>> get_supported_profiles(); // bool = encode support
    
    /**
     * Detect DNxHD profile from codec data (public for testing)
     */
    static DNxHDProfile detect_dnxhd_profile(const std::vector<uint8_t>& codec_data);
    
    /**
     * Detect DNxHR profile from codec data (public for testing)
     */
    static DNxHRProfile detect_dnxhr_profile(const std::vector<uint8_t>& codec_data);
private:
    static bool validate_dnxhd_resolution(uint32_t width, uint32_t height);
    static bool validate_dnxhr_resolution(uint32_t width, uint32_t height);
    static bool validate_dnx_framerate(uint32_t fps_num, uint32_t fps_den);
    
    // Performance calculation helpers
    static uint64_t calculate_frame_memory_mb(uint32_t width, uint32_t height, 
                                            uint8_t bit_depth, bool has_alpha);
    static float estimate_decode_complexity(const DNxInfo& dnx_info);
    static uint32_t get_optimal_thread_count(const DNxInfo& dnx_info);
};

/**
 * DNx Integration with Format Detection System
 * Extends the format detector with DNx-specific capabilities
 */
class DNxFormatIntegration {
public:
    /**
     * Register DNx capabilities with format detector
     * @param detector Format detector instance to enhance
     */
    static void register_dnx_capabilities(class FormatDetector& detector);
    
    /**
     * Create DNx-specific format detection result
     * @param dnx_info Detected DNx information
     * @return DetectedFormat with DNx-specific data
     */
    static struct DetectedFormat create_dnx_detected_format(const DNxInfo& dnx_info);
    
    /**
     * Validate DNx workflow compatibility
     * @param detected_format Format detection result
     * @return Workflow recommendations and warnings
     */
    struct DNxWorkflowRecommendations {
        std::vector<std::string> recommendations;
        std::vector<std::string> warnings;
        float broadcast_compatibility_score = 0.0f;
        bool edit_friendly = false;
        bool broadcast_legal = false;
    };
    
    static DNxWorkflowRecommendations validate_dnx_workflow(const struct DetectedFormat& detected_format);
    
    /**
     * Get broadcast compatibility matrix for different DNx profiles
     * @return Compatibility information for broadcast systems
     */
    struct BroadcastCompatibility {
        std::string system_name;
        bool supports_dnxhd = false;
        bool supports_dnxhr = false;
        std::vector<std::string> supported_profiles;
        std::vector<std::string> recommended_profiles;
    };
    
    static std::vector<BroadcastCompatibility> get_broadcast_compatibility_matrix();
};

} // namespace ve::media_io
