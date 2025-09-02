#include "media_io/dv_formats.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <cstring>

namespace ve::media_io {

bool DVDecoder::detectDVFormat(const uint8_t* data, size_t size) {
    if (!data || size < DV_FRAME_SIZE_NTSC) {
        ve::log::warning("DV detection: insufficient data size");
        return false;
    }
    
    // Reset detection state
    detected_format_ = DVFormat::UNKNOWN;
    is_pal_ = false;
    wide_screen_ = false;
    timecode_valid_ = false;
    
    // Analyze first few DV blocks to determine format
    const uint8_t* frame_start = data;
    bool format_detected = false;
    
    for (size_t offset = 0; offset < std::min(size, static_cast<size_t>(1000)); offset += DV_BLOCK_SIZE) {
        const DVDataBlock* block = reinterpret_cast<const DVDataBlock*>(frame_start + offset);
        
        if (block->is_valid_sync()) {
            if (analyzeDVBlock(*block)) {
                format_detected = true;
                break;
            }
        }
    }
    
    if (!format_detected) {
        // Try alternative detection methods
        format_detected = detectFormat(data);
    }
    
    if (format_detected) {
        // Extract additional information
        extractTimecodeInfo(data);
        extractAudioInfo(data);
        extractCameraInfo(data);
        
        ve::log::info("DV format detected: " + std::to_string(static_cast<int>(detected_format_)) + 
                     (is_pal_ ? " (PAL)" : " (NTSC)"));
    }
    
    return format_detected;
}

bool DVDecoder::analyzeDVBlock(const DVDataBlock& block) {
    // Determine PAL vs NTSC
    is_pal_ = block.is_pal();
    
    // Check for widescreen
    wide_screen_ = block.is_16_9();
    
    // Analyze chroma subsampling to distinguish formats
    uint8_t chroma = block.get_chroma_subsampling();
    
    // Check for professional format markers
    bool is_professional = (block.format_info & 0x40) != 0;
    
    if (is_professional) {
        // Likely DVCPRO format
        if (chroma == 0x22) {  // 4:2:2 chroma
            detected_format_ = DVFormat::DVCPRO50;
        } else {
            detected_format_ = DVFormat::DVCPRO25;
        }
    } else {
        // Standard DV format
        if (chroma == 0x11 || chroma == 0x20) {
            detected_format_ = DVFormat::DV25;
        }
    }
    
    return detected_format_ != DVFormat::UNKNOWN;
}

bool DVDecoder::detectFormat(const uint8_t* frame_data) {
    // Alternative detection based on frame size and patterns
    
    // Check for HDV (MPEG-2 transport stream)
    if (isHDV(frame_data)) {
        detected_format_ = DVFormat::HDV_1080I;  // Default to 1080i, could be refined
        return true;
    }
    
    // Check for DVCPRO specific markers
    if (isDVCPRO(frame_data)) {
        detected_format_ = DVFormat::DVCPRO25;  // Default to 25, could be 50
        return true;
    }
    
    // Check for DVCAM markers
    if (isDVCAM(frame_data)) {
        detected_format_ = DVFormat::DVCAM;
        return true;
    }
    
    // Default to standard DV
    detected_format_ = DVFormat::DV25;
    return true;
}

bool DVDecoder::isDVCPRO(const uint8_t* data) const {
    // DVCPRO has specific sequence patterns and track layouts
    // Look for DVCPRO identifier in the aux data
    const uint8_t dvcpro_pattern[] = {0x61, 0x62, 0x63, 0x64};  // Simplified pattern
    
    for (size_t i = 0; i < 1000; i += 4) {
        if (std::memcmp(data + i, dvcpro_pattern, 4) == 0) {
            return true;
        }
    }
    
    return false;
}

bool DVDecoder::isHDV(const uint8_t* data) const {
    // HDV uses MPEG-2 transport stream
    // Look for MPEG-2 sync bytes (0x47) at regular intervals
    const uint8_t mpeg2_sync = 0x47;
    int sync_count = 0;
    
    for (size_t i = 0; i < 1000; i += 188) {  // TS packet size
        if (data[i] == mpeg2_sync) {
            sync_count++;
        }
    }
    
    return sync_count > 3;  // Multiple TS packets found
}

bool DVDecoder::isDVCAM(const uint8_t* data) const {
    // DVCAM has Sony-specific identifiers
    // Look for Sony manufacturer code in the aux data
    const uint8_t sony_pattern[] = {0x53, 0x4F, 0x4E, 0x59};  // "SONY"
    
    for (size_t i = 0; i < 1000; i++) {
        if (std::memcmp(data + i, sony_pattern, 4) == 0) {
            return true;
        }
    }
    
    return false;
}

void DVDecoder::extractTimecodeInfo(const uint8_t* subcode_blocks) {
    // DV timecode is stored in subcode area
    // This is a simplified extraction - real implementation would parse DV structure
    
    // Look for timecode pattern (simplified)
    for (size_t i = 0; i < 500; i += 10) {
        const uint8_t* tc_data = subcode_blocks + i;
        
        // Check if this looks like timecode data
        if ((tc_data[0] & 0xF0) <= 0x50 &&  // Hours <= 5x
            (tc_data[1] & 0xF0) <= 0x60 &&  // Minutes <= 6x
            (tc_data[2] & 0xF0) <= 0x60) {  // Seconds <= 6x
            
            current_timecode_.hours = (tc_data[0] >> 4) * 10 + (tc_data[0] & 0x0F);
            current_timecode_.minutes = (tc_data[1] >> 4) * 10 + (tc_data[1] & 0x0F);
            current_timecode_.seconds = (tc_data[2] >> 4) * 10 + (tc_data[2] & 0x0F);
            current_timecode_.frames = (tc_data[3] >> 4) * 10 + (tc_data[3] & 0x0F);
            
            // Set frame rate based on detected format
            if (is_pal_) {
                current_timecode_.frame_rate = {25, 1, false};
            } else {
                current_timecode_.frame_rate = {30000, 1001, true};
            }
            current_timecode_.drop_frame = !is_pal_;
            
            timecode_valid_ = true;
            break;
        }
    }
}

void DVDecoder::extractAudioInfo(const uint8_t* audio_blocks) {
    // DV audio is stored in dedicated audio blocks
    // Simplified audio info extraction
    
    audio_channels_ = 2;  // Default stereo
    audio_locked_ = true; // Assume locked for now
    
    // Look for audio configuration in the first audio block
    const uint8_t* audio_config = audio_blocks;
    
    // Check audio mode bits (simplified)
    if ((audio_config[0] & 0x0F) == 0x00) {
        audio_channels_ = 2;  // Stereo
    } else if ((audio_config[0] & 0x0F) == 0x01) {
        audio_channels_ = 4;  // 4-channel
    }
    
    // Check sample rate (simplified)
    if ((audio_config[1] & 0x07) == 0x00) {
        // 48kHz (default for DV)
    } else if ((audio_config[1] & 0x07) == 0x01) {
        // 44.1kHz
    }
}

void DVDecoder::extractCameraInfo(const uint8_t* aux_blocks) {
    // Camera manufacturer and model info is in aux data
    // This is a simplified extraction
    
    const uint8_t* aux_data = aux_blocks;
    
    // Look for common manufacturer patterns
    const uint8_t sony_id[] = {0x53, 0x4F, 0x4E, 0x59};
    const uint8_t panasonic_id[] = {0x50, 0x41, 0x4E, 0x41};
    const uint8_t canon_id[] = {0x43, 0x41, 0x4E, 0x4F};
    
    for (size_t i = 0; i < 200; i++) {
        if (std::memcmp(aux_data + i, sony_id, 4) == 0) {
            camera_manufacturer_ = "Sony";
            break;
        } else if (std::memcmp(aux_data + i, panasonic_id, 4) == 0) {
            camera_manufacturer_ = "Panasonic";
            break;
        } else if (std::memcmp(aux_data + i, canon_id, 4) == 0) {
            camera_manufacturer_ = "Canon";
            break;
        }
    }
    
    // Check for color bars pattern
    has_color_bars_ = false;
    for (size_t i = 0; i < 100; i++) {
        if (aux_data[i] == 0xCB && aux_data[i+1] == 0xAR) {  // Color bar marker
            has_color_bars_ = true;
            break;
        }
    }
}

DVFormatSpec DVDecoder::getFormatSpec() const {
    return DVFormatUtils::getDVFormatSpec(detected_format_, is_pal_);
}

bool DVDecoder::isInterlaced() const {
    // Most DV formats are interlaced except for some DVCPRO HD modes
    return detected_format_ != DVFormat::DVCPRO_HD || 
           current_timecode_.frame_rate.numerator <= 30000;
}

FieldOrder DVDecoder::getFieldOrder() const {
    if (!isInterlaced()) {
        return FieldOrder::PROGRESSIVE;
    }
    
    // Field order depends on format and region
    if (is_pal_) {
        return FieldOrder::TOP_FIELD_FIRST;  // PAL is typically TFF
    } else {
        return FieldOrder::BOTTOM_FIELD_FIRST;  // NTSC is typically BFF
    }
}

LegacyTimecode DVDecoder::extractTimecode() const {
    return current_timecode_;
}

uint32_t DVDecoder::getAudioSampleRate() const {
    // DV typically uses 48kHz, some consumer formats use 44.1kHz
    if (detected_format_ == DVFormat::DV25 && !is_pal_) {
        return 44100;  // Some NTSC DV uses 44.1kHz
    }
    return 48000;  // Professional standard
}

// DVFormatUtils implementation
DVFormatSpec DVFormatUtils::getDVFormatSpec(DVFormat format, bool is_pal) {
    switch (format) {
        case DVFormat::DV25:
            return is_pal ? dv_formats::DV25_PAL : dv_formats::DV25_NTSC;
        case DVFormat::DVCPRO50:
            return dv_formats::DVCPRO50_NTSC;  // Could be extended for PAL
        case DVFormat::DVCPRO_HD:
            return dv_formats::DVCPRO_HD_720P;
        case DVFormat::HDV_1080I:
            return dv_formats::HDV_1080I_NTSC;
        default:
            return dv_formats::DV25_NTSC;  // Default fallback
    }
}

std::vector<DVFormat> DVFormatUtils::getSupportedFormats() {
    return {
        DVFormat::DV25,
        DVFormat::DVCAM,
        DVFormat::DVCPRO25,
        DVFormat::DVCPRO50,
        DVFormat::DVCPRO_HD,
        DVFormat::HDV_720P,
        DVFormat::HDV_1080I
    };
}

bool DVFormatUtils::isFormatSupported(DVFormat format) {
    auto supported = getSupportedFormats();
    return std::find(supported.begin(), supported.end(), format) != supported.end();
}

bool DVFormatUtils::canConvertBetween(DVFormat from, DVFormat to) {
    // Basic conversion compatibility matrix
    // Professional formats can generally convert to consumer formats
    // HD formats can downconvert to SD
    
    if (from == to) return true;
    
    // Professional to consumer conversions
    if ((from == DVFormat::DVCPRO25 || from == DVFormat::DVCPRO50) && 
        (to == DVFormat::DV25 || to == DVFormat::DVCAM)) {
        return true;
    }
    
    // HD to SD downconversion
    if ((from == DVFormat::HDV_720P || from == DVFormat::HDV_1080I || from == DVFormat::DVCPRO_HD) &&
        (to == DVFormat::DV25 || to == DVFormat::DVCAM || to == DVFormat::DVCPRO25)) {
        return true;
    }
    
    return false;
}

LegacyFormatInfo DVFormatUtils::toDVLegacyFormat(const DVFormatSpec& dv_spec) {
    LegacyFormatInfo legacy_info = {};
    
    legacy_info.width = dv_spec.width;
    legacy_info.height = dv_spec.height;
    legacy_info.frame_rate = dv_spec.frame_rate;
    legacy_info.interlaced = dv_spec.interlaced;
    legacy_info.field_order = dv_spec.field_order;
    legacy_info.standard_name = dv_spec.name;
    legacy_info.description = dv_spec.description;
    
    // Set pixel aspect based on format
    if (dv_spec.width == 720 && dv_spec.height == 576) {
        legacy_info.pixel_aspect = {59, 54};  // PAL
        legacy_info.resolution = LegacyResolution::PAL_SD;
    } else if (dv_spec.width == 720 && dv_spec.height == 480) {
        legacy_info.pixel_aspect = {10, 11};  // NTSC
        legacy_info.resolution = LegacyResolution::NTSC_SD;
    } else {
        legacy_info.pixel_aspect = {1, 1};    // Square pixels for HD
        legacy_info.resolution = static_cast<LegacyResolution>(0);  // Unknown
    }
    
    return legacy_info;
}

bool DVFormatUtils::requiresProfessionalEquipment(DVFormat format) {
    return format == DVFormat::DVCPRO25 || 
           format == DVFormat::DVCPRO50 || 
           format == DVFormat::DVCPRO_HD;
}

std::string DVFormatUtils::getRecommendedCaptureSettings(DVFormat format) {
    switch (format) {
        case DVFormat::DV25:
            return "Capture via FireWire/IEEE1394 at 25Mbps, maintain original timecode";
        case DVFormat::DVCPRO50:
            return "Professional capture card required, 50Mbps, dual-track audio";
        case DVFormat::HDV_1080I:
            return "HDV-compatible capture device, preserve MPEG-2 stream, 25Mbps";
        default:
            return "Standard DV capture via FireWire at native bitrate";
    }
}

std::vector<std::string> DVFormatUtils::getCompatibleNLEs(DVFormat format) {
    std::vector<std::string> nles;
    
    // All formats support these basic NLEs
    nles.push_back("Adobe Premiere Pro");
    nles.push_back("DaVinci Resolve");
    nles.push_back("Final Cut Pro");
    
    if (requiresProfessionalEquipment(format)) {
        nles.push_back("Avid Media Composer");
        nles.push_back("Quantel Pablo");
    }
    
    if (format == DVFormat::HDV_1080I || format == DVFormat::HDV_720P) {
        nles.push_back("Sony Vegas Pro");
        nles.push_back("Edius Pro");
    }
    
    return nles;
}

} // namespace ve::media_io
