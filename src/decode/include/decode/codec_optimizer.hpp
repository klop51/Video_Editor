#pragma once
#include "frame.hpp"
#include "gpu_upload.hpp"
#include <string>
#include <memory>
#include <functional>

namespace ve::decode {

// Codec-specific optimization strategies
enum class CodecOptimization {
    SOFTWARE_ONLY,
    HARDWARE_ACCELERATED,
    ZERO_COPY_GPU,
    PREDICTIVE_DECODE
};

// Optimization configuration for specific codecs
struct CodecOptimizerConfig {
    std::string codec_name;
    CodecOptimization strategy = CodecOptimization::SOFTWARE_ONLY;
    bool enable_gpu_memory_pool = false;
    bool enable_predictive_caching = false;
    int max_decode_threads = 1;
    size_t gpu_memory_pool_size = 0;
    
    // Hardware-specific settings
    bool prefer_d3d11va = true;
    bool prefer_dxva2 = false;
    bool prefer_nvdec = true;
    
    // Format-specific optimizations
    bool enable_zero_copy_nv12 = false;
    bool enable_10bit_optimization = false;
    bool enable_hdr_fast_path = false;
};

// Performance statistics for optimization feedback
struct CodecOptimizationStats {
    double decode_fps = 0.0;
    double gpu_utilization = 0.0;
    size_t frames_decoded = 0;
    size_t frames_dropped = 0;
    size_t zero_copy_frames = 0;
    size_t hardware_frames = 0;
    double avg_decode_time_ms = 0.0;
};

class CodecOptimizer {
public:
    using OptimizationCallback = std::function<void(const CodecOptimizationStats&)>;
    
    CodecOptimizer();
    ~CodecOptimizer();
    
    // Configure optimization for specific codec
    void configure_codec(const std::string& codec, const CodecOptimizerConfig& config);
    
    // Get optimization configuration for codec
    CodecOptimizerConfig get_codec_config(const std::string& codec) const;
    
    // Apply format-specific optimizations
    bool apply_prores_optimization(const std::string& prores_variant);
    bool apply_hevc_optimization(bool is_10bit, bool is_hdr);
    bool apply_h264_optimization(bool is_high_profile);
    
    // GPU memory management
    bool allocate_gpu_memory_pool(size_t size_bytes);
    void* get_gpu_memory_block(size_t size);
    void release_gpu_memory_block(void* ptr);
    
    // Performance monitoring
    void update_stats(const std::string& codec, const CodecOptimizationStats& stats);
    CodecOptimizationStats get_stats(const std::string& codec) const;
    
    // Adaptive optimization based on performance
    void enable_adaptive_optimization(OptimizationCallback callback);
    void disable_adaptive_optimization();
    
    // Hardware capability detection
    struct HardwareCapabilities {
        bool supports_d3d11va = false;
        bool supports_dxva2 = false;
        bool supports_nvdec = false;
        bool supports_quicksync = false;
        bool supports_zero_copy = false;
        size_t max_gpu_memory = 0;
        int max_decode_sessions = 0;
    };
    
    HardwareCapabilities detect_hardware_capabilities() const;
    
    // Optimization recommendations
    CodecOptimizerConfig recommend_config(const std::string& codec, 
                                        int width, int height, 
                                        double target_fps) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    // Private helper methods
    void initialize_default_configs();
};

// Format-specific optimization utilities
namespace codec_utils {
    
    // ProRes optimization
    struct ProResOptimization {
        static CodecOptimizerConfig get_optimal_config(const std::string& variant);
        static bool is_gpu_accelerated_variant(const std::string& variant);
        static size_t estimate_memory_requirements(int width, int height, const std::string& variant);
    };
    
    // HEVC optimization  
    struct HEVCOptimization {
        static CodecOptimizerConfig get_optimal_config(bool is_10bit, bool is_hdr);
        static bool should_use_hardware_decode(int width, int height, bool is_10bit);
        static int get_optimal_thread_count(int width, int height);
    };
    
    // H.264 optimization
    struct H264Optimization {
        static CodecOptimizerConfig get_optimal_config(bool is_high_profile);
        static bool benefits_from_gpu_decode(int width, int height);
        static size_t get_optimal_buffer_size(int width, int height);
    };
    
} // namespace codec_utils

} // namespace ve::decode
