#include "media_io/modern_codec_support.hpp"
#include <algorithm>
#include <map>
#include <cstring>

/**
 * Modern Codec Support Implementation for Phase 1 Week 4
 * FORMAT_SUPPORT_ROADMAP.md - Modern codec integration
 * 
 * Implementation of AV1, HEVC 10/12-bit, and VP9 support
 * with hardware acceleration and streaming optimization
 */

namespace ve::media_io {

namespace {
    // AV1 FourCC constants
    constexpr uint32_t AV1_FOURCC = 0x31305641; // 'AV01'
    constexpr uint32_t AV1_CODEC_FOURCC = 0x31565641; // 'AV1' followed by version
    
    // HEVC FourCC constants  
    constexpr uint32_t HEVC_FOURCC = 0x43564548; // 'HEVC'
    constexpr uint32_t H265_FOURCC = 0x35363248; // 'H265'
    constexpr uint32_t HVC1_FOURCC = 0x31435648; // 'HVC1'
    constexpr uint32_t HEV1_FOURCC = 0x31564548; // 'HEV1'
    
    // VP9 FourCC constants
    constexpr uint32_t VP9_FOURCC = 0x30395056; // 'VP90'
    constexpr uint32_t VP90_FOURCC = 0x30395056; // 'VP90'
    
    // Hardware vendor detection helpers
    bool is_intel_gpu_available() {
        // Simplified: In real implementation, query DirectX/OpenGL/Vulkan
        return true; // Assume Intel iGPU available on most systems
    }
    
    bool is_nvidia_gpu_available() {
        // Simplified: Check for NVIDIA GPU presence
        return false; // Conservative assumption
    }
    
    bool is_amd_gpu_available() {
        // Simplified: Check for AMD GPU presence  
        return false; // Conservative assumption
    }
    
    // Profile detection from codec data
    uint8_t extract_profile_from_data(const std::vector<uint8_t>& codec_data) {
        if (codec_data.size() < 4) return 0;
        return codec_data[1]; // Simplified profile extraction
    }
    
    uint8_t extract_level_from_data(const std::vector<uint8_t>& codec_data) {
        if (codec_data.size() < 4) return 0;
        return codec_data[3]; // Simplified level extraction
    }
    
    // Compression efficiency lookup tables
    const std::map<CodecFamily, float> compression_efficiency_map = {
        {CodecFamily::AV1, 2.5f},      // AV1 ~40% better than HEVC
        {CodecFamily::HEVC, 2.0f},     // HEVC ~50% better than H.264
        {CodecFamily::VP9, 1.8f},      // VP9 ~45% better than H.264
        {CodecFamily::H264, 1.0f}      // H.264 baseline
    };
    
