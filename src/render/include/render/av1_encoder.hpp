#pragma once

#include "core/frame.hpp"
#include "render/encoder_interface.hpp"
#include "decode/av1_decoder.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declarations for AV1 encoding libraries
struct aom_codec_ctx;
struct SvtAv1EncConfiguration;

namespace ve::render {

/**
 * AV1 Encoder with SVT-AV1 and libaom support
 * Optimized for production workflows with advanced encoding features
 */

enum class AV1EncodingMode {
    REALTIME,           // Real-time encoding (low latency)
    LIVE_STREAMING,     // Live streaming optimized
    VOD_STANDARD,       // Video-on-demand standard quality
    VOD_HIGH_QUALITY,   // High quality for archival/distribution
    LOSSLESS,           // Mathematically lossless encoding
    NEAR_LOSSLESS       // Visually lossless with slight compression
};

enum class AV1RateControlMode {
    CONSTANT_QP,        // Constant quantization parameter
    VARIABLE_BITRATE,   // Variable bitrate (VBR)
    CONSTANT_BITRATE,   // Constant bitrate (CBR)  
    CONSTRAINED_VBR,    // Constrained variable bitrate
    CONSTANT_QUALITY    // Constant quality mode
};

enum class AV1EncoderImpl {
    AUTO_SELECT,        // Automatically choose best encoder
    SVT_AV1,           // Intel SVT-AV1 (fastest, production-ready)
    LIBAOM,            // Reference implementation (slowest, highest quality)
    RAV1E,             // Rust-based encoder (good speed/quality balance)
    HARDWARE           // Hardware AV1 encoding (when available)
};

struct AV1EncodingParams {
    // Basic encoding parameters
    AV1EncodingMode encoding_mode = AV1EncodingMode::VOD_STANDARD;
    AV1RateControlMode rate_control = AV1RateControlMode::VARIABLE_BITRATE;
    uint32_t target_bitrate_kbps = 0;     // 0 = auto-calculate
    uint32_t max_bitrate_kbps = 0;        // 0 = no limit
    uint32_t quality_level = 50;          // 0-100, higher = better quality
    
    // Resolution and timing
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frame_rate_num = 30000;
    uint32_t frame_rate_den = 1001;
    
    // Advanced encoding features
    bool enable_film_grain_synthesis = false;
    bool enable_superres = false;
    bool enable_cdef = true;              // Constrained Directional Enhancement Filter
    bool enable_restoration = true;       // Loop restoration filter
    bool enable_palette_mode = false;     // Palette mode for screen content
    bool enable_intrabc = false;          // Intra block copy for screen content
    
    // GOP structure
    uint32_t keyframe_interval = 250;     // Keyframe every N frames
    uint32_t max_reference_frames = 7;    // Maximum reference frames
    bool enable_temporal_layers = false;  // Temporal scalability
    uint32_t temporal_layers = 1;
    
    // Speed vs quality trade-offs
    uint32_t encoder_speed_preset = 6;    // 0=slowest/best, 13=fastest/worst
    uint32_t max_quantizer = 63;
    uint32_t min_quantizer = 0;
    
    // Threading and performance
    uint32_t encoder_threads = 0;         // 0 = auto-detect
    bool enable_row_multithreading = true;
    bool enable_tile_parallelism = true;
    uint32_t tile_columns = 0;            // 0 = auto
    uint32_t tile_rows = 0;               // 0 = auto
    
    // Color and HDR
    uint32_t bit_depth = 8;               // 8, 10, or 12
    uint32_t chroma_subsampling = 1;      // 0=4:4:4, 1=4:2:0, 2=4:2:2
    uint32_t color_primaries = 1;         // ITU-T H.273
    uint32_t transfer_characteristics = 1;
    uint32_t matrix_coefficients = 1;
    bool full_color_range = false;
    
    // Film grain parameters (when enabled)
    decode::AV1FilmGrainParams film_grain_params;
};

struct AV1EncoderConfig {
    AV1EncoderImpl preferred_impl = AV1EncoderImpl::AUTO_SELECT;
    AV1EncodingParams encoding_params;
    
