#pragma once

#include "standards/compliance_engine.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace ve::standards {

/**
 * Comprehensive Broadcast Standards Definitions
 * Industry-standard broadcast specifications and technical requirements
 */

namespace broadcast_standards {

// SMPTE Standards Database
constexpr struct SMPTEStandard {
    const char* id;
    const char* name;
    const char* description;
    const char* scope;
    const char* version;
} SMPTE_STANDARDS[] = {
    {
        "SMPTE ST 2067-2",
        "Interoperable Master Format — Core Constraints",
        "Defines core constraints for IMF packages including track file structure, composition playlist, and assetmap requirements",
        "Digital cinema mastering and distribution",
        "2013"
    },
    {
        "SMPTE ST 2067-3",
        "Interoperable Master Format — Audio Track File",
        "Specifications for audio essence in IMF packages including multi-channel audio, sample rates, and encoding",
        "IMF audio workflows",
        "2013"
    },
    {
        "SMPTE ST 2067-5",
        "Interoperable Master Format — Video Track File",
        "Video essence specifications for IMF including color spaces, frame rates, and compression",
        "IMF video workflows",
        "2013"
    },
    {
        "SMPTE ST 377-1",
        "Material Exchange Format (MXF) — File Format Specification",
        "Defines MXF file format for professional media exchange",
        "Professional media workflows",
        "2019"
    },
    {
        "SMPTE ST 428-1",
        "D-Cinema Distribution Master — Image Characteristics",
        "Digital cinema image format specifications",
        "Digital cinema distribution",
        "2019"
    },
    {
        "SMPTE ST 429-2",
        "D-Cinema Packaging — Digital Cinema Package",
        "Digital cinema package structure and requirements",
        "Digital cinema packaging",
        "2020"
    },
    {
        "SMPTE RDD 18",
        "Operational Guidelines for Interoperable Master Format",
        "Practical guidelines for IMF implementation",
        "IMF operational workflows",
        "2014"
    },
    {
        "SMPTE ST 2084",
        "High Dynamic Range Electro-Optical Transfer Function",
        "Perceptual quantizer (PQ) transfer function for HDR",
        "HDR video workflows",
        "2014"
    },
    {
        "SMPTE ST 2086",
        "Mastering Display Color Volume Metadata",
        "HDR mastering display metadata specification",
        "HDR content mastering",
        "2018"
    }
};

// EBU Standards Database
constexpr struct EBUStandard {
    const char* id;
    const char* name;
    const char* description;
    const char* scope;
    const char* version;
} EBU_STANDARDS[] = {
    {
        "EBU R 128",
        "Loudness normalisation and permitted maximum level of audio signals",
        "Audio loudness measurement and normalization for broadcast",
        "Broadcast audio loudness",
        "2020"
    },
    {
        "EBU R 103",
        "Video Quality in Broadcasting",
        "Technical guidelines for video quality in broadcast production",
        "Broadcast video quality",
        "2000"
    },
    {
        "EBU Tech 3299",
        "High Definition (HD) Video Interfaces",
        "Technical specifications for HD video interfaces",
        "HD broadcast interfaces",
        "2004"
    },
    {
        "EBU Tech 3320",
        "User Requirements for Video Monitors in Television Production",
        "Requirements for professional video monitoring",
        "Broadcast monitoring",
        "2017"
    },
    {
        "EBU Tech 3333",
        "Guidelines for the Distribution of Programmes in HDTV Format",
        "HDTV distribution guidelines and technical parameters",
        "HDTV distribution",
        "2008"
    }
};

// AS-11 Standards (UK DPP)
constexpr struct AS11Standard {
    const char* id;
    const char* name;
    const char* description;
    const char* media_profile;
    const char* version;
} AS11_STANDARDS[] = {
    {
        "AS-11 DPP HD",
        "AS-11 Digital Production Partnership HD",
        "UK DPP technical delivery requirements for HD content",
        "XDCAM HD422 50Mbps",
        "1.1"
    },
    {
        "AS-11 DPP UHD",
        "AS-11 Digital Production Partnership UHD",
        "UK DPP technical delivery requirements for UHD content", 
        "XAVC-I Class480 or XAVC-L",
        "1.0"
    },
    {
        "AS-11 X1",
        "AS-11 AMWA Extended Profile",
        "Extended AS-11 profile with additional metadata requirements",
        "Multiple codecs supported",
        "1.2"
    },
    {
        "AS-11 X7",
        "AS-11 AMWA Acquisition Profile",
        "AS-11 profile for acquisition and production workflows",
        "Long-GOP and I-frame codecs",
        "1.0"
    }
};

} // namespace broadcast_standards

