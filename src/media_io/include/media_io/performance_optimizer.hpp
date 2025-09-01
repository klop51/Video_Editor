#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <future>
#include <chrono>
#include <functional>

// Compatibility for older compilers
#if __cplusplus >= 201402L || (defined(_MSC_VER) && _MSC_VER >= 1900)
#include <shared_mutex>
#else
// Fallback to regular mutex for older compilers
namespace std {
    using shared_mutex = mutex;
    template<typename Mutex> using shared_lock = lock_guard<Mutex>;
}
#endif

namespace media_io {

// Forward declarations
struct MediaFrame;
struct DecoderConfig;

/**
 * Hardware acceleration capabilities
 */
enum class HardwareAcceleration {
    NONE,                    // CPU-only decode
    NVIDIA_NVDEC,           // NVIDIA GPU decode
    INTEL_QUICKSYNC,        // Intel Quick Sync Video
    AMD_VCE,                // AMD Video Coding Engine
    APPLE_VIDEOTOOLBOX,     // Apple VideoToolbox (future)
    DXVA2,                  // DirectX Video Acceleration
    D3D11VA,                // DirectX 11 Video Acceleration
    VULKAN_VIDEO            // Vulkan Video (future)
};

/**
 * Performance optimization strategies
 */
enum class OptimizationStrategy {
    QUALITY_FIRST,          // Prioritize quality over speed
    SPEED_FIRST,            // Prioritize speed over quality
    BALANCED,               // Balance quality and speed
    MEMORY_EFFICIENT,       // Minimize memory usage
    POWER_EFFICIENT,        // Minimize power consumption
    REAL_TIME               // Real-time performance critical
};

/**
 * Codec performance characteristics
 */
struct CodecPerformance {
    std::string codec_name;
    HardwareAcceleration best_hw_accel;
    double cpu_decode_factor;       // Relative CPU cost (1.0 = baseline H.264)
    double gpu_decode_factor;       // Relative GPU cost
    size_t memory_per_frame;        // Bytes per frame
    bool supports_zero_copy;        // Zero-copy decode support
    bool supports_predictive_cache; // Predictive caching support
    
    // Performance targets (frames per second)
    struct {
        double hd_1080p;            // 1920x1080
        double uhd_4k;              // 3840x2160
        double uhd_8k;              // 7680x4320
    } target_fps;
};

/**
 * Memory allocation strategy
 */
enum class MemoryStrategy {
    STANDARD,               // Standard allocator
    POOL_BASED,            // Memory pool allocation
    NUMA_AWARE,            // NUMA-aware allocation
    ZERO_COPY,             // Zero-copy when possible
    STREAMING,             // Streaming-optimized
    CACHE_FRIENDLY         // Cache-optimized layout
};

/**
 * Frame cache prediction
 */
struct CachePrediction {
    int64_t frame_number;
    double prediction_confidence;   // 0.0 to 1.0
    std::chrono::steady_clock::time_point predicted_access_time;
    size_t memory_cost;
    bool is_keyframe;
};

/**
 * Performance metrics tracking
 */
struct PerformanceMetrics {
    // Decode performance
    std::chrono::microseconds avg_decode_time;
    std::chrono::microseconds max_decode_time;
    double frames_per_second;
    
    // Memory usage
    size_t peak_memory_usage;
    size_t current_memory_usage;
    size_t cache_hit_rate_percent;
    
    // Hardware utilization
    double cpu_usage_percent;
    double gpu_usage_percent;
    double memory_bandwidth_utilization;
    
    // Queue metrics
    size_t decode_queue_depth;
    size_t max_queue_depth;
    std::chrono::microseconds avg_queue_wait_time;
    
