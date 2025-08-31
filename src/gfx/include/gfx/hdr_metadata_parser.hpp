// Week 9: HDR Metadata Parser
// Comprehensive HDR metadata parsing and handling for professional video workflows

#pragma once

#include "core/base_types.hpp"
#include "core/result.hpp"
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <chrono>

namespace VideoEditor::GFX {

// =============================================================================
// HDR Metadata Standards
// =============================================================================

// SMPTE ST 2086 - Mastering Display Color Volume
struct MasteringDisplayMetadata {
    // Display primaries in 0.00002 increments
    struct ChromaticityCoordinate {
        uint16_t x;  // 0-50000 (0.0 to 1.0)
        uint16_t y;  // 0-50000 (0.0 to 1.0)
    };
    
    ChromaticityCoordinate display_primaries[3];  // R, G, B
    ChromaticityCoordinate white_point;
    
    uint32_t max_display_mastering_luminance;  // 0.0001 cd/m² increments
    uint32_t min_display_mastering_luminance;  // 0.0001 cd/m² increments
    
    bool is_valid() const noexcept;
    float get_max_luminance_nits() const noexcept;
    float get_min_luminance_nits() const noexcept;
};

// SMPTE ST 2094-10 - Dynamic Range and Mastering InfoFrame
struct ContentLightLevelInfo {
    uint16_t max_content_light_level;     // MaxCLL in cd/m²
    uint16_t max_frame_average_light_level; // MaxFALL in cd/m²
    
    bool is_valid() const noexcept;
};

// SMPTE ST 2094-40 - Dynamic Metadata (HDR10+)
struct DynamicMetadataFrame {
    uint32_t frame_number;
    uint16_t target_max_pq_quantized;     // Target maximum PQ value
    uint16_t trim_slope;                  // Tone mapping curve slope
    uint16_t trim_offset;                 // Tone mapping curve offset
    uint16_t trim_power;                  // Tone mapping curve power
    uint16_t trim_chroma_weight;          // Chroma preservation weight
    uint16_t trim_saturation_gain;        // Saturation adjustment
    uint16_t ms_weight[9];                // Mastering display weights
    
    bool is_valid() const noexcept;
};

// ITU-R BT.2100 HLG System Parameters
struct HLGSystemParams {
    float system_gamma;                   // Typical 1.2
    float nominal_peak_luminance;         // cd/m²
    float black_level_lift;              // 0.0 - 1.0
    
    bool is_valid() const noexcept;
};

// Color Volume Transform (CVT) Metadata
struct ColorVolumeTransform {
    uint8_t processing_window[4];         // Top, bottom, left, right
    uint16_t targeted_system_display_maximum_luminance;
    bool targeted_system_display_actual_peak_luminance_flag;
    uint8_t num_rows_targeted_system_display_actual_peak_luminance;
    uint8_t num_cols_targeted_system_display_actual_peak_luminance;
    std::vector<uint8_t> targeted_system_display_actual_peak_luminance;
    
