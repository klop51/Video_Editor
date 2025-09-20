/**
 * @file export_presets.cpp
 * @brief Week 9 Audio Export Pipeline - Export Presets Implementation
 */

#include "audio/export_presets.hpp"
#include "core/log.hpp"
#include <algorithm>

namespace ve::audio {

// Static member initialization
std::unordered_map<std::string, AudioExportPreset> ExportPresetManager::presets_;
bool ExportPresetManager::initialized_ = false;

void ExportPresetManager::initialize() {
    if (initialized_) return;
    
    presets_.clear();
    
    create_broadcast_presets();
    create_web_presets();
    create_archive_presets();
    create_streaming_presets();
    create_mobile_presets();
    
    initialized_ = true;
    ve::log::info("Export preset manager initialized with " + std::to_string(presets_.size()) + " presets");
}

std::vector<AudioExportPreset> ExportPresetManager::get_all_presets() {
    if (!initialized_) initialize();
    
    std::vector<AudioExportPreset> result;
    for (const auto& [name, preset] : presets_) {
        result.push_back(preset);
    }
    
    return result;
}

std::vector<AudioExportPreset> ExportPresetManager::get_presets_by_category(ExportPresetCategory category) {
    if (!initialized_) initialize();
    
    std::vector<AudioExportPreset> result;
    for (const auto& [name, preset] : presets_) {
        if (preset.category == category) {
            result.push_back(preset);
        }
    }
    
    return result;
}

std::vector<AudioExportPreset> ExportPresetManager::get_presets_by_platform(DeliveryPlatform platform) {
    if (!initialized_) initialize();
    
    std::vector<AudioExportPreset> result;
    for (const auto& [name, preset] : presets_) {
        if (preset.platform == platform) {
            result.push_back(preset);
        }
    }
    
    return result;
}

AudioExportPreset ExportPresetManager::get_preset_by_name(const std::string& name) {
    if (!initialized_) initialize();
    
    auto it = presets_.find(name);
    if (it != presets_.end()) {
        return it->second;
    }
    
    // Return default preset if not found
    return QualityPresetFactory::create_broadcast_preset();
}

bool ExportPresetManager::has_preset(const std::string& name) {
    if (!initialized_) initialize();
    return presets_.find(name) != presets_.end();
}

void ExportPresetManager::add_custom_preset(const AudioExportPreset& preset) {
    if (!validate_preset(preset)) {
        ve::log::error("Invalid preset configuration: " + preset.name);
        return;
    }
    
    presets_[preset.name] = preset;
    ve::log::info("Added custom preset: " + preset.name);
}

bool ExportPresetManager::remove_custom_preset(const std::string& name) {
    auto it = presets_.find(name);
    if (it != presets_.end() && it->second.category == ExportPresetCategory::Custom) {
        presets_.erase(it);
        ve::log::info("Removed custom preset: " + name);
        return true;
    }
    return false;
}

AudioExportPreset ExportPresetManager::get_recommended_preset(DeliveryPlatform platform) {
    switch (platform) {
        case DeliveryPlatform::YouTube:
            return get_preset_by_name("YouTube Optimized");
        case DeliveryPlatform::Spotify:
            return get_preset_by_name("Spotify High Quality");
        case DeliveryPlatform::AppleMusic:
            return get_preset_by_name("Apple Music");
        case DeliveryPlatform::Netflix:
            return get_preset_by_name("Netflix Broadcast");
        case DeliveryPlatform::BBC:
            return get_preset_by_name("BBC Broadcast");
        case DeliveryPlatform::Podcast:
            return get_preset_by_name("Podcast Mono");
        case DeliveryPlatform::FilmTV:
            return get_preset_by_name("Broadcast Professional");
        default:
            return QualityPresetFactory::create_broadcast_preset();
    }
}

bool ExportPresetManager::validate_preset(const AudioExportPreset& preset) {
    // Validate export configuration
    if (!preset_utils::validate_export_config(preset.export_config)) {
        return false;
    }
    
    // Validate encoder configuration
    if (!preset_utils::validate_encoder_config(preset.encoder_config)) {
        return false;
    }
    
    // Check format compatibility
    if (!preset_utils::is_format_compatible(preset.preferred_format, preset.export_config)) {
        return false;
    }
    
    return true;
}

std::string ExportPresetManager::get_compliance_info(const AudioExportPreset& preset) {
    std::string info = "Preset: " + preset.name + "\n";
    info += "Category: ";
    
    switch (preset.category) {
        case ExportPresetCategory::Broadcast: info += "Broadcast Professional"; break;
        case ExportPresetCategory::Web: info += "Web Delivery"; break;
        case ExportPresetCategory::Archive: info += "Archival Quality"; break;
        case ExportPresetCategory::Streaming: info += "Streaming Platform"; break;
        case ExportPresetCategory::Mobile: info += "Mobile Optimized"; break;
        case ExportPresetCategory::Custom: info += "Custom"; break;
    }
    
    info += "\nFormat: " + encoder_utils::format_to_string(preset.preferred_format);
    info += "\nSample Rate: " + std::to_string(preset.export_config.sample_rate) + " Hz";
    info += "\nBit Depth: " + std::to_string(preset.export_config.bit_depth) + " bit";
    info += "\nChannels: " + std::to_string(preset.export_config.channel_count);
    
    if (!preset.compliance_standard.empty()) {
        info += "\nCompliance: " + preset.compliance_standard;
    }
    
    if (preset.enable_loudness_normalization) {
        info += "\nLoudness Target: " + std::to_string(preset.target_lufs) + " LUFS";
    }
    
    return info;
}

void ExportPresetManager::create_broadcast_presets() {
    // Broadcast Professional (EBU R128)
    {
        AudioExportPreset preset;
        preset.name = "Broadcast Professional";
        preset.description = "Professional broadcast quality with EBU R128 compliance";
        preset.category = ExportPresetCategory::Broadcast;
        preset.platform = DeliveryPlatform::Generic;
        preset.preferred_format = AudioExportFormat::FLAC;
        
        preset.export_config.format = ExportFormat::FLAC;
        preset.export_config.sample_rate = 48000;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = 24;
        preset.export_config.quality = QualityPreset::Maximum;
        
        preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::FLAC, "broadcast");
        
        preset.enable_loudness_normalization = true;
        preset.target_lufs = -23.0;
        preset.peak_limiter_threshold = -1.0;
        preset.compliance_standard = "EBU R128";
        preset.stereo_compatibility_check = true;
        preset.phase_coherence_check = true;
        
        presets_[preset.name] = preset;
    }
    
