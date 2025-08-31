// Week 8: GPU Memory Management System
// Intelligent GPU memory allocation and optimization

#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>

namespace ve::gfx {

// Forward declarations
using TextureHandle = uint32_t;
using BufferHandle = uint32_t;

/**
 * @brief GPU memory allocation information
 */
struct GPUAllocation {
    enum class Type {
        TEXTURE_2D,
        TEXTURE_3D,
        VERTEX_BUFFER,
        INDEX_BUFFER,
        CONSTANT_BUFFER,
        STAGING_BUFFER
    };
    
    uint32_t handle;                               // Resource handle
    Type type;                                     // Allocation type
    size_t size_bytes;                            // Memory usage
    std::chrono::steady_clock::time_point created_time;
    std::chrono::steady_clock::time_point last_used_time;
    uint32_t access_count;                        // Number of times accessed
    bool is_persistent;                           // Should not be evicted
    
    // Texture-specific information
    struct TextureInfo {
        uint32_t width, height, depth;
        uint32_t mip_levels;
        uint32_t format;                          // DXGI_FORMAT
        bool is_render_target;
        bool is_shader_resource;
    } texture_info;
    
    // Buffer-specific information
    struct BufferInfo {
        uint32_t element_count;
        uint32_t element_size;
        uint32_t usage_flags;
    } buffer_info;
};

/**
 * @brief GPU memory usage statistics
 */
struct GPUMemoryStats {
    // Hardware limits
    size_t total_gpu_memory = 0;                  // Total VRAM
    size_t available_gpu_memory = 0;              // Available VRAM
    
    // Current usage
    size_t used_gpu_memory = 0;                   // Currently allocated
    size_t cached_gpu_memory = 0;                 // Cached but reclaimable
    size_t fragmentation_bytes = 0;               // Memory fragmentation
    
    // Allocation breakdown
    size_t texture_memory = 0;
    size_t buffer_memory = 0;
    size_t staging_memory = 0;
    size_t render_target_memory = 0;
    
    // Performance metrics
    size_t allocation_count = 0;
    size_t deallocation_count = 0;
    size_t eviction_count = 0;
    size_t cache_hit_count = 0;
    size_t cache_miss_count = 0;
    
    // Utilization percentages
    float memory_utilization_percent() const {
        return total_gpu_memory > 0 ? (float(used_gpu_memory) / total_gpu_memory) * 100.0f : 0.0f;
    }
    
    float cache_hit_rate() const {
        size_t total = cache_hit_count + cache_miss_count;
        return total > 0 ? (float(cache_hit_count) / total) * 100.0f : 0.0f;
    }
    
    float fragmentation_percent() const {
        return available_gpu_memory > 0 ? (float(fragmentation_bytes) / available_gpu_memory) * 100.0f : 0.0f;
    }
};

/**
 * @brief Memory pressure levels for automatic management
 */
enum class MemoryPressure {
    LOW,        // <60% memory usage
    MEDIUM,     // 60-80% memory usage  
    HIGH,       // 80-90% memory usage
    CRITICAL    // >90% memory usage
};

/**
 * @brief Callback for memory pressure events
 */
using MemoryPressureCallback = std::function<void(MemoryPressure level, const GPUMemoryStats& stats)>;

/**
 * @brief LRU eviction policy for GPU resources
 */
class LRUEvictionPolicy {
public:
    /**
     * @brief Update access time for resource
     */
    void record_access(uint32_t handle);
    
    /**
     * @brief Get least recently used resources for eviction
     * @param target_bytes Amount of memory to free
     * @return List of handles to evict
     */
    std::vector<uint32_t> get_eviction_candidates(size_t target_bytes, 
                                                  const std::unordered_map<uint32_t, GPUAllocation>& allocations);
    
    /**
     * @brief Remove handle from tracking
     */
    void remove_handle(uint32_t handle);
    
    /**
     * @brief Clear all tracked handles
     */
    void clear();

private:
    struct AccessInfo {
        std::chrono::steady_clock::time_point last_access;
        uint32_t access_count;
    };
    
    std::unordered_map<uint32_t, AccessInfo> access_times_;
    mutable std::mutex access_mutex_;
};

/**
 * @brief Intelligent GPU memory management system
 * 
 * Provides automatic memory monitoring, eviction policies, and optimization
 * to handle large video files efficiently within GPU memory constraints.
 */
class GPUMemoryManager {
public:
    /**
     * @brief Configuration for memory management behavior
     */
    struct Config {
        float high_watermark = 0.8f;              // Trigger cleanup at 80% usage
        float low_watermark = 0.6f;               // Stop cleanup at 60% usage
        size_t min_eviction_size = 16 * 1024 * 1024;  // Minimum 16MB to evict
        size_t max_eviction_size = 256 * 1024 * 1024; // Maximum 256MB per eviction
        bool enable_automatic_eviction = true;     // Automatic memory management
        bool enable_defragmentation = true;        // Periodically defragment memory
        uint32_t defrag_interval_seconds = 300;    // Defrag every 5 minutes
        bool enable_preemptive_cleanup = true;     // Clean before hitting limits
    };
    
