#include "media_io/legacy_formats.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ve::media_io {

LegacyFormatInfo LegacyFormatDetector::detect_format(uint32_t width, uint32_t height, 
                                                   double frame_rate, bool interlaced) {
    // Check for PAL SD (720x576)
    if (width == 720 && height == 576) {
        LegacyFormatInfo format = legacy_formats::PAL_SD;
        
        // Adjust for different PAL frame rates
        if (std::abs(frame_rate - 25.0) < FRAME_RATE_TOLERANCE) {
            format.frame_rate = {25, 1, false};
        } else if (std::abs(frame_rate - 50.0) < FRAME_RATE_TOLERANCE) {
            // PAL progressive or field rate
            format.frame_rate = {50, 1, false};
            format.interlaced = !interlaced; // 50p vs 50i
        }
        
        format.interlaced = interlaced;
        if (!interlaced) {
            format.field_order = FieldOrder::PROGRESSIVE;
        }
        
        return format;
    }
    
    // Check for NTSC SD (720x480)
    if (width == 720 && height == 480) {
        LegacyFormatInfo format = legacy_formats::NTSC_SD;
        
        // Adjust for different NTSC frame rates
        if (std::abs(frame_rate - 29.97) < FRAME_RATE_TOLERANCE) {
            format.frame_rate = {30000, 1001, true}; // Drop frame
        } else if (std::abs(frame_rate - 30.0) < FRAME_RATE_TOLERANCE) {
            format.frame_rate = {30, 1, false}; // Non-drop frame
        } else if (std::abs(frame_rate - 59.94) < FRAME_RATE_TOLERANCE) {
            // NTSC field rate or 60p
            format.frame_rate = {60000, 1001, true};
            format.interlaced = !interlaced; // 59.94p vs 59.94i
        } else if (std::abs(frame_rate - 60.0) < FRAME_RATE_TOLERANCE) {
            format.frame_rate = {60, 1, false};
            format.interlaced = !interlaced;
        }
        
        format.interlaced = interlaced;
        if (!interlaced) {
            format.field_order = FieldOrder::PROGRESSIVE;
        }
        
        return format;
    }
    
    // Check for CIF (352x288)
    if (width == 352 && height == 288) {
        return legacy_formats::CIF;
    }
    
    // Check for QCIF (176x144)
    if (width == 176 && height == 144) {
        return legacy_formats::QCIF;
    }
    
    // Unknown legacy format - create a generic one
    LegacyFormatInfo unknown_format = {};
    unknown_format.resolution = static_cast<LegacyResolution>(0);
    unknown_format.width = width;
    unknown_format.height = height;
    unknown_format.frame_rate = convert_frame_rate(frame_rate);
    unknown_format.pixel_aspect = {1, 1}; // Assume square pixels
    unknown_format.interlaced = interlaced;
    unknown_format.field_order = interlaced ? FieldOrder::UNKNOWN : FieldOrder::PROGRESSIVE;
    unknown_format.standard_name = "Unknown";
    unknown_format.description = "Unknown legacy format (" + 
                                std::to_string(width) + "x" + std::to_string(height) + ")";
    
    return unknown_format;
}

LegacyFormatInfo LegacyFormatDetector::get_format_info(LegacyResolution resolution) {
    switch (resolution) {
        case LegacyResolution::PAL_SD:
            return legacy_formats::PAL_SD;
        case LegacyResolution::NTSC_SD:
            return legacy_formats::NTSC_SD;
        case LegacyResolution::CIF:
            return legacy_formats::CIF;
        case LegacyResolution::QCIF:
            return legacy_formats::QCIF;
        default:
            return {}; // Empty format info
    }
}

bool LegacyFormatDetector::is_legacy_resolution(uint32_t width, uint32_t height) {
    // Check all known legacy resolutions
    return (width == 720 && height == 576) ||  // PAL SD
           (width == 720 && height == 480) ||  // NTSC SD
           (width == 352 && height == 288) ||  // CIF
           (width == 176 && height == 144);    // QCIF
}

LegacyFrameRate LegacyFormatDetector::convert_frame_rate(double fps, bool prefer_drop_frame) {
    // Common legacy frame rates
    if (std::abs(fps - 25.0) < FRAME_RATE_TOLERANCE) {
        return {25, 1, false};
    }
    
    if (std::abs(fps - 29.97) < FRAME_RATE_TOLERANCE) {
        return {30000, 1001, prefer_drop_frame};
    }
    
    if (std::abs(fps - 30.0) < FRAME_RATE_TOLERANCE) {
        return {30, 1, false};
    }
    
    if (std::abs(fps - 50.0) < FRAME_RATE_TOLERANCE) {
        return {50, 1, false};
    }
    
    if (std::abs(fps - 59.94) < FRAME_RATE_TOLERANCE) {
        return {60000, 1001, prefer_drop_frame};
    }
    
    if (std::abs(fps - 60.0) < FRAME_RATE_TOLERANCE) {
        return {60, 1, false};
    }
    
    if (std::abs(fps - 15.0) < FRAME_RATE_TOLERANCE) {
        return {15, 1, false}; // QCIF typical rate
    }
    
    // Convert arbitrary frame rate to rational
    uint32_t numerator = static_cast<uint32_t>(fps * 1000);
    uint32_t denominator = 1000;
    
    // Simplify the fraction
    uint32_t gcd_val = std::__gcd(numerator, denominator);
    numerator /= gcd_val;
    denominator /= gcd_val;
    
    return {numerator, denominator, false};
}

