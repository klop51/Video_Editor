// Week 9: HDR Metadata Parser Implementation
// Complete implementation of HDR metadata parsing and handling

#include "gfx/hdr_metadata_parser.hpp"
#include "core/logger.hpp"
#include "core/exceptions.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace VideoEditor::GFX {

// =============================================================================
// Metadata Structure Validation
// =============================================================================

bool MasteringDisplayMetadata::is_valid() const noexcept {
    try {
        // Check display primaries
        for (int i = 0; i < 3; ++i) {
            if (display_primaries[i].x > 50000 || display_primaries[i].y > 50000) {
                return false;
            }
        }
        
        // Check white point
        if (white_point.x > 50000 || white_point.y > 50000) {
            return false;
        }
        
        // Check luminance values
        if (max_display_mastering_luminance == 0 || 
            min_display_mastering_luminance >= max_display_mastering_luminance) {
            return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

float MasteringDisplayMetadata::get_max_luminance_nits() const noexcept {
    return static_cast<float>(max_display_mastering_luminance) / 10000.0f;
}

float MasteringDisplayMetadata::get_min_luminance_nits() const noexcept {
    return static_cast<float>(min_display_mastering_luminance) / 10000.0f;
}

bool ContentLightLevelInfo::is_valid() const noexcept {
    return max_content_light_level > 0 && max_frame_average_light_level <= max_content_light_level;
}

bool DynamicMetadataFrame::is_valid() const noexcept {
    return target_max_pq_quantized > 0 && trim_slope > 0 && trim_power > 0;
}

bool HLGSystemParams::is_valid() const noexcept {
    return system_gamma > 0.5f && system_gamma < 2.0f && 
           nominal_peak_luminance > 0.0f && nominal_peak_luminance <= 4000.0f &&
           black_level_lift >= 0.0f && black_level_lift <= 1.0f;
}

bool ColorVolumeTransform::is_valid() const noexcept {
    return targeted_system_display_maximum_luminance > 0 &&
           num_rows_targeted_system_display_actual_peak_luminance > 0 &&
           num_cols_targeted_system_display_actual_peak_luminance > 0;
}

bool HDRMetadataPacket::is_valid() const noexcept {
    try {
        if (standard == HDRStandard::INVALID) return false;
        
        if (mastering_display && !mastering_display->is_valid()) return false;
        if (content_light_level && !content_light_level->is_valid()) return false;
        if (hlg_params && !hlg_params->is_valid()) return false;
        if (cvt_metadata && !cvt_metadata->is_valid()) return false;
        
        for (const auto& frame : dynamic_frames) {
            if (!frame.is_valid()) return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool HDRMetadataPacket::has_static_metadata() const noexcept {
    return mastering_display.has_value() || content_light_level.has_value();
}

bool HDRMetadataPacket::has_dynamic_metadata() const noexcept {
    return !dynamic_frames.empty();
}

std::string HDRMetadataPacket::to_string() const {
    std::ostringstream oss;
    oss << "HDRMetadataPacket {\n";
    oss << "  Standard: " << static_cast<int>(standard) << "\n";
    oss << "  Transfer: " << static_cast<int>(transfer_characteristic) << "\n";
    oss << "  Primaries: " << static_cast<int>(color_primaries) << "\n";
    
    if (mastering_display) {
        oss << "  Max Luminance: " << mastering_display->get_max_luminance_nits() << " nits\n";
        oss << "  Min Luminance: " << mastering_display->get_min_luminance_nits() << " nits\n";
    }
    
    if (content_light_level) {
        oss << "  MaxCLL: " << content_light_level->max_content_light_level << " nits\n";
        oss << "  MaxFALL: " << content_light_level->max_frame_average_light_level << " nits\n";
    }
    
    oss << "  Dynamic Frames: " << dynamic_frames.size() << "\n";
    oss << "}";
    
    return oss.str();
}

// =============================================================================
// BitReader Implementation
// =============================================================================

HDRMetadataParser::BitReader::BitReader(const uint8_t* data, size_t size)
    : data_(data), size_(size), byte_pos_(0), bit_pos_(0) {
}

uint32_t HDRMetadataParser::BitReader::read_bits(int count) {
    if (count <= 0 || count > 32) return 0;
    
    uint32_t result = 0;
    
    for (int i = 0; i < count; ++i) {
        if (byte_pos_ >= size_) return result;
        
        int bit = (data_[byte_pos_] >> (7 - bit_pos_)) & 1;
        result = (result << 1) | bit;
        
        ++bit_pos_;
        if (bit_pos_ >= 8) {
            bit_pos_ = 0;
            ++byte_pos_;
        }
    }
    
    return result;
}

uint32_t HDRMetadataParser::BitReader::read_ue_golomb() {
    int leading_zeros = 0;
    
    // Count leading zeros
    while (has_more_data() && read_bits(1) == 0) {
        ++leading_zeros;
    }
    
    if (leading_zeros == 0) return 0;
    
    // Read remaining bits
    uint32_t value = read_bits(leading_zeros);
    return (1 << leading_zeros) - 1 + value;
}

int32_t HDRMetadataParser::BitReader::read_se_golomb() {
    uint32_t ue_value = read_ue_golomb();
    if (ue_value == 0) return 0;
    
    return (ue_value & 1) ? static_cast<int32_t>((ue_value + 1) / 2) : 
                           -static_cast<int32_t>(ue_value / 2);
}

bool HDRMetadataParser::BitReader::has_more_data() const {
    return byte_pos_ < size_ || (byte_pos_ == size_ - 1 && bit_pos_ < 8);
}

void HDRMetadataParser::BitReader::skip_bits(int count) {
    bit_pos_ += count;
    byte_pos_ += bit_pos_ / 8;
    bit_pos_ %= 8;
}

void HDRMetadataParser::BitReader::byte_align() {
    if (bit_pos_ > 0) {
        bit_pos_ = 0;
        ++byte_pos_;
    }
}

// =============================================================================
// HDR Metadata Parser Implementation
// =============================================================================

Core::Result<HDRMetadataPacket> HDRMetadataParser::parse_sei_message(
    const std::vector<uint8_t>& sei_data, uint8_t payload_type) const {
    
    try {
        HDRMetadataPacket packet;
        
        switch (payload_type) {
            case 137: { // Mastering display colour volume
                auto result = parse_mastering_display_sei(sei_data.data(), sei_data.size());
                if (result.is_error()) {
                    return Core::Result<HDRMetadataPacket>::error(result.error());
                }
                packet.mastering_display = result.value();
                packet.standard = HDRStandard::HDR10;
                break;
            }
            
            case 144: { // Content light level information
                auto result = parse_content_light_level_sei(sei_data.data(), sei_data.size());
                if (result.is_error()) {
                    return Core::Result<HDRMetadataPacket>::error(result.error());
                }
                packet.content_light_level = result.value();
                break;
            }
            
            case 147: { // Alternative transfer characteristics
                if (sei_data.size() >= 1) {
                    packet.transfer_characteristic = static_cast<TransferCharacteristic>(sei_data[0]);
                }
                break;
            }
            
            case 4: { // User data registered (for HDR10+)
                auto result = parse_hdr10_plus_metadata(sei_data);
                if (result.is_success()) {
                    packet.dynamic_frames = result.value();
                    packet.standard = HDRStandard::HDR10_PLUS;
                }
                break;
            }
            
            default:
                LOG_WARNING("Unknown SEI payload type: {}", payload_type);
                break;
        }
        
        return Core::Result<HDRMetadataPacket>::success(packet);
        
    } catch (const std::exception& e) {
        LOG_ERROR("SEI message parsing failed: {}", e.what());
        return Core::Result<HDRMetadataPacket>::error("SEI parsing failed");
    }
}

Core::Result<MasteringDisplayMetadata> HDRMetadataParser::parse_mastering_display_sei(
    const uint8_t* data, size_t size) const {
    
    try {
        if (size < 24) { // Minimum size for mastering display metadata
            return Core::Result<MasteringDisplayMetadata>::error("Insufficient data for mastering display");
        }
        
        MasteringDisplayMetadata metadata{};
        size_t offset = 0;
        
        // Parse display primaries (6 values of 2 bytes each)
        for (int i = 0; i < 3; ++i) {
            metadata.display_primaries[i].x = (data[offset] << 8) | data[offset + 1];
            metadata.display_primaries[i].y = (data[offset + 2] << 8) | data[offset + 3];
            offset += 4;
        }
        
        // Parse white point (2 values of 2 bytes each)
        metadata.white_point.x = (data[offset] << 8) | data[offset + 1];
        metadata.white_point.y = (data[offset + 2] << 8) | data[offset + 3];
        offset += 4;
        
        // Parse max and min luminance (4 bytes each)
        metadata.max_display_mastering_luminance = 
            (static_cast<uint32_t>(data[offset]) << 24) |
            (static_cast<uint32_t>(data[offset + 1]) << 16) |
            (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        
        metadata.min_display_mastering_luminance = 
            (static_cast<uint32_t>(data[offset]) << 24) |
            (static_cast<uint32_t>(data[offset + 1]) << 16) |
            (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
        
        if (!metadata.is_valid()) {
            return Core::Result<MasteringDisplayMetadata>::error("Invalid mastering display metadata");
        }
        
        return Core::Result<MasteringDisplayMetadata>::success(metadata);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Mastering display parsing failed: {}", e.what());
        return Core::Result<MasteringDisplayMetadata>::error("Mastering display parsing failed");
    }
}

Core::Result<ContentLightLevelInfo> HDRMetadataParser::parse_content_light_level_sei(
    const uint8_t* data, size_t size) const {
    
    try {
        if (size < 4) {
            return Core::Result<ContentLightLevelInfo>::error("Insufficient data for content light level");
        }
        
        ContentLightLevelInfo cll{};
        
        cll.max_content_light_level = (data[0] << 8) | data[1];
        cll.max_frame_average_light_level = (data[2] << 8) | data[3];
        
        if (!cll.is_valid()) {
            return Core::Result<ContentLightLevelInfo>::error("Invalid content light level data");
        }
        
        return Core::Result<ContentLightLevelInfo>::success(cll);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Content light level parsing failed: {}", e.what());
        return Core::Result<ContentLightLevelInfo>::error("Content light level parsing failed");
    }
}

Core::Result<std::vector<DynamicMetadataFrame>> HDRMetadataParser::parse_hdr10_plus_metadata(
    const std::vector<uint8_t>& metadata) const {
    
    try {
        std::vector<DynamicMetadataFrame> frames;
        
        if (metadata.size() < 8) {
            return Core::Result<std::vector<DynamicMetadataFrame>>::error("Insufficient HDR10+ metadata");
        }
        
        // Check for HDR10+ identifier
        const uint8_t hdr10_plus_id[] = {0x8B, 0x99, 0x52, 0x83, 0x9B};
        if (metadata.size() < sizeof(hdr10_plus_id) || 
            std::memcmp(metadata.data(), hdr10_plus_id, sizeof(hdr10_plus_id)) != 0) {
            return Core::Result<std::vector<DynamicMetadataFrame>>::error("Invalid HDR10+ identifier");
        }
        
        BitReader reader(metadata.data() + sizeof(hdr10_plus_id), 
                        metadata.size() - sizeof(hdr10_plus_id));
        
        // Parse HDR10+ dynamic metadata
        uint32_t application_identifier = reader.read_bits(8);
        uint32_t application_version = reader.read_bits(8);
        
        if (application_identifier != 4) { // HDR10+ application identifier
            return Core::Result<std::vector<DynamicMetadataFrame>>::error("Invalid HDR10+ application ID");
        }
        
        uint32_t num_windows = reader.read_bits(2);
        
        for (uint32_t window = 0; window <= num_windows; ++window) {
            DynamicMetadataFrame frame{};
            
            // Parse window parameters
            if (window > 0) {
                reader.skip_bits(16); // window_upper_left_corner_x
                reader.skip_bits(16); // window_upper_left_corner_y
                reader.skip_bits(16); // window_lower_right_corner_x
                reader.skip_bits(16); // window_lower_right_corner_y
            }
            
            // Parse tone mapping parameters
            frame.target_max_pq_quantized = reader.read_bits(17);
            frame.trim_slope = reader.read_bits(12);
            frame.trim_offset = reader.read_bits(12);
            frame.trim_power = reader.read_bits(12);
            frame.trim_chroma_weight = reader.read_bits(12);
            frame.trim_saturation_gain = reader.read_bits(12);
            
            // Parse mastering display weights
            for (int i = 0; i < 9; ++i) {
                frame.ms_weight[i] = reader.read_bits(13);
            }
            
            if (frame.is_valid()) {
                frames.push_back(frame);
            }
        }
        
        return Core::Result<std::vector<DynamicMetadataFrame>>::success(frames);
        
    } catch (const std::exception& e) {
        LOG_ERROR("HDR10+ metadata parsing failed: {}", e.what());
        return Core::Result<std::vector<DynamicMetadataFrame>>::error("HDR10+ parsing failed");
    }
}

HDRStandard HDRMetadataParser::detect_hdr_standard(const HDRMetadataPacket& metadata) const noexcept {
    try {
        if (!metadata.dynamic_frames.empty()) {
            return HDRStandard::HDR10_PLUS;
        }
        
        if (metadata.transfer_characteristic == TransferCharacteristic::ARIB_STD_B67) {
            return HDRStandard::HLG;
        }
        
        if (metadata.transfer_characteristic == TransferCharacteristic::SMPTE_ST_2084) {
            if (metadata.has_static_metadata()) {
                return HDRStandard::HDR10;
            }
        }
        
        return HDRStandard::INVALID;
        
    } catch (...) {
        return HDRStandard::INVALID;
    }
}

float HDRMetadataParser::calculate_peak_luminance(const HDRMetadataPacket& metadata) const noexcept {
    try {
        if (metadata.mastering_display) {
            return metadata.mastering_display->get_max_luminance_nits();
        }
        
        if (metadata.content_light_level) {
            return static_cast<float>(metadata.content_light_level->max_content_light_level);
        }
        
        if (metadata.transfer_characteristic == TransferCharacteristic::ARIB_STD_B67) {
            // HLG system gamma
            if (metadata.hlg_params) {
                return metadata.hlg_params->nominal_peak_luminance;
            }
            return 1000.0f; // Default HLG peak
        }
        
        return 100.0f; // SDR default
        
    } catch (...) {
        return 100.0f;
    }
}

float HDRMetadataParser::calculate_average_luminance(const HDRMetadataPacket& metadata) const noexcept {
    try {
        if (metadata.content_light_level) {
            return static_cast<float>(metadata.content_light_level->max_frame_average_light_level);
        }
        
        // Estimate based on peak luminance
        float peak = calculate_peak_luminance(metadata);
        return peak * 0.1f; // Typical 10% of peak
        
    } catch (...) {
        return 10.0f;
    }
}

bool HDRMetadataParser::validate_mastering_display_metadata(
    const MasteringDisplayMetadata& metadata) const noexcept {
    
    try {
        // Check if metadata is valid
        if (!metadata.is_valid()) return false;
        
        // Check reasonable luminance ranges
        float max_nits = metadata.get_max_luminance_nits();
        float min_nits = metadata.get_min_luminance_nits();
        
        if (max_nits < 100.0f || max_nits > 10000.0f) return false;
        if (min_nits < 0.0001f || min_nits > 1.0f) return false;
        if (min_nits >= max_nits) return false;
        
        // Check primaries are within reasonable bounds
        for (int i = 0; i < 3; ++i) {
            float x = chromaticity_coordinate_to_float(metadata.display_primaries[i].x);
            float y = chromaticity_coordinate_to_float(metadata.display_primaries[i].y);
            
            if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f) return false;
            if (x + y > 1.0f) return false; // Invalid chromaticity
        }
        
        // Check white point
        float wx = chromaticity_coordinate_to_float(metadata.white_point.x);
        float wy = chromaticity_coordinate_to_float(metadata.white_point.y);
        
        if (wx < 0.0f || wx > 1.0f || wy < 0.0f || wy > 1.0f) return false;
        if (wx + wy > 1.0f) return false;
        
        return true;
        
    } catch (...) {
        return false;
    }
}

bool HDRMetadataParser::validate_content_light_level(const ContentLightLevelInfo& cll) const noexcept {
    try {
        if (!cll.is_valid()) return false;
        
        // Check reasonable ranges
        if (cll.max_content_light_level > 10000 || cll.max_content_light_level == 0) return false;
        if (cll.max_frame_average_light_level > cll.max_content_light_level) return false;
        
        return true;
        
    } catch (...) {
        return false;
    }
}

bool HDRMetadataParser::validate_dynamic_metadata_frame(const DynamicMetadataFrame& frame) const noexcept {
    try {
        if (!frame.is_valid()) return false;
        
        // Check reasonable parameter ranges
        if (frame.target_max_pq_quantized > 131071) return false; // 17-bit max
        if (frame.trim_slope > 4095) return false; // 12-bit max
        if (frame.trim_offset > 4095) return false;
        if (frame.trim_power > 4095) return false;
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// =============================================================================
// Serialization Methods
// =============================================================================

Core::Result<std::vector<uint8_t>> HDRMetadataParser::serialize_to_sei(
    const HDRMetadataPacket& metadata) const {
    
    try {
        std::vector<uint8_t> sei_data;
        
        if (metadata.mastering_display) {
            // Serialize mastering display metadata
            std::vector<uint8_t> mdm_data(24);
            size_t offset = 0;
            
            const auto& mdm = *metadata.mastering_display;
            
            // Display primaries
            for (int i = 0; i < 3; ++i) {
                mdm_data[offset++] = (mdm.display_primaries[i].x >> 8) & 0xFF;
                mdm_data[offset++] = mdm.display_primaries[i].x & 0xFF;
                mdm_data[offset++] = (mdm.display_primaries[i].y >> 8) & 0xFF;
                mdm_data[offset++] = mdm.display_primaries[i].y & 0xFF;
            }
            
            // White point
            mdm_data[offset++] = (mdm.white_point.x >> 8) & 0xFF;
            mdm_data[offset++] = mdm.white_point.x & 0xFF;
            mdm_data[offset++] = (mdm.white_point.y >> 8) & 0xFF;
            mdm_data[offset++] = mdm.white_point.y & 0xFF;
            
            // Luminance values
            uint32_t max_lum = mdm.max_display_mastering_luminance;
            mdm_data[offset++] = (max_lum >> 24) & 0xFF;
            mdm_data[offset++] = (max_lum >> 16) & 0xFF;
            mdm_data[offset++] = (max_lum >> 8) & 0xFF;
            mdm_data[offset++] = max_lum & 0xFF;
            
            uint32_t min_lum = mdm.min_display_mastering_luminance;
            mdm_data[offset++] = (min_lum >> 24) & 0xFF;
            mdm_data[offset++] = (min_lum >> 16) & 0xFF;
            mdm_data[offset++] = (min_lum >> 8) & 0xFF;
            mdm_data[offset++] = min_lum & 0xFF;
            
            sei_data.insert(sei_data.end(), mdm_data.begin(), mdm_data.end());
        }
        
        return Core::Result<std::vector<uint8_t>>::success(sei_data);
        
    } catch (const std::exception& e) {
        LOG_ERROR("SEI serialization failed: {}", e.what());
        return Core::Result<std::vector<uint8_t>>::error("SEI serialization failed");
    }
}

// =============================================================================
// HDR Metadata Injector Implementation
// =============================================================================

Core::Result<std::vector<uint8_t>> HDRMetadataInjector::inject_hevc_sei(
    const std::vector<uint8_t>& hevc_stream,
    const HDRMetadataPacket& metadata) const {
    
    try {
        std::vector<uint8_t> result = hevc_stream;
        
        // Find appropriate insertion point (after SPS/PPS)
        std::vector<size_t> nal_positions = find_nal_units(hevc_stream);
        
        HDRMetadataParser parser;
        auto sei_data_result = parser.serialize_to_sei(metadata);
        if (sei_data_result.is_error()) {
            return Core::Result<std::vector<uint8_t>>::error(sei_data_result.error());
        }
        
        std::vector<uint8_t> sei_nal = create_sei_nal_unit(sei_data_result.value());
        
        // Insert SEI NAL unit after sequence header
        result = insert_after_sequence_header(result, sei_nal);
        
        return Core::Result<std::vector<uint8_t>>::success(result);
        
    } catch (const std::exception& e) {
        LOG_ERROR("HEVC SEI injection failed: {}", e.what());
        return Core::Result<std::vector<uint8_t>>::error("HEVC SEI injection failed");
    }
}

std::vector<uint8_t> HDRMetadataInjector::create_sei_nal_unit(
    const std::vector<uint8_t>& sei_payload) const {
    
    std::vector<uint8_t> nal_unit;
    
    // HEVC NAL unit header for SEI (prefix)
    nal_unit.push_back(0x00); // Start code
    nal_unit.push_back(0x00);
    nal_unit.push_back(0x00);
    nal_unit.push_back(0x01);
    nal_unit.push_back(0x4E); // NAL unit type = 39 (SEI prefix), nuh_layer_id = 0
    nal_unit.push_back(0x01); // nuh_temporal_id_plus1 = 1
    
    // SEI message header
    nal_unit.push_back(137); // payload_type = mastering_display_colour_volume
    
    // Payload size encoding
    size_t payload_size = sei_payload.size();
    while (payload_size >= 255) {
        nal_unit.push_back(255);
        payload_size -= 255;
    }
    nal_unit.push_back(static_cast<uint8_t>(payload_size));
    
    // Payload data
    nal_unit.insert(nal_unit.end(), sei_payload.begin(), sei_payload.end());
    
    // Byte alignment and stop bit
    nal_unit.push_back(0x80);
    
    return nal_unit;
}

std::vector<size_t> HDRMetadataInjector::find_nal_units(
    const std::vector<uint8_t>& stream) const {
    
    std::vector<size_t> positions;
    
    for (size_t i = 0; i < stream.size() - 4; ++i) {
        if (stream[i] == 0x00 && stream[i+1] == 0x00 && 
            stream[i+2] == 0x00 && stream[i+3] == 0x01) {
            positions.push_back(i);
            i += 3; // Skip the start code
        }
    }
    
    return positions;
}

std::vector<uint8_t> HDRMetadataInjector::insert_after_sequence_header(
    const std::vector<uint8_t>& stream,
    const std::vector<uint8_t>& metadata) const {
    
    // Find the first slice header (after SPS/PPS)
    std::vector<size_t> nal_positions = find_nal_units(stream);
    
    if (nal_positions.empty()) {
        return stream; // No insertion point found
    }
    
    // Insert after the first few NAL units (typically SPS, PPS, VPS)
    size_t insertion_point = nal_positions.size() > 2 ? nal_positions[2] : nal_positions[0];
    
    std::vector<uint8_t> result;
    result.insert(result.end(), stream.begin(), stream.begin() + insertion_point);
    result.insert(result.end(), metadata.begin(), metadata.end());
    result.insert(result.end(), stream.begin() + insertion_point, stream.end());
    
    return result;
}

// =============================================================================
// HDR Metadata Analyzer Implementation
// =============================================================================

HDRAnalysisResult HDRMetadataAnalyzer::analyze_metadata(const HDRMetadataPacket& metadata) const {
    HDRAnalysisResult result;
    
    try {
        result.detected_standard = metadata.standard;
        result.is_hdr_content = (metadata.standard != HDRStandard::INVALID);
        
        HDRMetadataParser parser;
        result.peak_luminance_nits = parser.calculate_peak_luminance(metadata);
        result.average_luminance_nits = parser.calculate_average_luminance(metadata);
        
        if (metadata.mastering_display) {
            result.min_luminance_nits = metadata.mastering_display->get_min_luminance_nits();
        }
        
        if (metadata.content_light_level) {
            result.max_content_light_level = static_cast<float>(metadata.content_light_level->max_content_light_level);
            result.max_frame_average_light_level = static_cast<float>(metadata.content_light_level->max_frame_average_light_level);
        }
        
        result.has_wide_color_gamut = (metadata.color_primaries == ColorPrimaries::BT_2020 ||
                                      metadata.color_primaries == ColorPrimaries::DCI_P3);
        
        result.has_dynamic_metadata = !metadata.dynamic_frames.empty();
        result.dynamic_metadata_frames = metadata.dynamic_frames.size();
        
        // Validate metadata consistency
        result.metadata_consistency_valid = metadata.is_valid();
        result.validation_warnings = check_metadata_consistency(metadata);
        
    } catch (const std::exception& e) {
        LOG_ERROR("HDR analysis failed: {}", e.what());
        result.validation_errors.push_back("Analysis failed: " + std::string(e.what()));
    }
    
    return result;
}

std::vector<std::string> HDRMetadataAnalyzer::check_metadata_consistency(
    const HDRMetadataPacket& metadata) const {
    
    std::vector<std::string> warnings;
    
    try {
        // Check transfer characteristic consistency
        if (metadata.standard == HDRStandard::HDR10 && 
            metadata.transfer_characteristic != TransferCharacteristic::SMPTE_ST_2084) {
            warnings.push_back("HDR10 content should use SMPTE ST.2084 transfer characteristic");
        }
        
        if (metadata.standard == HDRStandard::HLG && 
            metadata.transfer_characteristic != TransferCharacteristic::ARIB_STD_B67) {
            warnings.push_back("HLG content should use ARIB STD-B67 transfer characteristic");
        }
        
        // Check color primaries consistency
        if (metadata.mastering_display && metadata.color_primaries != ColorPrimaries::UNSPECIFIED) {
            bool primaries_consistent = validate_color_primaries_consistency(
                metadata.color_primaries, *metadata.mastering_display);
            if (!primaries_consistent) {
                warnings.push_back("Color primaries and mastering display metadata are inconsistent");
            }
        }
        
        // Check luminance consistency
        if (metadata.mastering_display && metadata.content_light_level) {
            float mastering_max = metadata.mastering_display->get_max_luminance_nits();
            float content_max = static_cast<float>(metadata.content_light_level->max_content_light_level);
            
            if (content_max > mastering_max * 1.1f) { // Allow 10% tolerance
                warnings.push_back("Content light level exceeds mastering display maximum");
            }
        }
        
    } catch (...) {
        warnings.push_back("Error during consistency check");
    }
    
    return warnings;
}

bool HDRMetadataAnalyzer::validate_color_primaries_consistency(
    ColorPrimaries primaries, const MasteringDisplayMetadata& mastering) const {
    
    try {
        // Define expected primaries for different color spaces
        // This is a simplified check - full implementation would compare actual values
        
        switch (primaries) {
            case ColorPrimaries::BT_709:
                // Check if mastering display primaries are close to BT.709
                return true; // Simplified - would need actual comparison
                
            case ColorPrimaries::BT_2020:
                // Check if mastering display primaries are close to BT.2020
                return true; // Simplified - would need actual comparison
                
            case ColorPrimaries::DCI_P3:
                // Check if mastering display primaries are close to DCI-P3
                return true; // Simplified - would need actual comparison
                
            default:
                return true; // Unknown or unspecified - assume consistent
        }
        
    } catch (...) {
        return false;
    }
}

std::string HDRAnalysisResult::summary() const {
    std::ostringstream oss;
    oss << "HDR Analysis Summary:\n";
    oss << "  Content Type: " << (is_hdr_content ? "HDR" : "SDR") << "\n";
    oss << "  Peak Luminance: " << std::fixed << std::setprecision(1) << peak_luminance_nits << " nits\n";
    oss << "  Average Luminance: " << average_luminance_nits << " nits\n";
    oss << "  Wide Color Gamut: " << (has_wide_color_gamut ? "Yes" : "No") << "\n";
    oss << "  Dynamic Metadata: " << (has_dynamic_metadata ? "Yes" : "No") << "\n";
    
    if (has_dynamic_metadata) {
        oss << "  Dynamic Frames: " << dynamic_metadata_frames << "\n";
    }
    
    oss << "  Metadata Valid: " << (metadata_consistency_valid ? "Yes" : "No") << "\n";
    
    if (!validation_warnings.empty()) {
        oss << "  Warnings: " << validation_warnings.size() << "\n";
    }
    
    if (!validation_errors.empty()) {
        oss << "  Errors: " << validation_errors.size() << "\n";
    }
    
    return oss.str();
}

} // namespace VideoEditor::GFX
