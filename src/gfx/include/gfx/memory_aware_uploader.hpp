// Week 8: Memory Aware Uploader
// Smart texture uploader with memory pressure integration

#pragma once

#include "gfx/streaming_uploader.hpp"
#include "gfx/gpu_memory_manager.hpp"
#include <functional>
#include <atomic>
#include <mutex>

namespace ve::gfx {

/**
 * @brief Enhanced upload job with memory awareness
 */
struct MemoryAwareUploadJob : public UploadJob {
    bool check_memory_before_upload = true;     // Check memory pressure before uploading
    bool auto_compress_if_needed = true;        // Compress if memory pressure is high
    bool can_be_delayed = true;                 // Can delay upload if memory pressure is critical
    size_t memory_threshold_bytes = 0;          // Minimum memory required (0 = auto-calculate)
    
    // Callbacks for memory events
    std::function<void(MemoryPressure level)> memory_pressure_callback;
    std::function<void(bool compressed, float ratio)> compression_callback;
    std::function<void(uint32_t delay_ms)> delay_callback;
};

/**
 * @brief Statistics for memory-aware uploads
 */
struct MemoryAwareUploadStats : public UploadStats {
    // Memory coordination stats
    size_t uploads_delayed_for_memory = 0;
    size_t uploads_compressed_for_memory = 0;
    size_t uploads_cancelled_for_memory = 0;
    size_t memory_pressure_events_handled = 0;
    
    // Memory efficiency stats
    float average_memory_usage_during_uploads = 0.0f;
    float peak_memory_usage_during_uploads = 0.0f;
    size_t memory_evictions_triggered = 0;
    
    // Compression stats
    size_t total_compressed_uploads = 0;
    float average_compression_ratio = 0.0f;
    float compression_time_overhead_ms = 0.0f;
    
    void reset() {
        UploadStats::reset();
        uploads_delayed_for_memory = 0;
        uploads_compressed_for_memory = 0;
        uploads_cancelled_for_memory = 0;
        memory_pressure_events_handled = 0;
        average_memory_usage_during_uploads = 0.0f;
        peak_memory_usage_during_uploads = 0.0f;
        memory_evictions_triggered = 0;
        total_compressed_uploads = 0;
        average_compression_ratio = 0.0f;
        compression_time_overhead_ms = 0.0f;
    }
};

/**
 * @brief Memory-aware texture uploader
 * 
 * Extends StreamingTextureUploader with intelligent memory management.
 * Automatically handles memory pressure by delaying uploads, compressing data,
 * or triggering memory cleanup when needed.
 */
class MemoryAwareUploader {
public:
    /**
     * @brief Configuration for memory-aware uploads
     */
    struct Config {
        // Memory pressure thresholds
        float delay_uploads_threshold = 0.85f;      // Delay uploads at 85% memory
        float compress_uploads_threshold = 0.75f;   // Compress uploads at 75% memory
        float cancel_uploads_threshold = 0.95f;     // Cancel uploads at 95% memory
        
        // Compression settings
        bool enable_automatic_compression = true;
        float min_compression_ratio = 1.2f;         // Only compress if 20%+ savings
        uint32_t compression_timeout_ms = 5000;     // 5 second compression timeout
        
        // Delay settings
        uint32_t max_delay_time_ms = 10000;         // Maximum 10 second delay
        uint32_t delay_check_interval_ms = 100;     // Check every 100ms during delay
        
        // Memory management
        bool enable_preemptive_eviction = true;    // Evict memory before uploads
        float eviction_target_threshold = 0.6f;    // Evict down to 60% memory usage
        bool enable_memory_defragmentation = true; // Defrag during large uploads
        
        // Monitoring
        bool enable_detailed_logging = true;
        bool enable_memory_tracking = true;
    };
    