/**
 * Broadcast Technical Specifications Manager
 * Manages technical requirements for broadcast delivery
 */
class BroadcastTechnicalSpecs {
public:
    // Technical specification categories
    enum class SpecCategory {
        VIDEO_CODEC,        // Video codec requirements
        AUDIO_CODEC,        // Audio codec requirements  
        CONTAINER_FORMAT,   // Container format specs
        METADATA,           // Metadata requirements
        QUALITY_CONTROL,    // Quality control thresholds
        DELIVERY_FORMAT,    // Final delivery specifications
        SUBTITLE_CAPTIONS,  // Subtitle and caption requirements
        SECURITY,          // Content protection requirements
        WORKFLOW           // Production workflow requirements
    };
    
    struct TechnicalRequirement {
        std::string parameter_name;
        std::string required_value;
        std::string allowable_values;       // Comma-separated or range
        bool is_mandatory = true;
        std::string description;
        std::string test_method;
        std::string reference_standard;
    };
    
    struct BroadcastProfile {
        std::string profile_name;
        std::string organization;
        std::string version;
        std::string description;
        std::string target_audience;        // "broadcast", "streaming", "cinema", etc.
        
        std::map<SpecCategory, std::vector<TechnicalRequirement>> requirements;
        std::vector<std::string> supported_formats;
        std::vector<std::string> mandatory_metadata;
        std::map<std::string, std::string> quality_thresholds;
    };
    
    // Profile management
    static BroadcastProfile getBroadcastProfile(const std::string& profile_name);
    static std::vector<std::string> getAvailableProfiles();
    static std::vector<std::string> getProfilesByOrganization(const std::string& organization);
    
    // Technical specification queries
    static std::vector<TechnicalRequirement> getRequirements(const std::string& profile_name,
                                                            SpecCategory category);
    static TechnicalRequirement getSpecificRequirement(const std::string& profile_name,
                                                      const std::string& parameter_name);
    
    // Validation helpers
    static bool validateVideoSpec(const quality::FormatValidationReport& report,
                                const std::string& profile_name);
    static bool validateAudioSpec(const quality::FormatValidationReport& report,
                                const std::string& profile_name);
    static bool validateMetadataSpec(const quality::FormatValidationReport& report,
                                   const std::string& profile_name);
    
    // Profile comparison
    static std::map<std::string, std::string> compareProfiles(const std::string& profile1,
                                                             const std::string& profile2);
    static std::vector<std::string> findCompatibleProfiles(const quality::FormatValidationReport& report);

private:
    static std::map<std::string, BroadcastProfile> broadcast_profiles_;
    static void initializeBroadcastProfiles();
    static void loadAS11Profiles();
    static void loadEBUProfiles();
    static void loadSMPTEProfiles();
    static void loadStreamingProfiles();
};

/**
 * Audio Loudness Standards Compliance
 * EBU R128 and other audio loudness standards
 */
class AudioLoudnessStandards {
public:
    enum class LoudnessStandard {
        EBU_R128,           // EBU R128 (-23 LUFS)
        ATSC_A85,           // ATSC A/85 (-24 LKFS)
        ITU_R_BS1770,       // ITU-R BS.1770 measurement
        ARIB_TR_B32,        // ARIB TR-B32 (-24 LKFS)
        AGCOM_664,          // AGCOM 664/13/CONS (-24 LUFS)
        STREAMING_LOUD,     // Streaming loudness (-14 to -16 LUFS)
        CINEMA_STANDARD     // Cinema standard (varies)
    };
    