    // BBC Broadcast
    {
        AudioExportPreset preset;
        preset.name = "BBC Broadcast";
        preset.description = "BBC broadcast standards with EBU R128 compliance";
        preset.category = ExportPresetCategory::Broadcast;
        preset.platform = DeliveryPlatform::BBC;
        preset.preferred_format = AudioExportFormat::FLAC;
        
        preset.export_config.format = ExportFormat::FLAC;
        preset.export_config.sample_rate = platform_configs::BBCConfig::sample_rate;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = platform_configs::BBCConfig::bit_depth;
        preset.export_config.quality = QualityPreset::Maximum;
        
        preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::FLAC, "broadcast");
        
        preset.enable_loudness_normalization = true;
        preset.target_lufs = platform_configs::BBCConfig::target_lufs;
        preset.peak_limiter_threshold = platform_configs::BBCConfig::peak_limit;
        preset.compliance_standard = platform_configs::BBCConfig::compliance_standard;
        
        presets_[preset.name] = preset;
    }
    
    // Netflix Broadcast
    {
        AudioExportPreset preset;
        preset.name = "Netflix Broadcast";
        preset.description = "Netflix streaming broadcast quality";
        preset.category = ExportPresetCategory::Broadcast;
        preset.platform = DeliveryPlatform::Netflix;
        preset.preferred_format = AudioExportFormat::AAC;
        
        preset.export_config.format = ExportFormat::AAC;
        preset.export_config.sample_rate = platform_configs::NetflixConfig::sample_rate;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = platform_configs::NetflixConfig::bit_depth;
        preset.export_config.quality = QualityPreset::High;
        
        preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::AAC, "broadcast");
        preset.encoder_config.bitrate = platform_configs::NetflixConfig::target_bitrate;
        
        preset.enable_loudness_normalization = true;
        preset.target_lufs = platform_configs::NetflixConfig::target_lufs;
        preset.compliance_standard = platform_configs::NetflixConfig::compliance_standard;
        
        presets_[preset.name] = preset;
    }
}

