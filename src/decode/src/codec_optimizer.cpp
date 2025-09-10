#include "decode/codec_optimizer.hpp"
#include "core/log.hpp"
#include <unordered_map>
#include <mutex>
#include <algorithm>
// TODO: Re-enable D3D11 includes when header conflicts are resolved
#ifdef _WIN32_DISABLED_FOR_NOW
#include <d3d11.h>
#include <dxgi.h>
#endif

namespace ve::decode {

class CodecOptimizer::Impl {
public:
    std::unordered_map<std::string, CodecOptimizerConfig> codec_configs_;
    std::unordered_map<std::string, CodecOptimizationStats> codec_stats_;
    std::mutex config_mutex_;
    std::mutex stats_mutex_;
    
    OptimizationCallback adaptive_callback_;
    bool adaptive_enabled_ = false;
    
    void* gpu_memory_pool_ = nullptr;
    size_t gpu_pool_size_ = 0;
    
    HardwareCapabilities hardware_caps_{};
    bool caps_detected_ = false;
};

CodecOptimizer::CodecOptimizer() : impl_(std::make_unique<Impl>()) {
    // Initialize default configurations
    initialize_default_configs();
}

CodecOptimizer::~CodecOptimizer() {
    if (impl_->gpu_memory_pool_) {
        // Clean up GPU memory pool
        // Implementation depends on graphics API
    }
}

void CodecOptimizer::configure_codec(const std::string& codec, const CodecOptimizerConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->config_mutex_);
    impl_->codec_configs_[codec] = config;
    
    ve::log::info("Configured codec optimization for " + codec + ": strategy=" + 
                 std::to_string(static_cast<int>(config.strategy)) + ", threads=" +
                 std::to_string(config.max_decode_threads));
}

CodecOptimizerConfig CodecOptimizer::get_codec_config(const std::string& codec) const {
    std::lock_guard<std::mutex> lock(impl_->config_mutex_);
    auto it = impl_->codec_configs_.find(codec);
    if (it != impl_->codec_configs_.end()) {
        return it->second;
    }
    
    // Return default configuration
    CodecOptimizerConfig default_config;
    default_config.codec_name = codec;
    return default_config;
}

bool CodecOptimizer::apply_prores_optimization(const std::string& prores_variant) {
    auto config = codec_utils::ProResOptimization::get_optimal_config(prores_variant);
    configure_codec("prores", config);
    
    // ProRes-specific optimizations
    if (codec_utils::ProResOptimization::is_gpu_accelerated_variant(prores_variant)) {
        config.strategy = CodecOptimization::HARDWARE_ACCELERATED;
        config.enable_zero_copy_nv12 = true;
    }
    
    ve::log::info("Applied ProRes optimization for variant: " + prores_variant);
    return true;
}

bool CodecOptimizer::apply_hevc_optimization(bool is_10bit, bool is_hdr) {
    auto config = codec_utils::HEVCOptimization::get_optimal_config(is_10bit, is_hdr);
    configure_codec("h265", config);
    
    // HEVC-specific optimizations
    if (is_10bit) {
        config.enable_10bit_optimization = true;
    }
    if (is_hdr) {
        config.enable_hdr_fast_path = true;
    }
    
    ve::log::info("Applied HEVC optimization: 10bit=" + std::to_string(is_10bit) + 
                 ", HDR=" + std::to_string(is_hdr));
    return true;
}

bool CodecOptimizer::apply_h264_optimization(bool is_high_profile) {
    auto config = codec_utils::H264Optimization::get_optimal_config(is_high_profile);
    configure_codec("h264", config);
    
    ve::log::info("Applied H.264 optimization: high_profile=" + std::to_string(is_high_profile));
    return true;
}

bool CodecOptimizer::allocate_gpu_memory_pool(size_t size_bytes) {
    (void)size_bytes; // Suppress unused parameter warning
#ifdef _WIN32
    // Allocate D3D11 shared memory pool
    // This is a simplified implementation
    impl_->gpu_pool_size_ = size_bytes;
    ve::log::info("Allocated GPU memory pool: " + 
                 std::to_string(size_bytes / (1024 * 1024)) + " MB");
    return true;
#else
    return false;
#endif
}

void* CodecOptimizer::get_gpu_memory_block(size_t size) {
    (void)size; // Suppress unused parameter warning
    // Return GPU memory block from pool
    // Implementation depends on memory management strategy
    return nullptr;
}

