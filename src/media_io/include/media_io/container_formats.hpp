#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

namespace ve {
namespace media_io {

/**
 * @brief Professional container format enumeration
 * 
 * Supports major professional container formats used in broadcast,
 * cinema, and high-end production workflows.
 */
enum class ContainerFormat {
    UNKNOWN = 0,
    
    // Professional Broadcast Formats
    MXF,                    // Material Exchange Format (SMPTE 377M)
    GXF,                    // General Exchange Format (SMPTE 360M) 
    LXF,                    // Layered Exchange Format (Leitch)
    
    // Professional Editing Formats
    MOV_PRORES,             // QuickTime with ProRes codec
    MOV_DNXHD,              // QuickTime with DNxHD codec
    AVI_DNXHD,              // AVI with DNxHD wrapper
    
    // High-End Container Formats
    MKV_PROFESSIONAL,       // Matroska with professional features
    MP4_PROFESSIONAL,       // MP4 with extended metadata
    
    // Cinema Formats
    DCP,                    // Digital Cinema Package
    IMF,                    // Interoperable Master Format
    
    // Archive Formats  
    BWF,                    // Broadcast Wave Format
    RF64,                   // 64-bit WAV extension
    
    // Streaming Professional
    MPEG_TS,                // MPEG Transport Stream
    MPEG_PS,                // MPEG Program Stream
    
    // Legacy Professional
    OMF,                    // Open Media Framework
    AAF,                    // Advanced Authoring Format
    
    COUNT
};

/**
 * @brief Timecode format specifications
 */
enum class TimecodeFormat {
    NONE = 0,
    SMPTE_NON_DROP,         // 30fps, 25fps, 24fps
    SMPTE_DROP_FRAME,       // 29.97fps with drop frame compensation
    EBU,                    // European Broadcasting Union
    FILM_24,                // 24fps film standard
    NTSC,                   // 29.97fps NTSC
    PAL,                    // 25fps PAL
    COUNT
};

/**
 * @brief Audio track types for professional workflows
 */
enum class AudioTrackType {
    UNKNOWN = 0,
    MONO,                   // Single channel
    STEREO,                 // L/R stereo pair
    SURROUND_5_1,          // 5.1 surround sound
    SURROUND_7_1,          // 7.1 surround sound
    ATMOS,                 // Dolby Atmos object audio
    AMBISONIC,             // 360-degree spatial audio
    COUNT
};

/**
 * @brief Closed caption and subtitle formats
 */
enum class CaptionFormat {
    NONE = 0,
    CEA_608,               // Line 21 closed captions
    CEA_708,               // Digital TV captions
    SCC,                   // Scenarist Closed Caption
    SRT,                   // SubRip Text
    VTT,                   // WebVTT
    TTML,                  // Timed Text Markup Language
    STL,                   // Spruce Subtitle Format
    COUNT
};

/**
 * @brief Container metadata structure
 */
struct ContainerMetadata {
    // Basic Information
    std::string title;
    std::string description;
    std::string creator;
    std::string creation_date;
    
    // Professional Metadata
    std::string project_name;
    std::string scene;
    std::string take;
    std::string reel;
    std::string camera_id;
    
    // Technical Metadata
    TimecodeFormat timecode_format = TimecodeFormat::NONE;
    std::string start_timecode;      // HH:MM:SS:FF format
    uint32_t frame_rate_num = 0;
    uint32_t frame_rate_den = 1;
    
    // Audio Metadata
    std::vector<AudioTrackType> audio_tracks;
    bool has_embedded_audio = false;
    
    // Caption/Subtitle Metadata
    std::vector<CaptionFormat> caption_formats;
    
    // Broadcast Metadata (AS-11, EBU R128)
    std::string program_title;
    std::string episode_title;
    std::string series_title;
    float loudness_lufs = 0.0f;      // EBU R128 loudness
    float true_peak_dbfs = 0.0f;     // True peak level
    
    // Quality Control
    std::string qc_status;
    std::string delivery_status;
    
    ContainerMetadata() = default;
};

/**
 * @brief Track information for multi-track containers
 */
struct TrackInfo {
    uint32_t track_id = 0;
    std::string track_name;
    std::string codec_name;
    std::string language;           // ISO 639-2 language code
    bool is_default = false;
    bool is_forced = false;
    
    // Video specific
    uint32_t width = 0;
    uint32_t height = 0;
    std::string pixel_format;
    
    // Audio specific
    uint32_t channels = 0;
    uint32_t sample_rate = 0;
    std::string channel_layout;
    
    TrackInfo() = default;
};

/**
 * @brief Professional container format support class
 * 
 * Handles detection, parsing, and metadata extraction for professional
 * video container formats used in broadcast and cinema workflows.
 */
class ContainerFormatSupport {
public:
    ContainerFormatSupport();
    ~ContainerFormatSupport();
    