    // Hardware support matrix
    const std::map<std::pair<CodecFamily, HardwareVendor>, bool> hw_support_matrix = {
        {{CodecFamily::AV1, HardwareVendor::INTEL}, true},     // Intel Arc/Xe
        {{CodecFamily::AV1, HardwareVendor::AMD}, true},       // RDNA2+
        {{CodecFamily::AV1, HardwareVendor::NVIDIA}, true},    // RTX 40 series
        {{CodecFamily::HEVC, HardwareVendor::INTEL}, true},    // Intel QuickSync
        {{CodecFamily::HEVC, HardwareVendor::AMD}, true},      // AMD VCE
        {{CodecFamily::HEVC, HardwareVendor::NVIDIA}, true},   // NVIDIA NVENC
        {{CodecFamily::VP9, HardwareVendor::INTEL}, true},     // Intel Gen9+
        {{CodecFamily::VP9, HardwareVendor::AMD}, false},      // Limited AMD support
        {{CodecFamily::VP9, HardwareVendor::NVIDIA}, true}     // NVIDIA Maxwell+
    };
}

ModernCodecInfo ModernCodecDetector::detect_modern_codec(const uint8_t* container_data,
                                                         size_t data_size,
                                                         CodecFamily codec_hint) {
    ModernCodecInfo info;
    
    if (!container_data || data_size < 8) {
        return info;
    }
    
    // Read potential FourCC from container
    uint32_t fourcc = 0;
    std::memcpy(&fourcc, container_data, 4);
    
    // Detect codec family
    if (fourcc == AV1_FOURCC || fourcc == AV1_CODEC_FOURCC || codec_hint == CodecFamily::AV1) {
        info.codec_family = CodecFamily::AV1;
        info.av1_profile = detect_av1_profile(std::vector<uint8_t>(container_data + 4, 
                                                                   container_data + std::min(data_size, size_t(20))));
    }
    else if (fourcc == HEVC_FOURCC || fourcc == H265_FOURCC || 
             fourcc == HVC1_FOURCC || fourcc == HEV1_FOURCC || 
             codec_hint == CodecFamily::HEVC) {
        info.codec_family = CodecFamily::HEVC;
        info.hevc_profile = detect_hevc_profile(std::vector<uint8_t>(container_data + 4,
                                                                     container_data + std::min(data_size, size_t(20))));
    }
    else if (fourcc == VP9_FOURCC || fourcc == VP90_FOURCC || codec_hint == CodecFamily::VP9) {
        info.codec_family = CodecFamily::VP9;
        info.vp9_profile = detect_vp9_profile(std::vector<uint8_t>(container_data + 4,
                                                                   container_data + std::min(data_size, size_t(20))));
    }
    
    // Extract basic stream information
    if (data_size >= 16) {
        // Simplified extraction - real implementation would parse headers properly
        info.width = static_cast<uint32_t>((container_data[8] << 8) | container_data[9]);
        info.height = static_cast<uint32_t>((container_data[10] << 8) | container_data[11]); 
        info.bit_depth = (container_data[12] & 0x0F) + 8; // 8-12 bit range
        info.supports_alpha = (container_data[13] & 0x01) != 0;
        info.is_hdr = info.bit_depth > 8;
    }
    
    // Set compression efficiency
    auto eff_it = compression_efficiency_map.find(info.codec_family);
    if (eff_it != compression_efficiency_map.end()) {
        info.compression_efficiency = eff_it->second;
    }
    
    // Detect hardware acceleration
    info = detect_hardware_acceleration(info);
    
    // Set streaming and archival quality estimates
    switch (info.codec_family) {
        case CodecFamily::AV1:
            info.streaming_suitability = 0.95f; // Excellent for streaming
            info.archival_quality = 0.90f;      // Excellent for archival
            break;
        case CodecFamily::HEVC:
            info.streaming_suitability = 0.85f; // Very good for streaming
            info.archival_quality = 0.95f;      // Excellent for archival
            break;
        case CodecFamily::VP9:
            info.streaming_suitability = 0.90f; // Excellent for web streaming
            info.archival_quality = 0.80f;      // Good for archival
            break;
        default:
            info.streaming_suitability = 0.50f;
            info.archival_quality = 0.50f;
            break;
    }
    
    // Set real-time capability based on hardware acceleration
    info.real_time_capable = info.hw_acceleration_available || info.bit_depth <= 8;
    
    return info;
}

ModernCodecInfo ModernCodecDetector::detect_hardware_acceleration(const ModernCodecInfo& codec_info) {
    ModernCodecInfo info = codec_info;
    
    // Detect available hardware vendor
    info.hw_vendor = detect_available_hardware();
    
    // Check if codec is supported by detected hardware
    auto support_key = std::make_pair(info.codec_family, info.hw_vendor);
    auto it = hw_support_matrix.find(support_key);
    
    if (it != hw_support_matrix.end()) {
        info.hw_acceleration_available = it->second;
    }
    
    // Determine if hardware acceleration is required for real-time playback
    if (info.width >= 3840 || info.height >= 2160) { // 4K+
        info.hw_acceleration_required = true;
    }
    else if (info.bit_depth > 8) { // HDR content
        info.hw_acceleration_required = (info.width >= 1920 && info.height >= 1080);
    }
    
    return info;
}

ModernCodecDecodeSettings ModernCodecDetector::get_decode_settings(const ModernCodecInfo& codec_info) {
    ModernCodecDecodeSettings settings;
    
    // Hardware acceleration preferences
    settings.prefer_hardware_acceleration = codec_info.hw_acceleration_available;
    settings.fallback_to_software = !codec_info.hw_acceleration_required;
    settings.preferred_hw_vendor = codec_info.hw_vendor;
    
    // Thread settings based on codec and resolution
    if (codec_info.width >= 3840) { // 4K
        settings.decode_threads = 8;
        settings.frame_buffer_count = 6;
    }
    else if (codec_info.width >= 1920) { // 1080p
        settings.decode_threads = 4;
        settings.frame_buffer_count = 4;
    }
    else { // Lower resolutions
        settings.decode_threads = 2;
        settings.frame_buffer_count = 3;
    }
    
    // Codec-specific optimizations
    switch (codec_info.codec_family) {
        case CodecFamily::AV1:
            settings.enable_frame_threading = true;
            settings.enable_parallel_processing = true;
            settings.max_decode_delay_frames = 12; // AV1 can have higher delay
            break;
            
        case CodecFamily::HEVC:
            settings.enable_frame_threading = true;
            settings.enable_parallel_processing = true;
            settings.max_decode_delay_frames = 6;
            break;
            
        case CodecFamily::VP9:
            settings.enable_frame_threading = codec_info.width >= 1920;
            settings.enable_parallel_processing = true;
            settings.max_decode_delay_frames = 4;
            break;
            
        default:
            break;
    }
    
    // HDR and high bit-depth settings
    if (codec_info.is_hdr) {
        settings.preserve_hdr_metadata = true;
        settings.target_pixel_format = ve::decode::PixelFormat::YUV420P10LE;
    }
    
    // Memory optimization for modern codecs
    settings.enable_zero_copy = codec_info.hw_acceleration_available;
    settings.enable_memory_pooling = true;
    
    return settings;
}

ModernCodecPerformanceRequirements ModernCodecDetector::estimate_performance_requirements(
    const ModernCodecInfo& codec_info) {
    
    ModernCodecPerformanceRequirements req;
    
    // Base requirements calculation
    uint64_t pixel_count = static_cast<uint64_t>(codec_info.width) * codec_info.height;
    uint32_t bytes_per_pixel = (codec_info.bit_depth + 7) / 8;
    
    // Memory requirements
    req.frame_memory_mb = (pixel_count * bytes_per_pixel * 3) / (1024 * 1024); // YUV
    req.decode_memory_mb = req.frame_memory_mb * 4; // Multiple frame buffers
    req.total_memory_mb = req.decode_memory_mb + 256; // Additional overhead
    
    // CPU requirements
    uint64_t complexity = estimate_decode_complexity(codec_info);
    
    if (complexity > 1000000) { // High complexity
        req.recommended_cores = 8;
        req.recommended_threads = 16;
        req.cpu_usage_estimate = 0.8f;
    }
    else if (complexity > 500000) { // Medium complexity
        req.recommended_cores = 4;
        req.recommended_threads = 8;
        req.cpu_usage_estimate = 0.6f;
    }
    else { // Low complexity
        req.recommended_cores = 2;
        req.recommended_threads = 4;
        req.cpu_usage_estimate = 0.4f;
    }
    
    // Hardware acceleration adjustments
    if (codec_info.hw_acceleration_available) {
        req.cpu_usage_estimate *= 0.3f; // Significant CPU reduction
        req.gpu_memory_mb = req.frame_memory_mb * 2;
        req.gpu_usage_estimate = 0.4f;
        req.real_time_factor = 2.0f; // Can decode faster than real-time
    }
    else {
        req.real_time_factor = codec_info.codec_family == CodecFamily::AV1 ? 0.5f : 0.8f;
    }
    
    // Required vs recommended hardware acceleration
    req.hardware_acceleration_required = codec_info.hw_acceleration_required;
    req.software_fallback_viable = !codec_info.hw_acceleration_required;
    req.requires_modern_gpu = requires_modern_hardware(codec_info);
    
    // Bandwidth requirements (estimated from resolution and codec efficiency)
    req.bandwidth_kbps = static_cast<uint64_t>(static_cast<double>(pixel_count) * 0.1 / codec_info.compression_efficiency);
    req.adaptive_streaming_capable = (codec_info.codec_family == CodecFamily::AV1 || 
                                     codec_info.codec_family == CodecFamily::VP9);
    
    return req;
}

bool ModernCodecDetector::validate_streaming_compatibility(const ModernCodecInfo& codec_info,
                                                          uint32_t target_bandwidth_kbps) {
    // Check if codec is suitable for streaming at target bandwidth
    if (codec_info.streaming_suitability < 0.7f) {
        return false;
    }
    
    // Estimate required bandwidth based on resolution and efficiency
    uint64_t estimated_bandwidth = static_cast<uint64_t>(
        static_cast<double>(codec_info.width) * static_cast<double>(codec_info.height) * 0.1 / codec_info.compression_efficiency
    );
    
    return estimated_bandwidth <= target_bandwidth_kbps;
}

float ModernCodecDetector::get_compression_efficiency(const ModernCodecInfo& codec_info) {
    return codec_info.compression_efficiency;
}

bool ModernCodecDetector::supports_hdr_workflows(const ModernCodecInfo& codec_info) {
    return codec_info.is_hdr && codec_info.bit_depth >= 10;
}

ve::decode::PixelFormat ModernCodecDetector::get_recommended_pixel_format(const ModernCodecInfo& codec_info) {
    if (codec_info.bit_depth >= 12) {
        return ve::decode::PixelFormat::YUV420P12LE;
    }
    else if (codec_info.bit_depth >= 10) {
        return ve::decode::PixelFormat::YUV420P10LE;
    }
    else {
        return ve::decode::PixelFormat::YUV420P;
    }
}

std::vector<std::pair<std::string, bool>> ModernCodecDetector::get_supported_modern_codecs() {
    std::vector<std::pair<std::string, bool>> codecs;
    
    // AV1 support
    bool av1_hw = check_codec_hw_support(CodecFamily::AV1, detect_available_hardware());
    codecs.emplace_back("AV1", av1_hw);
    
    // HEVC support  
    bool hevc_hw = check_codec_hw_support(CodecFamily::HEVC, detect_available_hardware());
    codecs.emplace_back("HEVC/H.265", hevc_hw);
    
    // VP9 support
    bool vp9_hw = check_codec_hw_support(CodecFamily::VP9, detect_available_hardware());
    codecs.emplace_back("VP9", vp9_hw);
    
    return codecs;
}

// Private helper implementations
AV1Profile ModernCodecDetector::detect_av1_profile(const std::vector<uint8_t>& codec_data) {
    if (codec_data.empty()) return AV1Profile::MAIN;
    
    uint8_t profile = extract_profile_from_data(codec_data);
    
    switch (profile) {
        case 0: return AV1Profile::MAIN;
        case 1: return AV1Profile::HIGH;
        case 2: return AV1Profile::PROFESSIONAL;
        default: return AV1Profile::MAIN;
    }
}

HEVCProfile ModernCodecDetector::detect_hevc_profile(const std::vector<uint8_t>& codec_data) {
    if (codec_data.empty()) return HEVCProfile::MAIN;
    
    uint8_t profile = extract_profile_from_data(codec_data);
    
    switch (profile) {
        case 1: return HEVCProfile::MAIN;
        case 2: return HEVCProfile::MAIN10;
        case 3: return HEVCProfile::MAIN444;
        case 4: return HEVCProfile::MAIN12;
        case 5: return HEVCProfile::MAIN444_10;
        case 6: return HEVCProfile::MAIN444_12;
        default: return HEVCProfile::MAIN;
    }
}

VP9Profile ModernCodecDetector::detect_vp9_profile(const std::vector<uint8_t>& codec_data) {
    if (codec_data.empty()) return VP9Profile::PROFILE_0;
    
    uint8_t profile = extract_profile_from_data(codec_data);
    
    switch (profile) {
        case 0: return VP9Profile::PROFILE_0;
        case 1: return VP9Profile::PROFILE_1;
        case 2: return VP9Profile::PROFILE_2;
        case 3: return VP9Profile::PROFILE_3;
        default: return VP9Profile::PROFILE_0;
    }
}

HardwareVendor ModernCodecDetector::detect_available_hardware() {
    // Priority order: NVIDIA, Intel, AMD, Software
    if (is_nvidia_gpu_available()) {
        return HardwareVendor::NVIDIA;
    }
    else if (is_intel_gpu_available()) {
        return HardwareVendor::INTEL;
    }
    else if (is_amd_gpu_available()) {
        return HardwareVendor::AMD;
    }
    else {
        return HardwareVendor::SOFTWARE;
    }
}

bool ModernCodecDetector::check_codec_hw_support(CodecFamily codec, HardwareVendor vendor) {
    auto key = std::make_pair(codec, vendor);
    auto it = hw_support_matrix.find(key);
    return it != hw_support_matrix.end() && it->second;
}

uint64_t ModernCodecDetector::estimate_decode_complexity(const ModernCodecInfo& codec_info) {
    uint64_t base_complexity = static_cast<uint64_t>(codec_info.width) * codec_info.height;
    
    // Codec complexity multipliers
    float codec_multiplier = 1.0f;
    switch (codec_info.codec_family) {
        case CodecFamily::AV1: codec_multiplier = 3.0f; break;      // AV1 is computationally intensive
        case CodecFamily::HEVC: codec_multiplier = 2.0f; break;     // HEVC moderate complexity
        case CodecFamily::VP9: codec_multiplier = 2.5f; break;      // VP9 moderate-high complexity
        default: codec_multiplier = 1.0f; break;
    }
    
    // Bit depth multiplier
    float depth_multiplier = (static_cast<float>(codec_info.bit_depth) / 8.0f);
    
    return static_cast<uint64_t>(static_cast<double>(base_complexity) * static_cast<double>(codec_multiplier) * static_cast<double>(depth_multiplier));
}

float ModernCodecDetector::calculate_bandwidth_efficiency(const ModernCodecInfo& codec_info) {
    return codec_info.compression_efficiency;
}

bool ModernCodecDetector::requires_modern_hardware(const ModernCodecInfo& codec_info) {
    // AV1 benefits significantly from modern hardware
    if (codec_info.codec_family == CodecFamily::AV1) {
        return codec_info.width >= 1920; // 1080p+ AV1 benefits from modern hardware
    }
    
    // 4K+ generally requires modern hardware for real-time decode
    return (codec_info.width >= 3840 && codec_info.height >= 2160);
}

} // namespace ve::media_io
