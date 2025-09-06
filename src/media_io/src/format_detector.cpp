#include "media_io/format_detector.hpp"
#include "media_io/prores_support.hpp"  // Phase 1 Week 2: ProRes support
#include "media_io/dnxhd_support.hpp"   // Phase 1 Week 3: DNxHD/DNxHR support
#include "media_io/modern_codec_support.hpp" // Phase 1 Week 4: Modern codec support
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cctype>

namespace ve::media_io {

FormatDetector::FormatDetector() 
    : hdr_infrastructure_(std::make_unique<HDRInfrastructure>()) { // Phase 2 Week 5: HDR infrastructure
    initialize_professional_capabilities();
    initialize_broadcast_capabilities();
    initialize_modern_codec_capabilities();
    
    // Phase 1 Week 2: Register ProRes capabilities
    ProResFormatIntegration::register_prores_capabilities(*this);
    
    // Phase 1 Week 3: Register DNx capabilities
    DNxFormatIntegration::register_dnx_capabilities(*this);
    
    // Phase 1 Week 4: Register modern codec capabilities
    ModernCodecFormatIntegration::register_modern_codec_capabilities(*this);
}

std::optional<DetectedFormat> FormatDetector::detect_file_format(const std::string& file_path) {
    // Extract file extension
    size_t dot_pos = file_path.find_last_of('.');
    std::string extension = (dot_pos != std::string::npos) ? 
        file_path.substr(dot_pos + 1) : "";
    
    // Convert to lowercase
    std::transform(extension.begin(), extension.end(), extension.begin(), 
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    
    // Read file header for signature detection
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::vector<uint8_t> header(1024); // Read first 1KB for format detection
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    size_t bytes_read = static_cast<size_t>(file.gcount());
    header.resize(bytes_read);
    
    return detect_format_from_header(header, extension);
}

std::optional<DetectedFormat> FormatDetector::detect_format_from_header(
    const std::vector<uint8_t>& header_data,
    const std::string& file_extension) {
    
    DetectedFormat format;
    
    // Detect container from file signature
    format.container = detect_container_from_signature(header_data);
    
    // If container detection failed, try extension-based detection
    if (format.container == ContainerType::UNKNOWN && !file_extension.empty()) {
        if (file_extension == "mov" || file_extension == "qt") {
            format.container = ContainerType::MOV;
        } else if (file_extension == "mp4" || file_extension == "m4v") {
            format.container = ContainerType::MP4;
        } else if (file_extension == "avi") {
            format.container = ContainerType::AVI;
        } else if (file_extension == "mkv") {
            format.container = ContainerType::MKV;
        } else if (file_extension == "webm") { // Phase 1 Week 4: WebM support
            format.container = ContainerType::WEBM;
        } else if (file_extension == "ts") { // Phase 1 Week 4: Transport Stream
            format.container = ContainerType::TS;
        } else if (file_extension == "mxf") {
            format.container = ContainerType::MXF;
        } else if (file_extension == "r3d") {
            format.container = ContainerType::R3D;
            format.codec = CodecFamily::REDCODE;
        } else if (file_extension == "ari") {
            format.container = ContainerType::ARI;
            format.codec = CodecFamily::ARRIRAW;
        } else if (file_extension == "braw") {
            format.container = ContainerType::BRAW;
            format.codec = CodecFamily::BLACKMAGIC_RAW;
        }
    }
    
    // Phase 1 Week 4: Try modern codec detection with registered detectors
    if (format.codec == CodecFamily::UNKNOWN && !header_data.empty()) {
        float best_confidence = 0.0f;
        CodecFamily detected_codec = CodecFamily::UNKNOWN;
        
        for (const auto& [codec_name, detector_func] : codec_detectors_) {
            float confidence = detector_func(header_data.data(), header_data.size());
            if (confidence > best_confidence) {
                best_confidence = confidence;
                
                // Map codec name to family
                if (codec_name == "AV1") {
                    detected_codec = CodecFamily::AV1;
                } else if (codec_name == "HEVC") {
                    detected_codec = CodecFamily::HEVC;
                } else if (codec_name == "VP9") {
                    detected_codec = CodecFamily::VP9;
                }
            }
        }
        
        if (best_confidence > 0.7f) { // High confidence threshold
            format.codec = detected_codec;
            format.confidence = best_confidence;
            
            // Get detailed modern codec information
            ModernCodecInfo modern_info = ModernCodecDetector::detect_modern_codec(
                header_data.data(), header_data.size(), detected_codec
            );
            
            if (modern_info.codec_family != CodecFamily::UNKNOWN) {
                // Update format with modern codec details
                format = ModernCodecFormatIntegration::create_modern_codec_detected_format(modern_info);
                format.container = format.container != ContainerType::UNKNOWN ? format.container : 
                    (format.codec == CodecFamily::VP9 ? ContainerType::WEBM : ContainerType::MP4);
            }
        }
    }
    
    // For demonstration purposes, set some professional format examples
    // In real implementation, this would parse actual file metadata
    if (format.container == ContainerType::MOV) {
        // Enhanced ProRes detection (Phase 1 Week 2)
        ProResDetector prores_detector;
        
        // Try to detect ProRes profile from file extension and header
        // In real implementation, would parse actual FourCC from file
        auto prores_info = prores_detector.detect_prores_profile("apcn"); // ProRes 422 Standard
        
        if (prores_info) {
            format.codec = CodecFamily::PRORES;
            format.profile_name = prores_info->profile_name;
            format.pixel_format = ProResDetector::get_recommended_pixel_format(prores_info->profile);
            format.color_space = ve::decode::ColorSpace::BT709;
            format.width = prores_info->width;
            format.height = prores_info->height;
            format.framerate_num = prores_info->framerate_num;
            format.framerate_den = prores_info->framerate_den;
            format.bit_depth = prores_info->bit_depth;
            
            // Add ProRes-specific metadata
            format.metadata_keys.push_back("ProRes Profile: " + prores_info->profile_name);
            format.metadata_keys.push_back("Target Bitrate: " + std::to_string(prores_info->target_bitrate_mbps) + " Mbps");
            if (prores_info->has_alpha) {
                format.metadata_keys.push_back("Alpha Channel: Supported");
            }
        } else {
            // Fallback to generic ProRes detection
            format.codec = CodecFamily::PRORES;
            format.profile_name = "Apple ProRes 422";
            format.pixel_format = ve::decode::PixelFormat::YUV422P10LE;
            format.color_space = ve::decode::ColorSpace::BT709;
            format.width = 1920;
            format.height = 1080;
            format.framerate_num = 24;
            format.framerate_den = 1;
            format.bit_depth = 10;
        }
    } else if (format.container == ContainerType::MXF) {
        // Enhanced DNx detection (Phase 1 Week 3)
        // Try to detect DNx profile from container data
        // In real implementation, would parse actual container data
        DNxInfo dnx_info = DNxDetector::detect_dnx_profile(nullptr, 0); // Placeholder
        
        // Simulate DNxHR HQ detection for demonstration
        dnx_info.is_dnxhr = true;
        dnx_info.dnxhr_profile = DNxHRProfile::DNxHR_HQ;
        dnx_info.width = 1920;
        dnx_info.height = 1080;
        dnx_info.bit_depth = 8;
        dnx_info.target_bitrate_mbps = 220;
        dnx_info.broadcast_legal = true;
        dnx_info.edit_friendly = true;
        
        if (dnx_info.is_dnxhr) {
            format.codec = CodecFamily::DNXHR;
            format.profile_name = "DNxHR HQ";
        } else {
            format.codec = CodecFamily::DNXHD; 
            format.profile_name = "DNxHD 220";
        }
        
        format.pixel_format = DNxDetector::get_recommended_pixel_format(dnx_info);
        format.color_space = ve::decode::ColorSpace::BT709;
        format.width = dnx_info.width;
        format.height = dnx_info.height;
        format.bit_depth = dnx_info.bit_depth;
        
        // Add DNx-specific metadata
        format.metadata_keys.push_back("DNx Profile: " + format.profile_name);
        format.metadata_keys.push_back("Target Bitrate: " + std::to_string(dnx_info.target_bitrate_mbps) + " Mbps");
        format.metadata_keys.push_back("Broadcast Legal: " + std::string(dnx_info.broadcast_legal ? "Yes" : "No"));
        format.metadata_keys.push_back("Edit Friendly: " + std::string(dnx_info.edit_friendly ? "Yes" : "No"));
        
        if (dnx_info.supports_alpha) {
            format.metadata_keys.push_back("Alpha Channel: Supported");
        }
        
    }
    
    // Get capability information
    format.capability = get_format_capability(format.codec, format.container);
    
    // Phase 2 Week 5: Assess HDR capability
    assess_hdr_capability(format);
    
    // Calculate professional score
    format.professional_score = calculate_professional_score(format);
    
    // Generate warnings and recommendations
    generate_format_warnings(format);
    
    return format;
}

FormatCapability FormatDetector::get_format_capability(CodecFamily codec, ContainerType container) {
    auto codec_it = capability_matrix_.find(codec);
    if (codec_it != capability_matrix_.end()) {
        auto container_it = codec_it->second.find(container);
        if (container_it != codec_it->second.end()) {
            return container_it->second;
        }
    }
    
    // Return default (unsupported) capability
    return FormatCapability{};
}

bool FormatDetector::is_format_supported(const DetectedFormat& format) {
    return format.capability.supports_decode;
}

std::vector<std::string> FormatDetector::get_optimization_recommendations(const DetectedFormat& format) {
    std::vector<std::string> recommendations;
    
    if (format.codec == CodecFamily::PRORES) {
        recommendations.push_back("Use hardware acceleration for optimal ProRes performance");
        if (format.width >= 3840) {
            recommendations.push_back("Consider proxy generation for 4K+ ProRes files");
        }
    } else if (format.codec == CodecFamily::H265_HEVC) {
        recommendations.push_back("Enable hardware HEVC decode for better performance");
        if (format.color_space == ve::decode::ColorSpace::BT2020) {
            recommendations.push_back("HDR content detected - ensure HDR-capable display");
        }
    } else if (format.codec == CodecFamily::DNXHD || format.codec == CodecFamily::DNXHR) {
        recommendations.push_back("DNx codecs are optimized for real-time editing");
    }
    
    return recommendations;
}

float FormatDetector::calculate_professional_score(const DetectedFormat& format) {
    float score = 0.0f;
    
    // Codec scoring
    switch (format.codec) {
        case CodecFamily::PRORES:
        case CodecFamily::DNXHD:
        case CodecFamily::DNXHR:
            score += 0.4f; // Professional acquisition codecs
            break;
        case CodecFamily::H264:
            score += 0.2f; // Delivery codec
            break;
        case CodecFamily::H265_HEVC:
        case CodecFamily::AV1:
            score += 0.3f; // Modern delivery codecs
            break;
        default:
            score += 0.1f;
            break;
    }
    
    // Resolution scoring
    if (format.width >= 3840) score += 0.2f; // 4K+
    else if (format.width >= 1920) score += 0.15f; // HD
    else score += 0.05f;
    
    // Bit depth scoring
    if (format.bit_depth >= 12) score += 0.2f;
    else if (format.bit_depth >= 10) score += 0.15f;
    else score += 0.05f;
    
    // Professional features
    if (format.capability.supports_alpha) score += 0.05f;
    if (format.capability.supports_hdr) score += 0.1f;
    if (format.capability.supports_timecode) score += 0.05f;
    
    return std::min(score, 1.0f);
}

void FormatDetector::register_format_capability(CodecFamily codec, ContainerType container, FormatCapability capability) {
    capability_matrix_[codec][container] = capability;
}

std::vector<std::pair<CodecFamily, ContainerType>> FormatDetector::get_supported_formats() {
    std::vector<std::pair<CodecFamily, ContainerType>> supported_formats;
    
    for (const auto& codec_entry : capability_matrix_) {
        for (const auto& container_entry : codec_entry.second) {
            if (container_entry.second.supports_decode) {
                supported_formats.emplace_back(codec_entry.first, container_entry.first);
            }
        }
    }
    
    return supported_formats;
}

void FormatDetector::initialize_professional_capabilities() {
    // ProRes capabilities
    FormatCapability prores_cap;
    prores_cap.supports_decode = true;
    prores_cap.supports_encode = false; // Requires Apple licensing
    prores_cap.hardware_accelerated = true;
    prores_cap.real_time_capable = true;
    prores_cap.max_width = 8192;
    prores_cap.max_height = 4320;
    prores_cap.max_framerate = 60;
    prores_cap.max_bit_depth = 12;
    prores_cap.supports_alpha = true;
    prores_cap.supports_hdr = true;
    prores_cap.supports_timecode = true;
    prores_cap.supports_metadata = true;
    prores_cap.supports_multitrack_audio = true;
    
    capability_matrix_[CodecFamily::PRORES][ContainerType::MOV] = prores_cap;
    
    // DNxHD capabilities
    FormatCapability dnxhd_cap;
    dnxhd_cap.supports_decode = true;
    dnxhd_cap.supports_encode = true;
    dnxhd_cap.real_time_capable = true;
    dnxhd_cap.max_width = 1920;
    dnxhd_cap.max_height = 1080;
    dnxhd_cap.max_framerate = 60;
    dnxhd_cap.max_bit_depth = 8;
    dnxhd_cap.supports_timecode = true;
    dnxhd_cap.supports_metadata = true;
    
    capability_matrix_[CodecFamily::DNXHD][ContainerType::MXF] = dnxhd_cap;
    capability_matrix_[CodecFamily::DNXHD][ContainerType::MOV] = dnxhd_cap;
    
    // DNxHR capabilities (resolution independent)
    FormatCapability dnxhr_cap = dnxhd_cap;
    dnxhr_cap.max_width = 8192;
    dnxhr_cap.max_height = 4320;
    dnxhr_cap.max_bit_depth = 12;
    dnxhr_cap.supports_alpha = true;
    
    capability_matrix_[CodecFamily::DNXHR][ContainerType::MXF] = dnxhr_cap;
    capability_matrix_[CodecFamily::DNXHR][ContainerType::MOV] = dnxhr_cap;
}

void FormatDetector::initialize_modern_codec_capabilities() {
    // H.264 capabilities (baseline for comparison)
    FormatCapability h264_cap;
    h264_cap.supports_decode = true;
    h264_cap.supports_encode = true;
    h264_cap.hardware_accelerated = true;
    h264_cap.real_time_capable = true;
    h264_cap.max_width = 4096;
    h264_cap.max_height = 2160;
    h264_cap.max_framerate = 60;
    h264_cap.max_bit_depth = 8;
    
    capability_matrix_[CodecFamily::H264][ContainerType::MP4] = h264_cap;
    capability_matrix_[CodecFamily::H264][ContainerType::MOV] = h264_cap;
    capability_matrix_[CodecFamily::H264][ContainerType::MKV] = h264_cap;
    
    // AV1 capabilities (Phase 1 Week 4: Next-generation codec)
    FormatCapability av1_cap;
    av1_cap.supports_decode = true;
    av1_cap.supports_encode = true;
    av1_cap.hardware_accelerated = true; // Modern GPUs
    av1_cap.real_time_capable = true;    // With hardware acceleration
    av1_cap.max_width = 8192;
    av1_cap.max_height = 4320;
    av1_cap.max_framerate = 120;
    av1_cap.max_bit_depth = 12;
    av1_cap.supports_hdr = true;
    av1_cap.supports_alpha = true;
    av1_cap.compression_efficiency = 2.5f; // Superior to HEVC
    av1_cap.streaming_optimized = true;
    av1_cap.supports_variable_framerate = true;
    
    capability_matrix_[CodecFamily::AV1][ContainerType::MP4] = av1_cap;
    capability_matrix_[CodecFamily::AV1][ContainerType::MKV] = av1_cap;
    capability_matrix_[CodecFamily::AV1][ContainerType::WEBM] = av1_cap;
    
    // HEVC (H.265) capabilities (Phase 1 Week 4: Enhanced 10/12-bit support)
    FormatCapability hevc_cap;
    hevc_cap.supports_decode = true;
    hevc_cap.supports_encode = true;
    hevc_cap.hardware_accelerated = true;
    hevc_cap.real_time_capable = true;
    hevc_cap.max_width = 8192;
    hevc_cap.max_height = 4320;
    hevc_cap.max_framerate = 120;
    hevc_cap.max_bit_depth = 12;        // Professional 12-bit support
    hevc_cap.supports_hdr = true;       // HDR10, HDR10+, Dolby Vision
    hevc_cap.supports_alpha = true;     // 4:4:4 profiles
    hevc_cap.compression_efficiency = 2.0f; // ~50% better than H.264
    hevc_cap.streaming_optimized = true;
    hevc_cap.supports_variable_framerate = true;
    hevc_cap.supports_timecode = true;
    
    capability_matrix_[CodecFamily::HEVC][ContainerType::MP4] = hevc_cap;
    capability_matrix_[CodecFamily::HEVC][ContainerType::MOV] = hevc_cap;
    capability_matrix_[CodecFamily::HEVC][ContainerType::MKV] = hevc_cap;
    capability_matrix_[CodecFamily::HEVC][ContainerType::TS] = hevc_cap;
    
    // VP9 capabilities (Phase 1 Week 4: Web streaming optimization)
    FormatCapability vp9_cap;
    vp9_cap.supports_decode = true;
    vp9_cap.supports_encode = true;
    vp9_cap.hardware_accelerated = true; // Intel/NVIDIA support
    vp9_cap.real_time_capable = true;
    vp9_cap.max_width = 8192;
    vp9_cap.max_height = 4320;
    vp9_cap.max_framerate = 60;
    vp9_cap.max_bit_depth = 12;
    vp9_cap.supports_hdr = true;
    vp9_cap.supports_alpha = true;       // VP9 alpha channel support
    vp9_cap.compression_efficiency = 1.8f; // Good compression
    vp9_cap.streaming_optimized = true;  // Excellent for web
    vp9_cap.supports_variable_framerate = true;
    vp9_cap.adaptive_streaming_ready = true;
    
    capability_matrix_[CodecFamily::VP9][ContainerType::WEBM] = vp9_cap;
    capability_matrix_[CodecFamily::VP9][ContainerType::MKV] = vp9_cap;
    
    // Legacy H.265 alias for compatibility
    capability_matrix_[CodecFamily::H265_HEVC] = capability_matrix_[CodecFamily::HEVC];
}

void FormatDetector::initialize_broadcast_capabilities() {
    // Add broadcast format capabilities as needed
    // Implementation would include DV, DVCPRO, HDV, etc.
}

ContainerType FormatDetector::detect_container_from_signature(const std::vector<uint8_t>& header) {
    if (header.size() < 8) return ContainerType::UNKNOWN;
    
    // QuickTime/MOV signature
    if (header.size() >= 8 && 
        header[4] == 'f' && header[5] == 't' && header[6] == 'y' && header[7] == 'p') {
        return ContainerType::MOV;
    }
    
    // AVI signature
    if (header.size() >= 12 &&
        header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F' &&
        header[8] == 'A' && header[9] == 'V' && header[10] == 'I' && header[11] == ' ') {
        return ContainerType::AVI;
    }
    
    // MXF signature
    if (header.size() >= 16) {
        // MXF partition pack signature
        const uint8_t mxf_signature[] = {0x06, 0x0E, 0x2B, 0x34, 0x02, 0x05, 0x01, 0x01};
        if (std::equal(mxf_signature, mxf_signature + 8, header.begin())) {
            return ContainerType::MXF;
        }
    }
    
    return ContainerType::UNKNOWN;
}

void FormatDetector::generate_format_warnings(DetectedFormat& format) {
    // Generate format-specific warnings
    if (format.codec == CodecFamily::PRORES && format.container != ContainerType::MOV) {
        format.warnings.push_back("ProRes outside QuickTime container may have compatibility issues");
    }
    
    if (format.width > 4096 && !format.capability.hardware_accelerated) {
        format.warnings.push_back("Large resolution without hardware acceleration may impact performance");
    }
    
    if (format.professional_score < 0.5f) {
        format.warnings.push_back("Format may not be optimal for professional workflows");
    }
}

void FormatDetector::register_codec_detector(const std::string& codec_name, CodecDetectorFunction detector_func) {
    codec_detectors_[codec_name] = detector_func;
}

// Utility function implementations
namespace format_utils {

std::string codec_family_to_string(CodecFamily codec) {
    switch (codec) {
        case CodecFamily::PRORES: return "Apple ProRes";
        case CodecFamily::DNXHD: return "Avid DNxHD";
        case CodecFamily::DNXHR: return "Avid DNxHR";
        case CodecFamily::H264: return "H.264/AVC";
        case CodecFamily::H265_HEVC: return "H.265/HEVC";
        case CodecFamily::AV1: return "AV1";
        case CodecFamily::VP9: return "VP9";
        case CodecFamily::REDCODE: return "RED Code";
        case CodecFamily::ARRIRAW: return "ARRI Raw";
        case CodecFamily::BLACKMAGIC_RAW: return "Blackmagic Raw";
        case CodecFamily::PRORES_RAW: return "Apple ProRes Raw";
        default: return "Unknown";
    }
}

std::string container_type_to_string(ContainerType container) {
    switch (container) {
        case ContainerType::MOV: return "QuickTime";
        case ContainerType::MP4: return "MPEG-4";
        case ContainerType::AVI: return "Audio Video Interleave";
        case ContainerType::MKV: return "Matroska";
        case ContainerType::MXF: return "Material Exchange Format";
        case ContainerType::R3D: return "RED Media";
        case ContainerType::ARI: return "ARRI Media";
        case ContainerType::BRAW: return "Blackmagic Raw";
        default: return "Unknown";
    }
}

bool is_professional_acquisition_codec(CodecFamily codec) {
    return codec == CodecFamily::PRORES ||
           codec == CodecFamily::DNXHD ||
           codec == CodecFamily::DNXHR ||
           codec == CodecFamily::REDCODE ||
           codec == CodecFamily::ARRIRAW ||
           codec == CodecFamily::BLACKMAGIC_RAW ||
           codec == CodecFamily::PRORES_RAW;
}

bool supports_hdr_workflow(CodecFamily codec) {
    return codec == CodecFamily::H265_HEVC ||
           codec == CodecFamily::AV1 ||
           codec == CodecFamily::PRORES ||
           codec == CodecFamily::DNXHR;
}

} // namespace format_utils

// Phase 2 Week 5: HDR Detection Methods
std::optional<HDRMetadata> FormatDetector::detect_hdr_metadata(const std::vector<uint8_t>& stream_data) {
    if (!hdr_infrastructure_) {
        return std::nullopt;
    }
    
    // Use HDR infrastructure to detect metadata
    auto hdr_metadata = hdr_infrastructure_->detect_hdr_metadata(stream_data.data(), stream_data.size());
    
    if (hdr_infrastructure_->validate_hdr_metadata(hdr_metadata)) {
        return hdr_metadata;
    }
    
    return std::nullopt;
}

void FormatDetector::assess_hdr_capability(DetectedFormat& format) {
    if (!hdr_infrastructure_) {
        return;
    }
    
    // Check if codec supports HDR
    bool codec_supports_hdr = format_utils::supports_hdr_workflow(format.codec);
    
    if (codec_supports_hdr) {
        format.capability.supports_hdr = true;
        
        // Check for existing HDR metadata
        if (format.hdr_metadata.has_value()) {
            format.has_hdr_content = true;
            
            // Add HDR-specific recommendations
            const auto& hdr_meta = format.hdr_metadata.value();
            
            switch (hdr_meta.hdr_standard) {
                case HDRStandard::HDR10:
                    format.recommendations.push_back("HDR10 detected - suitable for streaming platforms");
                    break;
                case HDRStandard::DOLBY_VISION:
                    format.recommendations.push_back("Dolby Vision detected - premium HDR format");
                    format.professional_score += 0.2f; // Boost professional score
                    break;
                case HDRStandard::HLG:
                    format.recommendations.push_back("HLG detected - broadcast HDR standard");
                    break;
                case HDRStandard::HDR10_PLUS:
                    format.recommendations.push_back("HDR10+ detected - dynamic metadata available");
                    break;
                default:
                    break;
            }
            
            // Check system HDR capabilities
            auto system_caps = hdr_infrastructure_->get_system_hdr_capabilities();
            if (!system_caps.supports_hdr10 && !system_caps.supports_hlg && !system_caps.supports_dolby_vision) {
                format.warnings.push_back("HDR content detected but system may not support HDR display");
            }
            
            // Validate HDR metadata consistency
            if (hdr_meta.mastering_display.max_display_mastering_luminance > 10000.0f) {
                format.warnings.push_back("Very high peak luminance may not be displayable on most monitors");
            }
            
            if (hdr_meta.content_light_level.max_content_light_level > 4000) {
                format.warnings.push_back("High content light level may require tone mapping");
            }
        } else {
            // Codec supports HDR but no HDR metadata found
            if (format.bit_depth >= 10) {
                format.recommendations.push_back("10-bit content detected - suitable for HDR workflows");
            }
            
            if (format.color_space == ve::decode::ColorSpace::BT2020) {
                format.recommendations.push_back("BT.2020 color space - HDR-capable");
            }
        }
    } else {
        // Codec doesn't support HDR
        if (format.bit_depth >= 10 || format.color_space == ve::decode::ColorSpace::BT2020) {
            format.warnings.push_back("Wide color gamut detected but codec may not support HDR");
        }
    }
}

} // namespace ve::media_io