    // Format Detection
    ContainerFormat detectFormat(const std::string& file_path);
    ContainerFormat detectFormatFromData(const uint8_t* data, size_t size);
    bool isFormatSupported(ContainerFormat format);
    
    // Metadata Operations
    bool extractMetadata(const std::string& file_path, ContainerMetadata& metadata);
    bool extractTrackInfo(const std::string& file_path, std::vector<TrackInfo>& tracks);
    
    // Professional Features
    bool hasTimecode(const std::string& file_path);
    std::string extractTimecode(const std::string& file_path);
    bool hasMultipleAudioTracks(const std::string& file_path);
    bool hasClosedCaptions(const std::string& file_path);
    
    // Format Information
    std::string getFormatName(ContainerFormat format);
    std::string getFormatDescription(ContainerFormat format);
    std::vector<std::string> getSupportedExtensions(ContainerFormat format);
    
    // Broadcasting Standards
    bool isBroadcastCompliant(const std::string& file_path);
    bool validateAS11Metadata(const ContainerMetadata& metadata);
    bool validateEBUR128Audio(const std::string& file_path);
    
    // Utility Functions
    std::vector<ContainerFormat> getSupportedFormats();
    bool isContainerExtension(const std::string& extension);
    ContainerFormat extensionToFormat(const std::string& extension);
    
    // Performance Estimation
    bool supportsRandomAccess(ContainerFormat format);
    bool supportsStreaming(ContainerFormat format);
    size_t estimateHeaderSize(ContainerFormat format);
    
private:
    // Format Detection Helpers
    bool detectMXF(const uint8_t* data, size_t size);
    bool detectGXF(const uint8_t* data, size_t size);
    bool detectLXF(const uint8_t* data, size_t size);
    bool detectDCP(const uint8_t* data, size_t size);
    bool detectIMF(const uint8_t* data, size_t size);
    bool detectOMF(const uint8_t* data, size_t size);
    bool detectAAF(const uint8_t* data, size_t size);
    
    // Metadata Parsers
    bool parseMXFMetadata(const std::string& file_path, ContainerMetadata& metadata);
    bool parseGXFMetadata(const std::string& file_path, ContainerMetadata& metadata);
    bool parseQuickTimeMetadata(const std::string& file_path, ContainerMetadata& metadata);
    bool parseMatroskaMetadata(const std::string& file_path, ContainerMetadata& metadata);
    
    // Timecode Handlers
    std::string extractSMPTETimecode(const std::string& file_path);
    std::string extractEBUTimecode(const std::string& file_path);
    TimecodeFormat detectTimecodeFormat(const std::string& timecode_string);
    
    // Audio Analysis
    std::vector<AudioTrackType> analyzeAudioTracks(const std::string& file_path);
    bool measureEBUR128Loudness(const std::string& file_path, float& lufs, float& true_peak);
    
    // Caption Detection
    std::vector<CaptionFormat> detectCaptions(const std::string& file_path);
    bool extractCEA608(const std::string& file_path);
    bool extractCEA708(const std::string& file_path);
    
    // Broadcasting Validation
    bool validateSMPTECompliance(const std::string& file_path);
    bool validateEBUCompliance(const std::string& file_path);
    bool validateDPPCompliance(const std::string& file_path);  // UK Digital Production Partnership
    
    // Internal data structures
    std::unordered_map<std::string, ContainerFormat> extension_map_;
    std::unordered_map<ContainerFormat, std::string> format_names_;
    std::unordered_map<ContainerFormat, std::string> format_descriptions_;
    std::unordered_map<ContainerFormat, std::vector<std::string>> format_extensions_;
    
    // Configuration
    void initializeFormatMappings();
    void initializeExtensionMappings();
    void initializeBroadcastStandards();
};

/**
 * @brief Utility functions for container format handling
 */
namespace container_utils {
    
    // Timecode Utilities
    std::string timecodeToString(uint32_t hours, uint32_t minutes, uint32_t seconds, uint32_t frames);
    bool parseTimecodeString(const std::string& timecode, uint32_t& hours, uint32_t& minutes, uint32_t& seconds, uint32_t& frames);
    bool isDropFrameTimecode(const std::string& timecode);
    uint64_t timecodeToFrames(const std::string& timecode, float frame_rate);
    std::string framesToTimecode(uint64_t frames, float frame_rate, bool drop_frame = false);
    
    // Audio Utilities
    std::string channelLayoutToString(AudioTrackType track_type);
    uint32_t getChannelCount(AudioTrackType track_type);
    bool isLosslessAudio(const std::string& codec_name);
    
    // Broadcasting Utilities
    bool isValidBroadcastFrameRate(float frame_rate);
    bool isValidBroadcastResolution(uint32_t width, uint32_t height);
    std::string generateUMID();  // Unique Material Identifier
    
    // Quality Control
    bool validateContainerIntegrity(const std::string& file_path);
    std::vector<std::string> checkBroadcastCompliance(const std::string& file_path);
    
} // namespace container_utils

} // namespace media_io
} // namespace ve