    struct LoudnessRequirements {
        LoudnessStandard standard;
        double target_loudness_lufs = -23.0;
        double loudness_tolerance_lu = 1.0;
        double max_true_peak_dbfs = -1.0;
        double max_momentary_lufs = -18.0;
        double max_short_term_lufs = -18.0;
        bool enable_dialogue_gating = true;
        std::string measurement_method;
        std::string gating_method;
    };
    
    struct LoudnessMeasurement {
        double integrated_loudness_lufs = 0.0;
        double loudness_range_lu = 0.0;
        double max_true_peak_dbfs = 0.0;
        double max_momentary_lufs = 0.0;
        double max_short_term_lufs = 0.0;
        
        // Compliance status
        bool loudness_compliant = false;
        bool true_peak_compliant = false;
        bool overall_compliant = false;
        
        // Detailed measurements
        std::vector<double> momentary_loudness_timeline;
        std::vector<double> short_term_loudness_timeline;
        std::vector<double> true_peak_timeline;
    };
    
    // Loudness measurement
    static LoudnessMeasurement measureLoudness(const std::vector<float>& audio_data,
                                              uint32_t sample_rate,
                                              uint32_t channels,
                                              LoudnessStandard standard = LoudnessStandard::EBU_R128);
    
    static LoudnessMeasurement measureFileLoudness(const std::string& audio_file_path,
                                                  LoudnessStandard standard = LoudnessStandard::EBU_R128);
    
    // Compliance testing
    static bool testLoudnessCompliance(const LoudnessMeasurement& measurement,
                                     LoudnessStandard standard);
    
    static std::vector<std::string> getLoudnessIssues(const LoudnessMeasurement& measurement,
                                                     LoudnessStandard standard);
    
    // Standard specifications
    static LoudnessRequirements getLoudnessRequirements(LoudnessStandard standard);
    static std::string getLoudnessStandardName(LoudnessStandard standard);
    static std::vector<LoudnessStandard> getApplicableStandards(const std::string& delivery_type);
    
    // Correction recommendations
    static double calculateRequiredGain(const LoudnessMeasurement& measurement,
                                      LoudnessStandard target_standard);
    static std::vector<std::string> getLoudnessCorrectionRecommendations(
        const LoudnessMeasurement& measurement,
        LoudnessStandard target_standard);

private:
    static LoudnessMeasurement calculateR128Loudness(const std::vector<float>& audio_data,
                                                    uint32_t sample_rate,
                                                    uint32_t channels);
    static double calculateTruePeak(const std::vector<float>& audio_data, uint32_t sample_rate);
    static std::vector<double> calculateMomentaryLoudness(const std::vector<float>& audio_data,
                                                         uint32_t sample_rate,
                                                         uint32_t channels);
    static double applyGating(const std::vector<double>& loudness_blocks, const std::string& gating_method);
};

/**
 * Video Quality Standards Compliance
 * Technical video quality requirements for broadcast
 */
class VideoQualityStandards {
public:
    enum class VideoQualityStandard {
        EBU_R103,           // EBU R103 Video Quality
        SMPTE_RP177,        // SMPTE RP 177 Video Quality
        ITU_R_BT601,        // ITU-R BT.601 SD quality
        ITU_R_BT709,        // ITU-R BT.709 HD quality  
        ITU_R_BT2020,       // ITU-R BT.2020 UHD quality
        ATSC_QUALITY,       // ATSC quality guidelines
        DVB_QUALITY,        // DVB quality guidelines
        STREAMING_QUALITY   // Streaming service quality
    };
    
    struct VideoQualityRequirements {
        VideoQualityStandard standard;
        
        // Resolution requirements
        std::vector<std::pair<uint32_t, uint32_t>> allowed_resolutions;
        double min_pixel_aspect_ratio = 1.0;
        double max_pixel_aspect_ratio = 1.0;
        
        // Frame rate requirements
        std::vector<double> allowed_frame_rates;
        bool progressive_required = false;
        bool interlaced_allowed = true;
        
        // Quality thresholds
        double min_psnr_db = 30.0;
        double min_ssim = 0.9;
        double min_vmaf = 70.0;
        double max_blockiness = 0.1;
        double max_blur = 0.1;
        double max_noise = 0.1;
        
