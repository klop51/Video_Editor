#pragma once
#include <cstdint>
#include <string>

namespace ve::media_io {

// Legacy broadcast resolutions and standards
enum class LegacyResolution : uint8_t {
    PAL_SD      = 0x01,  // 720x576, 50i (European standard)
    NTSC_SD     = 0x02,  // 720x480, 59.94i (North American standard)
    CIF         = 0x04,  // 352x288, video conferencing standard
    QCIF        = 0x08,  // 176x144, low bandwidth standard
    BROADCAST_SD = PAL_SD | NTSC_SD  // Both broadcast standards
};

// Frame rate specifications for legacy formats
struct LegacyFrameRate {
    uint32_t numerator;
    uint32_t denominator;
    bool drop_frame;
    
    double to_double() const {
        return static_cast<double>(numerator) / denominator;
    }
    
    std::string to_string() const {
        if (drop_frame) {
            return std::to_string(numerator) + "/" + std::to_string(denominator) + " DF";
        }
        return std::to_string(numerator) + "/" + std::to_string(denominator);
    }
};

// Pixel aspect ratio for legacy formats
struct LegacyPixelAspect {
    uint32_t width;
    uint32_t height;
    
    double ratio() const {
        return static_cast<double>(width) / height;
    }
};

// Field order for interlaced content
enum class FieldOrder {
    PROGRESSIVE,         // No interlacing
    TOP_FIELD_FIRST,    // TFF - top field displayed first
    BOTTOM_FIELD_FIRST, // BFF - bottom field displayed first
    UNKNOWN             // Cannot determine field order
};

// Complete legacy format specification
struct LegacyFormatInfo {
    LegacyResolution resolution;
    uint32_t width;
    uint32_t height;
    LegacyFrameRate frame_rate;
    LegacyPixelAspect pixel_aspect;
    bool interlaced;
    FieldOrder field_order;
    std::string standard_name;
    std::string description;
    
    // Helper methods
    bool is_broadcast_standard() const {
        return (resolution & LegacyResolution::BROADCAST_SD) != static_cast<LegacyResolution>(0);
    }
    
    bool is_pal() const {
        return (resolution & LegacyResolution::PAL_SD) != static_cast<LegacyResolution>(0);
    }
    
    bool is_ntsc() const {
        return (resolution & LegacyResolution::NTSC_SD) != static_cast<LegacyResolution>(0);
    }
    
    double display_aspect_ratio() const {
        return (static_cast<double>(width) / height) * pixel_aspect.ratio();
    }
};

// Predefined legacy format specifications
namespace legacy_formats {

// PAL Standard Definition (European broadcast)
constexpr LegacyFormatInfo PAL_SD = {
    LegacyResolution::PAL_SD,
    720, 576,
    {25, 1, false},      // 25 fps, non-drop frame
    {59, 54},            // PAL pixel aspect ratio
    true,                // Interlaced
    FieldOrder::TOP_FIELD_FIRST,
    "PAL",
    "PAL Standard Definition (720x576, 25fps, TFF)"
};

// NTSC Standard Definition (North American broadcast)
constexpr LegacyFormatInfo NTSC_SD = {
    LegacyResolution::NTSC_SD,
    720, 480,
    {30000, 1001, true}, // 29.97 fps, drop frame
    {10, 11},            // NTSC pixel aspect ratio
    true,                // Interlaced
    FieldOrder::BOTTOM_FIELD_FIRST,
    "NTSC",
    "NTSC Standard Definition (720x480, 29.97fps, BFF)"
};

// CIF (Common Intermediate Format) - video conferencing
constexpr LegacyFormatInfo CIF = {
    LegacyResolution::CIF,
    352, 288,
    {25, 1, false},      // 25 fps
    {1, 1},              // Square pixels
    false,               // Progressive
    FieldOrder::PROGRESSIVE,
    "CIF",
    "Common Intermediate Format (352x288, 25fps, Progressive)"
};

// QCIF (Quarter CIF) - low bandwidth
constexpr LegacyFormatInfo QCIF = {
    LegacyResolution::QCIF,
    176, 144,
    {15, 1, false},      // 15 fps
    {1, 1},              // Square pixels
    false,               // Progressive
    FieldOrder::PROGRESSIVE,
    "QCIF",
    "Quarter Common Intermediate Format (176x144, 15fps, Progressive)"
};

} // namespace legacy_formats

// Legacy format detection and utilities
class LegacyFormatDetector {
public:
    // Detect legacy format from resolution and frame rate
    static LegacyFormatInfo detect_format(uint32_t width, uint32_t height, 
                                        double frame_rate, bool interlaced = false);
    
    // Get format info by resolution enum
    static LegacyFormatInfo get_format_info(LegacyResolution resolution);
    
    // Validate if dimensions match a known legacy format
    static bool is_legacy_resolution(uint32_t width, uint32_t height);
    
    // Convert frame rate to legacy frame rate structure
    static LegacyFrameRate convert_frame_rate(double fps, bool prefer_drop_frame = false);
    
    // Detect if content is likely from tape source
    static bool is_tape_source(const LegacyFormatInfo& format, 
                              bool has_timecode = false, 
                              bool has_color_bars = false);
    
private:
    static constexpr double FRAME_RATE_TOLERANCE = 0.1;
};

// Timecode support for legacy formats
struct LegacyTimecode {
    uint8_t hours;
    uint8_t minutes; 
    uint8_t seconds;
    uint8_t frames;
    bool drop_frame;
    LegacyFrameRate frame_rate;
    
    // Convert to total frame count
    uint64_t to_frame_count() const;
    
    // Convert from total frame count
    static LegacyTimecode from_frame_count(uint64_t frames, const LegacyFrameRate& rate);
    
    // Format as string (HH:MM:SS:FF or HH:MM:SS;FF for drop frame)
    std::string to_string() const;
    
    // Parse from string
    static LegacyTimecode from_string(const std::string& tc_string, const LegacyFrameRate& rate);
};

} // namespace ve::media_io