    bool is_valid() const noexcept;
};

// =============================================================================
// Metadata Container
// =============================================================================

enum class HDRStandard : uint8_t {
    INVALID = 0,
    HDR10,              // Static HDR with ST.2086 + ST.2094-10
    HDR10_PLUS,         // Dynamic HDR with ST.2094-40
    DOLBY_VISION,       // Dolby proprietary dynamic HDR
    HLG,                // Hybrid Log-Gamma (BBC/NHK)
    TECHNICOLOR_SL_HDR1, // Single Layer HDR
    TECHNICOLOR_SL_HDR2, // Advanced Single Layer HDR
    SAMSUNG_HDR10_PLUS   // Samsung's HDR10+ variant
};

enum class TransferCharacteristic : uint8_t {
    INVALID = 0,
    BT_709 = 1,         // Rec. ITU-R BT.709-6
    UNSPECIFIED = 2,
    BT_470_M = 4,       // Rec. ITU-R BT.470-6 System M
    BT_470_BG = 5,      // Rec. ITU-R BT.470-6 System B, G
    SMPTE_170M = 6,     // SMPTE 170M
    SMPTE_240M = 7,     // SMPTE 240M
    LINEAR = 8,         // Linear transfer
    LOG_100 = 9,        // Logarithmic (100:1 range)
    LOG_316 = 10,       // Logarithmic (100 * Sqrt(10) : 1 range)
    IEC_61966_2_4 = 11, // IEC 61966-2-4
    BT_1361 = 12,       // Rec. ITU-R BT.1361
    IEC_61966_2_1 = 13, // IEC 61966-2-1 (sRGB)
    BT_2020_10 = 14,    // Rec. ITU-R BT.2020 (10-bit)
    BT_2020_12 = 15,    // Rec. ITU-R BT.2020 (12-bit)
    SMPTE_ST_2084 = 16, // PQ (Perceptual Quantizer)
    SMPTE_ST_428 = 17,  // SMPTE ST 428-1
    ARIB_STD_B67 = 18   // HLG (Hybrid Log-Gamma)
};

enum class ColorPrimaries : uint8_t {
    INVALID = 0,
    BT_709 = 1,         // Rec. ITU-R BT.709-6
    UNSPECIFIED = 2,
    BT_470_M = 4,       // Rec. ITU-R BT.470-6 System M
    BT_470_BG = 5,      // Rec. ITU-R BT.470-6 System B, G
    SMPTE_170M = 6,     // SMPTE-170M (1999)
    SMPTE_240M = 7,     // SMPTE-240M (1999)
    FILM = 8,           // Film
    BT_2020 = 9,        // Rec. ITU-R BT.2020-2
    SMPTE_ST_428 = 10,  // SMPTE ST 428-1
    DCI_P3 = 11,        // DCI-P3
    DISPLAY_P3 = 12     // Display P3
};

struct HDRMetadataPacket {
    HDRStandard standard = HDRStandard::INVALID;
    TransferCharacteristic transfer_characteristic = TransferCharacteristic::INVALID;
    ColorPrimaries color_primaries = ColorPrimaries::INVALID;
    
    // Static metadata
    std::optional<MasteringDisplayMetadata> mastering_display;
    std::optional<ContentLightLevelInfo> content_light_level;
    
    // Dynamic metadata
    std::vector<DynamicMetadataFrame> dynamic_frames;
    
    // HLG specific
    std::optional<HLGSystemParams> hlg_params;
    
    // Color volume transform
    std::optional<ColorVolumeTransform> cvt_metadata;
    
    // Timing information
    std::chrono::microseconds timestamp{0};
    uint64_t frame_number = 0;
    
    // Validation
    bool is_valid() const noexcept;
    bool has_static_metadata() const noexcept;
    bool has_dynamic_metadata() const noexcept;
    std::string to_string() const;
};

// =============================================================================
// HDR Metadata Parser
// =============================================================================

class HDRMetadataParser {
public:
    HDRMetadataParser() = default;
    ~HDRMetadataParser() = default;
    
    // Parse metadata from various sources
    Core::Result<HDRMetadataPacket> parse_sei_message(const std::vector<uint8_t>& sei_data,
                                                      uint8_t payload_type) const;
    
    Core::Result<HDRMetadataPacket> parse_hevc_vui(const std::vector<uint8_t>& vui_data) const;
    
    Core::Result<HDRMetadataPacket> parse_av1_metadata(const std::vector<uint8_t>& metadata_obu) const;
    
    Core::Result<HDRMetadataPacket> parse_mkv_color_metadata(const std::vector<uint8_t>& color_data) const;
    
    Core::Result<HDRMetadataPacket> parse_mp4_box(const std::vector<uint8_t>& box_data,
                                                  const std::string& box_type) const;
    
    // Parse dynamic metadata
    Core::Result<std::vector<DynamicMetadataFrame>> parse_hdr10_plus_metadata(
        const std::vector<uint8_t>& metadata) const;
    
    Core::Result<std::vector<DynamicMetadataFrame>> parse_dolby_vision_metadata(
        const std::vector<uint8_t>& metadata) const;
    
