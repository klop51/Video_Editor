#include "media_io/prores_support.hpp"
#include "media_io/format_detector.hpp"
#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace ve::media_io {

std::optional<ProResInfo> ProResDetector::detect_prores_profile(
    const std::string& fourcc,
    const std::vector<uint8_t>& codec_data) {
    
    ProResInfo info;
    info.fourcc = fourcc;
    info.profile = fourcc_to_profile(fourcc);
    
    if (info.profile == ProResProfile::UNKNOWN) {
        return std::nullopt;
    }
    
    // Set profile-specific defaults
    switch (info.profile) {
        case ProResProfile::PROXY:
            info.profile_name = "Apple ProRes 422 Proxy";
            info.target_bitrate_mbps = 45;
            info.bit_depth = 10;
            info.has_alpha = false;
            break;
        case ProResProfile::LT:
            info.profile_name = "Apple ProRes 422 LT";
            info.target_bitrate_mbps = 102;
            info.bit_depth = 10;
            info.has_alpha = false;
            break;
        case ProResProfile::STANDARD:
            info.profile_name = "Apple ProRes 422";
            info.target_bitrate_mbps = 147;
            info.bit_depth = 10;
            info.has_alpha = false;
            break;
        case ProResProfile::HQ:
            info.profile_name = "Apple ProRes 422 HQ";
            info.target_bitrate_mbps = 220;
            info.bit_depth = 10;
            info.has_alpha = false;
            break;
        case ProResProfile::FOUR444:
            info.profile_name = "Apple ProRes 4444";
            info.target_bitrate_mbps = 330;
            info.bit_depth = 12;
            info.has_alpha = true;
            break;
        case ProResProfile::FOUR444_XQ:
            info.profile_name = "Apple ProRes 4444 XQ";
            info.target_bitrate_mbps = 500;
            info.bit_depth = 12;
            info.has_alpha = true;
            break;
        default:
            return std::nullopt;
    }
    
    // Detect color space from metadata
    info.color_space = detect_color_space(codec_data);
    
    // Set default technical specs (would be read from actual file in real implementation)
    info.width = 1920;
    info.height = 1080;
    info.framerate_num = 24;
    info.framerate_den = 1;
    
    return info;
}

bool ProResDetector::validate_prores_compatibility(const ProResInfo& prores_info) {
    // Validate resolution
    if (!validate_resolution(prores_info.width, prores_info.height)) {
        return false;
    }
    
    // Validate framerate
    if (!validate_framerate(prores_info.framerate_num, prores_info.framerate_den)) {
        return false;
    }
    
    // Validate bitrate is reasonable for profile
    if (!validate_bitrate(prores_info.actual_bitrate_mbps, prores_info.profile)) {
        return false;
    }
    
    return true;
}

ProResDetector::DecodeSettings ProResDetector::get_optimal_decode_settings(ProResProfile profile) {
    DecodeSettings settings;
    
    switch (profile) {
        case ProResProfile::PROXY:
        case ProResProfile::LT:
            settings.use_hardware_acceleration = true;
            settings.target_pixel_format = ve::decode::PixelFormat::YUV422P;
            settings.decode_threads = 2;
            break;
            
        case ProResProfile::STANDARD:
        case ProResProfile::HQ:
            settings.use_hardware_acceleration = true;
            settings.target_pixel_format = ve::decode::PixelFormat::YUV422P10LE;
            settings.decode_threads = 4;
            break;
            
        case ProResProfile::FOUR444:
        case ProResProfile::FOUR444_XQ:
            settings.use_hardware_acceleration = true;
            settings.enable_alpha_channel = true;
            settings.target_pixel_format = ve::decode::PixelFormat::YUVA444P16LE;
            settings.decode_threads = 6;
            break;
            
        default:
            break;
    }
    
    return settings;
}