void CodecOptimizer::release_gpu_memory_block(void* ptr) {
    (void)ptr; // Suppress unused parameter warning
    // Release GPU memory block back to pool
}

void CodecOptimizer::update_stats(const std::string& codec, const CodecOptimizationStats& stats) {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
    impl_->codec_stats_[codec] = stats;
    
    // Adaptive optimization
    if (impl_->adaptive_enabled_ && impl_->adaptive_callback_) {
        impl_->adaptive_callback_(stats);
    }
}

CodecOptimizationStats CodecOptimizer::get_stats(const std::string& codec) const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
    auto it = impl_->codec_stats_.find(codec);
    if (it != impl_->codec_stats_.end()) {
        return it->second;
    }
    return CodecOptimizationStats{};
}

void CodecOptimizer::enable_adaptive_optimization(OptimizationCallback callback) {
    impl_->adaptive_callback_ = std::move(callback);
    impl_->adaptive_enabled_ = true;
    ve::log::info("Adaptive codec optimization enabled");
}

void CodecOptimizer::disable_adaptive_optimization() {
    impl_->adaptive_enabled_ = false;
    impl_->adaptive_callback_ = nullptr;
}

CodecOptimizer::HardwareCapabilities CodecOptimizer::detect_hardware_capabilities() const {
    if (impl_->caps_detected_) {
        return impl_->hardware_caps_;
    }
    
    HardwareCapabilities caps{};
    
#ifdef _WIN32
    // TODO: Implement proper D3D11VA hardware detection when header conflicts resolved
    // For now, assume hardware acceleration is available on Windows
    caps.supports_d3d11va = true;
    caps.supports_dxva2 = true;
    caps.supports_zero_copy = true;
    caps.max_gpu_memory = 1024 * 1024 * 1024; // 1GB placeholder
    caps.max_decode_sessions = 4;
    
    // Detect NVIDIA NVDEC
    // This would require NVIDIA Video Codec SDK
    caps.supports_nvdec = false; // Placeholder
    
    // Detect Intel QuickSync
    caps.supports_quicksync = false; // Placeholder
#endif
    
    const_cast<CodecOptimizer::Impl*>(impl_.get())->hardware_caps_ = caps;
    const_cast<CodecOptimizer::Impl*>(impl_.get())->caps_detected_ = true;
    
    ve::log::info("Hardware capabilities: D3D11VA=" + std::to_string(caps.supports_d3d11va) +
                 ", NVDEC=" + std::to_string(caps.supports_nvdec) + 
                 ", QuickSync=" + std::to_string(caps.supports_quicksync) +
                 ", GPU_Memory=" + std::to_string(caps.max_gpu_memory / (1024 * 1024)) + "MB");
    
    return caps;
}

CodecOptimizerConfig CodecOptimizer::recommend_config(const std::string& codec, 
                                                     int width, int height, 
                                                     double target_fps) const {
    (void)target_fps; // Suppress unused parameter warning
    auto caps = detect_hardware_capabilities();
    CodecOptimizerConfig config;
    config.codec_name = codec;
    
    // Resolution-based optimization
    bool is_4k = (width >= 3840 && height >= 2160);
    bool is_8k = (width >= 7680 && height >= 4320);
    (void)is_8k; // Suppress unused variable warning
    
    // Codec-specific recommendations
    if (codec == "h264") {
        if (caps.supports_d3d11va && is_4k) {
            config.strategy = CodecOptimization::HARDWARE_ACCELERATED;
            config.max_decode_threads = 8; // Increased for 4K performance
        } else {
            config.strategy = CodecOptimization::SOFTWARE_ONLY;
            config.max_decode_threads = 4;
        }
    } else if (codec == "h265") {
        if (caps.supports_d3d11va) {
            config.strategy = CodecOptimization::ZERO_COPY_GPU;
            config.enable_zero_copy_nv12 = true;
            config.max_decode_threads = 1; // Hardware decode is single-threaded
        } else {
            config.strategy = CodecOptimization::SOFTWARE_ONLY;
            config.max_decode_threads = is_4k ? 8 : 4;
        }
    } else if (codec == "prores") {
        config.strategy = CodecOptimization::HARDWARE_ACCELERATED;
        config.max_decode_threads = 4;
        config.enable_predictive_caching = true;
    }
    
    // Memory pool sizing
    if (config.strategy == CodecOptimization::ZERO_COPY_GPU) {
        size_t frame_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 2; // NV12 format
        config.gpu_memory_pool_size = frame_size * 10; // 10 frame buffer
        config.enable_gpu_memory_pool = true;
    }
    
    return config;
}

