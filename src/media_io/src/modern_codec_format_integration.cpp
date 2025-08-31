#include "media_io/modern_codec_support.hpp"
#include "media_io/format_detector.hpp"

/**
 * Modern Codec Format Integration Implementation
 * FORMAT_SUPPORT_ROADMAP.md Phase 1 Week 4 - Integration Layer
 * 
 * Integrates modern codec support with existing format detection system
 * Provides streaming platform compatibility and hardware vendor support
 */

namespace ve::media_io {

void ModernCodecFormatIntegration::register_modern_codec_capabilities(FormatDetector& detector) {
    // Register AV1 capabilities
    auto av1_caps = [](const uint8_t* data, size_t size) -> float {
        ModernCodecInfo info = ModernCodecDetector::detect_modern_codec(data, size, CodecFamily::AV1);
        if (info.codec_family == CodecFamily::AV1) {
            return info.hw_acceleration_available ? 0.95f : 0.80f;
        }
        return 0.0f;
    };
    
    // Register HEVC capabilities
    auto hevc_caps = [](const uint8_t* data, size_t size) -> float {
        ModernCodecInfo info = ModernCodecDetector::detect_modern_codec(data, size, CodecFamily::HEVC);
        if (info.codec_family == CodecFamily::HEVC) {
            float base_confidence = 0.85f;
            if (info.is_hdr) base_confidence += 0.10f; // HDR bonus
            if (info.hw_acceleration_available) base_confidence += 0.05f;
            return std::min(base_confidence, 1.0f);
        }
        return 0.0f;
    };
    
    // Register VP9 capabilities  
    auto vp9_caps = [](const uint8_t* data, size_t size) -> float {
        ModernCodecInfo info = ModernCodecDetector::detect_modern_codec(data, size, CodecFamily::VP9);
        if (info.codec_family == CodecFamily::VP9) {
            return info.hw_acceleration_available ? 0.90f : 0.75f;
        }
        return 0.0f;
    };
    
    // Register with format detector (assuming it has a registration method)
    // Note: This assumes the FormatDetector has been enhanced to support modern codecs
    detector.register_codec_detector("AV1", av1_caps);
    detector.register_codec_detector("HEVC", hevc_caps);
    detector.register_codec_detector("VP9", vp9_caps);
}

DetectedFormat ModernCodecFormatIntegration::create_modern_codec_detected_format(const ModernCodecInfo& codec_info) {
    DetectedFormat format;
    
    // Basic format information
    format.confidence = 0.90f; // High confidence for modern codecs
    format.codec_family = codec_info.codec_family;
    
    // Resolution and timing
    format.width = codec_info.width;
    format.height = codec_info.height;
    format.framerate_num = codec_info.framerate_num;
    format.framerate_den = codec_info.framerate_den;
    
    // Color information
    format.color_space = codec_info.color_space;
    format.color_range = codec_info.color_range;
    format.pixel_format = ModernCodecDetector::get_recommended_pixel_format(codec_info);
    
    // Codec-specific profile information
    switch (codec_info.codec_family) {
        case CodecFamily::AV1:
            format.profile_name = "AV1 ";
            switch (codec_info.av1_profile) {
                case AV1Profile::MAIN: format.profile_name += "Main"; break;
                case AV1Profile::HIGH: format.profile_name += "High"; break;
                case AV1Profile::PROFESSIONAL: format.profile_name += "Professional"; break;
            }
            break;
            
        case CodecFamily::HEVC:
            format.profile_name = "HEVC ";
            switch (codec_info.hevc_profile) {
                case HEVCProfile::MAIN: format.profile_name += "Main"; break;
                case HEVCProfile::MAIN10: format.profile_name += "Main 10"; break;
                case HEVCProfile::MAIN12: format.profile_name += "Main 12"; break;
                case HEVCProfile::MAIN444: format.profile_name += "Main 4:4:4"; break;
                case HEVCProfile::MAIN444_10: format.profile_name += "Main 4:4:4 10"; break;
                case HEVCProfile::MAIN444_12: format.profile_name += "Main 4:4:4 12"; break;
            }
            break;
            
        case CodecFamily::VP9:
            format.profile_name = "VP9 Profile ";
            switch (codec_info.vp9_profile) {
                case VP9Profile::PROFILE_0: format.profile_name += "0"; break;
                case VP9Profile::PROFILE_1: format.profile_name += "1"; break;
                case VP9Profile::PROFILE_2: format.profile_name += "2"; break;
                case VP9Profile::PROFILE_3: format.profile_name += "3"; break;
            }
            break;
            
        default:
            format.profile_name = "Unknown";
            break;
    }
    
    // Performance estimates
    ModernCodecPerformanceRequirements perf = ModernCodecDetector::estimate_performance_requirements(codec_info);
    format.decode_complexity = perf.cpu_usage_estimate;
    format.memory_requirement_mb = static_cast<uint32_t>(perf.total_memory_mb);
    
    // Hardware acceleration info
    format.hardware_acceleration_available = codec_info.hw_acceleration_available;
    format.hardware_acceleration_required = codec_info.hw_acceleration_required;
    
    // Quality indicators
    format.streaming_optimized = codec_info.streaming_suitability > 0.8f;
    format.archival_quality = codec_info.archival_quality > 0.8f;
    
    return format;
}

ModernCodecFormatIntegration::ModernCodecWorkflowRecommendations 
ModernCodecFormatIntegration::validate_modern_codec_workflow(const DetectedFormat& detected_format) {
    
    ModernCodecWorkflowRecommendations recommendations;
    
    // Hardware acceleration recommendations
    if (detected_format.hardware_acceleration_available) {
        recommendations.recommendations.push_back("✓ Hardware acceleration available - enable for optimal performance");
        recommendations.hardware_acceleration_recommended = true;
    }
    else if (detected_format.hardware_acceleration_required) {
        recommendations.warnings.push_back("⚠ Hardware acceleration required for real-time playback");
        recommendations.hardware_acceleration_recommended = true;
    }
    
    // Resolution-specific recommendations
    if (detected_format.width >= 3840) { // 4K+
        recommendations.recommendations.push_back("✓ 4K content detected - ensure sufficient system resources");
        if (!detected_format.hardware_acceleration_available) {
            recommendations.warnings.push_back("⚠ 4K software decoding may not achieve real-time performance");
        }
    }
    
    // Codec-specific recommendations
    switch (detected_format.codec_family) {
        case CodecFamily::AV1:
            recommendations.recommendations.push_back("✓ AV1 codec - excellent compression efficiency for streaming");
            recommendations.streaming_score = 0.95f;
            recommendations.future_compatibility_score = 0.98f;
            if (!detected_format.hardware_acceleration_available) {
                recommendations.warnings.push_back("⚠ AV1 software decode is CPU intensive");
            }
            break;
            
        case CodecFamily::HEVC:
            recommendations.recommendations.push_back("✓ HEVC codec - excellent quality and hardware support");
            recommendations.streaming_score = 0.85f;
            recommendations.future_compatibility_score = 0.90f;
            if (detected_format.profile_name.find("10") != std::string::npos) {
                recommendations.recommendations.push_back("✓ HDR 10-bit content detected - preserve for HDR workflows");
            }
            break;
            
        case CodecFamily::VP9:
            recommendations.recommendations.push_back("✓ VP9 codec - optimized for web streaming");
            recommendations.streaming_score = 0.90f;
            recommendations.future_compatibility_score = 0.85f;
            break;
            
        default:
            recommendations.streaming_score = 0.50f;
            recommendations.future_compatibility_score = 0.50f;
            break;
    }
    
    // Streaming recommendations
    if (detected_format.streaming_optimized) {
        recommendations.recommendations.push_back("✓ Content optimized for streaming delivery");
    }
    
    // Quality warnings
    if (detected_format.decode_complexity > 0.8f) {
        recommendations.warnings.push_back("⚠ High decode complexity - may impact real-time performance");
    }
    
    if (detected_format.memory_requirement_mb > 2048) {
        recommendations.warnings.push_back("⚠ High memory requirements - ensure sufficient RAM available");
    }
    
    return recommendations;
}

std::vector<ModernCodecFormatIntegration::StreamingPlatformCompatibility> 
ModernCodecFormatIntegration::get_streaming_platform_compatibility() {
    
    std::vector<StreamingPlatformCompatibility> platforms;
    
    // YouTube
    platforms.push_back({
        .platform_name = "YouTube",
        .supports_av1 = true,
        .supports_hevc_10bit = false,  // Limited HEVC support
        .supports_vp9 = true,
        .recommended_profiles = {"VP9 Profile 0", "VP9 Profile 2", "AV1 Main"},
        .max_bitrate_kbps = 68000,     // 4K recommendation
        .hdr_support = true
    });
    
    // Netflix
    platforms.push_back({
        .platform_name = "Netflix",
        .supports_av1 = true,
        .supports_hevc_10bit = true,
        .supports_vp9 = true,
        .recommended_profiles = {"AV1 Main", "HEVC Main 10", "VP9 Profile 2"},
        .max_bitrate_kbps = 25000,     // 4K streaming
        .hdr_support = true
    });
    
    // Twitch
    platforms.push_back({
        .platform_name = "Twitch",
        .supports_av1 = false,         // Not yet supported
        .supports_hevc_10bit = false,
        .supports_vp9 = false,
        .recommended_profiles = {"H.264 High"},
        .max_bitrate_kbps = 6000,      // 1080p60 max
        .hdr_support = false
    });
    
    // Apple TV+
    platforms.push_back({
        .platform_name = "Apple TV+",
        .supports_av1 = true,
        .supports_hevc_10bit = true,
        .supports_vp9 = false,
        .recommended_profiles = {"HEVC Main 10", "AV1 Main"},
        .max_bitrate_kbps = 41000,     // 4K Dolby Vision
        .hdr_support = true
    });
    
    // Amazon Prime Video
    platforms.push_back({
        .platform_name = "Amazon Prime Video", 
        .supports_av1 = true,
        .supports_hevc_10bit = true,
        .supports_vp9 = true,
        .recommended_profiles = {"HEVC Main 10", "AV1 Main", "VP9 Profile 2"},
        .max_bitrate_kbps = 35000,     // 4K HDR
        .hdr_support = true
    });
    
    return platforms;
}

std::vector<ModernCodecFormatIntegration::HardwareVendorSupport>
ModernCodecFormatIntegration::get_hardware_vendor_support() {
    
    std::vector<HardwareVendorSupport> vendors;
    
    // Intel
    vendors.push_back({
        .vendor = HardwareVendor::INTEL,
        .vendor_name = "Intel QuickSync Video",
        .av1_decode = true,            // Arc/Xe graphics
        .av1_encode = true,
        .hevc_10bit_decode = true,     // Gen9+ 
        .hevc_10bit_encode = true,
        .vp9_decode = true,            // Gen9+
        .vp9_encode = true,
        .supported_resolutions = {"1080p", "4K", "8K (limited)"}
    });
    
    // NVIDIA
    vendors.push_back({
        .vendor = HardwareVendor::NVIDIA,
        .vendor_name = "NVIDIA NVENC/NVDEC",
        .av1_decode = true,            // RTX 40 series
        .av1_encode = true,            // RTX 40 series
        .hevc_10bit_decode = true,     // Maxwell+
        .hevc_10bit_encode = true,     // Pascal+
        .vp9_decode = true,            // Maxwell+
        .vp9_encode = false,           // Limited VP9 encode
        .supported_resolutions = {"1080p", "4K", "8K"}
    });
    
    // AMD
    vendors.push_back({
        .vendor = HardwareVendor::AMD,
        .vendor_name = "AMD VCE/VCN",
        .av1_decode = true,            // RDNA2+
        .av1_encode = true,            // RDNA3+
        .hevc_10bit_decode = true,     // GCN4+
        .hevc_10bit_encode = true,     // GCN4+
        .vp9_decode = false,           // Limited VP9 support
        .vp9_encode = false,
        .supported_resolutions = {"1080p", "4K"}
    });
    
    // Apple Silicon
    vendors.push_back({
        .vendor = HardwareVendor::APPLE,
        .vendor_name = "Apple VideoToolbox",
        .av1_decode = true,            // M3+
        .av1_encode = false,           // Encode coming
        .hevc_10bit_decode = true,     // All Apple Silicon
        .hevc_10bit_encode = true,     // All Apple Silicon
        .vp9_decode = false,           // Limited support
        .vp9_encode = false,
        .supported_resolutions = {"1080p", "4K", "8K"}
    });
    
    return vendors;
}

} // namespace ve::media_io