ProResDetector::PerformanceRequirements ProResDetector::estimate_performance_requirements(
    const ProResInfo& prores_info) {
    
    PerformanceRequirements reqs;
    
    // Calculate memory requirements
    reqs.memory_mb_per_frame = calculate_frame_memory_mb(
        prores_info.width, 
        prores_info.height, 
        prores_info.bit_depth,
        prores_info.has_alpha
    );
    
    // Estimate decode complexity
    [[maybe_unused]] float complexity = estimate_decode_complexity(prores_info.profile);
    
    // Set CPU requirements based on profile
    switch (prores_info.profile) {
        case ProResProfile::PROXY:
        case ProResProfile::LT:
            reqs.cpu_threads_recommended = 2;
            reqs.real_time_factor = 2.0f; // Can decode faster than real-time
            break;
        case ProResProfile::STANDARD:
        case ProResProfile::HQ:
            reqs.cpu_threads_recommended = 4;
            reqs.real_time_factor = 1.5f;
            break;
        case ProResProfile::FOUR444:
        case ProResProfile::FOUR444_XQ:
            reqs.cpu_threads_recommended = 6;
            reqs.real_time_factor = 1.0f; // Real-time decode
            reqs.requires_hardware_decode = true;
            break;
        default:
            break;
    }
    
    // GPU memory for hardware acceleration
    if (prores_info.width >= 3840) { // 4K+
        reqs.gpu_memory_mb = 512.0f;
    } else {
        reqs.gpu_memory_mb = 256.0f;
    }
    
    return reqs;
}

std::vector<std::pair<ProResProfile, bool>> ProResDetector::get_supported_profiles() {
    return {
        {ProResProfile::PROXY, false},      // Decode only (licensing restrictions)
        {ProResProfile::LT, false},
        {ProResProfile::STANDARD, false},
        {ProResProfile::HQ, false},
        {ProResProfile::FOUR444, false},
        {ProResProfile::FOUR444_XQ, false}
    };
}

std::string ProResDetector::profile_to_string(ProResProfile profile) {
    switch (profile) {
        case ProResProfile::PROXY: return "ProRes 422 Proxy";
        case ProResProfile::LT: return "ProRes 422 LT";
        case ProResProfile::STANDARD: return "ProRes 422";
        case ProResProfile::HQ: return "ProRes 422 HQ";
        case ProResProfile::FOUR444: return "ProRes 4444";
        case ProResProfile::FOUR444_XQ: return "ProRes 4444 XQ";
        default: return "Unknown ProRes";
    }
}

uint32_t ProResDetector::get_target_bitrate_mbps(
    ProResProfile profile,
    uint32_t width,
    uint32_t height,
    uint32_t framerate) {
    
    // Base bitrates for 1080p24
    uint32_t base_bitrate = 0;
    switch (profile) {
        case ProResProfile::PROXY: base_bitrate = 45; break;
        case ProResProfile::LT: base_bitrate = 102; break;
        case ProResProfile::STANDARD: base_bitrate = 147; break;
        case ProResProfile::HQ: base_bitrate = 220; break;
        case ProResProfile::FOUR444: base_bitrate = 330; break;
        case ProResProfile::FOUR444_XQ: base_bitrate = 500; break;
        default: return 0;
    }
    
    // Scale by resolution
    float resolution_factor = (static_cast<float>(width) * static_cast<float>(height)) / (1920.0f * 1080.0f);
    
    // Scale by framerate
    float framerate_factor = static_cast<float>(framerate) / 24.0f;
    
    return static_cast<uint32_t>(static_cast<float>(base_bitrate) * resolution_factor * framerate_factor);
}

bool ProResDetector::supports_alpha_channel(ProResProfile profile) {
    return profile == ProResProfile::FOUR444 || profile == ProResProfile::FOUR444_XQ;
}

ve::decode::PixelFormat ProResDetector::get_recommended_pixel_format(ProResProfile profile) {
    switch (profile) {
        case ProResProfile::PROXY:
        case ProResProfile::LT:
            return ve::decode::PixelFormat::YUV422P;
        case ProResProfile::STANDARD:
        case ProResProfile::HQ:
            return ve::decode::PixelFormat::YUV422P10LE;
        case ProResProfile::FOUR444:
        case ProResProfile::FOUR444_XQ:
            return ve::decode::PixelFormat::YUVA444P16LE;
        default:
            return ve::decode::PixelFormat::YUV422P10LE;
    }
}

// Private helper methods
ProResProfile ProResDetector::fourcc_to_profile(const std::string& fourcc) {
    static const std::unordered_map<std::string, ProResProfile> fourcc_map = {
        {"apco", ProResProfile::PROXY},
        {"apcs", ProResProfile::LT},
        {"apcn", ProResProfile::STANDARD},
        {"apch", ProResProfile::HQ},
        {"ap4h", ProResProfile::FOUR444},
        {"ap4x", ProResProfile::FOUR444_XQ}
    };
    
    auto it = fourcc_map.find(fourcc);
    return (it != fourcc_map.end()) ? it->second : ProResProfile::UNKNOWN;
}

