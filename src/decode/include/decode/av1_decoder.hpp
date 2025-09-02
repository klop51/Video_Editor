#pragma once

#include "core/frame.hpp"
#include "decode/decoder_interface.hpp"
#include <memory>
#include <string>
#include <vector>

// Forward declarations for AV1 libraries
struct aom_codec_ctx;
struct dav1d_context;
struct aom_image;

namespace ve::decode {

/**
 * AV1 Decoder supporting multiple AV1 implementations
 * Integrates libaom, libdav1d, and SVT-AV1 for optimal performance
 */

enum class AV1Implementation {
    AUTO_SELECT,    // Automatically choose best implementation
    LIBAOM,         // Reference implementation (slower, most compatible)
    LIBDAV1D,       // VideoLAN implementation (faster decode)
    SVT_AV1,        // Intel implementation (optimized for Intel hardware)
    HARDWARE        // Hardware-accelerated AV1 (when available)
};

enum class AV1Profile {
    MAIN = 0,           // 8-bit and 10-bit, 4:2:0 chroma subsampling
    HIGH = 1,           // 8-bit and 10-bit, 4:4:4 chroma subsampling  
    PROFESSIONAL = 2    // 8-bit to 12-bit, 4:2:0, 4:2:2, and 4:4:4
};

enum class AV1Level {
    LEVEL_2_0 = 0,   // Up to 147456 luma samples (e.g., 640×360)
    LEVEL_2_1 = 1,   // Up to 278784 luma samples (e.g., 720×480)
    LEVEL_3_0 = 4,   // Up to 665856 luma samples (e.g., 1280×720)
    LEVEL_4_0 = 8,   // Up to 2359296 luma samples (e.g., 2048×1080)
    LEVEL_5_0 = 12,  // Up to 8912896 luma samples (e.g., 4096×2160)
    LEVEL_6_0 = 16,  // Up to 35651584 luma samples (e.g., 8192×4320)
    LEVEL_7_0 = 20   // Up to 142606336 luma samples (e.g., 16384×8640)
};

struct AV1FilmGrainParams {
    bool apply_grain = false;
    uint16_t grain_seed = 0;
    uint8_t num_y_points = 0;
    uint8_t num_cb_points = 0;
    uint8_t num_cr_points = 0;
    uint8_t grain_scaling_minus_8 = 0;
    uint8_t ar_coeff_lag = 0;
    bool grain_scale_shift = false;
    bool chroma_scaling_from_luma = false;
    bool overlap_flag = false;
    bool clip_to_restricted_range = false;
};

struct AV1DecoderConfig {
    AV1Implementation preferred_impl = AV1Implementation::AUTO_SELECT;
    bool enable_film_grain = true;
    bool enable_loop_restoration = true;
    bool enable_cdef = true;           // Constrained Directional Enhancement Filter
    bool enable_superres = true;       // Super-resolution
    bool enable_frame_parallel = true; // Frame-parallel decoding
    uint32_t max_threads = 0;          // 0 = auto-detect
    bool low_latency_mode = false;
    bool error_resilient = false;
    
    // Memory management
    uint32_t frame_buffer_pool_size = 10;
    bool use_external_frame_buffers = false;
    
    // Hardware acceleration preferences
    bool prefer_hardware = true;
    std::vector<std::string> hw_device_types = {"d3d11va", "dxva2", "cuda", "vaapi"};
};

struct AV1FrameInfo {
    AV1Profile profile = AV1Profile::MAIN;
    AV1Level level = AV1Level::LEVEL_4_0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bit_depth = 8;
    uint32_t chroma_subsampling = 0;  // 0=4:2:0, 1=4:2:2, 2=4:4:4
    bool monochrome = false;
    
    // Color information
    uint32_t color_primaries = 1;     // ITU-T H.273
    uint32_t transfer_characteristics = 1;
    uint32_t matrix_coefficients = 1;
    bool color_range = false;         // false=limited, true=full
    
    // Timing information
    uint32_t frame_rate_num = 0;
    uint32_t frame_rate_den = 0;
    
    // Film grain
    AV1FilmGrainParams film_grain;
    
    // Advanced features
    bool has_superres = false;
    uint32_t superres_denom = 8;      // Superres scaling denominator
    bool enable_order_hint = false;
    uint32_t order_hint_bits = 0;
};

class AV1Decoder : public DecoderInterface {
public:
    AV1Decoder();
    ~AV1Decoder() override;
    
    // DecoderInterface implementation
    bool initialize(const DecoderConfig& config) override;
    bool isSupported(const MediaInfo& media_info) const override;
    DecodeResult decode(const EncodedFrame& encoded_frame) override;
    void flush() override;
    void reset() override;
    
    // AV1-specific configuration
    bool configure(const AV1DecoderConfig& av1_config);
    void setImplementation(AV1Implementation impl);
    AV1Implementation getCurrentImplementation() const { return current_impl_; }
    
    // Frame information
    AV1FrameInfo getFrameInfo() const { return frame_info_; }
    bool supportsFilmGrain() const;
    bool supportsHardwareAcceleration() const;
    