void ExportPresetManager::create_web_presets() {
    // Web Standard MP3
    {
        AudioExportPreset preset;
        preset.name = "Web Standard MP3";
        preset.description = "Standard web delivery with good quality/size balance";
        preset.category = ExportPresetCategory::Web;
        preset.platform = DeliveryPlatform::Generic;
        preset.preferred_format = AudioExportFormat::MP3;
        
        preset.export_config.format = ExportFormat::MP3;
        preset.export_config.sample_rate = 44100;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = 16;
        preset.export_config.quality = QualityPreset::Standard;
        
        preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::MP3, "web");
        preset.encoder_config.bitrate = 192000;
        preset.encoder_config.vbr_mode = true;
        
        presets_[preset.name] = preset;
    }
    
    // Web High Quality
    {
        AudioExportPreset preset;
        preset.name = "Web High Quality";
        preset.description = "High quality web delivery for critical listening";
        preset.category = ExportPresetCategory::Web;
        preset.platform = DeliveryPlatform::Generic;
        preset.preferred_format = AudioExportFormat::AAC;
        
        preset.export_config.format = ExportFormat::AAC;
        preset.export_config.sample_rate = 48000;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = 16;
        preset.export_config.quality = QualityPreset::High;
        
        preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::AAC, "web");
        preset.encoder_config.bitrate = 256000;
        
        presets_[preset.name] = preset;
    }
}

void ExportPresetManager::create_archive_presets() {
    // Archive Master 96k
    {
        AudioExportPreset preset;
        preset.name = "Archive Master 96k";
        preset.description = "High-resolution archival master with future-proofing";
        preset.category = ExportPresetCategory::Archive;
        preset.platform = DeliveryPlatform::Generic;
        preset.preferred_format = AudioExportFormat::FLAC;
        
        preset.export_config.format = ExportFormat::FLAC;
        preset.export_config.sample_rate = 96000;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = 32;
        preset.export_config.quality = QualityPreset::Maximum;
        
        preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::FLAC, "archive");
        preset.encoder_config.compression_level = 8;
        
        preset.include_metadata = true;
        preset.enable_quality_analysis = true;
        
        presets_[preset.name] = preset;
    }
    
    // Archive Standard
    {
        AudioExportPreset preset;
        preset.name = "Archive Standard";
        preset.description = "Standard archival quality for long-term storage";
        preset.category = ExportPresetCategory::Archive;
        preset.platform = DeliveryPlatform::Generic;
        preset.preferred_format = AudioExportFormat::FLAC;
        
        preset.export_config.format = ExportFormat::FLAC;
        preset.export_config.sample_rate = 48000;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = 24;
        preset.export_config.quality = QualityPreset::High;
        
        preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::FLAC, "broadcast");
        
        preset.include_metadata = true;
        
        presets_[preset.name] = preset;
    }
}