bool ProResDetector::validate_resolution(uint32_t width, uint32_t height) {
    // ProRes supports wide range of resolutions
    return width >= 320 && width <= 8192 && height >= 240 && height <= 4320;
}

bool ProResDetector::validate_framerate(uint32_t num, uint32_t den) {
    if (den == 0) return false;
    float fps = static_cast<float>(num) / static_cast<float>(den);
    return fps >= 1.0f && fps <= 120.0f;
}

bool ProResDetector::validate_bitrate(uint32_t bitrate_mbps, ProResProfile profile) {
    uint32_t expected = get_target_bitrate_mbps(profile, 1920, 1080, 24);
    // Allow 50% variance from expected bitrate
    return static_cast<float>(bitrate_mbps) >= static_cast<float>(expected) * 0.5f && static_cast<float>(bitrate_mbps) <= static_cast<float>(expected) * 1.5f;
}

ProResColorSpace ProResDetector::detect_color_space([[maybe_unused]] const std::vector<uint8_t>& metadata) {
    // In real implementation, would parse actual metadata
    // For now, default to Rec.709
    return ProResColorSpace::REC709;
}

uint64_t ProResDetector::calculate_frame_memory_mb(
    uint32_t width, uint32_t height, uint8_t bit_depth, bool has_alpha) {
    
    uint64_t pixels = static_cast<uint64_t>(width) * height;
    uint64_t bits_per_pixel = bit_depth;
    
    if (has_alpha) {
        bits_per_pixel += bit_depth; // Add alpha channel
    }
    
    // YUV 4:2:2 or 4:4:4 sampling
    uint64_t total_bits = pixels * bits_per_pixel * (has_alpha ? 4 : 3);
    uint64_t bytes = total_bits / 8;
    uint64_t mb = bytes / (1024 * 1024);
    
    return std::max(mb, static_cast<uint64_t>(1)); // Minimum 1MB
}

float ProResDetector::estimate_decode_complexity(ProResProfile profile) {
    switch (profile) {
        case ProResProfile::PROXY: return 1.0f;
        case ProResProfile::LT: return 1.2f;
        case ProResProfile::STANDARD: return 1.5f;
        case ProResProfile::HQ: return 2.0f;
        case ProResProfile::FOUR444: return 3.0f;
        case ProResProfile::FOUR444_XQ: return 4.0f;
        default: return 1.0f;
    }
}

// ProRes Format Integration
void ProResFormatIntegration::register_prores_capabilities(FormatDetector& detector) {
    // Register all ProRes profiles with their capabilities
    auto profiles = ProResDetector::get_supported_profiles();
    
    for (const auto& profile_info : profiles) {
        ProResProfile profile = profile_info.first;
        bool encode_support = profile_info.second;
        
        FormatCapability capability;
        capability.supports_decode = true;
        capability.supports_encode = encode_support;
        capability.hardware_accelerated = true;
        capability.real_time_capable = true;
        capability.max_width = 8192;
        capability.max_height = 4320;
        capability.max_framerate = 120;
        capability.max_bit_depth = ProResDetector::supports_alpha_channel(profile) ? 12 : 10;
        capability.supports_alpha = ProResDetector::supports_alpha_channel(profile);
        capability.supports_hdr = true;
        capability.supports_timecode = true;
        capability.supports_metadata = true;
        capability.supports_multitrack_audio = true;
        
        detector.register_format_capability(CodecFamily::PRORES, ContainerType::MOV, capability);
    }
}

DetectedFormat ProResFormatIntegration::create_prores_detected_format(const ProResInfo& prores_info) {
    DetectedFormat format;
    format.codec = CodecFamily::PRORES;
    format.container = ContainerType::MOV;
    format.width = prores_info.width;
    format.height = prores_info.height;
    format.framerate_num = prores_info.framerate_num;
    format.framerate_den = prores_info.framerate_den;
    format.bit_depth = prores_info.bit_depth;
    format.profile_name = prores_info.profile_name;
    format.pixel_format = ProResDetector::get_recommended_pixel_format(prores_info.profile);
    
    // Set color space
    switch (prores_info.color_space) {
        case ProResColorSpace::REC709:
            format.color_space = ve::decode::ColorSpace::BT709;
            break;
        case ProResColorSpace::REC2020:
            format.color_space = ve::decode::ColorSpace::BT2020;
            break;
        case ProResColorSpace::P3_D65:
            format.color_space = ve::decode::ColorSpace::DCI_P3;
            break;
        default:
            format.color_space = ve::decode::ColorSpace::BT709;
            break;
    }
    
    return format;
}