    /**
     * @brief Create GPU memory manager
     * @param config Configuration options
     */
    explicit GPUMemoryManager(const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~GPUMemoryManager();
    
    // Non-copyable, non-movable
    GPUMemoryManager(const GPUMemoryManager&) = delete;
    GPUMemoryManager& operator=(const GPUMemoryManager&) = delete;
    
    /**
     * @brief Initialize with GPU device information
     * @param total_memory Total GPU memory in bytes
     * @param available_memory Initially available memory
     */
    void initialize(size_t total_memory, size_t available_memory);
    
    /**
     * @brief Register memory allocation
     * @param allocation Information about the allocated resource
     */
    void register_allocation(const GPUAllocation& allocation);
    
    /**
     * @brief Unregister memory allocation
     * @param handle Resource handle to unregister
     */
    void unregister_allocation(uint32_t handle);
    
    /**
     * @brief Record resource access for LRU tracking
     * @param handle Resource handle that was accessed
     */
    void record_access(uint32_t handle);
    
    /**
     * @brief Check if allocation would exceed memory limits
     * @param requested_bytes Size of proposed allocation
     * @return True if allocation is safe
     */
    bool can_allocate(size_t requested_bytes);
    
    /**
     * @brief Attempt to free enough memory for allocation
     * @param required_bytes Amount of memory needed
     * @return True if enough memory was freed
     */
    bool ensure_available_memory(size_t required_bytes);
    
    /**
     * @brief Force eviction of least recently used resources
     * @param target_bytes Amount of memory to free
     * @return Actual bytes freed
     */
    size_t evict_least_recently_used(size_t target_bytes);
    
    /**
     * @brief Get current memory statistics
     */
    GPUMemoryStats get_stats() const;
    
    /**
     * @brief Get current memory pressure level
     */
    MemoryPressure get_memory_pressure() const;
    
    /**
     * @brief Register callback for memory pressure events
     * @param callback Function to call when memory pressure changes
     */
    void set_pressure_callback(MemoryPressureCallback callback);
    
    /**
     * @brief Update available memory (call after GPU operations)
     * @param new_available Amount of memory currently available
     */
    void update_available_memory(size_t new_available);
    
    /**
     * @brief Mark resource as persistent (won't be evicted)
     * @param handle Resource handle
     * @param persistent Whether resource should be persistent
     */
    void set_persistent(uint32_t handle, bool persistent);
    
    /**
     * @brief Perform memory defragmentation if beneficial
     * @return True if defragmentation was performed
     */
    bool defragment_memory();
    
    /**
     * @brief Get allocation information for a resource
     * @param handle Resource handle
     * @return Pointer to allocation info, or nullptr if not found
     */
    const GPUAllocation* get_allocation_info(uint32_t handle) const;
    
    /**
     * @brief Get list of all allocations (for debugging)
     */
    std::vector<GPUAllocation> get_all_allocations() const;
    
    /**
     * @brief Reset all statistics
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
     * @brief Enable or disable automatic memory management
     * @param enabled Whether to enable automatic management
     */
    void set_automatic_management(bool enabled);

private:
    // Core functionality
    void check_memory_pressure();
    void trigger_pressure_callback(MemoryPressure level);
    bool perform_automatic_cleanup();
    void update_memory_stats();
    size_t calculate_fragmentation() const;
    
    // Eviction logic
    std::vector<uint32_t> select_eviction_candidates(size_t target_bytes);
    bool should_evict_allocation(const GPUAllocation& allocation) const;
    
    // Background management
    void background_management_thread();
    
    // Configuration and state
    Config config_;
    std::atomic<bool> shutdown_requested_{false};
    std::thread background_thread_;
    
    // Memory tracking
    mutable std::mutex allocations_mutex_;
    std::unordered_map<uint32_t, GPUAllocation> allocations_;
    LRUEvictionPolicy lru_policy_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    GPUMemoryStats stats_;
    MemoryPressure current_pressure_ = MemoryPressure::LOW;
    MemoryPressureCallback pressure_callback_;
    
    // Timing
    std::chrono::steady_clock::time_point last_defrag_time_;
    std::chrono::steady_clock::time_point last_pressure_check_;
};

} // namespace ve::gfx