void ExportPresetManager::create_streaming_presets() {
    // YouTube Optimized
    {
        AudioExportPreset preset;
        preset.name = "YouTube Optimized";
        preset.description = "Optimized for YouTube upload with platform compliance";
        preset.category = ExportPresetCategory::Streaming;
        preset.platform = DeliveryPlatform::YouTube;
        preset.preferred_format = AudioExportFormat::AAC;
        
        preset.export_config.format = ExportFormat::AAC;
        preset.export_config.sample_rate = platform_configs::YouTubeConfig::sample_rate;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = platform_configs::YouTubeConfig::bit_depth;
        preset.export_config.quality = QualityPreset::High;
        
        preset.encoder_config = AudioEncoderFactory::get_default_config(AudioExportFormat::AAC);
        preset.encoder_config.bitrate = platform_configs::YouTubeConfig::max_bitrate;
        
        preset.enable_loudness_normalization = true;
        preset.target_lufs = platform_configs::YouTubeConfig::target_lufs;
        
        presets_[preset.name] = preset;
    }
    
    // Spotify High Quality
    {
        AudioExportPreset preset;
        preset.name = "Spotify High Quality";
        preset.description = "High quality for Spotify streaming platform";
        preset.category = ExportPresetCategory::Streaming;
        preset.platform = DeliveryPlatform::Spotify;
        preset.preferred_format = AudioExportFormat::OGG;
        
        preset.export_config.format = ExportFormat::OGG;
        preset.export_config.sample_rate = platform_configs::SpotifyConfig::sample_rate;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = platform_configs::SpotifyConfig::bit_depth;
        preset.export_config.quality = QualityPreset::High;
        
        preset.encoder_config = AudioEncoderFactory::get_default_config(AudioExportFormat::OGG);
        preset.encoder_config.bitrate = platform_configs::SpotifyConfig::target_bitrate;
        
        preset.enable_loudness_normalization = true;
        preset.target_lufs = platform_configs::SpotifyConfig::target_lufs;
        
        presets_[preset.name] = preset;
    }
    
    // Apple Music
    {
        AudioExportPreset preset;
        preset.name = "Apple Music";
        preset.description = "Apple Music platform optimization";
        preset.category = ExportPresetCategory::Streaming;
        preset.platform = DeliveryPlatform::AppleMusic;
        preset.preferred_format = AudioExportFormat::AAC;
        
        preset.export_config.format = ExportFormat::AAC;
        preset.export_config.sample_rate = platform_configs::AppleMusicConfig::sample_rate;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = platform_configs::AppleMusicConfig::bit_depth;
        preset.export_config.quality = QualityPreset::High;
        
        preset.encoder_config = AudioEncoderFactory::get_default_config(AudioExportFormat::AAC);
        preset.encoder_config.bitrate = platform_configs::AppleMusicConfig::target_bitrate;
        
        preset.enable_loudness_normalization = true;
        preset.target_lufs = platform_configs::AppleMusicConfig::target_lufs;
        
        presets_[preset.name] = preset;
    }
    
    // Podcast Mono
    {
        AudioExportPreset preset;
        preset.name = "Podcast Mono";
        preset.description = "Mono podcast optimization for speech content";
        preset.category = ExportPresetCategory::Streaming;
        preset.platform = DeliveryPlatform::Podcast;
        preset.preferred_format = AudioExportFormat::MP3;
        
        preset.export_config.format = ExportFormat::MP3;
        preset.export_config.sample_rate = platform_configs::PodcastConfig::sample_rate;
        preset.export_config.channel_count = platform_configs::PodcastConfig::channels;
        preset.export_config.bit_depth = platform_configs::PodcastConfig::bit_depth;
        preset.export_config.quality = QualityPreset::Standard;
        
        preset.encoder_config = AudioEncoderFactory::get_default_config(AudioExportFormat::MP3);
        preset.encoder_config.bitrate = platform_configs::PodcastConfig::target_bitrate;
        preset.encoder_config.joint_stereo = false; // Not applicable for mono
        
        preset.enable_loudness_normalization = true;
        preset.target_lufs = platform_configs::PodcastConfig::target_lufs;
        
        presets_[preset.name] = preset;
    }
}

