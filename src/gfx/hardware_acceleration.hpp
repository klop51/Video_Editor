// Week 11: Hardware Decode/Encode Integration
// Advanced hardware acceleration for video decode and encode operations

#pragma once

#include "multi_gpu_system.hpp"
#include "core/result.hpp"
#include "core/frame.hpp"
#include "media_io/video_codec.hpp"
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d11_4.h>
#include <dxva2api.h>
#include <codecapi.h>
#include <mfapi.h>
#include <mftransform.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <queue>
#include <functional>

namespace VideoEditor::GFX {

// =============================================================================
// Hardware Codec Types and Enums
// =============================================================================

enum class HardwareCodec {
    // Decode codecs
    H264_DECODE,
    H265_DECODE,
    AV1_DECODE,
    VP9_DECODE,
    MPEG2_DECODE,
    VC1_DECODE,
    
    // Encode codecs
    H264_ENCODE,
    H265_ENCODE,
    AV1_ENCODE,
    VP9_ENCODE
};

enum class HardwareProfile {
    // H.264 profiles
    H264_BASELINE,
    H264_MAIN,
    H264_HIGH,
    H264_HIGH_10,
    H264_HIGH_422,
    H264_HIGH_444,
    
    // H.265 profiles
    H265_MAIN,
    H265_MAIN_10,
    H265_MAIN_12,
    H265_MAIN_422_10,
    H265_MAIN_444_10,
    H265_MAIN_444_12,
    
    // AV1 profiles
    AV1_MAIN,
    AV1_HIGH,
    AV1_PROFESSIONAL
};

enum class HardwareAccelerationLevel {
    NONE,           // Software only
    PARTIAL,        // Some operations accelerated
    FULL,           // Fully hardware accelerated
    ENHANCED        // Hardware with advanced features
};

enum class DecodeFlags {
    NONE = 0,
    LOW_LATENCY = 1 << 0,
    HIGH_QUALITY = 1 << 1,
    POWER_EFFICIENT = 1 << 2,
    REAL_TIME = 1 << 3,
    SECURE_CONTENT = 1 << 4
};

enum class EncodeFlags {
    NONE = 0,
    LOW_LATENCY = 1 << 0,
    HIGH_QUALITY = 1 << 1,
    CONSTANT_QUALITY = 1 << 2,
    VARIABLE_BITRATE = 1 << 3,
    LOSSLESS = 1 << 4,
    TWO_PASS = 1 << 5
};

// =============================================================================
// Hardware Codec Capabilities
// =============================================================================

struct HardwareCodecCapabilities {
    HardwareCodec codec;
    std::vector<HardwareProfile> supported_profiles;
    
    // Resolution support
    uint32_t min_width;
    uint32_t min_height;
    uint32_t max_width;
    uint32_t max_height;
    
    // Bit depth support
    std::vector<uint32_t> supported_bit_depths;
    
    // Chroma format support
    bool supports_yuv420;
    bool supports_yuv422;
    bool supports_yuv444;
    bool supports_rgb;
    
    // Performance characteristics
    uint32_t max_decode_sessions;
    uint32_t max_encode_sessions;
    uint32_t max_concurrent_operations;
    float decode_throughput_fps;
    float encode_throughput_fps;
    
    // Feature support
    bool supports_b_frames;
    bool supports_interlaced;
    bool supports_field_encoding;
    bool supports_low_power_mode;
    bool supports_rate_control;
    bool supports_quality_control;
    bool supports_temporal_layers;
    bool supports_roi_encoding;
    
    // Hardware-specific features
    bool supports_nvidia_nvenc;
    bool supports_amd_vce;
    bool supports_intel_quicksync;
    bool supports_apple_videotoolbox;
    
    HardwareAccelerationLevel acceleration_level;
};

// =============================================================================
// Decode/Encode Parameters
// =============================================================================

struct HardwareDecodeParams {
    HardwareCodec codec;
    HardwareProfile profile;
    uint32_t width;
    uint32_t height;
    uint32_t bit_depth;
    uint32_t chroma_format;
    DecodeFlags flags;
    
    // Output preferences
    DXGI_FORMAT preferred_output_format;
    bool decode_to_texture;
    bool enable_post_processing;
    
    // Performance settings
    uint32_t max_decode_threads;
    bool enable_gpu_scheduling;
    float target_framerate;
    
    // Quality settings
    bool enable_deblocking;
    bool enable_deringing;
    float noise_reduction_strength;
};

struct HardwareEncodeParams {
    HardwareCodec codec;
    HardwareProfile profile;
    uint32_t width;
    uint32_t height;
    uint32_t bit_depth;
    uint32_t chroma_format;
    EncodeFlags flags;
    