ProResFormatIntegration::WorkflowRecommendations ProResFormatIntegration::validate_prores_workflow(
    const DetectedFormat& detected_format) {
    
    WorkflowRecommendations recommendations;
    
    // ProRes is professional format - high score
    recommendations.professional_score = 0.95f;
    recommendations.real_time_capable = true;
    
    // Add specific recommendations based on profile
    if (detected_format.profile_name.find("Proxy") != std::string::npos) {
        recommendations.recommendations.push_back("ProRes Proxy detected - ideal for offline editing workflows");
        recommendations.recommendations.push_back("Consider using higher quality ProRes for final delivery");
    } else if (detected_format.profile_name.find("4444") != std::string::npos) {
        recommendations.recommendations.push_back("ProRes 4444 detected - premium quality with alpha channel support");
        recommendations.recommendations.push_back("Ensure sufficient storage and processing power for 4444 workflows");
    } else if (detected_format.profile_name.find("HQ") != std::string::npos) {
        recommendations.recommendations.push_back("ProRes HQ detected - excellent for finishing and color grading");
    }
    
    // Resolution-specific recommendations
    if (detected_format.width >= 3840) {
        recommendations.recommendations.push_back("4K+ ProRes detected - enable hardware acceleration for optimal performance");
        recommendations.warnings.push_back("Large ProRes files may require significant storage space");
    }
    
    return recommendations;
}

// Utility functions
namespace prores_utils {

bool is_prores_extension(const std::string& extension) {
    std::string lower_ext = extension;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), 
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    
    return lower_ext == "mov" || lower_ext == "qt";
}

std::vector<std::string> get_prores_fourccs() {
    return {"apco", "apcs", "apcn", "apch", "ap4h", "ap4x"};
}

uint64_t estimate_file_size_mb(
    ProResProfile profile,
    uint32_t width,
    uint32_t height,
    uint32_t framerate,
    uint32_t duration_seconds) {
    
    uint32_t bitrate_mbps = ProResDetector::get_target_bitrate_mbps(profile, width, height, framerate);
    uint64_t total_mb = static_cast<uint64_t>(bitrate_mbps) * duration_seconds / 8; // Convert Mbps to MB
    return total_mb;
}

std::vector<CameraCompatibility> get_camera_compatibility_matrix() {
    return {
        {"Apple", {"ProRes 422 Proxy", "ProRes 422", "ProRes 422 HQ"}, "iPhone 13 Pro and later"},
        {"Canon", {"ProRes 422", "ProRes 422 HQ", "ProRes 4444"}, "C70, R5C, R7 with firmware updates"},
        {"RED", {"ProRes 422 HQ", "ProRes 4444", "ProRes 4444 XQ"}, "Via RED Control Room software"},
        {"ARRI", {"ProRes 422", "ProRes 422 HQ", "ProRes 4444"}, "Alexa Mini, Alexa 35 native recording"},
        {"Blackmagic", {"ProRes 422", "ProRes 422 HQ", "ProRes 4444"}, "URSA, Pocket Cinema cameras"},
        {"Sony", {"ProRes 422", "ProRes 422 HQ"}, "FX6, FX9 with optional licenses"}
    };
}

std::vector<ConversionRecommendation> get_conversion_recommendations(ProResProfile source_profile) {
    std::vector<ConversionRecommendation> recommendations;
    
    switch (source_profile) {
        case ProResProfile::PROXY:
            recommendations.push_back({"H.264", "Smaller file size for delivery", 0.9f, 0.3f});
            recommendations.push_back({"DNxHD 120", "Broadcast compatibility", 0.95f, 0.8f});
            break;
        case ProResProfile::HQ:
            recommendations.push_back({"DNxHR HQ", "Avid compatibility", 0.98f, 0.9f});
            recommendations.push_back({"H.265", "Modern delivery codec", 0.92f, 0.4f});
            break;
        case ProResProfile::FOUR444:
            recommendations.push_back({"DNxHR 444", "Alternative professional codec", 0.99f, 1.1f});
            recommendations.push_back({"Uncompressed", "Maximum quality retention", 1.0f, 5.0f});
            break;
        default:
            break;
    }
    
    return recommendations;
}

} // namespace prores_utils

} // namespace ve::media_io
