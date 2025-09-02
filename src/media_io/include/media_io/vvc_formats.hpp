#pragma once

#include "decode/vvc_decoder.hpp"
#include <map>
#include <string>
#include <vector>

namespace ve::media_io {

/**
 * VVC (H.266) Format Specifications and Utilities
 * Comprehensive format definitions for Versatile Video Coding support
 */

namespace vvc_formats {

// Standard VVC format configurations
constexpr struct VVCFormatSpec {
    const char* name;
    const char* description;
    decode::VVCProfile profile;
    decode::VVCLevel level;
    decode::VVCTier tier;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t max_fps;
    uint32_t bit_depth;
    decode::VVCChromaFormat chroma_format;
    bool hdr_capable;
    const char* use_case;
} FORMAT_SPECIFICATIONS[] = {
    // Consumer formats
    {
        "VVC_MAIN_10_4K",
        "VVC Main 10 Profile for 4K consumer content",
        decode::VVCProfile::MAIN_10,
        decode::VVCLevel::LEVEL_5_0,
        decode::VVCTier::MAIN,
        3840, 2160, 60, 10,
        decode::VVCChromaFormat::YUV_420,
        true,
        "4K streaming, UHD Blu-ray"
    },
    {
        "VVC_MAIN_10_HD",
        "VVC Main 10 Profile for HD content",
        decode::VVCProfile::MAIN_10,
        decode::VVCLevel::LEVEL_4_0,
        decode::VVCTier::MAIN,
        1920, 1080, 60, 10,
        decode::VVCChromaFormat::YUV_420,
        true,
        "HD streaming, broadcast"
    },
    {
        "VVC_MAIN_12_4K_HDR",
        "VVC Main 12 Profile for 4K HDR content",
        decode::VVCProfile::MAIN_12,
        decode::VVCLevel::LEVEL_5_1,
        decode::VVCTier::HIGH,
        3840, 2160, 120, 12,
        decode::VVCChromaFormat::YUV_420,
        true,
        "Premium 4K HDR, cinema applications"
    },
    
    // Professional formats
    {
        "VVC_444_10_4K",
        "VVC 4:4:4 10-bit for professional 4K production",
        decode::VVCProfile::MAIN_444_10,
        decode::VVCLevel::LEVEL_5_2,
        decode::VVCTier::HIGH,
        4096, 2160, 60, 10,
        decode::VVCChromaFormat::YUV_444,
        true,
        "Professional post-production, mastering"
    },
    {
        "VVC_444_12_4K",
        "VVC 4:4:4 12-bit for high-end 4K production",
        decode::VVCProfile::MAIN_444_12,
        decode::VVCLevel::LEVEL_5_2,
        decode::VVCTier::HIGH,
        4096, 2160, 60, 12,
        decode::VVCChromaFormat::YUV_444,
        true,
        "High-end post-production, archival"
    },
    
    // 8K formats
    {
        "VVC_MAIN_10_8K",
        "VVC Main 10 Profile for 8K content",
        decode::VVCProfile::MAIN_10,
        decode::VVCLevel::LEVEL_6_0,
        decode::VVCTier::MAIN,
        7680, 4320, 60, 10,
        decode::VVCChromaFormat::YUV_420,
        true,
        "8K streaming, next-gen broadcast"
    },
    {
        "VVC_444_12_8K",
        "VVC 4:4:4 12-bit for 8K professional content",
        decode::VVCProfile::MAIN_444_12,
        decode::VVCLevel::LEVEL_6_2,
        decode::VVCTier::HIGH,
        7680, 4320, 120, 12,
        decode::VVCChromaFormat::YUV_444,
        true,
        "8K professional production, cinema"
    },
    
    // Screen content formats
    {
        "VVC_SCC_4K",
        "VVC Screen Content Coding for 4K displays",
        decode::VVCProfile::MAIN_SCC,
        decode::VVCLevel::LEVEL_5_0,
        decode::VVCTier::MAIN,
        3840, 2160, 60, 10,
        decode::VVCChromaFormat::YUV_444,
        false,
        "Desktop sharing, remote display"
    },
    
    // Range extensions
    {
        "VVC_RExt_4K_422",
        "VVC Range Extensions 4:2:2 for professional content",
        decode::VVCProfile::MAIN_RExt,
        decode::VVCLevel::LEVEL_5_1,
        decode::VVCTier::HIGH,
        4096, 2160, 60, 12,
        decode::VVCChromaFormat::YUV_422,
        true,
        "Professional broadcast, acquisition"
    }
};

} // namespace vvc_formats

/**
 * VVC Format Utilities and Management
 */
class VVCFormatUtils {
public:
    // Format specification lookup
    static const vvc_formats::VVCFormatSpec* getFormatSpec(const std::string& format_name);
    static std::vector<std::string> getAvailableFormats();
    static std::vector<const vvc_formats::VVCFormatSpec*> getFormatsForUseCase(const std::string& use_case);
    
