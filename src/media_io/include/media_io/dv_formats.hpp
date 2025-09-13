#pragma once
#include "legacy_formats.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace ve::media_io {

// DV Format family classification
enum class DVFormat {
    UNKNOWN = 0,
    
    // Standard DV formats
    DV25,           // DV standard (25 Mbps) - 720x480/576
    DVCAM,          // Sony DVCAM variant
    
    // Professional DV formats  
    DVCPRO25,       // Panasonic DVCPRO (25 Mbps)
    DVCPRO50,       // Panasonic DVCPRO50 (50 Mbps)
    DVCPRO_HD,      // DVCPRO HD (100 Mbps) - 1280x720, 1440x1080
    
    // HDV formats (MPEG-2 based, but DV tape workflow)
    HDV_720P,       // HDV 720p60/50 (19.7 Mbps)
    HDV_1080I,      // HDV 1080i60/50 (25 Mbps)
    
    // Digital8 (consumer format using DV compression)
    DIGITAL8
};

// DV Format specifications
struct DVFormatSpec {
    DVFormat format;
    std::string name;
    std::string description;
    
    // Video specifications
    uint32_t width;
    uint32_t height;
    LegacyFrameRate frame_rate;
    bool interlaced;
    FieldOrder field_order;
    
    // Technical specs
    uint32_t bitrate_mbps;      // Nominal bitrate in Mbps
    uint32_t track_count;       // Number of DV tracks
    bool professional;          // Professional vs consumer format
    
    // Tape specifications
    bool uses_dv_tape;          // Uses DV cassette format
    bool uses_metal_particle;   // Metal particle vs metal evaporated
    uint32_t max_recording_time_min; // Maximum recording time in minutes
    
    // Color specifications
    bool supports_16_9;         // 16:9 aspect ratio support
    bool supports_progressive;  // Progressive scan support
    uint8_t chroma_subsampling; // 4:1:1 or 4:2:0
};

// Predefined DV format specifications
namespace dv_formats {

constexpr DVFormatSpec DV25_NTSC = {
    DVFormat::DV25, "DV25 NTSC", "Standard DV NTSC (720x480, 29.97fps)",
    720, 480, {30000, 1001, true}, true, FieldOrder::BOTTOM_FIELD_FIRST,
    25, 1, false, true, false, 60,
    true, true, 0x11  // 4:1:1 chroma
};

constexpr DVFormatSpec DV25_PAL = {
    DVFormat::DV25, "DV25 PAL", "Standard DV PAL (720x576, 25fps)", 
    720, 576, {25, 1, false}, true, FieldOrder::TOP_FIELD_FIRST,
    25, 1, false, true, false, 60,
    true, true, 0x20  // 4:2:0 chroma
};

constexpr DVFormatSpec DVCPRO50_NTSC = {
    DVFormat::DVCPRO50, "DVCPRO50 NTSC", "Panasonic DVCPRO50 NTSC (720x480, 29.97fps)",
    720, 480, {30000, 1001, true}, true, FieldOrder::BOTTOM_FIELD_FIRST,
    50, 2, true, true, true, 32,
    true, true, 0x22  // 4:2:2 chroma
};

constexpr DVFormatSpec DVCPRO_HD_720P = {
    DVFormat::DVCPRO_HD, "DVCPRO HD 720p", "DVCPRO HD 720p (1280x720, 59.94fps)",
    1280, 720, {60000, 1001, true}, false, FieldOrder::PROGRESSIVE,
    100, 4, true, true, true, 32,
    false, true, 0x22  // 4:2:2 chroma
};

constexpr DVFormatSpec HDV_1080I_NTSC = {
    DVFormat::HDV_1080I, "HDV 1080i", "HDV 1080i NTSC (1440x1080, 29.97fps)",
    1440, 1080, {30000, 1001, true}, true, FieldOrder::TOP_FIELD_FIRST,
    25, 1, false, true, false, 60,
    true, false, 0x20  // 4:2:0 chroma (MPEG-2)
};

} // namespace dv_formats

// DV data block structure for format detection
struct DVDataBlock {
    uint8_t sync_pattern[3];    // DV sync pattern
    uint8_t block_id;           // Block identifier
    uint8_t sequence_count;     // Sequence counter
    uint8_t format_info;        // Format information byte
    uint8_t reserved[2];        // Reserved bytes
    
    // Format detection helpers
    bool is_valid_sync() const {
        return sync_pattern[0] == 0xFF && 
               sync_pattern[1] == 0xFF && 
               sync_pattern[2] == 0xFF;
    }
    
    bool is_pal() const {
        return (format_info & 0x80) != 0;
    }
    
    bool is_16_9() const {
        return (format_info & 0x07) == 0x02;
    }
    