    /**
     * @brief Create memory-aware uploader
     * @param base_uploader Underlying streaming uploader
     * @param memory_manager GPU memory manager for coordination
     * @param config Configuration options
     */
    MemoryAwareUploader(StreamingTextureUploader* base_uploader, 
                       GPUMemoryManager* memory_manager,
                       const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~MemoryAwareUploader();
    
    // Non-copyable, non-movable
    MemoryAwareUploader(const MemoryAwareUploader&) = delete;
    MemoryAwareUploader& operator=(const MemoryAwareUploader&) = delete;
    
    /**
     * @brief Queue memory-aware upload job
     * @param job Memory-aware upload job with optimization options
     * @return Future that resolves when upload completes
     */
    std::future<bool> queue_upload_with_memory_check(const MemoryAwareUploadJob& job);
    
    /**
     * @brief Queue simple upload with automatic memory management
     * @param target Target texture handle
     * @param data Source data
     * @param data_size Data size in bytes
     * @param width Texture width
     * @param height Texture height
     * @param bytes_per_pixel Format information
     * @param priority Upload priority
     * @return Future that resolves when upload completes
     */
    std::future<bool> queue_upload_smart(TextureHandle target, const uint8_t* data, size_t data_size,
                                        uint32_t width, uint32_t height, uint32_t bytes_per_pixel,
                                        UploadJob::Priority priority = UploadJob::Priority::NORMAL);
    
    /**
     * @brief Handle memory pressure event from memory manager
     * @param level Current memory pressure level
     * @param stats Current memory statistics
     */
    void handle_memory_pressure_callback(MemoryPressure level, const GPUMemoryStats& stats);
    
    /**
     * @brief Force memory optimization before next upload
     * @param target_memory_usage Target memory usage percentage (0.0-1.0)
     * @return True if optimization was successful
     */
    bool optimize_memory_for_upload(float target_memory_usage = 0.6f);
    
    /**
     * @brief Cancel pending uploads that can be delayed
     * @param memory_threshold Only cancel uploads requiring more than this memory
     * @return Number of uploads cancelled
     */
    size_t cancel_delayable_uploads(size_t memory_threshold = 0);
    
    /**
     * @brief Get memory-aware upload statistics
     */
    MemoryAwareUploadStats get_memory_aware_stats() const;
    
    /**
     * @brief Reset statistics
     */
    void reset_stats();
    
    /**
     * @brief Update configuration
     * @param new_config New configuration options
     */
    void update_config(const Config& new_config);
    
    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }
    
    /**
     * @brief Enable or disable memory awareness
     * @param enabled Whether to enable memory-aware uploads
     */
    void set_memory_awareness_enabled(bool enabled);
    
    /**
     * @brief Check if memory awareness is enabled
     */
    bool is_memory_awareness_enabled() const { return memory_awareness_enabled_.load(); }
    
    /**
     * @brief Get current memory pressure level
     */
    MemoryPressure get_current_memory_pressure() const { return current_memory_pressure_.load(); }

private:
    // Core memory-aware functionality
    bool check_memory_for_upload(const MemoryAwareUploadJob& job) const;
    bool should_compress_upload(const MemoryAwareUploadJob& job) const;
    bool should_delay_upload(const MemoryAwareUploadJob& job) const;
    bool should_cancel_upload(const MemoryAwareUploadJob& job) const;
    
    // Memory optimization
    bool ensure_memory_available_for_upload(size_t required_bytes);
    size_t calculate_upload_memory_requirement(const MemoryAwareUploadJob& job) const;
    bool trigger_memory_cleanup(size_t target_free_bytes);
    
    // Compression handling
    std::unique_ptr<uint8_t[]> compress_upload_data(const uint8_t* data, size_t data_size, 
                                                    size_t& compressed_size, float& compression_ratio);
    bool is_compression_beneficial(size_t original_size, size_t compressed_size) const;
    
    // Delay handling
    std::future<bool> delay_upload_until_memory_available(const MemoryAwareUploadJob& job);
    bool wait_for_memory_availability(size_t required_bytes, uint32_t max_wait_ms);
    
    // Statistics and monitoring
    void update_memory_statistics(const MemoryAwareUploadJob& job, bool success, 
                                 bool was_compressed, bool was_delayed);
    void track_memory_usage_during_upload(size_t upload_size);
    
    // Event handling
    void handle_memory_pressure_change(MemoryPressure old_level, MemoryPressure new_level);
    void notify_upload_memory_event(const MemoryAwareUploadJob& job, const std::string& event);
    
    // Configuration and dependencies
    Config config_;
    StreamingTextureUploader* base_uploader_;
    GPUMemoryManager* memory_manager_;
    
    // State management
    std::atomic<bool> memory_awareness_enabled_{true};
    std::atomic<MemoryPressure> current_memory_pressure_{MemoryPressure::LOW};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    MemoryAwareUploadStats memory_aware_stats_;
    
    // Memory tracking
    mutable std::mutex memory_tracking_mutex_;
    std::vector<size_t> recent_memory_usage_;
    std::chrono::steady_clock::time_point last_memory_check_;
    
    // Delayed uploads tracking
    mutable std::mutex delayed_uploads_mutex_;
    std::vector<std::future<bool>> delayed_upload_futures_;
};

} // namespace ve::gfx