    // Performance monitoring
    struct PerformanceStats {
        uint64_t frames_decoded = 0;
        uint64_t total_decode_time_us = 0;
        uint64_t average_decode_time_us = 0;
        uint32_t decode_errors = 0;
        AV1Implementation active_implementation = AV1Implementation::AUTO_SELECT;
        bool hardware_acceleration_active = false;
    };
    
    PerformanceStats getPerformanceStats() const { return perf_stats_; }
    void resetPerformanceStats();
    
    // Capability detection
    static std::vector<AV1Implementation> getAvailableImplementations();
    static bool isImplementationAvailable(AV1Implementation impl);
    static std::string getImplementationName(AV1Implementation impl);
    static std::vector<std::string> getSupportedHardwareDevices();

private:
    AV1DecoderConfig config_;
    AV1Implementation current_impl_ = AV1Implementation::AUTO_SELECT;
    AV1FrameInfo frame_info_;
    PerformanceStats perf_stats_;
    
    // Implementation-specific contexts
    std::unique_ptr<aom_codec_ctx> aom_context_;
    std::unique_ptr<dav1d_context> dav1d_context_;
    void* svt_av1_context_ = nullptr;
    void* hw_context_ = nullptr;
    
    // Internal methods
    bool initializeAOM();
    bool initializeDAV1D();
    bool initializeSVTAV1();
    bool initializeHardware();
    
    void selectOptimalImplementation();
    AV1Implementation detectBestImplementation(const AV1FrameInfo& frame_info);
    
    // Decoding methods for each implementation
    DecodeResult decodeWithAOM(const EncodedFrame& encoded_frame);
    DecodeResult decodeWithDAV1D(const EncodedFrame& encoded_frame);
    DecodeResult decodeWithSVTAV1(const EncodedFrame& encoded_frame);
    DecodeResult decodeWithHardware(const EncodedFrame& encoded_frame);
    
    // Frame processing
    Frame convertAOMImage(const aom_image& aom_img);
    Frame processFilmGrain(const Frame& input_frame, const AV1FilmGrainParams& grain_params);
    
    // Sequence header parsing
    bool parseSequenceHeader(const uint8_t* data, size_t size);
    bool parseFrameHeader(const uint8_t* data, size_t size);
    
    // Error handling
    void handleDecoderError(const std::string& implementation, const std::string& error_msg);
    bool shouldFallbackToSoftware(const std::string& error_code);
};

/**
 * AV1 Format Detection and Analysis
 * Detects AV1 streams and extracts format information
 */
class AV1FormatDetector {
public:
    struct AV1StreamInfo {
        bool is_av1 = false;
        AV1Profile profile = AV1Profile::MAIN;
        AV1Level level = AV1Level::LEVEL_4_0;
        uint32_t max_width = 0;
        uint32_t max_height = 0;
        uint32_t bit_depth = 8;
        bool has_film_grain = false;
        bool has_superres = false;
        bool is_monochrome = false;
        std::string codec_string;  // e.g., "av01.0.04M.08"
    };
    
    static AV1StreamInfo detectAV1Stream(const uint8_t* data, size_t size);
    static bool isAV1Stream(const uint8_t* data, size_t size);
    static std::string generateCodecString(const AV1StreamInfo& stream_info);
    static AV1Level calculateRequiredLevel(uint32_t width, uint32_t height, uint32_t frame_rate);
    
private:
    static bool parseOBU(const uint8_t* data, size_t size, AV1StreamInfo& stream_info);
    static bool parseSequenceOBU(const uint8_t* data, size_t size, AV1StreamInfo& stream_info);
};

/**
 * AV1 Film Grain Synthesis
 * Implements AV1 film grain synthesis for authentic film look
 */
class AV1FilmGrainSynthesis {
public:
    static Frame applyFilmGrain(const Frame& input_frame, const AV1FilmGrainParams& params);
    static bool isFilmGrainSupported(const AV1FrameInfo& frame_info);
    
    // Predefined film grain presets for common looks
    static AV1FilmGrainParams getPreset(const std::string& preset_name);
    static std::vector<std::string> getAvailablePresets();
    
    // Film grain analysis
    static AV1FilmGrainParams analyzeSourceGrain(const Frame& reference_frame);
    static double calculateGrainStrength(const Frame& frame);

private:
    static void synthesizeLumaGrain(uint8_t* luma_data, uint32_t width, uint32_t height,
                                  const AV1FilmGrainParams& params);
    static void synthesizeChromaGrain(uint8_t* chroma_data, uint32_t width, uint32_t height,
                                    const AV1FilmGrainParams& params, bool cb_plane);
    
    static uint16_t generateGrainNoise(uint16_t seed, uint32_t pos_x, uint32_t pos_y);
    static void applyAutoRegression(int16_t* grain_buffer, uint32_t width, uint32_t height,
                                  const int8_t* ar_coeffs, uint32_t ar_coeff_lag);
};

} // namespace ve::decode