        // Temporal requirements
        double max_temporal_variation = 0.05;
        double max_flicker_frequency = 0.0;
        bool scene_change_detection_required = false;
        
        // Color requirements
        std::vector<std::string> allowed_color_spaces;
        std::vector<std::string> allowed_transfer_functions;
        std::vector<uint32_t> allowed_bit_depths;
        
        // Codec requirements
        std::vector<std::string> allowed_codecs;
        std::map<std::string, std::string> codec_parameters;
    };
    
    struct VideoQualityAssessment {
        VideoQualityStandard standard;
        bool overall_compliant = false;
        
        // Quality measurements
        double psnr_db = 0.0;
        double ssim = 0.0;
        double vmaf = 0.0;
        double blockiness = 0.0;
        double blur = 0.0;
        double noise = 0.0;
        double temporal_variation = 0.0;
        
        // Compliance status
        bool resolution_compliant = false;
        bool frame_rate_compliant = false;
        bool quality_compliant = false;
        bool color_compliant = false;
        bool codec_compliant = false;
        
        // Issues
        std::vector<std::string> quality_issues;
        std::vector<std::string> technical_issues;
        std::vector<std::string> recommendations;
    };
    
    // Quality assessment
    static VideoQualityAssessment assessVideoQuality(const quality::FormatValidationReport& format_report,
                                                    const quality::QualityAnalysisReport& quality_report,
                                                    VideoQualityStandard standard);
    
    static VideoQualityAssessment assessVideoFile(const std::string& video_file_path,
                                                 VideoQualityStandard standard,
                                                 const std::string& reference_file_path = "");
    
    // Standard specifications
    static VideoQualityRequirements getVideoQualityRequirements(VideoQualityStandard standard);
    static std::string getVideoQualityStandardName(VideoQualityStandard standard);
    static std::vector<VideoQualityStandard> getApplicableStandards(const std::string& delivery_type);
    
    // Compliance testing
    static bool testVideoQualityCompliance(const VideoQualityAssessment& assessment);
    static std::vector<std::string> getVideoQualityIssues(const VideoQualityAssessment& assessment);
    static std::vector<std::string> getVideoQualityRecommendations(const VideoQualityAssessment& assessment);

private:
    static bool validateResolution(uint32_t width, uint32_t height, const VideoQualityRequirements& requirements);
    static bool validateFrameRate(double frame_rate, const VideoQualityRequirements& requirements);
    static bool validateColorSpace(const std::string& color_space, const VideoQualityRequirements& requirements);
    static bool validateCodec(const std::string& codec, const VideoQualityRequirements& requirements);
};

/**
 * Subtitle and Caption Standards
 * Technical requirements for subtitles and closed captions
 */
class SubtitleCaptionStandards {
public:
    enum class SubtitleStandard {
        EBU_STL,            // EBU STL (Subtitle Text Language)
        SMPTE_TT,           // SMPTE Timed Text
        WebVTT,             // Web Video Text Tracks
        SRT,                // SubRip Text
        ASS_SSA,            // Advanced SubStation Alpha
        DVD_SUB,            // DVD Subtitle format
        PGS,                // Presentation Graphics Stream
        CEA_608,            // CEA-608 Closed Captions
        CEA_708,            // CEA-708 Closed Captions
        IMSC1,              // IMSC1 (TTML Profile)
        SCC,                // Scenarist Closed Caption
        DFXP_TTML          // Distribution Format Exchange Profile
    };
    
    struct SubtitleRequirements {
        SubtitleStandard standard;
        std::vector<std::string> supported_languages;
        uint32_t max_characters_per_line = 40;
        uint32_t max_lines = 2;
        double max_display_duration_seconds = 7.0;
        double min_display_duration_seconds = 0.8;
        double max_reading_speed_cps = 20.0;        // Characters per second
        bool positioning_required = false;
        bool styling_supported = false;
        std::vector<std::string> required_metadata;
    };
    
    struct SubtitleValidation {
        SubtitleStandard standard;
        bool overall_compliant = false;
        
        // Content validation
        bool timing_compliant = false;
        bool length_compliant = false;
        bool reading_speed_compliant = false;
        bool positioning_compliant = false;
        bool encoding_compliant = false;
        