    // Output configuration
    bool enable_two_pass_encoding = false;
    std::string stats_file_path;          // For two-pass encoding
    bool output_ivf_headers = true;       // Include IVF container headers
    bool output_annexb = false;           // Output Annex-B format
    
    // Performance monitoring
    bool enable_performance_logging = false;
    uint32_t stats_reporting_interval = 100; // Report stats every N frames
    
    // Error handling
    bool continue_on_errors = false;
    uint32_t max_consecutive_errors = 5;
};

class AV1Encoder : public EncoderInterface {
public:
    AV1Encoder();
    ~AV1Encoder() override;
    
    // EncoderInterface implementation
    bool initialize(const EncoderConfig& config) override;
    bool isSupported(const MediaInfo& media_info) const override;
    EncodeResult encode(const Frame& frame) override;
    EncodeResult finalize() override;
    void reset() override;
    
    // AV1-specific configuration
    bool configure(const AV1EncoderConfig& av1_config);
    void setEncodingParams(const AV1EncodingParams& params);
    AV1EncodingParams getEncodingParams() const { return config_.encoding_params; }
    
    // Implementation selection
    void setEncoderImplementation(AV1EncoderImpl impl);
    AV1EncoderImpl getCurrentImplementation() const { return current_impl_; }
    
    // Two-pass encoding support
    bool startFirstPass(const std::string& stats_file);
    bool startSecondPass(const std::string& stats_file);
    bool isFirstPassComplete() const { return first_pass_complete_; }
    
    // Performance monitoring
    struct EncodingStats {
        uint64_t frames_encoded = 0;
        uint64_t total_encode_time_us = 0;
        uint64_t average_encode_time_us = 0;
        uint64_t total_output_bytes = 0;
        double average_bitrate_kbps = 0.0;
        double average_psnr = 0.0;
        double average_ssim = 0.0;
        uint32_t keyframes_generated = 0;
        uint32_t encoding_errors = 0;
        AV1EncoderImpl active_implementation = AV1EncoderImpl::AUTO_SELECT;
    };
    
    EncodingStats getEncodingStats() const { return encoding_stats_; }
    void resetEncodingStats();
    
    // Capability detection
    static std::vector<AV1EncoderImpl> getAvailableEncoders();
    static bool isEncoderAvailable(AV1EncoderImpl impl);
    static std::string getEncoderName(AV1EncoderImpl impl);
    static std::vector<std::string> getSupportedHardwareEncoders();
    
    // Preset management
    static AV1EncodingParams getPreset(const std::string& preset_name);
    static std::vector<std::string> getAvailablePresets();
    static AV1EncodingParams createCustomPreset(AV1EncodingMode mode, uint32_t target_quality);

private:
    AV1EncoderConfig config_;
    AV1EncoderImpl current_impl_ = AV1EncoderImpl::AUTO_SELECT;
    EncodingStats encoding_stats_;
    
    // Two-pass encoding state
    bool first_pass_complete_ = false;
    std::string stats_file_path_;
    
    // Implementation-specific contexts
    std::unique_ptr<aom_codec_ctx> aom_context_;
    std::unique_ptr<SvtAv1EncConfiguration> svt_av1_config_;
    void* svt_av1_context_ = nullptr;
    void* hardware_context_ = nullptr;
    
    // Internal methods
    bool initializeSVTAV1();
    bool initializeAOM();
    bool initializeHardware();
    
    void selectOptimalEncoder();
    AV1EncoderImpl detectBestEncoder(const AV1EncodingParams& params);
    
    // Encoding methods for each implementation
    EncodeResult encodeWithSVTAV1(const Frame& frame);
    EncodeResult encodeWithAOM(const Frame& frame);
    EncodeResult encodeWithHardware(const Frame& frame);
    
    // Configuration helpers
    void configureSVTAV1Params(SvtAv1EncConfiguration& svt_config);
    void configureAOMParams(aom_codec_ctx& aom_config);
    
    // Frame preprocessing
    Frame preprocessFrame(const Frame& input_frame);
    Frame applyFilmGrainPreprocessing(const Frame& frame);
    