    uint8_t get_chroma_subsampling() const {
        return (format_info & 0x38) >> 3;
    }
};

// DV Decoder and format detection
class DVDecoder {
public:
    DVDecoder() = default;
    ~DVDecoder() = default;
    
    // Primary detection methods
    bool detectDVFormat(const uint8_t* data, size_t size);
    DVFormat getDVVariant() const { return detected_format_; }
    DVFormatSpec getFormatSpec() const;
    
    // Format analysis
    bool isInterlaced() const;
    bool isWidescreen() const { return wide_screen_; }
    bool isPAL() const { return is_pal_; }
    FieldOrder getFieldOrder() const;
    
    // Timecode extraction
    LegacyTimecode extractTimecode() const;
    bool hasValidTimecode() const { return timecode_valid_; }
    
    // Audio information
    uint8_t getAudioChannels() const { return audio_channels_; }
    uint32_t getAudioSampleRate() const;
    bool hasAudioLocked() const { return audio_locked_; }
    
    // Quality assessment
    uint8_t getDroppedFrameCount() const { return dropped_frames_; }
    float getSignalQuality() const { return signal_quality_; }
    bool hasTapeDropouts() const { return has_dropouts_; }
    
    // Professional features
    bool hasColorBars() const { return has_color_bars_; }
    bool hasBlackBurst() const { return has_black_burst_; }
    std::string getCameraManufacturer() const { return camera_manufacturer_; }
    
private:
    // Detection state
    DVFormat detected_format_ = DVFormat::UNKNOWN;
    bool is_pal_ = false;
    bool wide_screen_ = false;
    bool interlaced_ = true;
    
    // Timecode state
    LegacyTimecode current_timecode_ = {};
    bool timecode_valid_ = false;
    
    // Audio state
    uint8_t audio_channels_ = 2;
    bool audio_locked_ = false;
    
    // Quality metrics
    uint8_t dropped_frames_ = 0;
    float signal_quality_ = 1.0f;
    bool has_dropouts_ = false;
    
    // Professional markers
    bool has_color_bars_ = false;
    bool has_black_burst_ = false;
    std::string camera_manufacturer_;
    
    // Helper methods
    bool analyzeDVBlock(const DVDataBlock& block);
    bool detectFormat(const uint8_t* frame_data);
    void extractAudioInfo(const uint8_t* audio_blocks);
    void extractTimecodeInfo(const uint8_t* subcode_blocks);
    void extractCameraInfo(const uint8_t* aux_blocks);
    
    // Format-specific analyzers
    bool isDVCPRO(const uint8_t* data) const;
    bool isHDV(const uint8_t* data) const;
    bool isDVCAM(const uint8_t* data) const;
    
    static constexpr size_t DV_FRAME_SIZE_NTSC = 120000;  // 120KB per frame
    static constexpr size_t DV_FRAME_SIZE_PAL = 144000;   // 144KB per frame
    static constexpr size_t DV_BLOCK_SIZE = 80;           // DV block size
};

// DV Format utilities
class DVFormatUtils {
public:
    // Format conversion utilities
    static DVFormatSpec getDVFormatSpec(DVFormat format, bool is_pal = false);
    static std::vector<DVFormat> getSupportedFormats();
    static bool isFormatSupported(DVFormat format);
    
    // Compatibility checks
    static bool canConvertBetween(DVFormat from, DVFormat to);
    static LegacyFormatInfo toDVLegacyFormat(const DVFormatSpec& dv_spec);
    
    // Quality assessment
    static float assessDVQuality(const uint8_t* frame_data, size_t size);
    static bool detectTapeDropouts(const uint8_t* frame_data, size_t size);
    
    // Professional workflow helpers
    static bool requiresProfessionalEquipment(DVFormat format);
    static std::string getRecommendedCaptureSettings(DVFormat format);
    static std::vector<std::string> getCompatibleNLEs(DVFormat format);
    
private:
    static constexpr float QUALITY_THRESHOLD_GOOD = 0.85f;
    static constexpr float QUALITY_THRESHOLD_FAIR = 0.70f;
};

// DV Tape workflow integration
struct DVTapeInfo {
    std::string tape_label;
    std::string recording_date;
    LegacyTimecode start_timecode;
    LegacyTimecode end_timecode;
    DVFormat format;
    
    // Tape condition
    float tape_condition;       // 0.0 = poor, 1.0 = excellent
    uint32_t dropout_count;     // Number of detected dropouts
    bool head_cleaning_needed;  // Recommends head cleaning
    
    // Content analysis
    bool has_continuous_timecode;
    bool has_scene_detection_markers;
    std::vector<LegacyTimecode> scene_breaks;
    
    // Archive information
    std::string archive_location;
    std::string digitization_notes;
    bool preservation_priority;
};

} // namespace ve::media_io