    // Error rates
    size_t decode_errors;
    size_t cache_misses;
    size_t hardware_fallbacks;
};

/**
 * Decode work item for threading
 */
struct DecodeWorkItem {
    int64_t frame_number;
    int priority;                   // Higher number = higher priority
    std::vector<uint8_t> compressed_data;
    std::promise<std::shared_ptr<MediaFrame>> result_promise;
    std::chrono::steady_clock::time_point submit_time;
    HardwareAcceleration preferred_hw_accel;
};

/**
 * Lock-free decode queue
 */
class LockFreeDecodeQueue {
public:
    LockFreeDecodeQueue(size_t capacity = 1024);
    ~LockFreeDecodeQueue();
    
    bool enqueue(DecodeWorkItem&& item);
    bool dequeue(DecodeWorkItem& item);
    size_t size() const;
    bool empty() const;
    
private:
    struct Node {
        std::atomic<DecodeWorkItem*> data{nullptr};
        std::atomic<size_t> sequence{0};
        
        // Make Node non-copyable but movable
        Node() = default;
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = default;
        Node& operator=(Node&&) = default;
    };
    
    std::unique_ptr<Node[]> buffer_;
    std::atomic<size_t> enqueue_pos_{0};
    std::atomic<size_t> dequeue_pos_{0};
    size_t buffer_mask_;
    size_t capacity_;
};

/**
 * NUMA-aware memory allocator
 */
class NUMAAllocator {
public:
    NUMAAllocator();
    ~NUMAAllocator();
    
    void* allocate(size_t size, int preferred_node = -1);
    void deallocate(void* ptr);
    int get_optimal_node_for_thread() const;
    
private:
    std::vector<int> numa_nodes_;
    std::unordered_map<std::thread::id, int> thread_to_node_;
    mutable std::mutex node_mapping_mutex_;
};

/**
 * Predictive frame cache
 */
class PredictiveFrameCache {
public:
    PredictiveFrameCache(size_t max_memory_bytes = 2LL * 1024 * 1024 * 1024); // 2GB default
    ~PredictiveFrameCache();
    
    void predict_access_pattern(const std::vector<int64_t>& recent_accesses);
    std::shared_ptr<MediaFrame> get_frame(int64_t frame_number);
    void cache_frame(int64_t frame_number, std::shared_ptr<MediaFrame> frame);
    void evict_least_likely();
    
    size_t get_memory_usage() const;
    double get_hit_rate() const;
    
private:
    struct CacheEntry {
        std::shared_ptr<MediaFrame> frame;
        double access_probability;
        std::chrono::steady_clock::time_point last_access;
        size_t memory_size;
    };
    
    std::unordered_map<int64_t, CacheEntry> cache_;
    std::vector<CachePrediction> predictions_;
    mutable std::shared_mutex cache_mutex_;
    
    size_t max_memory_bytes_;
    std::atomic<size_t> current_memory_usage_{0};
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_accesses_{0};
};

/**
 * Main performance optimizer class
 */
class PerformanceOptimizer {
public:
    PerformanceOptimizer();
    ~PerformanceOptimizer();
    
    // Initialization and configuration
    bool initialize(OptimizationStrategy strategy = OptimizationStrategy::BALANCED);
    void set_strategy(OptimizationStrategy strategy);
    void set_memory_strategy(MemoryStrategy strategy);
    
    // Hardware capability detection
    std::vector<HardwareAcceleration> detect_available_hardware();
    HardwareAcceleration select_optimal_hardware(const std::string& codec);
    bool is_hardware_available(HardwareAcceleration hw_accel);
    
    // Codec performance management
    void register_codec_performance(const CodecPerformance& perf);
    CodecPerformance get_codec_performance(const std::string& codec);
    bool can_achieve_target_fps(const std::string& codec, int width, int height, double target_fps);
    
    // Decode queue management
    std::future<std::shared_ptr<MediaFrame>> submit_decode_work(DecodeWorkItem&& work);
    void set_max_decode_threads(size_t thread_count);
    void set_priority_boost_factor(double factor);
    
    // Memory optimization
    void* allocate_frame_memory(size_t size);
    void deallocate_frame_memory(void* ptr);
    void optimize_memory_layout();
    void trigger_garbage_collection();
    