    // Rate control
    uint32_t target_bitrate_kbps;
    uint32_t max_bitrate_kbps;
    uint32_t buffer_size_kbits;
    float quality_factor;    // 0.0 - 1.0
    
    // Frame structure
    uint32_t keyframe_interval;
    uint32_t b_frame_count;
    uint32_t reference_frames;
    bool enable_adaptive_keyframes;
    
    // Performance settings
    uint32_t encode_preset;  // 0=fastest, 7=slowest
    bool enable_parallel_processing;
    bool enable_low_latency_mode;
    
    // Advanced features
    bool enable_temporal_layers;
    bool enable_spatial_layers;
    bool enable_roi_encoding;
    std::vector<std::pair<uint32_t, uint32_t>> roi_regions; // (priority, area)
};

// =============================================================================
// Hardware Decoder Class
// =============================================================================

class HardwareDecoder {
public:
    HardwareDecoder() = default;
    ~HardwareDecoder() = default;

    // Initialization and capability querying
    Core::Result<void> initialize(GraphicsDevice* device, uint32_t device_index);
    void shutdown();

    // Capability queries
    std::vector<HardwareCodecCapabilities> get_supported_codecs() const;
    bool supports_codec(HardwareCodec codec, HardwareProfile profile = HardwareProfile::H264_MAIN) const;
    Core::Result<HardwareCodecCapabilities> get_codec_capabilities(HardwareCodec codec) const;

    // Decoder session management
    Core::Result<void> create_decode_session(const HardwareDecodeParams& params);
    void destroy_decode_session();
    bool has_active_session() const { return decode_session_active_; }

    // Frame decoding
    struct DecodedFrame {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11VideoDecoderOutputView> output_view;
        uint64_t timestamp;
        uint32_t frame_number;
        bool is_keyframe;
        DXGI_FORMAT format;
        uint32_t width;
        uint32_t height;
    };

    Core::Result<DecodedFrame> decode_frame(const Core::EncodedVideoFrame& encoded_frame);
    Core::Result<std::vector<DecodedFrame>> decode_frame_batch(const std::vector<Core::EncodedVideoFrame>& frames);

    // Asynchronous decoding
    using DecodeCallback = std::function<void(const std::string&, const DecodedFrame&, bool success)>;
    
    Core::Result<std::string> decode_frame_async(const Core::EncodedVideoFrame& encoded_frame, 
                                                 DecodeCallback callback);
    void cancel_decode(const std::string& decode_id);
    void wait_for_all_decodes();

    // Performance monitoring
    struct DecodePerformanceMetrics {
        uint32_t frames_decoded;
        float decode_fps;
        float average_decode_time_ms;
        float gpu_utilization_percent;
        size_t memory_usage_mb;
        uint32_t hardware_acceleration_percent;
        std::vector<float> decode_times_ms;
    };

    DecodePerformanceMetrics get_performance_metrics() const;
    void reset_performance_metrics();
    void enable_performance_monitoring(bool enabled) { performance_monitoring_enabled_ = enabled; }

    // Advanced features
    Core::Result<void> set_post_processing_params(bool enable_deblocking, bool enable_deringing, 
                                                  float noise_reduction = 0.0f);
    Core::Result<void> configure_low_latency_mode(bool enabled);
    Core::Result<void> set_output_color_space(DXGI_COLOR_SPACE_TYPE color_space);

private:
    struct DecodeSession {
        Microsoft::WRL::ComPtr<ID3D11VideoDecoder> decoder;
        Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context;
        Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
        HardwareDecodeParams params;
        GUID decoder_profile;
        bool is_active;
        uint32_t session_id;
    };

    struct AsyncDecodeTask {
        std::string task_id;
        Core::EncodedVideoFrame frame;
        DecodeCallback callback;
        std::chrono::steady_clock::time_point submit_time;
        bool is_processing;
    };

    Core::Result<void> enumerate_decoder_capabilities();
    Core::Result<GUID> get_decoder_profile_guid(HardwareCodec codec, HardwareProfile profile) const;
    Core::Result<void> create_decoder_surfaces(uint32_t width, uint32_t height, DXGI_FORMAT format);
    
    void process_async_decodes();
    void update_performance_metrics();

    GraphicsDevice* graphics_device_ = nullptr;
    uint32_t device_index_ = 0;
    
    // Video API objects
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
    
    // Decoder session
    std::unique_ptr<DecodeSession> current_session_;
    bool decode_session_active_ = false;
    
    // Decoder surfaces and views
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> decoder_surfaces_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11VideoDecoderOutputView>> output_views_;
    uint32_t current_surface_index_ = 0;
    
