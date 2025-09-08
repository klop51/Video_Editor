// GPU Memory Optimizer - Intelligent VRAM Management and Caching
// Professional-grade memory optimization for video editing workloads
// Supports 8K+ video processing with efficient resource utilization

#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>
#include <functional>
#include "graphics_device.hpp"
#include "include/gfx/texture_manager.hpp"

namespace video_editor::gfx {

// Forward declarations
class GraphicsDevice;
class TextureHandle;
enum class TextureFormat;
enum class CompressionLevel;

// Memory allocation statistics
struct MemoryStats {
    size_t total_vram = 0;
    size_t used_vram = 0;
    size_t available_vram = 0;
    size_t cached_memory = 0;
    size_t compressed_memory = 0;
    float fragmentation_ratio = 0.0f;
    uint32_t active_allocations = 0;
    uint32_t cache_hits = 0;
    uint32_t cache_misses = 0;
    float hit_ratio = 0.0f;
    
    void update_hit_ratio() {
        uint32_t total_requests = cache_hits + cache_misses;
        hit_ratio = total_requests > 0 ? float(cache_hits) / float(total_requests) : 0.0f;
    }
};

// Cache entry with intelligent scoring
struct CacheEntry {
    uint64_t hash = 0;
    TextureHandle texture;
    std::chrono::steady_clock::time_point last_access_time;
    std::chrono::steady_clock::time_point creation_time;
    uint32_t access_count = 0;
    uint32_t frame_last_used = 0;
    float quality_score = 0.0f;
    size_t memory_size = 0;
    bool is_compressed = false;
    bool is_critical = false;          // Critical textures (current frame, etc.)
    bool is_predicted_needed = false;  // AI prediction flag
    uint32_t reference_count = 0;      // Number of active references
    
    // Calculate dynamic priority for eviction decisions
    float calculate_priority(uint32_t current_frame) const {
        auto now = std::chrono::steady_clock::now();
        auto time_since_access = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_access_time).count();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - creation_time).count();
        
        float time_factor = 1.0f / (1.0f + time_since_access * 0.001f); // Decay over time
        float usage_factor = std::min(access_count / 10.0f, 2.0f);       // Frequent use bonus
        float recency_factor = 1.0f / (1.0f + (current_frame - frame_last_used)); // Frame recency
        float critical_factor = is_critical ? 10.0f : 1.0f;             // Critical protection
        float prediction_factor = is_predicted_needed ? 2.0f : 1.0f;    // AI prediction boost
        
        return quality_score * time_factor * usage_factor * recency_factor * 
               critical_factor * prediction_factor;
    }
};

// Compression levels for different scenarios
enum class CompressionLevel {
    None = 0,           // No compression (fastest access)
    Fast = 1,           // Fast compression (BC1/BC3)
    Balanced = 2,       // Balanced quality/size (BC7)
    Maximum = 3,        // Maximum compression (custom codecs)
    Lossless = 4        // Lossless compression (when quality is critical)
};

// Texture access pattern analysis
struct AccessPattern {
    enum class Type {
        Sequential,     // Linear access (timeline playback)
        Random,         // Random access (scrubbing)
        Predictable,    // Predictable pattern (effects processing)
        Burst           // Burst access (export rendering)
    };
    
    Type pattern_type = Type::Random;
    float confidence = 0.0f;
    std::vector<uint32_t> recent_frames;
    uint32_t predicted_next_frame = 0;
    
    void analyze_recent_access(const std::vector<uint32_t>& frame_history);
    uint32_t predict_next_access() const;
};

// Intelligent Cache System
class IntelligentCache {
public:
    struct Config {
        size_t max_cache_size = 2ULL * 1024 * 1024 * 1024; // 2GB default
        size_t min_free_vram = 512ULL * 1024 * 1024;       // Keep 512MB free
        float eviction_threshold = 0.85f;                   // Start eviction at 85% full
        uint32_t max_entries = 10000;                       // Maximum cache entries
        bool enable_compression = true;                     // Enable texture compression
        bool enable_prediction = true;                      // Enable AI prediction
        uint32_t prediction_lookahead = 60;                // Frames to predict ahead
        float quality_threshold = 0.1f;                    // Minimum quality for caching
    };

private:
    Config config_;
    std::unordered_map<uint64_t, std::unique_ptr<CacheEntry>> cache_entries_;
    std::unordered_set<uint64_t> critical_hashes_;
    mutable std::shared_mutex cache_mutex_;
    
    // Statistics and monitoring
    MemoryStats stats_;
    std::atomic<uint32_t> current_frame_{0};
    std::vector<uint32_t> frame_access_history_;
    AccessPattern access_pattern_;
    