void ExportPresetManager::create_mobile_presets() {
    // Mobile Standard
    {
        AudioExportPreset preset;
        preset.name = "Mobile Standard";
        preset.description = "Mobile device optimization for good quality and efficiency";
        preset.category = ExportPresetCategory::Mobile;
        preset.platform = DeliveryPlatform::Generic;
        preset.preferred_format = AudioExportFormat::AAC;
        
        preset.export_config.format = ExportFormat::AAC;
        preset.export_config.sample_rate = 44100;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = 16;
        preset.export_config.quality = QualityPreset::Standard;
        
        preset.encoder_config = AudioEncoderFactory::get_default_config(AudioExportFormat::AAC);
        preset.encoder_config.bitrate = 128000;
        
        presets_[preset.name] = preset;
    }
    
    // Mobile High Quality
    {
        AudioExportPreset preset;
        preset.name = "Mobile High Quality";
        preset.description = "Higher quality mobile optimization for premium content";
        preset.category = ExportPresetCategory::Mobile;
        preset.platform = DeliveryPlatform::Generic;
        preset.preferred_format = AudioExportFormat::AAC;
        
        preset.export_config.format = ExportFormat::AAC;
        preset.export_config.sample_rate = 48000;
        preset.export_config.channel_count = 2;
        preset.export_config.bit_depth = 16;
        preset.export_config.quality = QualityPreset::High;
        
        preset.encoder_config = AudioEncoderFactory::get_default_config(AudioExportFormat::AAC);
        preset.encoder_config.bitrate = 192000;
        
        presets_[preset.name] = preset;
    }
}

// Quality Preset Factory Implementation

AudioExportPreset QualityPresetFactory::create_broadcast_preset(AudioExportFormat format, const std::string& standard) {
    AudioExportPreset preset;
    preset.name = "Custom Broadcast";
    preset.description = "Professional broadcast quality with " + standard + " compliance";
    preset.category = ExportPresetCategory::Broadcast;
    preset.platform = DeliveryPlatform::Generic;
    preset.preferred_format = format;
    
    preset.export_config.format = (format == AudioExportFormat::FLAC) ? ExportFormat::FLAC : ExportFormat::WAV;
    preset.export_config.sample_rate = 48000;
    preset.export_config.channel_count = 2;
    preset.export_config.bit_depth = 24;
    preset.export_config.quality = QualityPreset::Maximum;
    
    preset.encoder_config = AudioEncoderFactory::get_quality_config(format, "broadcast");
    
    preset.enable_loudness_normalization = true;
    preset.target_lufs = (standard == "EBU R128") ? -23.0 : -24.0;
    preset.peak_limiter_threshold = -1.0;
    preset.compliance_standard = standard;
    preset.stereo_compatibility_check = true;
    preset.phase_coherence_check = true;
    
    return preset;
}

AudioExportPreset QualityPresetFactory::create_web_preset(AudioExportFormat format, uint32_t target_bitrate) {
    AudioExportPreset preset;
    preset.name = "Custom Web";
    preset.description = "Web delivery optimization";
    preset.category = ExportPresetCategory::Web;
    preset.platform = DeliveryPlatform::Generic;
    preset.preferred_format = format;
    
    preset.export_config.format = (format == AudioExportFormat::MP3) ? ExportFormat::MP3 : ExportFormat::AAC;
    preset.export_config.sample_rate = 44100;
    preset.export_config.channel_count = 2;
    preset.export_config.bit_depth = 16;
    preset.export_config.quality = QualityPreset::Standard;
    
    preset.encoder_config = AudioEncoderFactory::get_quality_config(format, "web");
    preset.encoder_config.bitrate = target_bitrate;
    
    return preset;
}

AudioExportPreset QualityPresetFactory::create_archive_preset(uint32_t sample_rate, uint32_t bit_depth) {
    AudioExportPreset preset;
    preset.name = "Custom Archive";
    preset.description = "High-quality archival storage";
    preset.category = ExportPresetCategory::Archive;
    preset.platform = DeliveryPlatform::Generic;
    preset.preferred_format = AudioExportFormat::FLAC;
    
    preset.export_config.format = ExportFormat::FLAC;
    preset.export_config.sample_rate = sample_rate;
    preset.export_config.channel_count = 2;
    preset.export_config.bit_depth = bit_depth;
    preset.export_config.quality = QualityPreset::Maximum;
    
    preset.encoder_config = AudioEncoderFactory::get_quality_config(AudioExportFormat::FLAC, "archive");
    
    preset.include_metadata = true;
    preset.enable_quality_analysis = true;
    
    return preset;
}