    // Capabilities cache
    std::vector<HardwareCodecCapabilities> supported_codecs_;
    bool capabilities_enumerated_ = false;
    
    // Async processing
    std::queue<AsyncDecodeTask> decode_queue_;
    std::unordered_map<std::string, AsyncDecodeTask> active_decodes_;
    std::thread decode_thread_;
    std::mutex decode_mutex_;
    std::condition_variable decode_condition_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Performance monitoring
    bool performance_monitoring_enabled_ = false;
    mutable DecodePerformanceMetrics performance_metrics_;
    std::chrono::steady_clock::time_point last_metrics_update_;
    std::mutex metrics_mutex_;
};

// =============================================================================
// Hardware Encoder Class
// =============================================================================

class HardwareEncoder {
public:
    HardwareEncoder() = default;
    ~HardwareEncoder() = default;

    // Initialization and capability querying
    Core::Result<void> initialize(GraphicsDevice* device, uint32_t device_index);
    void shutdown();

    // Capability queries
    std::vector<HardwareCodecCapabilities> get_supported_codecs() const;
    bool supports_codec(HardwareCodec codec, HardwareProfile profile = HardwareProfile::H264_MAIN) const;
    Core::Result<HardwareCodecCapabilities> get_codec_capabilities(HardwareCodec codec) const;

    // Encoder session management
    Core::Result<void> create_encode_session(const HardwareEncodeParams& params);
    void destroy_encode_session();
    bool has_active_session() const { return encode_session_active_; }

    // Frame encoding
    struct EncodedFrame {
        std::vector<uint8_t> data;
        uint64_t timestamp;
        uint32_t frame_number;
        bool is_keyframe;
        uint32_t size_bytes;
        float quality_score;
        HardwareCodec codec;
    };

    Core::Result<EncodedFrame> encode_frame(ID3D11Texture2D* input_texture, uint64_t timestamp);
    Core::Result<std::vector<EncodedFrame>> encode_frame_batch(const std::vector<std::pair<ID3D11Texture2D*, uint64_t>>& frames);

    // Asynchronous encoding
    using EncodeCallback = std::function<void(const std::string&, const EncodedFrame&, bool success)>;
    
    Core::Result<std::string> encode_frame_async(ID3D11Texture2D* input_texture, uint64_t timestamp,
                                                 EncodeCallback callback);
    void cancel_encode(const std::string& encode_id);
    void wait_for_all_encodes();

    // Rate control and quality management
    Core::Result<void> update_bitrate(uint32_t new_bitrate_kbps);
    Core::Result<void> update_quality_factor(float quality_factor);
    Core::Result<void> force_keyframe();
    Core::Result<void> set_roi_regions(const std::vector<std::pair<uint32_t, uint32_t>>& regions);

    // Performance monitoring
    struct EncodePerformanceMetrics {
        uint32_t frames_encoded;
        float encode_fps;
        float average_encode_time_ms;
        float average_bitrate_kbps;
        float average_quality_score;
        float gpu_utilization_percent;
        size_t memory_usage_mb;
        uint32_t hardware_acceleration_percent;
        std::vector<float> encode_times_ms;
        std::vector<uint32_t> frame_sizes_bytes;
    };

    EncodePerformanceMetrics get_performance_metrics() const;
    void reset_performance_metrics();
    void enable_performance_monitoring(bool enabled) { performance_monitoring_enabled_ = enabled; }

    // Advanced encoding features
    Core::Result<void> enable_temporal_layers(uint32_t layer_count);
    Core::Result<void> configure_b_frame_structure(uint32_t b_frame_count, bool adaptive = true);
    Core::Result<void> set_encode_preset(uint32_t preset_level); // 0=fastest, 7=highest quality

private:
    struct EncodeSession {
        Microsoft::WRL::ComPtr<IMFTransform> encoder_transform;
        Microsoft::WRL::ComPtr<ID3D11VideoEncoder> video_encoder;
        Microsoft::WRL::ComPtr<ID3D11VideoContext1> video_context;
        Microsoft::WRL::ComPtr<ID3D11VideoDevice1> video_device;
        HardwareEncodeParams params;
        GUID encoder_profile;
        bool is_active;
        uint32_t session_id;
        uint32_t frame_count;
    };

    struct AsyncEncodeTask {
        std::string task_id;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
        uint64_t timestamp;
        EncodeCallback callback;
        std::chrono::steady_clock::time_point submit_time;
        bool is_processing;
    };

    Core::Result<void> enumerate_encoder_capabilities();
    Core::Result<GUID> get_encoder_profile_guid(HardwareCodec codec, HardwareProfile profile) const;
    Core::Result<void> configure_encoder_parameters(const HardwareEncodeParams& params);
    