bool LegacyFormatDetector::is_tape_source(const LegacyFormatInfo& format, 
                                        bool has_timecode, bool has_color_bars) {
    // Indicators of tape source:
    // 1. Interlaced broadcast standard format
    // 2. Presence of timecode
    // 3. Presence of color bars
    // 4. Specific frame rates (29.97 drop frame, 25fps)
    
    bool is_broadcast = format.is_broadcast_standard();
    bool is_interlaced = format.interlaced;
    bool is_tape_framerate = 
        (format.frame_rate.numerator == 30000 && format.frame_rate.denominator == 1001) || // 29.97
        (format.frame_rate.numerator == 25 && format.frame_rate.denominator == 1);         // 25
    
    // Strong indicators
    if (has_timecode || has_color_bars) {
        return true;
    }
    
    // Weak indicators - need multiple matches
    int indicators = 0;
    if (is_broadcast) indicators++;
    if (is_interlaced) indicators++;
    if (is_tape_framerate) indicators++;
    
    return indicators >= 2;
}

// Timecode implementation
uint64_t LegacyTimecode::to_frame_count() const {
    double fps = frame_rate.to_double();
    
    if (drop_frame && frame_rate.numerator == 30000 && frame_rate.denominator == 1001) {
        // NTSC drop frame calculation (29.97fps)
        // Drop 2 frames every minute except for every 10th minute
        
        uint64_t total_minutes = hours * 60 + minutes;
        uint64_t drop_count = 2 * (total_minutes - (total_minutes / 10));
        
        uint64_t total_frames = 
            static_cast<uint64_t>(hours * 60 * 60 * fps) +
            static_cast<uint64_t>(minutes * 60 * fps) +
            static_cast<uint64_t>(seconds * fps) +
            frames;
            
        return total_frames - drop_count;
    } else {
        // Non-drop frame calculation
        return static_cast<uint64_t>(hours * 60 * 60 * fps) +
               static_cast<uint64_t>(minutes * 60 * fps) +
               static_cast<uint64_t>(seconds * fps) +
               frames;
    }
}

LegacyTimecode LegacyTimecode::from_frame_count(uint64_t frames, const LegacyFrameRate& rate) {
    LegacyTimecode tc = {};
    tc.frame_rate = rate;
    tc.drop_frame = rate.drop_frame;
    
    double fps = rate.to_double();
    uint64_t remaining_frames = frames;
    
    if (tc.drop_frame && rate.numerator == 30000 && rate.denominator == 1001) {
        // NTSC drop frame reverse calculation
        // This is complex - simplified version
        tc.drop_frame = true;
        
        // Approximate calculation (proper implementation would handle drop frame math)
        uint64_t total_seconds = static_cast<uint64_t>(remaining_frames / fps);
        tc.hours = static_cast<uint8_t>(total_seconds / 3600);
        tc.minutes = static_cast<uint8_t>((total_seconds % 3600) / 60);
        tc.seconds = static_cast<uint8_t>(total_seconds % 60);
        tc.frames = static_cast<uint8_t>(remaining_frames % static_cast<uint64_t>(fps));
    } else {
        // Non-drop frame calculation
        uint64_t frames_per_second = static_cast<uint64_t>(fps);
        uint64_t frames_per_minute = frames_per_second * 60;
        uint64_t frames_per_hour = frames_per_minute * 60;
        
        tc.hours = static_cast<uint8_t>(remaining_frames / frames_per_hour);
        remaining_frames %= frames_per_hour;
        
        tc.minutes = static_cast<uint8_t>(remaining_frames / frames_per_minute);
        remaining_frames %= frames_per_minute;
        
        tc.seconds = static_cast<uint8_t>(remaining_frames / frames_per_second);
        tc.frames = static_cast<uint8_t>(remaining_frames % frames_per_second);
    }
    
    return tc;
}

std::string LegacyTimecode::to_string() const {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << static_cast<int>(hours) << ":"
       << std::setfill('0') << std::setw(2) << static_cast<int>(minutes) << ":"
       << std::setfill('0') << std::setw(2) << static_cast<int>(seconds);
    
    // Use semicolon for drop frame, colon for non-drop frame
    if (drop_frame) {
        ss << ";";
    } else {
        ss << ":";
    }
    
    ss << std::setfill('0') << std::setw(2) << static_cast<int>(frames);
    
    return ss.str();
}

LegacyTimecode LegacyTimecode::from_string(const std::string& tc_string, const LegacyFrameRate& rate) {
    LegacyTimecode tc = {};
    tc.frame_rate = rate;
    
    // Parse format: HH:MM:SS:FF or HH:MM:SS;FF
    if (tc_string.length() != 11) {
        return tc; // Invalid format
    }
    
    try {
        tc.hours = static_cast<uint8_t>(std::stoi(tc_string.substr(0, 2)));
        tc.minutes = static_cast<uint8_t>(std::stoi(tc_string.substr(3, 2)));
        tc.seconds = static_cast<uint8_t>(std::stoi(tc_string.substr(6, 2)));
        tc.frames = static_cast<uint8_t>(std::stoi(tc_string.substr(9, 2)));
        
        // Check drop frame indicator
        tc.drop_frame = (tc_string[8] == ';');
        
    } catch (const std::exception&) {
        // Invalid timecode format - return zero timecode
        tc = {};
        tc.frame_rate = rate;
    }
    
    return tc;
}

} // namespace ve::media_io
