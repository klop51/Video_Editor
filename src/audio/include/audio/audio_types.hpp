/**
 * @file audio_types.hpp
 * @brief Common audio type definitions
 * 
 * Shared type definitions to avoid circular dependencies between
 * export_presets.hpp and audio_render_engine.hpp
 */

#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace ve::audio {

// Forward declarations
using NodeID = uint32_t;

/**
 * @brief Audio export format specifications
 */
enum class ExportFormat {
    WAV,        ///< Uncompressed WAV format
    MP3,        ///< MPEG-1 Audio Layer III
    FLAC,       ///< Free Lossless Audio Codec
    AAC,        ///< Advanced Audio Codec
    OGG,        ///< Ogg Vorbis
    AIFF        ///< Audio Interchange File Format
};

/**
 * @brief Audio quality preset levels
 */
enum class QualityPreset {
    Draft,      ///< Fast rendering, lower quality
    Standard,   ///< Balanced quality and speed
    High,       ///< High quality, slower rendering
    Maximum,    ///< Maximum quality, slowest rendering
    Custom      ///< User-defined custom settings
};

/**
 * @brief Audio rendering mode
 */
enum class RenderMode {
    Realtime,   ///< Real-time rendering (for monitoring)
    Offline,    ///< Offline rendering (for export)
    Preview     ///< Low-latency preview rendering
};

/**
 * @brief Export format configuration
 */
struct ExportConfig {
    ExportFormat format = ExportFormat::WAV;
    uint32_t sample_rate = 48000;           ///< Target sample rate
    uint16_t channel_count = 2;             ///< Target channel count
    uint32_t bit_depth = 24;                ///< Bit depth (8, 16, 24, 32)
    QualityPreset quality = QualityPreset::High;
    
    // Format-specific settings
    struct {
        uint32_t bitrate = 320;             ///< MP3/AAC bitrate (kbps)
        bool vbr = true;                    ///< Variable bitrate
        uint32_t compression_level = 5;      ///< FLAC compression (0-8)
        bool joint_stereo = true;           ///< Joint stereo encoding
    } codec_settings;
    
    // Metadata
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string comment;
    uint32_t year = 0;
    uint32_t track_number = 0;
    
    bool normalize_output = false;          ///< Apply output normalization
    double target_lufs = -23.0;            ///< Target LUFS for normalization
    bool apply_dithering = true;           ///< Apply dithering for bit depth reduction
};

/**
 * @brief Multi-track mix-down configuration
 */
struct MixdownConfig {
    struct TrackConfig {
        uint32_t track_id;
        double volume = 1.0;                ///< Track volume (0.0-2.0)
        double pan = 0.0;                   ///< Pan position (-1.0 to 1.0)
        bool muted = false;                 ///< Track mute status (compatible with render engine)
        bool solo = false;                  ///< Track solo status
        
        // EQ settings
        struct {
            bool enabled = false;
            double low_gain = 0.0;          ///< Low frequency gain (dB)
            double mid_gain = 0.0;          ///< Mid frequency gain (dB)
            double high_gain = 0.0;         ///< High frequency gain (dB)
            double low_freq = 80.0;         ///< Low frequency cutoff (Hz)
            double high_freq = 12000.0;     ///< High frequency cutoff (Hz)
        } eq;
        
        // Compression settings
        struct {
            bool enabled = false;
            double threshold = -20.0;        ///< Compression threshold (dB)
            double ratio = 4.0;             ///< Compression ratio
            double attack = 5.0;            ///< Attack time (ms)
            double release = 100.0;         ///< Release time (ms)
            double makeup_gain = 0.0;       ///< Makeup gain (dB)
        } compression;
    };
    
    std::vector<TrackConfig> tracks;
    
    // Master section (compatible with audio_render_engine.cpp expectations)
    double master_volume = 1.0;             ///< Master output volume
    bool enable_master_effects = true;      ///< Apply master effect chain
    std::vector<NodeID> master_effect_chain; ///< Master effects (NodeID type)
    
    // Mix settings
    bool enable_side_chain = false;         ///< Enable side-chain compression
    bool enable_bus_sends = false;          ///< Enable auxiliary bus sends
    uint32_t max_polyphony = 128;           ///< Maximum simultaneous voices
    bool auto_normalize = false;            ///< Auto-normalize mix levels
    double headroom = -6.0;                ///< Target headroom (dB)
    bool dither_output = true;             ///< Apply dithering to final output
};

} // namespace ve::audio