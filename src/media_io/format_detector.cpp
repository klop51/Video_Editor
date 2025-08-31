#include "media_io/format_detector.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>

namespace ve::media_io {

FormatDetector::FormatDetector() {
    initialize_professional_capabilities();
    initialize_broadcast_capabilities();
    initialize_modern_codec_capabilities();
}

std::optional<DetectedFormat> FormatDetector::detect_file_format(const std::string& file_path) {
    // Extract file extension
    size_t dot_pos = file_path.find_last_of('.');
    std::string extension = (dot_pos != std::string::npos) ? 
        file_path.substr(dot_pos + 1) : "";
    
    // Convert to lowercase
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    // Read file header for signature detection
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::vector<uint8_t> header(1024); // Read first 1KB for format detection
    file.read(reinterpret_cast<char*>(header.data()), header.size());
    size_t bytes_read = file.gcount();
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
        } else if (file_extension == "mkv" || file_extension == "webm") {
            format.container = ContainerType::MKV;
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
    
    // For demonstration purposes, set some professional format examples
    // In real implementation, this would parse actual file metadata
    if (format.container == ContainerType::MOV) {
        // Common ProRes detection
        format.codec = CodecFamily::PRORES;
        format.profile_name = "ProRes 422 HQ";
        format.pixel_format = ve::decode::PixelFormat::YUV422P10LE;
        format.color_space = ve::decode::ColorSpace::BT709;
        format.width = 1920;
        format.height = 1080;
        format.framerate_num = 24;
        format.framerate_den = 1;
        format.bit_depth = 10;
    } else if (format.container == ContainerType::MXF) {
        // Common DNxHD detection
        format.codec = CodecFamily::DNXHD;
        format.profile_name = "DNxHD 220";
        format.pixel_format = ve::decode::PixelFormat::YUV422P;
        format.color_space = ve::decode::ColorSpace::BT709;
        format.width = 1920;
        format.height = 1080;
        format.bit_depth = 8;
    }
    
    // Get capability information
    format.capability = get_format_capability(format.codec, format.container);
    
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
    // H.264 capabilities
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
    
    // H.265/HEVC capabilities
    FormatCapability h265_cap;
    h265_cap.supports_decode = true;
    h265_cap.supports_encode = true;
    h265_cap.hardware_accelerated = true;
    h265_cap.real_time_capable = true;
    h265_cap.max_width = 8192;
    h265_cap.max_height = 4320;
    h265_cap.max_framerate = 60;
    h265_cap.max_bit_depth = 12;
    h265_cap.supports_hdr = true;
    
    capability_matrix_[CodecFamily::H265_HEVC][ContainerType::MP4] = h265_cap;
    capability_matrix_[CodecFamily::H265_HEVC][ContainerType::MOV] = h265_cap;
    capability_matrix_[CodecFamily::H265_HEVC][ContainerType::MKV] = h265_cap;
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

} // namespace ve::media_io