    // Predictive caching
    void enable_predictive_caching(bool enable);
    void update_access_pattern(const std::vector<int64_t>& frame_accesses);
    void preload_predicted_frames(int64_t current_frame, int lookahead_frames = 10);
    
    // Performance monitoring
    PerformanceMetrics get_performance_metrics() const;
    void reset_performance_metrics();
    void log_performance_event(const std::string& event, std::chrono::microseconds duration);
    
    // Workload distribution
    void balance_cpu_gpu_workload();
    double get_optimal_cpu_threads() const;
    double get_optimal_gpu_utilization() const;
    
    // Quality control
    void set_quality_threshold(double threshold);
    bool meets_quality_requirements(const PerformanceMetrics& metrics);
    void adapt_quality_for_performance();
    
private:
    // Configuration
    OptimizationStrategy strategy_;
    MemoryStrategy memory_strategy_;
    std::vector<HardwareAcceleration> available_hardware_;
    std::unordered_map<std::string, CodecPerformance> codec_performance_;
    
    // Threading infrastructure
    std::vector<std::thread> decode_threads_;
    std::unique_ptr<LockFreeDecodeQueue> decode_queue_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Memory management
    std::unique_ptr<NUMAAllocator> numa_allocator_;
    std::unique_ptr<PredictiveFrameCache> frame_cache_;
    
    // Performance tracking
    mutable std::mutex metrics_mutex_;
    PerformanceMetrics current_metrics_;
    std::queue<std::chrono::microseconds> recent_decode_times_;
    
    // Workload balancing
    std::atomic<double> cpu_gpu_balance_{0.5}; // 0.0 = all CPU, 1.0 = all GPU
    std::atomic<size_t> active_decode_threads_{0};
    
    // Quality control
    double quality_threshold_{0.95};
    bool adaptive_quality_{true};
    
    // Internal methods
    void decode_worker_thread(int thread_id);
    HardwareAcceleration select_hardware_for_workload(const DecodeWorkItem& work);
    void update_performance_metrics();
    void optimize_thread_count();
    void balance_workload_distribution();
    bool should_use_hardware_acceleration(const std::string& codec, int width, int height);
    void handle_hardware_fallback(const DecodeWorkItem& work);
    void register_default_codec_performance(); // Add this method
};

// Performance optimization utilities
namespace performance_utils {
    /**
     * Get optimal thread count for current system
     */
    size_t get_optimal_thread_count();
    
    /**
     * Check if NUMA is available and beneficial
     */
    bool is_numa_available();
    
    /**
     * Get system memory information
     */
    struct SystemMemoryInfo {
        size_t total_physical_memory;
        size_t available_physical_memory;
        size_t total_virtual_memory;
        size_t available_virtual_memory;
        size_t cache_line_size;
        size_t page_size;
    };
    SystemMemoryInfo get_system_memory_info();
    
    /**
     * CPU feature detection
     */
    struct CPUFeatures {
        bool has_avx2;
        bool has_avx512;
        bool has_sse4_1;
        bool has_sse4_2;
        bool has_fma;
        int cache_levels;
        size_t l1_cache_size;
        size_t l2_cache_size;
        size_t l3_cache_size;
    };
    CPUFeatures detect_cpu_features();
    
    /**
     * GPU capability detection
     */
    struct GPUCapabilities {
        std::string vendor;
        std::string model;
        size_t total_memory;
        size_t available_memory;
        int compute_units;
        bool supports_h264_decode;
        bool supports_h265_decode;
        bool supports_av1_decode;
        bool supports_prores_decode;
    };
    std::vector<GPUCapabilities> detect_gpu_capabilities();
    
    /**
     * Performance benchmarking
     */
    double benchmark_decode_performance(const std::string& codec, int width, int height);
    double benchmark_memory_bandwidth();
    double benchmark_cpu_performance();
    
} // namespace performance_utils

} // namespace media_io