    // Output processing
    EncodedFrame processEncodedOutput(const uint8_t* data, size_t size, bool is_keyframe);
    void updateEncodingStats(const EncodedFrame& encoded_frame, uint64_t encode_time_us);
    
    // Error handling
    void handleEncoderError(const std::string& implementation, const std::string& error_msg);
    bool shouldFallbackToSoftware(const std::string& error_code);
};

/**
 * AV1 Rate Control Optimizer
 * Advanced rate control algorithms for optimal AV1 encoding
 */
class AV1RateController {
public:
    struct RateControlParams {
        AV1RateControlMode mode = AV1RateControlMode::VARIABLE_BITRATE;
        uint32_t target_bitrate_kbps = 1000;
        uint32_t max_bitrate_kbps = 0;        // 0 = 1.5x target
        uint32_t buffer_size_ms = 1000;       // Buffer size in milliseconds
        double quality_factor = 1.0;          // Quality bias factor
        bool adaptive_quantization = true;
        bool enable_scene_detection = true;
    };
    
    AV1RateController(const RateControlParams& params);
    
    // Frame-level rate control
    uint32_t calculateFrameQP(const Frame& frame, bool is_keyframe);
    void updateRateControl(uint32_t actual_frame_bits, uint32_t target_frame_bits);
    
    // Bitrate allocation
    uint32_t getTargetFrameBits(bool is_keyframe, uint32_t frame_number);
    double getQualityBoost(const Frame& frame);  // Scene-based quality boost
    
    // Buffer management
    bool isBufferUnderrun() const;
    bool isBufferOverrun() const;
    double getBufferFullness() const;
    
    // Statistics
    struct RateControlStats {
        double average_bitrate_kbps = 0.0;
        double bitrate_variance = 0.0;
        uint32_t buffer_underruns = 0;
        uint32_t buffer_overruns = 0;
        double quality_consistency = 0.0;
    };
    
    RateControlStats getStats() const { return stats_; }

private:
    RateControlParams params_;
    RateControlStats stats_;
    
    // Rate control state
    uint64_t total_bits_encoded_ = 0;
    uint64_t total_frames_encoded_ = 0;
    uint32_t current_buffer_bits_ = 0;
    uint32_t max_buffer_bits_ = 0;
    
    // Moving averages
    double avg_frame_bits_ = 0.0;
    double avg_qp_ = 0.0;
    
    // Scene detection
    double previous_frame_complexity_ = 0.0;
    bool scene_change_detected_ = false;
    
    double calculateFrameComplexity(const Frame& frame);
    bool detectSceneChange(const Frame& frame);
    uint32_t adjustQPForScene(uint32_t base_qp, double complexity_ratio);
};

/**
 * AV1 Quality Assessment
 * Tools for measuring and optimizing AV1 encoding quality
 */
class AV1QualityAnalyzer {
public:
    struct QualityMetrics {
        double psnr_y = 0.0;      // Peak Signal-to-Noise Ratio (luma)
        double psnr_u = 0.0;      // PSNR (chroma U)
        double psnr_v = 0.0;      // PSNR (chroma V)
        double ssim = 0.0;        // Structural Similarity Index
        double vmaf = 0.0;        // Video Multi-method Assessment Fusion
        double butteraugli = 0.0; // Perceptual quality metric
        double temporal_stability = 0.0; // Frame-to-frame consistency
    };
    
    static QualityMetrics analyzeQuality(const Frame& original, const Frame& encoded);
    static double calculateOverallQuality(const QualityMetrics& metrics);
    
    // Batch analysis
    static std::vector<QualityMetrics> analyzeSequence(
        const std::vector<Frame>& original_frames,
        const std::vector<Frame>& encoded_frames);
    
    // Quality prediction
    static double predictQuality(const AV1EncodingParams& params, const Frame& frame);
    static AV1EncodingParams optimizeForQuality(const Frame& reference_frame, 
                                               double target_quality);

private:
    static double calculatePSNR(const Frame& ref, const Frame& test, int plane);
    static double calculateSSIM(const Frame& ref, const Frame& test);
    static double calculateTemporalStability(const std::vector<Frame>& frames);
};

} // namespace ve::render