    // Background optimization thread
    std::thread optimization_thread_;
    std::atomic<bool> should_optimize_{true};
    std::mutex optimization_mutex_;
    
    // Prediction system
    std::queue<uint32_t> predicted_frames_;
    std::unordered_set<uint64_t> preloaded_hashes_;

public:
    explicit IntelligentCache(const Config& config = Config{});
    ~IntelligentCache();

    // Core cache operations
    TextureHandle get_texture(uint64_t hash);
    bool put_texture(uint64_t hash, TextureHandle texture, float quality_score = 1.0f);
    void remove_texture(uint64_t hash);
    void mark_critical(uint64_t hash, bool critical = true);
    
    // Frame-based operations for video editing
    void notify_frame_access(uint32_t frame_number);
    void preload_frame_range(uint32_t start_frame, uint32_t end_frame);
    void invalidate_frame_range(uint32_t start_frame, uint32_t end_frame);
    
    // Memory management
    void force_cleanup();
    void trigger_garbage_collection();
    bool ensure_free_memory(size_t required_bytes);
    void set_memory_pressure(float pressure); // 0.0 = no pressure, 1.0 = critical
    
    // Pattern analysis and prediction
    void update_access_patterns();
    void predict_future_needs();
    void preload_likely_textures();
    AccessPattern::Type get_current_pattern() const { return access_pattern_.pattern_type; }
    
    // Statistics and monitoring
    MemoryStats get_statistics() const;
    float get_hit_ratio() const { return stats_.hit_ratio; }
    size_t get_cache_size() const;
    void reset_statistics();
    
    // Configuration
    void update_config(const Config& new_config);
    Config get_config() const { return config_; }

private:
    // Internal cache management
    void evict_least_valuable();
    void evict_by_size(size_t target_size);
    std::vector<uint64_t> find_eviction_candidates(size_t required_space);
    void update_entry_scores();
    
    // Background optimization
    void optimization_thread_func();
    void optimize_cache_layout();
    void compress_eligible_textures();
    void cleanup_expired_entries();
    
    // Prediction algorithms
    void analyze_sequential_pattern();
    void analyze_random_pattern();
    void predict_scrubbing_behavior();
    uint64_t generate_texture_hash(uint32_t frame, const std::string& identifier) const;
};

// Texture Compression System
class TextureCompression {
public:
    struct CompressionInfo {
        CompressionLevel level;
        TextureFormat original_format;
        TextureFormat compressed_format;
        float compression_ratio;
        uint32_t compression_time_ms;
        uint32_t decompression_time_ms;
        bool is_lossy;
    };

private:
    GraphicsDevice* device_;
    std::unordered_map<TextureFormat, std::vector<CompressionInfo>> compression_profiles_;
    mutable std::shared_mutex profiles_mutex_;

public:
    explicit TextureCompression(GraphicsDevice* device);
    
    // Core compression operations
    TextureHandle compress_for_cache(TextureHandle input, CompressionLevel level);
    TextureHandle decompress_for_use(TextureHandle compressed);
    
    // Analysis and optimization
    float get_compression_ratio(TextureFormat format, CompressionLevel level) const;
    CompressionLevel recommend_compression_level(TextureFormat format, float quality_requirement) const;
    bool is_compression_beneficial(size_t texture_size, float access_frequency) const;
    
    // Profile management
    void benchmark_compression_methods();
    void add_compression_profile(TextureFormat format, const CompressionInfo& info);
    std::vector<CompressionInfo> get_available_compressions(TextureFormat format) const;

private:
    void initialize_default_profiles();
    CompressionInfo find_best_compression(TextureFormat format, CompressionLevel level) const;
};

// Streaming Optimizer for Large Video Files
class StreamingOptimizer {
public:
    struct StreamingConfig {
        size_t streaming_buffer_size = 256ULL * 1024 * 1024; // 256MB streaming buffer
        uint32_t read_ahead_frames = 30;                      // Frames to read ahead
        uint32_t max_concurrent_loads = 4;                    // Parallel loading threads
        float load_threshold = 0.7f;                          // Start loading at 70% buffer
        bool enable_adaptive_quality = true;                  // Dynamic quality adjustment
        bool enable_predictive_loading = true;                // Predict and preload
    };

    struct StreamingStats {
        uint64_t bytes_streamed = 0;
        uint32_t frames_streamed = 0;
        uint32_t cache_hits = 0;
        uint32_t cache_misses = 0;
        float average_load_time_ms = 0.0f;
        float buffer_utilization = 0.0f;
        bool is_underrun = false;
    };

private:
    StreamingConfig config_;
    IntelligentCache* cache_;
    GraphicsDevice* device_;
    