        // Statistics
        uint32_t total_subtitles = 0;
        uint32_t timing_violations = 0;
        uint32_t length_violations = 0;
        uint32_t speed_violations = 0;
        double average_reading_speed_cps = 0.0;
        double max_reading_speed_cps = 0.0;
        
        // Issues
        std::vector<std::string> validation_issues;
        std::vector<std::string> recommendations;
    };
    
    // Subtitle validation
    static SubtitleValidation validateSubtitles(const std::string& subtitle_file_path,
                                               SubtitleStandard standard);
    
    static SubtitleValidation validateEmbeddedSubtitles(const std::string& video_file_path,
                                                       SubtitleStandard standard);
    
    // Standard specifications
    static SubtitleRequirements getSubtitleRequirements(SubtitleStandard standard);
    static std::string getSubtitleStandardName(SubtitleStandard standard);
    static std::vector<SubtitleStandard> getApplicableStandards(const std::string& delivery_type);
    
    // Format conversion validation
    static bool validateSubtitleConversion(const std::string& source_file,
                                         const std::string& target_file,
                                         SubtitleStandard source_standard,
                                         SubtitleStandard target_standard);

private:
    static SubtitleValidation validateEBUSTL(const std::string& subtitle_file_path);
    static SubtitleValidation validateSMPTETT(const std::string& subtitle_file_path);
    static SubtitleValidation validateWebVTT(const std::string& subtitle_file_path);
    static SubtitleValidation validateCEA608(const std::vector<uint8_t>& caption_data);
    static SubtitleValidation validateCEA708(const std::vector<uint8_t>& caption_data);
    
    static double calculateReadingSpeed(const std::string& text, double duration_seconds);
    static bool validateTiming(double start_time, double end_time, double previous_end_time);
    static bool validateTextLength(const std::string& text, uint32_t max_chars_per_line, uint32_t max_lines);
};

/**
 * Delivery Format Specifications
 * Final delivery format requirements for different platforms
 */
class DeliveryFormatSpecs {
public:
    struct DeliverySpecification {
        std::string platform_name;
        std::string specification_version;
        
        // Video requirements
        std::vector<std::string> video_codecs;
        std::vector<std::string> video_profiles;
        std::vector<std::pair<uint32_t, uint32_t>> video_resolutions;
        std::vector<double> video_frame_rates;
        std::vector<uint32_t> video_bitrates_kbps;
        
        // Audio requirements
        std::vector<std::string> audio_codecs;
        std::vector<uint32_t> audio_sample_rates;
        std::vector<uint32_t> audio_channels;
        std::vector<uint32_t> audio_bitrates_kbps;
        
        // Container requirements
        std::vector<std::string> container_formats;
        std::vector<std::string> required_metadata;
        
        // Quality requirements
        std::map<std::string, double> quality_thresholds;
        std::vector<std::string> quality_checks;
        
        // Additional requirements
        bool subtitles_required = false;
        bool closed_captions_required = false;
        bool audio_description_required = false;
        std::vector<std::string> accessibility_requirements;
    };
    
    // Platform specifications
    static DeliverySpecification getBroadcastSpec(const std::string& broadcaster);
    static DeliverySpecification getStreamingSpec(const std::string& platform);
    static DeliverySpecification getCinemaSpec(const std::string& cinema_standard);
    static DeliverySpecification getArchiveSpec(const std::string& archive_standard);
    
    // Validation
    static bool validateDeliveryCompliance(const quality::FormatValidationReport& format_report,
                                         const DeliverySpecification& spec);
    
    static std::vector<std::string> getDeliveryIssues(const quality::FormatValidationReport& format_report,
                                                     const DeliverySpecification& spec);
    
    static std::vector<std::string> getDeliveryRecommendations(const quality::FormatValidationReport& format_report,
                                                             const DeliverySpecification& spec);

private:
    static std::map<std::string, DeliverySpecification> delivery_specs_;
    static void initializeDeliverySpecs();
    static void loadBroadcastSpecs();
    static void loadStreamingSpecs();
    static void loadCinemaSpecs();
    static void loadArchiveSpecs();
};

} // namespace ve::standards