    // Profile and level utilities
    static std::string getProfileName(decode::VVCProfile profile);
    static std::string getLevelName(decode::VVCLevel level);
    static std::string getTierName(decode::VVCTier tier);
    
    // Capability calculation
    static decode::VVCLevel calculateRequiredLevel(uint32_t width, uint32_t height, 
                                                  uint32_t fps, uint32_t bit_depth);
    static bool isResolutionSupported(uint32_t width, uint32_t height, decode::VVCLevel level);
    static uint64_t calculateMaxBitrate(decode::VVCLevel level, decode::VVCTier tier);
    
    // Format conversion and compatibility
    static bool canConvertBetweenProfiles(decode::VVCProfile from, decode::VVCProfile to);
    static std::vector<decode::VVCProfile> getCompatibleProfiles(decode::VVCProfile base_profile);
    
    // HDR support analysis
    static bool supportsHDR(decode::VVCProfile profile, uint32_t bit_depth);
    static std::vector<std::string> getHDRTransferFunctions(decode::VVCProfile profile);
    static std::vector<std::string> getHDRColorPrimaries(decode::VVCProfile profile);
    
    // Advanced feature support
    static std::vector<std::string> getSupportedFeatures(decode::VVCProfile profile);
    static bool isFeatureMandatory(const std::string& feature_name, decode::VVCProfile profile);
    static bool isFeatureOptional(const std::string& feature_name, decode::VVCProfile profile);
    
    // Bitrate estimation
    static uint32_t estimateBitrate(uint32_t width, uint32_t height, uint32_t fps,
                                  uint32_t bit_depth, const std::string& content_type);
    static uint32_t estimateBitrateForQuality(uint32_t width, uint32_t height, uint32_t fps,
                                            double target_quality, const std::string& content_type);
    
    // Format recommendation
    struct FormatRecommendation {
        std::string format_name;
        decode::VVCProfile recommended_profile;
        decode::VVCLevel recommended_level;
        decode::VVCTier recommended_tier;
        uint32_t estimated_bitrate_kbps;
        std::vector<std::string> recommended_features;
        std::string rationale;
    };
    
    static FormatRecommendation recommendFormat(uint32_t width, uint32_t height, uint32_t fps,
                                              uint32_t bit_depth, const std::string& use_case,
                                              bool hdr_required = false);
    
    // Codec string generation
    static std::string generateCodecString(const decode::VVCStreamInfo& stream_info);
    static bool parseCodecString(const std::string& codec_string, decode::VVCStreamInfo& stream_info);
    
    // Standards compliance
    static bool validateCompliance(const decode::VVCStreamInfo& stream_info);
    static std::vector<std::string> getComplianceIssues(const decode::VVCStreamInfo& stream_info);
    
    // Future compatibility
    static bool isFutureCompatible(const decode::VVCStreamInfo& stream_info);
    static std::vector<std::string> getFutureCompatibilityWarnings(const decode::VVCStreamInfo& stream_info);

private:
    static std::map<std::string, const vvc_formats::VVCFormatSpec*> format_map_;
    static void initializeFormatMap();
    
    // Level constraint calculations
    static uint64_t getLevelMaxSampleRate(decode::VVCLevel level);
    static uint64_t getLevelMaxPictureSize(decode::VVCLevel level);
    static uint32_t getLevelMaxTileColumns(decode::VVCLevel level);
    static uint32_t getLevelMaxTileRows(decode::VVCLevel level);
};

/**
 * VVC Hardware Acceleration Support
 * Detection and management of VVC hardware acceleration
 */
class VVCHardwareSupport {
public:
    enum class VVCHardwareType {
        NONE,
        INTEGRATED_GPU,     // Integrated graphics VVC decode
        DISCRETE_GPU,       // Dedicated graphics card VVC decode
        DEDICATED_ASIC,     // Specialized VVC decoding chip
        SOFTWARE_OPTIMIZED  // Highly optimized software implementation
    };
    