AudioExportPreset QualityPresetFactory::create_streaming_preset(DeliveryPlatform platform, AudioExportFormat format) {
    AudioExportPreset preset;
    preset.name = "Custom Streaming";
    preset.description = "Streaming platform optimization";
    preset.category = ExportPresetCategory::Streaming;
    preset.platform = platform;
    preset.preferred_format = format;
    
    // Configure based on platform
    switch (platform) {
        case DeliveryPlatform::YouTube:
            preset.export_config.sample_rate = platform_configs::YouTubeConfig::sample_rate;
            preset.target_lufs = platform_configs::YouTubeConfig::target_lufs;
            break;
        case DeliveryPlatform::Spotify:
            preset.export_config.sample_rate = platform_configs::SpotifyConfig::sample_rate;
            preset.target_lufs = platform_configs::SpotifyConfig::target_lufs;
            break;
        case DeliveryPlatform::AppleMusic:
            preset.export_config.sample_rate = platform_configs::AppleMusicConfig::sample_rate;
            preset.target_lufs = platform_configs::AppleMusicConfig::target_lufs;
            break;
        default:
            preset.export_config.sample_rate = 48000;
            preset.target_lufs = -14.0;
            break;
    }
    
    preset.export_config.format = (format == AudioExportFormat::MP3) ? ExportFormat::MP3 : ExportFormat::AAC;
    preset.export_config.channel_count = 2;
    preset.export_config.bit_depth = 16;
    preset.export_config.quality = QualityPreset::High;
    
    preset.encoder_config = AudioEncoderFactory::get_default_config(format);
    preset.enable_loudness_normalization = true;
    
    return preset;
}

AudioExportPreset QualityPresetFactory::create_mobile_preset(AudioExportFormat format, uint32_t target_bitrate) {
    AudioExportPreset preset;
    preset.name = "Custom Mobile";
    preset.description = "Mobile device optimization";
    preset.category = ExportPresetCategory::Mobile;
    preset.platform = DeliveryPlatform::Generic;
    preset.preferred_format = format;
    
    preset.export_config.format = (format == AudioExportFormat::AAC) ? ExportFormat::AAC : ExportFormat::MP3;
    preset.export_config.sample_rate = 44100;
    preset.export_config.channel_count = 2;
    preset.export_config.bit_depth = 16;
    preset.export_config.quality = QualityPreset::Standard;
    
    preset.encoder_config = AudioEncoderFactory::get_default_config(format);
    preset.encoder_config.bitrate = target_bitrate;
    
    return preset;
}

// Utility functions implementation

namespace preset_utils {
    
    bool validate_export_config(const ExportConfig& config) {
        // Validate sample rate
        if (config.sample_rate < 8000 || config.sample_rate > 192000) {
            return false;
        }
        
        // Validate channel count
        if (config.channel_count == 0 || config.channel_count > 8) {
            return false;
        }
        
        // Validate bit depth
        if (config.bit_depth != 8 && config.bit_depth != 16 && 
            config.bit_depth != 24 && config.bit_depth != 32) {
            return false;
        }
        
        return true;
    }
    
    bool validate_encoder_config(const AudioEncoderConfig& config) {
        // Validate sample rate
        if (config.sample_rate < 8000 || config.sample_rate > 192000) {
            return false;
        }
        
        // Validate channel count
        if (config.channel_count == 0 || config.channel_count > 8) {
            return false;
        }
        
        // Validate bitrate for compressed formats
        if (config.bitrate > 0 && (config.bitrate < 32000 || config.bitrate > 1000000)) {
            return false;
        }
        
        return true;
    }
    