    // Validation and conversion
    bool validate_mastering_display_metadata(const MasteringDisplayMetadata& metadata) const noexcept;
    bool validate_content_light_level(const ContentLightLevelInfo& cll) const noexcept;
    bool validate_dynamic_metadata_frame(const DynamicMetadataFrame& frame) const noexcept;
    
    // Utility functions
    HDRStandard detect_hdr_standard(const HDRMetadataPacket& metadata) const noexcept;
    float calculate_peak_luminance(const HDRMetadataPacket& metadata) const noexcept;
    float calculate_average_luminance(const HDRMetadataPacket& metadata) const noexcept;
    
    // Metadata serialization
    Core::Result<std::vector<uint8_t>> serialize_to_sei(const HDRMetadataPacket& metadata) const;
    Core::Result<std::vector<uint8_t>> serialize_to_mp4_box(const HDRMetadataPacket& metadata,
                                                            const std::string& box_type) const;
    
private:
    // Internal parsing helpers
    Core::Result<MasteringDisplayMetadata> parse_mastering_display_sei(
        const uint8_t* data, size_t size) const;
    
    Core::Result<ContentLightLevelInfo> parse_content_light_level_sei(
        const uint8_t* data, size_t size) const;
    
    Core::Result<DynamicMetadataFrame> parse_dynamic_frame_sei(
        const uint8_t* data, size_t size) const;
    
    Core::Result<HLGSystemParams> parse_hlg_params(
        const uint8_t* data, size_t size) const;
    
    // Bit stream reading utilities
    class BitReader {
    public:
        explicit BitReader(const uint8_t* data, size_t size);
        
        uint32_t read_bits(int count);
        uint32_t read_ue_golomb();  // Unsigned Exp-Golomb
        int32_t read_se_golomb();   // Signed Exp-Golomb
        bool has_more_data() const;
        void skip_bits(int count);
        void byte_align();
        
    private:
        const uint8_t* data_;
        size_t size_;
        size_t byte_pos_;
        int bit_pos_;
    };
    
    // Helper functions
    static constexpr uint16_t float_to_chromaticity_coordinate(float value) {
        return static_cast<uint16_t>(value * 50000.0f + 0.5f);
    }
    
    static constexpr float chromaticity_coordinate_to_float(uint16_t value) {
        return static_cast<float>(value) / 50000.0f;
    }
    
    static constexpr uint32_t nits_to_luminance_value(float nits) {
        return static_cast<uint32_t>(nits * 10000.0f + 0.5f);
    }
    
    static constexpr float luminance_value_to_nits(uint32_t value) {
        return static_cast<float>(value) / 10000.0f;
    }
};

// =============================================================================
// HDR Metadata Injector
// =============================================================================

class HDRMetadataInjector {
public:
    explicit HDRMetadataInjector() = default;
    ~HDRMetadataInjector() = default;
    
    // Inject metadata into encoded streams
    Core::Result<std::vector<uint8_t>> inject_hevc_sei(
        const std::vector<uint8_t>& hevc_stream,
        const HDRMetadataPacket& metadata) const;
    
    Core::Result<std::vector<uint8_t>> inject_av1_metadata_obu(
        const std::vector<uint8_t>& av1_stream,
        const HDRMetadataPacket& metadata) const;
    
    Core::Result<std::vector<uint8_t>> inject_h264_sei(
        const std::vector<uint8_t>& h264_stream,
        const HDRMetadataPacket& metadata) const;
    
    // Container-level metadata injection
    Core::Result<void> inject_mp4_metadata(
        const std::string& input_file,
        const std::string& output_file,
        const HDRMetadataPacket& metadata) const;
    
    Core::Result<void> inject_mkv_metadata(
        const std::string& input_file,
        const std::string& output_file,
        const HDRMetadataPacket& metadata) const;
    
    // Dynamic metadata injection
    Core::Result<std::vector<uint8_t>> inject_hdr10_plus_dynamic_metadata(
        const std::vector<uint8_t>& stream,
        const std::vector<DynamicMetadataFrame>& dynamic_frames) const;
    