    struct VVCHardwareInfo {
        VVCHardwareType type = VVCHardwareType::NONE;
        std::string device_name;
        std::string vendor;
        std::string driver_version;
        std::vector<decode::VVCProfile> supported_profiles;
        std::vector<decode::VVCLevel> supported_levels;
        uint32_t max_width = 0;
        uint32_t max_height = 0;
        uint32_t max_fps = 0;
        bool decode_only = true;        // Most early hardware is decode-only
        bool encode_support = false;
        std::vector<std::string> supported_features;
    };
    
    // Hardware detection
    static std::vector<VVCHardwareInfo> detectVVCHardware();
    static bool isVVCHardwareAvailable();
    static VVCHardwareInfo getBestVVCHardware(const decode::VVCStreamInfo& stream_requirements);
    
    // Capability queries
    static bool canHardwareDecode(const decode::VVCStreamInfo& stream_info);
    static bool canHardwareEncode(const decode::VVCStreamInfo& stream_info);
    static std::vector<std::string> getHardwareLimitations(const VVCHardwareInfo& hw_info);
    
    // Performance estimation
    static double estimateHardwareSpeedup(const VVCHardwareInfo& hw_info, 
                                        const decode::VVCStreamInfo& stream_info);
    static uint32_t estimateMaxConcurrentStreams(const VVCHardwareInfo& hw_info,
                                                const decode::VVCStreamInfo& stream_info);
    
    // Fallback strategies
    static bool shouldFallbackToSoftware(const VVCHardwareInfo& hw_info,
                                        const decode::VVCStreamInfo& stream_info);
    static std::string getRecommendedFallbackStrategy(const VVCHardwareInfo& hw_info);

private:
    static std::vector<VVCHardwareInfo> detected_hardware_;
    static bool hardware_detection_complete_;
    
    static void detectIntelVVCSupport(std::vector<VVCHardwareInfo>& hardware_list);
    static void detectNVIDIAVVCSupport(std::vector<VVCHardwareInfo>& hardware_list);
    static void detectAMDVVCSupport(std::vector<VVCHardwareInfo>& hardware_list);
    static void detectQualcommVVCSupport(std::vector<VVCHardwareInfo>& hardware_list);
};

/**
 * VVC Migration and Transition Tools
 * Tools for migrating from HEVC/AV1 to VVC
 */
class VVCMigrationTools {
public:
    // Migration analysis
    struct MigrationAnalysis {
        std::string source_codec;
        std::string source_profile;
        decode::VVCProfile recommended_vvc_profile;
        decode::VVCLevel recommended_vvc_level;
        double estimated_bitrate_savings;      // Percentage savings
        double estimated_quality_improvement;  // VMAF improvement
        std::vector<std::string> migration_benefits;
        std::vector<std::string> migration_challenges;
        std::string migration_timeline;
    };
    
    static MigrationAnalysis analyzeHEVCToVVCMigration(const std::string& hevc_profile,
                                                      uint32_t width, uint32_t height,
                                                      uint32_t current_bitrate_kbps);
    
    static MigrationAnalysis analyzeAV1ToVVCMigration(const std::string& av1_profile,
                                                     uint32_t width, uint32_t height,
                                                     uint32_t current_bitrate_kbps);
    
    // Content analysis for migration planning
    static std::vector<std::string> analyzeContentForVVC(const std::vector<std::string>& file_paths);
    static double calculateMigrationROI(const MigrationAnalysis& analysis, 
                                      uint64_t content_volume_hours);
    
    // Migration strategies
    static std::vector<std::string> getRecommendedMigrationPhases(const MigrationAnalysis& analysis);
    static std::string generateMigrationPlan(const std::vector<MigrationAnalysis>& analyses);
    
    // Compatibility tools
    static bool requiresTranscoding(const std::string& source_format, 
                                  const decode::VVCStreamInfo& target_vvc);
    static std::vector<std::string> getCompatibilityIssues(const std::string& source_format,
                                                          const decode::VVCStreamInfo& target_vvc);

private:
    static double calculateBitrateSavings(const std::string& source_codec, 
                                        decode::VVCProfile target_profile,
                                        uint32_t width, uint32_t height);
    static double calculateQualityImprovement(const std::string& source_codec,
                                            decode::VVCProfile target_profile);
};

} // namespace ve::media_io