    bool is_format_compatible(AudioExportFormat format, const ExportConfig& config) {
        // Check basic compatibility
        switch (format) {
            case AudioExportFormat::MP3:
                return config.sample_rate <= 48000 && config.channel_count <= 2;
            case AudioExportFormat::AAC:
                return config.sample_rate <= 96000 && config.channel_count <= 8;
            case AudioExportFormat::FLAC:
                return config.sample_rate <= 192000 && config.channel_count <= 8;
            case AudioExportFormat::OGG:
                return config.sample_rate <= 96000 && config.channel_count <= 8;
            default:
                return false;
        }
    }
    
    uint32_t get_recommended_bitrate(AudioExportFormat format, QualityPreset quality) {
        std::unordered_map<AudioExportFormat, std::vector<uint32_t>> bitrates = {
            {AudioExportFormat::MP3, {128000, 192000, 256000, 320000}},
            {AudioExportFormat::AAC, {96000, 128000, 192000, 256000}},
            {AudioExportFormat::OGG, {128000, 192000, 256000, 320000}}
        };
        
        auto it = bitrates.find(format);
        if (it == bitrates.end()) return 192000;
        
        int index = static_cast<int>(quality);
        if (index >= 0 && index < static_cast<int>(it->second.size())) {
            return it->second[index];
        }
        
        return 192000; // Default
    }
    
    double calculate_quality_score(const AudioExportPreset& preset) {
        double score = 0.0;
        
        // Sample rate contribution (0-25 points)
        score += std::min(25.0, preset.export_config.sample_rate / 1920.0);
        
        // Bit depth contribution (0-25 points)
        score += (preset.export_config.bit_depth / 32.0) * 25.0;
        
        // Format contribution (0-25 points)
        switch (preset.preferred_format) {
            case AudioExportFormat::FLAC: score += 25.0; break;
            case AudioExportFormat::AAC: score += 20.0; break;
            case AudioExportFormat::OGG: score += 18.0; break;
            case AudioExportFormat::MP3: score += 15.0; break;
        }
        
        // Quality settings contribution (0-25 points)
        switch (preset.export_config.quality) {
            case QualityPreset::Maximum: score += 25.0; break;
            case QualityPreset::High: score += 20.0; break;
            case QualityPreset::Standard: score += 15.0; break;
            case QualityPreset::Draft: score += 10.0; break;
            case QualityPreset::Custom: score += 15.0; break;
        }
        
        return score;
    }
    
    std::vector<std::string> get_compliance_requirements(const std::string& standard) {
        std::vector<std::string> requirements;
        
        if (standard == "EBU R128") {
            requirements = {
                "Loudness: -23 LUFS ±0.5",
                "Peak level: ≤ -1 dBFS",
                "Loudness range: < 20 LU",
                "Sample rate: 48 kHz",
                "Bit depth: ≥ 16 bit"
            };
        } else if (standard == "ATSC A/85") {
            requirements = {
                "Loudness: -24 LUFS ±2",
                "Peak level: ≤ -2 dBFS",
                "Dialogue normalization: enabled"
            };
        } else if (standard.find("Netflix") != std::string::npos) {
            requirements = {
                "Dialog loudness: -27 LUFS",
                "Peak level: ≤ -2 dBFS",
                "Sample rate: 48 kHz",
                "Bit depth: ≥ 16 bit",
                "Format: AAC or AC-3"
            };
        }
        
        return requirements;
    }
    
    bool check_loudness_compliance(double lufs, const std::string& standard) {
        if (standard == "EBU R128") {
            return std::abs(lufs - (-23.0)) <= 0.5;
        } else if (standard == "ATSC A/85") {
            return std::abs(lufs - (-24.0)) <= 2.0;
        } else if (standard.find("Netflix") != std::string::npos) {
            return std::abs(lufs - (-27.0)) <= 1.0;
        }
        
        return true; // No specific standard
    }
    
} // namespace preset_utils

} // namespace ve::audio