    void process_async_encodes();
    void update_performance_metrics();

    GraphicsDevice* graphics_device_ = nullptr;
    uint32_t device_index_ = 0;
    
    // Video API objects
    Microsoft::WRL::ComPtr<ID3D11VideoDevice1> video_device_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext1> video_context_;
    
    // Encoder session
    std::unique_ptr<EncodeSession> current_session_;
    bool encode_session_active_ = false;
    
    // Input/output surfaces
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> input_surfaces_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
    
    // Capabilities cache
    std::vector<HardwareCodecCapabilities> supported_codecs_;
    bool capabilities_enumerated_ = false;
    
    // Async processing
    std::queue<AsyncEncodeTask> encode_queue_;
    std::unordered_map<std::string, AsyncEncodeTask> active_encodes_;
    std::thread encode_thread_;
    std::mutex encode_mutex_;
    std::condition_variable encode_condition_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Performance monitoring
    bool performance_monitoring_enabled_ = false;
    mutable EncodePerformanceMetrics performance_metrics_;
    std::chrono::steady_clock::time_point last_metrics_update_;
    std::mutex metrics_mutex_;
};

// =============================================================================
// Hardware Acceleration Manager
// =============================================================================

class HardwareAccelerationManager {
public:
    HardwareAccelerationManager() = default;
    ~HardwareAccelerationManager() = default;

    // System initialization
    Core::Result<void> initialize(MultiGPUManager* gpu_manager);
    void shutdown();

    // Decoder management
    Core::Result<std::unique_ptr<HardwareDecoder>> create_decoder(uint32_t device_index);
    Core::Result<std::unique_ptr<HardwareDecoder>> create_decoder_for_codec(HardwareCodec codec);
    std::vector<uint32_t> get_decode_capable_devices(HardwareCodec codec) const;

    // Encoder management
    Core::Result<std::unique_ptr<HardwareEncoder>> create_encoder(uint32_t device_index);
    Core::Result<std::unique_ptr<HardwareEncoder>> create_encoder_for_codec(HardwareCodec codec);
    std::vector<uint32_t> get_encode_capable_devices(HardwareCodec codec) const;

    // System capabilities
    struct SystemCapabilities {
        std::unordered_map<uint32_t, std::vector<HardwareCodecCapabilities>> per_device_decode_caps;
        std::unordered_map<uint32_t, std::vector<HardwareCodecCapabilities>> per_device_encode_caps;
        uint32_t total_decode_sessions;
        uint32_t total_encode_sessions;
        bool supports_simultaneous_decode_encode;
        bool supports_cross_device_operations;
    };

    SystemCapabilities get_system_capabilities() const;
    void refresh_capabilities();

    // Load balancing and resource management
    Core::Result<uint32_t> get_optimal_device_for_decode(HardwareCodec codec, const HardwareDecodeParams& params);
    Core::Result<uint32_t> get_optimal_device_for_encode(HardwareCodec codec, const HardwareEncodeParams& params);
    
    void enable_dynamic_load_balancing(bool enabled) { dynamic_load_balancing_ = enabled; }
    void set_decode_priority_device(uint32_t device_index) { decode_priority_device_ = device_index; }
    void set_encode_priority_device(uint32_t device_index) { encode_priority_device_ = device_index; }

    // Performance monitoring
    struct SystemPerformanceMetrics {
        float total_decode_fps;
        float total_encode_fps;
        uint32_t active_decode_sessions;
        uint32_t active_encode_sessions;
        std::unordered_map<uint32_t, float> per_device_utilization;
        float average_decode_latency_ms;
        float average_encode_latency_ms;
    };

    SystemPerformanceMetrics get_system_performance_metrics() const;
    void enable_system_monitoring(bool enabled) { system_monitoring_enabled_ = enabled; }

private:
    Core::Result<void> enumerate_all_capabilities();
    void monitor_system_performance();

    MultiGPUManager* gpu_manager_ = nullptr;
    
    // Capability tracking
    SystemCapabilities system_capabilities_;
    bool capabilities_enumerated_ = false;
    
    // Active instances
    std::vector<std::weak_ptr<HardwareDecoder>> active_decoders_;
    std::vector<std::weak_ptr<HardwareEncoder>> active_encoders_;
    
    // Load balancing
    bool dynamic_load_balancing_ = true;
    uint32_t decode_priority_device_ = 0;
    uint32_t encode_priority_device_ = 0;
    
    // Performance monitoring
    bool system_monitoring_enabled_ = true;
    SystemPerformanceMetrics system_metrics_;
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};
    mutable std::mutex metrics_mutex_;
};

} // namespace VideoEditor::GFX
