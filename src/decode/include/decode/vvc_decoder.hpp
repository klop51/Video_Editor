#pragma once

#include "core/frame.hpp"
#include "decoder_interface.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declarations for VVC libraries
struct vvdec_context;
struct vvenc_context;

namespace ve::decode {

/**
 * VVC (H.266) Decoder - Early Adoption Framework
 * Experimental support for Versatile Video Coding (ITU-T H.266 / ISO/IEC 23090-3)
 * Built for future-ready infrastructure as VVC adoption increases
 */

enum class VVCProfile {
    MAIN_10 = 1,        // Main 10-bit profile
    MAIN_12 = 2,        // Main 12-bit profile  
    MAIN_444_10 = 3,    // Main 4:4:4 10-bit profile
    MAIN_444_12 = 4,    // Main 4:4:4 12-bit profile
    MAIN_RExt = 5,      // Range Extensions profile
    MAIN_SCC = 6        // Screen Content Coding profile
};

enum class VVCLevel {
    LEVEL_1_0 = 16,     // Level 1.0 (Up to CIF)
    LEVEL_2_0 = 32,     // Level 2.0 (Up to 480p)
    LEVEL_2_1 = 35,     // Level 2.1 (Up to WVGA)
    LEVEL_3_0 = 48,     // Level 3.0 (Up to 720p)
    LEVEL_3_1 = 51,     // Level 3.1 (Up to 720p higher bitrate)
    LEVEL_4_0 = 64,     // Level 4.0 (Up to 1080p)
    LEVEL_4_1 = 67,     // Level 4.1 (Up to 1080p higher bitrate)
    LEVEL_5_0 = 80,     // Level 5.0 (Up to 4K)
    LEVEL_5_1 = 83,     // Level 5.1 (Up to 4K higher bitrate)
    LEVEL_5_2 = 86,     // Level 5.2 (Up to 4K highest bitrate)
    LEVEL_6_0 = 96,     // Level 6.0 (Up to 8K)
    LEVEL_6_1 = 99,     // Level 6.1 (Up to 8K higher bitrate)
    LEVEL_6_2 = 102     // Level 6.2 (Up to 8K highest bitrate)
};

enum class VVCTier {
    MAIN = 0,           // Main tier (standard constraints)
    HIGH = 1            // High tier (relaxed constraints for higher bitrates)
};

enum class VVCChromaFormat {
    MONOCHROME = 0,     // 4:0:0 (grayscale)
    YUV_420 = 1,        // 4:2:0 chroma subsampling
    YUV_422 = 2,        // 4:2:2 chroma subsampling
    YUV_444 = 3         // 4:4:4 no chroma subsampling
};

struct VVCAdvancedFeatures {
    // VVC-specific coding tools
    bool qtbt_enabled = false;              // Quad-tree plus binary tree
    bool mtt_enabled = false;               // Multi-type tree
    bool alf_enabled = false;               // Adaptive Loop Filter
    bool sao_enabled = false;               // Sample Adaptive Offset
    bool lmcs_enabled = false;              // Luma Mapping with Chroma Scaling
    bool mip_enabled = false;               // Matrix-based Intra Prediction
    bool isp_enabled = false;               // Intra Sub-Partitions
    bool mrl_enabled = false;               // Multiple Reference Line
    bool bdof_enabled = false;              // Bi-Directional Optical Flow
    bool dmvr_enabled = false;              // Decoder-side Motion Vector Refinement
    bool prof_enabled = false;              // Prediction Refinement with Optical Flow
    bool mmvd_enabled = false;              // Merge Mode with Motion Vector Differences
    bool smvd_enabled = false;              // Symmetric Motion Vector Differences
    bool ciip_enabled = false;              // Combined Inter-Intra Prediction
    bool geo_enabled = false;               // Geometric Partitioning
    bool ladf_enabled = false;              // Loop filter Across Virtual Boundaries
    
    // Screen content coding tools
    bool ibc_enabled = false;               // Intra Block Copy
    bool palette_enabled = false;           // Palette mode
    bool act_enabled = false;               // Adaptive Color Transform
    
