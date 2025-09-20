/**
 * @file export_presets.hpp
 * @brief Week 9 Audio Export Pipeline - Professional Export Quality Presets
 * 
 * Comprehensive export preset system for professional audio workflows:
 * - Broadcast quality settings (48kHz/24-bit)
 * - Web delivery optimized (44.1kHz/16-bit)
 * - High-quality archival (96kHz/32-bit float)
 * - Format-specific optimizations
 * - Industry standard compliance
 */

#pragma once

#include "audio/ffmpeg_audio_encoder.hpp"
#include "audio/audio_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace ve::audio {

// All required types are now available from included headers

/**
 * @brief Export preset categories
 */
enum class ExportPresetCategory {
    Broadcast,      // Professional broadcast standards
    Web,           // Web delivery optimization
    Archive,       // High-quality archival
    Streaming,     // Streaming platform optimization
    Mobile,        // Mobile device optimization
    Custom         // User-defined preset
};

/**
 * @brief Delivery platform types
 */
enum class DeliveryPlatform {
    Generic,       // Generic export
    YouTube,       // YouTube optimization
    Spotify,       // Spotify optimization
    AppleMusic,    // Apple Music optimization
    Netflix,       // Netflix broadcast standard
    BBC,           // BBC broadcast standard
    Podcast,       // Podcast optimization
    Audiobook,     // Audiobook optimization
    FilmTV,        // Film/TV post-production
    Radio,         // Radio broadcast
    Vinyl,         // Vinyl mastering
    CD            // CD mastering
};

/**
 * @brief Professional audio quality preset
 */
struct AudioExportPreset {
    std::string name;
    std::string description;
    ExportPresetCategory category;
    DeliveryPlatform platform;
    
    // Export configuration
    ExportConfig export_config;
    MixdownConfig mixdown_config;
    
    // Encoder-specific settings
    AudioEncoderConfig encoder_config;
    AudioExportFormat preferred_format;
    
    // Quality control settings
    bool enable_loudness_normalization = false;
    double target_lufs = -23.0;             // EBU R128 standard
    double peak_limiter_threshold = -1.0;   // dBFS
    bool enable_quality_analysis = false;
    
    // Metadata settings
    bool include_metadata = true;
    bool include_cover_art = false;
    
    // Default metadata template
    struct {
        std::string title;
        std::string artist;
        std::string album;
        std::string genre;
        std::string comment;
        uint32_t year = 0;
        uint32_t track = 0;
    } metadata;
    
    // Professional compliance
    std::string compliance_standard;         // e.g., "EBU R128", "ATSC A/85"
    bool stereo_compatibility_check = false;
    bool phase_coherence_check = false;
};

/**
 * @brief Export preset manager for professional workflows
 */
class ExportPresetManager {
public:
    /**
     * @brief Initialize preset manager with built-in presets
     */
    static void initialize();

    /**
     * @brief Get all available presets
     */
    static std::vector<AudioExportPreset> get_all_presets();

    /**
     * @brief Get presets by category
     */
    static std::vector<AudioExportPreset> get_presets_by_category(ExportPresetCategory category);

    /**
     * @brief Get presets by platform
     */
    static std::vector<AudioExportPreset> get_presets_by_platform(DeliveryPlatform platform);

    /**
     * @brief Get preset by name
     */
    static AudioExportPreset get_preset_by_name(const std::string& name);

    /**
     * @brief Check if preset exists
     */
    static bool has_preset(const std::string& name);

    /**
     * @brief Add custom preset
     */
    static void add_custom_preset(const AudioExportPreset& preset);

    /**
     * @brief Remove custom preset
     */
    static bool remove_custom_preset(const std::string& name);

    /**
     * @brief Get recommended preset for platform
     */
    static AudioExportPreset get_recommended_preset(DeliveryPlatform platform);

    /**
     * @brief Validate preset configuration
     */
    static bool validate_preset(const AudioExportPreset& preset);

    /**
     * @brief Get preset compliance information
     */
    static std::string get_compliance_info(const AudioExportPreset& preset);

private:
    static std::unordered_map<std::string, AudioExportPreset> presets_;
    static bool initialized_;
    
    static void create_broadcast_presets();
    static void create_web_presets();
    static void create_archive_presets();
    static void create_streaming_presets();
    static void create_mobile_presets();
};

/**
 * @brief Professional quality preset factory
 */
class QualityPresetFactory {
public:
    /**
     * @brief Create broadcast quality preset
     * 
     * Professional broadcast standards:
     * - 48kHz/24-bit for professional workflows
     * - EBU R128 loudness compliance (-23 LUFS)
     * - Peak limiting at -1 dBFS
     * - Stereo compatibility validation
     */
    static AudioExportPreset create_broadcast_preset(
        AudioExportFormat format = AudioExportFormat::FLAC,
        const std::string& standard = "EBU R128"
    );