    // Metadata removal
    Core::Result<std::vector<uint8_t>> remove_hdr_metadata(
        const std::vector<uint8_t>& stream,
        const std::string& codec) const;
    
private:
    // NAL unit utilities
    std::vector<uint8_t> create_sei_nal_unit(const std::vector<uint8_t>& sei_payload) const;
    std::vector<uint8_t> create_metadata_obu(const std::vector<uint8_t>& metadata_payload) const;
    
    // Stream parsing utilities
    std::vector<size_t> find_nal_units(const std::vector<uint8_t>& stream) const;
    std::vector<size_t> find_obu_units(const std::vector<uint8_t>& stream) const;
    
    // Insertion helpers
    std::vector<uint8_t> insert_after_sequence_header(
        const std::vector<uint8_t>& stream,
        const std::vector<uint8_t>& metadata) const;
};

// =============================================================================
// HDR Metadata Analyzer
// =============================================================================

struct HDRAnalysisResult {
    bool is_hdr_content = false;
    HDRStandard detected_standard = HDRStandard::INVALID;
    
    float peak_luminance_nits = 0.0f;
    float average_luminance_nits = 0.0f;
    float min_luminance_nits = 0.0f;
    
    float max_content_light_level = 0.0f;
    float max_frame_average_light_level = 0.0f;
    
    bool has_wide_color_gamut = false;
    float gamut_coverage_percentage = 0.0f;
    
    bool has_dynamic_metadata = false;
    size_t dynamic_metadata_frames = 0;
    
    bool metadata_consistency_valid = true;
    std::vector<std::string> validation_warnings;
    std::vector<std::string> validation_errors;
    
    std::string summary() const;
};

class HDRMetadataAnalyzer {
public:
    explicit HDRMetadataAnalyzer() = default;
    ~HDRMetadataAnalyzer() = default;
    
    // Analyze HDR metadata for compliance and consistency
    HDRAnalysisResult analyze_metadata(const HDRMetadataPacket& metadata) const;
    
    // Analyze actual video content vs metadata
    HDRAnalysisResult analyze_content_vs_metadata(
        const std::vector<float>& luminance_data,
        const HDRMetadataPacket& metadata) const;
    
    // Validate metadata against standards
    bool validate_hdr10_compliance(const HDRMetadataPacket& metadata) const;
    bool validate_hdr10_plus_compliance(const HDRMetadataPacket& metadata) const;
    bool validate_hlg_compliance(const HDRMetadataPacket& metadata) const;
    bool validate_dolby_vision_compliance(const HDRMetadataPacket& metadata) const;
    
    // Recommendation engine
    HDRMetadataPacket recommend_metadata_corrections(
        const HDRMetadataPacket& metadata,
        const HDRAnalysisResult& analysis) const;
    
    // Statistics and reporting
    struct MetadataStatistics {
        size_t total_frames_analyzed = 0;
        size_t hdr_frames_detected = 0;
        float average_peak_luminance = 0.0f;
        float max_peak_luminance = 0.0f;
        float min_peak_luminance = 0.0f;
        std::map<HDRStandard, size_t> standard_distribution;
    };
    
    MetadataStatistics calculate_statistics(
        const std::vector<HDRMetadataPacket>& metadata_sequence) const;
    
private:
    // Validation helpers
    bool validate_chromaticity_coordinates(float x, float y) const noexcept;
    bool validate_luminance_range(float min_nits, float max_nits) const noexcept;
    bool validate_color_primaries_consistency(ColorPrimaries primaries,
                                            const MasteringDisplayMetadata& mastering) const;
    
    // Analysis helpers
    float calculate_gamut_volume(const MasteringDisplayMetadata& mastering) const;
    bool detect_wide_color_gamut(const MasteringDisplayMetadata& mastering) const;
    std::vector<std::string> check_metadata_consistency(const HDRMetadataPacket& metadata) const;
};

} // namespace VideoEditor::GFX