    // Advanced features
    bool rpr_enabled = false;               // Reference Picture Resampling
    bool scaling_list_enabled = false;      // Scaling lists
    bool dep_quant_enabled = false;         // Dependent quantization
    bool sign_hiding_enabled = false;       // Sign data hiding
    bool transform_skip_enabled = false;    // Transform skip mode
};

struct VVCDecoderConfig {
    // Basic configuration
    uint32_t max_threads = 0;               // 0 = auto-detect
    bool error_concealment = true;
    bool enable_parallel_processing = true;
    uint32_t frame_buffer_pool_size = 8;
    
    // Feature control
    VVCAdvancedFeatures features;
    
    // Performance tuning
    bool fast_decode_mode = false;          // Trade quality for speed
    bool low_delay_mode = false;            // Optimize for low latency
    uint32_t max_temporal_layers = 8;
    
    // Memory management
    bool use_external_buffers = false;
    uint32_t max_memory_usage_mb = 0;       // 0 = unlimited
    
    // Compatibility
    bool strict_compliance = true;          // Strict standard compliance
    bool enable_experimental_features = false;
};

struct VVCStreamInfo {
    VVCProfile profile = VVCProfile::MAIN_10;
    VVCLevel level = VVCLevel::LEVEL_4_0;
    VVCTier tier = VVCTier::MAIN;
    VVCChromaFormat chroma_format = VVCChromaFormat::YUV_420;
    
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bit_depth_luma = 8;
    uint32_t bit_depth_chroma = 8;
    
    // Frame rate information
    uint32_t frame_rate_num = 0;
    uint32_t frame_rate_den = 0;
    bool constant_frame_rate = true;
    
    // Color information (ITU-T H.273)
    uint32_t color_primaries = 1;
    uint32_t transfer_characteristics = 1;
    uint32_t matrix_coefficients = 1;
    bool full_range_flag = false;
    
    // HDR information
    bool hdr_capable = false;
    uint32_t max_content_light_level = 0;   // nits
    uint32_t max_frame_avg_light_level = 0; // nits
    
    // Advanced features in use
    VVCAdvancedFeatures active_features;
    
    // Conformance information
    std::string codec_fourcc = "vvc1";      // or "vvi1"
    std::string codec_string;               // Codec identification string
    uint32_t general_constraint_info = 0;
};

class VVCDecoder : public DecoderInterface {
public:
    VVCDecoder();
    ~VVCDecoder() override;
    
    // DecoderInterface implementation
    bool initialize(const DecoderConfig& config) override;
    bool isSupported(const MediaInfo& media_info) const override;
    DecodeResult decode(const EncodedFrame& encoded_frame) override;
    void flush() override;
    void reset() override;
    
    // VVC-specific configuration
    bool configure(const VVCDecoderConfig& vvc_config);
    VVCStreamInfo getStreamInfo() const { return stream_info_; }
    
    // Capability negotiation
    bool supportsProfile(VVCProfile profile) const;
    bool supportsLevel(VVCLevel level) const;
    bool supportsFeature(const std::string& feature_name) const;
    
    // Advanced features
    void enableFeature(const std::string& feature_name, bool enable);
    std::vector<std::string> getSupportedFeatures() const;
    std::vector<std::string> getActiveFeatures() const;
    
    // Error resilience
    void setErrorConcealmentMode(const std::string& mode);
    uint32_t getErrorCount() const { return error_count_; }
    
    // Performance monitoring
    struct VVCPerformanceStats {
        uint64_t frames_decoded = 0;
        uint64_t total_decode_time_us = 0;
        uint64_t average_decode_time_us = 0;
        uint32_t complexity_level = 0;         // 0-100 relative complexity
        uint32_t feature_usage_count = 0;      // Number of advanced features used
        bool hardware_acceleration = false;
        std::string decoder_version;
    };
    
    VVCPerformanceStats getPerformanceStats() const { return perf_stats_; }
    
    // Conformance testing
    bool validateConformance(const EncodedFrame& frame) const;
    std::vector<std::string> getConformanceIssues() const;
    
    // Future compatibility
    void setCompatibilityMode(const std::string& mode);
    bool isExperimentalFeature(const std::string& feature_name) const;

private:
    VVCDecoderConfig config_;
    VVCStreamInfo stream_info_;
    VVCPerformanceStats perf_stats_;
    uint32_t error_count_ = 0;
    