    /**
     * @brief Create web delivery preset
     * 
     * Optimized for web streaming:
     * - 44.1kHz/16-bit for compatibility
     * - Optimized compression settings
     * - Small file size with quality balance
     * - Fast encoding for rapid deployment
     */
    static AudioExportPreset create_web_preset(
        AudioExportFormat format = AudioExportFormat::MP3,
        uint32_t target_bitrate = 192000
    );

    /**
     * @brief Create archival quality preset
     * 
     * High-quality archival storage:
     * - 96kHz/32-bit float for future-proofing
     * - Lossless compression (FLAC)
     * - Maximum quality preservation
     * - Complete metadata inclusion
     */
    static AudioExportPreset create_archive_preset(
        uint32_t sample_rate = 96000,
        uint32_t bit_depth = 32
    );

    /**
     * @brief Create streaming platform preset
     * 
     * Platform-specific optimization:
     * - Platform-recommended settings
     * - Loudness normalization compliance
     * - Format optimization for platform
     * - Metadata requirements
     */
    static AudioExportPreset create_streaming_preset(
        DeliveryPlatform platform,
        AudioExportFormat format = AudioExportFormat::AAC
    );

    /**
     * @brief Create mobile optimization preset
     * 
     * Mobile device optimization:
     * - Efficient compression for storage
     * - Battery-efficient decoding
     * - Network-friendly file sizes
     * - Mobile player compatibility
     */
    static AudioExportPreset create_mobile_preset(
        AudioExportFormat format = AudioExportFormat::AAC,
        uint32_t target_bitrate = 128000
    );
};

/**
 * @brief Platform-specific export configurations
 */
namespace platform_configs {
    
    /**
     * @brief YouTube audio requirements
     */
    struct YouTubeConfig {
        static constexpr uint32_t sample_rate = 48000;
        static constexpr uint32_t bit_depth = 16;
        static constexpr uint32_t max_bitrate = 320000;
        static constexpr double target_lufs = -14.0;      // YouTube normalization
        static inline const char* preferred_format = "AAC";
    };

    /**
     * @brief Spotify audio requirements
     */
    struct SpotifyConfig {
        static constexpr uint32_t sample_rate = 44100;
        static constexpr uint32_t bit_depth = 16;
        static constexpr uint32_t target_bitrate = 320000;
        static constexpr double target_lufs = -14.0;      // Spotify normalization
        static inline const char* preferred_format = "OGG";
    };

    /**
     * @brief Apple Music requirements
     */
    struct AppleMusicConfig {
        static constexpr uint32_t sample_rate = 48000;
        static constexpr uint32_t bit_depth = 24;
        static constexpr uint32_t target_bitrate = 256000;
        static constexpr double target_lufs = -16.0;      // Apple normalization
        static inline const char* preferred_format = "AAC";
    };

    /**
     * @brief Netflix broadcast requirements
     */
    struct NetflixConfig {
        static constexpr uint32_t sample_rate = 48000;
        static constexpr uint32_t bit_depth = 24;
        static constexpr uint32_t target_bitrate = 320000;
        static constexpr double target_lufs = -27.0;      // Netflix dialog normalization
        static inline const char* preferred_format = "AAC";
        static inline const char* compliance_standard = "Netflix Audio Specifications";
    };

    /**
     * @brief BBC broadcast requirements
     */
    struct BBCConfig {
        static constexpr uint32_t sample_rate = 48000;
        static constexpr uint32_t bit_depth = 24;
        static constexpr double target_lufs = -23.0;      // EBU R128
        static constexpr double peak_limit = -1.0;
        static inline const char* preferred_format = "FLAC";
        static inline const char* compliance_standard = "EBU R128";
    };

    /**
     * @brief Podcast optimization
     */
    struct PodcastConfig {
        static constexpr uint32_t sample_rate = 44100;
        static constexpr uint32_t bit_depth = 16;
        static constexpr uint16_t channels = 1;           // Mono for speech
        static constexpr uint32_t target_bitrate = 96000;
        static constexpr double target_lufs = -16.0;      // Podcast loudness
        static inline const char* preferred_format = "MP3";
    };

} // namespace platform_configs

/**
 * @brief Preset validation utilities
 */
namespace preset_utils {
    
    /**
     * @brief Validate export configuration
     */
    bool validate_export_config(const ExportConfig& config);

    /**
     * @brief Validate encoder configuration
     */
    bool validate_encoder_config(const AudioEncoderConfig& config);

    /**
     * @brief Check format compatibility
     */
    bool is_format_compatible(AudioExportFormat format, const ExportConfig& config);

    /**
     * @brief Get recommended bitrate for quality
     */
    uint32_t get_recommended_bitrate(AudioExportFormat format, QualityPreset quality);

    /**
     * @brief Calculate export quality score
     */
    double calculate_quality_score(const AudioExportPreset& preset);

    /**
     * @brief Get compliance requirements
     */
    std::vector<std::string> get_compliance_requirements(const std::string& standard);

    /**
     * @brief Check loudness compliance
     */
    bool check_loudness_compliance(double lufs, const std::string& standard);

} // namespace preset_utils

} // namespace ve::audio