    // Streaming state
    std::atomic<uint32_t> current_playhead_{0};
    std::queue<uint32_t> loading_queue_;
    std::vector<std::thread> loader_threads_;
    std::atomic<bool> is_streaming_{false};
    mutable std::mutex streaming_mutex_;
    
    // Statistics
    StreamingStats stats_;
    std::chrono::steady_clock::time_point last_stats_update_;

public:
    StreamingOptimizer(IntelligentCache* cache, GraphicsDevice* device, 
                      const StreamingConfig& config = StreamingConfig{});
    ~StreamingOptimizer();
    
    // Streaming control
    void start_streaming(uint32_t start_frame);
    void stop_streaming();
    void seek_to_frame(uint32_t frame);
    void set_playback_speed(float speed); // 1.0 = normal, 2.0 = 2x, etc.
    
    // Analysis and optimization
    void analyze_access_patterns();
    void adjust_cache_size_dynamically();
    void prioritize_critical_textures();
    void optimize_for_playback_mode(bool is_realtime);
    
    // Configuration and statistics
    void update_config(const StreamingConfig& new_config);
    StreamingStats get_statistics() const;
    bool is_buffer_healthy() const;

private:
    void loader_thread_func();
    void load_frame_async(uint32_t frame);
    void update_loading_priorities();
    void adjust_quality_based_on_performance();
};

// VRAM Usage Monitor and Cleanup
struct VRAMMonitor {
    size_t total_vram = 0;
    size_t used_vram = 0;
    size_t available_vram = 0;
    size_t reserved_vram = 0;       // Memory reserved for critical operations
    float fragmentation_ratio = 0.0f;
    float memory_pressure = 0.0f;   // 0.0 = no pressure, 1.0 = critical
    
    // Thresholds for memory management
    struct Thresholds {
        float warning_threshold = 0.75f;    // 75% usage warning
        float critical_threshold = 0.90f;   // 90% usage critical
        float cleanup_threshold = 0.85f;    // 85% usage trigger cleanup
        size_t min_free_bytes = 128ULL * 1024 * 1024; // Always keep 128MB free
    } thresholds;
    
    // Callback system for memory events
    std::function<void(float)> on_memory_pressure_changed;
    std::function<void()> on_memory_warning;
    std::function<void()> on_memory_critical;
    
    void update_from_device(GraphicsDevice* device);
    void trigger_cleanup_if_needed(IntelligentCache* cache);
    void calculate_fragmentation();
    bool is_memory_available(size_t required_bytes) const;
    float get_usage_ratio() const;
};

// Main GPU Memory Optimizer - Orchestrates all components
class GPUMemoryOptimizer {
public:
    struct OptimizerConfig {
        IntelligentCache::Config cache_config;
        StreamingOptimizer::StreamingConfig streaming_config;
        VRAMMonitor::Thresholds memory_thresholds;
        bool enable_background_optimization = true;
        uint32_t optimization_interval_ms = 1000; // Run optimization every second
        bool enable_telemetry = true;              // Collect performance telemetry
    };

private:
    GraphicsDevice* device_;
    std::unique_ptr<IntelligentCache> cache_;
    std::unique_ptr<TextureCompression> compression_;
    std::unique_ptr<StreamingOptimizer> streaming_;
    VRAMMonitor vram_monitor_;
    
    OptimizerConfig config_;
    std::thread monitoring_thread_;
    std::atomic<bool> should_monitor_{true};

public:
    explicit GPUMemoryOptimizer(GraphicsDevice* device, 
                               const OptimizerConfig& config = OptimizerConfig{});
    ~GPUMemoryOptimizer();
    
    // Main interface
    TextureHandle get_texture(uint64_t hash);
    bool cache_texture(uint64_t hash, TextureHandle texture, float quality = 1.0f);
    void notify_frame_change(uint32_t new_frame);
    void optimize_for_workflow(const std::string& workflow_type);
    
    // Memory management
    bool ensure_memory_available(size_t required_bytes);
    void force_memory_cleanup();
    void set_memory_pressure_callback(std::function<void(float)> callback);
    
    // Statistics and monitoring
    MemoryStats get_memory_statistics() const;
    StreamingOptimizer::StreamingStats get_streaming_statistics() const;
    VRAMMonitor get_vram_status() const { return vram_monitor_; }
    
    // Configuration
    void update_configuration(const OptimizerConfig& new_config);
    OptimizerConfig get_configuration() const { return config_; }

private:
    void monitoring_thread_func();
    void optimize_for_realtime_playback();
    void optimize_for_scrubbing();
    void optimize_for_rendering();
    void handle_memory_pressure(float pressure);
};

} // namespace video_editor::gfx