    // VVC decoder context
    std::unique_ptr<vvdec_context> vvc_context_;
    
    // Parameter sets
    std::vector<uint8_t> vps_data_;         // Video Parameter Set
    std::vector<uint8_t> sps_data_;         // Sequence Parameter Set  
    std::vector<uint8_t> pps_data_;         // Picture Parameter Set
    std::vector<uint8_t> aps_data_;         // Adaptation Parameter Set
    
    // Internal methods
    bool initializeVVCContext();
    void configureVVCParams();
    
    // Parameter set parsing
    bool parseVPS(const uint8_t* data, size_t size);
    bool parseSPS(const uint8_t* data, size_t size);
    bool parsePPS(const uint8_t* data, size_t size);
    bool parseAPS(const uint8_t* data, size_t size);
    
    // NAL unit processing
    bool processNALUnit(const uint8_t* data, size_t size);
    uint32_t getNALUnitType(const uint8_t* data) const;
    
    // Feature detection and management
    void detectActiveFeatures(const VVCStreamInfo& stream_info);
    bool validateFeatureCompatibility() const;
    
    // Error handling
    void handleVVCError(int error_code, const std::string& context);
    bool isRecoverableError(int error_code) const;
    
    // Frame processing
    Frame convertVVCFrame(const void* vvc_frame);
    bool validateFrameOutput(const Frame& frame) const;
};

/**
 * VVC Format Detection and Capability Negotiation
 * Detects VVC streams and determines supported features
 */
class VVCFormatDetector {
public:
    static bool isVVCStream(const uint8_t* data, size_t size);
    static VVCStreamInfo analyzeVVCStream(const uint8_t* data, size_t size);
    static std::string generateCodecString(const VVCStreamInfo& stream_info);
    
    // Capability testing
    static bool isProfileSupported(VVCProfile profile);
    static bool isLevelSupported(VVCLevel level);
    static VVCLevel getMaxSupportedLevel();
    
    // Feature support matrix
    static std::vector<std::string> getSupportedFeatures();
    static bool isFeatureSupported(const std::string& feature_name);
    static std::string getFeatureDescription(const std::string& feature_name);
    
    // Hardware acceleration detection
    static bool isHardwareVVCAvailable();
    static std::vector<std::string> getHardwareVVCDevices();

private:
    static bool parseVVCHeader(const uint8_t* data, size_t size, VVCStreamInfo& info);
    static VVCProfile extractProfile(const uint8_t* sps_data);
    static VVCLevel extractLevel(const uint8_t* sps_data);
    static void extractColorInfo(const uint8_t* sps_data, VVCStreamInfo& info);
};

/**
 * VVC Future Compatibility Layer
 * Ensures forward compatibility as VVC standard evolves
 */
class VVCCompatibilityManager {
public:
    enum class VVCVersion {
        VVC_1_0,            // Initial standard (ITU-T H.266 v1)
        VVC_1_1,            // First amendment
        VVC_2_0,            // Major revision (future)
        VVC_DRAFT           // Draft/experimental versions
    };
    
    static void setTargetVersion(VVCVersion version);
    static VVCVersion getCurrentVersion();
    static bool isVersionSupported(VVCVersion version);
    
    // Forward compatibility
    static bool enableExperimentalFeatures(bool enable);
    static std::vector<std::string> getExperimentalFeatures();
    static void registerNewFeature(const std::string& feature_name, VVCVersion introduced_in);
    
    // Migration support
    static bool migrateConfig(VVCDecoderConfig& config, VVCVersion from, VVCVersion to);
    static std::vector<std::string> getDeprecatedFeatures(VVCVersion version);
    
    // Standards compliance
    static bool validateCompliance(const VVCStreamInfo& stream_info, VVCVersion target_version);
    static std::vector<std::string> getComplianceWarnings(const VVCStreamInfo& stream_info);

private:
    static VVCVersion target_version_;
    static bool experimental_features_enabled_;
    static std::map<std::string, VVCVersion> feature_introduction_map_;
};

} // namespace ve::decode