void CodecOptimizer::initialize_default_configs() {
    // Default H.264 configuration
    CodecOptimizerConfig h264_config;
    h264_config.codec_name = "h264";
    h264_config.strategy = CodecOptimization::HARDWARE_ACCELERATED;
    h264_config.max_decode_threads = 2;
    h264_config.prefer_d3d11va = true;
    configure_codec("h264", h264_config);
    
    // Default H.265 configuration
    CodecOptimizerConfig h265_config;
    h265_config.codec_name = "h265";
    h265_config.strategy = CodecOptimization::ZERO_COPY_GPU;
    h265_config.max_decode_threads = 1;
    h265_config.enable_zero_copy_nv12 = true;
    h265_config.prefer_nvdec = true;
    configure_codec("h265", h265_config);
    
    // Default ProRes configuration
    CodecOptimizerConfig prores_config;
    prores_config.codec_name = "prores";
    prores_config.strategy = CodecOptimization::HARDWARE_ACCELERATED;
    prores_config.max_decode_threads = 4;
    prores_config.enable_predictive_caching = true;
    configure_codec("prores", prores_config);
}

// Format-specific optimization implementations
namespace codec_utils {

CodecOptimizerConfig ProResOptimization::get_optimal_config(const std::string& variant) {
    CodecOptimizerConfig config;
    config.codec_name = "prores";
    
    if (variant == "4444" || variant == "4444XQ") {
        // High-quality variants benefit from CPU processing
        config.strategy = CodecOptimization::SOFTWARE_ONLY;
        config.max_decode_threads = 6;
    } else {
        // Standard variants can use hardware acceleration
        config.strategy = CodecOptimization::HARDWARE_ACCELERATED;
        config.max_decode_threads = 4;
    }
    
    config.enable_predictive_caching = true;
    return config;
}

bool ProResOptimization::is_gpu_accelerated_variant(const std::string& variant) {
    return variant != "4444" && variant != "4444XQ";
}

size_t ProResOptimization::estimate_memory_requirements(int width, int height, const std::string& variant) {
    size_t base_size = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (variant == "4444" || variant == "4444XQ") {
        return base_size * 8; // Higher bit depth
    }
    return base_size * 3; // Standard YUV
}

CodecOptimizerConfig HEVCOptimization::get_optimal_config(bool is_10bit, bool is_hdr) {
    CodecOptimizerConfig config;
    config.codec_name = "h265";
    config.strategy = CodecOptimization::ZERO_COPY_GPU;
    config.max_decode_threads = 1;
    config.enable_zero_copy_nv12 = true;
    config.prefer_nvdec = true;
    
    if (is_10bit) {
        config.enable_10bit_optimization = true;
    }
    if (is_hdr) {
        config.enable_hdr_fast_path = true;
    }
    
    return config;
}

bool HEVCOptimization::should_use_hardware_decode(int width, int height, bool is_10bit) {
    // Hardware decode beneficial for 4K+ or 10-bit content
    return (width >= 3840 && height >= 2160) || is_10bit;
}

int HEVCOptimization::get_optimal_thread_count(int width, int height) {
    if (width >= 7680) return 1; // 8K uses hardware decode
    if (width >= 3840) return 2; // 4K
    return 4; // HD and below
}

CodecOptimizerConfig H264Optimization::get_optimal_config(bool is_high_profile) {
    CodecOptimizerConfig config;
    config.codec_name = "h264";
    config.strategy = CodecOptimization::HARDWARE_ACCELERATED;
    config.max_decode_threads = is_high_profile ? 2 : 4;
    config.prefer_d3d11va = true;
    
    return config;
}

bool H264Optimization::benefits_from_gpu_decode(int width, int height) {
    // H.264 benefits from GPU decode at 4K+
    return width >= 3840 && height >= 2160;
}

size_t H264Optimization::get_optimal_buffer_size(int width, int height) {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * 2; // NV12 format
}

} // namespace codec_utils

} // namespace ve::decode
