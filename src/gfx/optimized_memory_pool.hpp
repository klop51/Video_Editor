#pragma once

#include "../core/types.hpp"
#include "../core/error_handling.hpp"
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>
#include <chrono>
#include <functional>

namespace gfx {

// Forward declarations
class GraphicsDevice;
class TextureHandle;
class BufferHandle;

// Memory pool statistics for monitoring and optimization
struct MemoryPoolStats {
    size_t total_allocated_bytes = 0;
    size_t currently_used_bytes = 0;
    size_t peak_usage_bytes = 0;
    size_t total_allocations = 0;
    size_t total_deallocations = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    float cache_hit_ratio = 0.0f;
    size_t fragmentation_bytes = 0;
    float memory_utilization = 0.0f;
    
    // Performance metrics
    std::chrono::nanoseconds avg_allocation_time{0};
    std::chrono::nanoseconds avg_deallocation_time{0};
    size_t forced_garbage_collections = 0;
    
    void reset() { *this = MemoryPoolStats{}; }
    void update_derived_stats();
};

// Texture pool resource with metadata
struct PooledTexture {
    TextureHandle handle;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    TextureFormat format = TextureFormat::RGBA8_UNORM;
    uint32_t mip_levels = 1;
    uint32_t array_size = 1;
    TextureUsage usage = TextureUsage::RENDER_TARGET;
    size_t size_bytes = 0;
    
    // Pool management
    std::chrono::high_resolution_clock::time_point last_used;
    std::chrono::high_resolution_clock::time_point created;
    uint32_t use_count = 0;
    bool in_use = false;
    
    // Hash and comparison for efficient lookup
    size_t get_hash() const;
    bool matches(uint32_t w, uint32_t h, uint32_t d, TextureFormat fmt, 
                uint32_t mips, uint32_t array, TextureUsage usg) const;
    bool is_compatible_for_reuse(uint32_t w, uint32_t h, TextureFormat fmt) const;
};

// Buffer pool resource with metadata
struct PooledBuffer {
    BufferHandle handle;
    size_t size_bytes = 0;
    BufferUsage usage = BufferUsage::VERTEX;
    BufferAccessPattern access = BufferAccessPattern::STATIC;
    
    // Pool management
    std::chrono::high_resolution_clock::time_point last_used;
    std::chrono::high_resolution_clock::time_point created;
    uint32_t use_count = 0;
    bool in_use = false;
    
    // Hash and comparison
    size_t get_hash() const;
    bool matches(size_t size, BufferUsage usg, BufferAccessPattern acc) const;
    bool is_compatible_for_reuse(size_t size, BufferUsage usg) const;
};

// Memory allocation strategy for different use cases
enum class AllocationStrategy {
    EXACT_MATCH,        // Find exact size match, create if needed
    BEST_FIT,           // Find smallest resource that fits
    FIRST_FIT,          // Use first resource that fits
    NEXT_FIT,           // Continue from last allocation point
    BUDDY_SYSTEM,       // Power-of-2 buddy allocation
    SEGREGATED_FIT,     // Separate pools for different size classes
    ADAPTIVE            // Dynamically choose best strategy
};

// Memory pool configuration
struct MemoryPoolConfig {
    // Pool limits
    size_t max_pool_size_bytes = 1024 * 1024 * 1024;  // 1GB default
    size_t max_texture_count = 1000;
    size_t max_buffer_count = 2000;
    
    // Allocation strategy
    AllocationStrategy allocation_strategy = AllocationStrategy::ADAPTIVE;
    float over_allocation_factor = 1.25f;  // Allocate 25% larger for reuse
    
    // Garbage collection
    std::chrono::seconds resource_timeout{30};  // Free unused resources after 30s
    std::chrono::seconds gc_interval{5};        // Run GC every 5 seconds
    float gc_pressure_threshold = 0.8f;         // Force GC at 80% memory usage
    
    // Performance tuning
    bool enable_resource_pooling = true;
    bool enable_automatic_gc = true;
    bool enable_memory_defragmentation = false;
    bool enable_detailed_tracking = false;
    size_t initial_pool_reserve = 100;  // Pre-allocate this many resources
    
    // Validation
    bool is_valid() const;
};

// Advanced memory pool with sophisticated allocation strategies
class OptimizedMemoryPool {
public:
    OptimizedMemoryPool(GraphicsDevice& device, const MemoryPoolConfig& config = {});
    ~OptimizedMemoryPool();

    // Texture management
    TextureHandle get_temporary_texture(uint32_t width, uint32_t height, 
                                      TextureFormat format = TextureFormat::RGBA8_UNORM,
                                      TextureUsage usage = TextureUsage::RENDER_TARGET);
    
    TextureHandle get_temporary_texture_3d(uint32_t width, uint32_t height, uint32_t depth,
                                         TextureFormat format = TextureFormat::RGBA8_UNORM,
                                         TextureUsage usage = TextureUsage::RENDER_TARGET);
    
    TextureHandle get_temporary_texture_array(uint32_t width, uint32_t height, uint32_t array_size,
                                            TextureFormat format = TextureFormat::RGBA8_UNORM,
                                            TextureUsage usage = TextureUsage::RENDER_TARGET);
    
    void return_texture(TextureHandle texture);
    
    // Buffer management
    BufferHandle get_temporary_buffer(size_t size_bytes, 
                                    BufferUsage usage = BufferUsage::VERTEX,
                                    BufferAccessPattern access = BufferAccessPattern::DYNAMIC);
    
    void return_buffer(BufferHandle buffer);
    
    // Advanced allocation features
    TextureHandle get_pooled_texture_with_fallback(uint32_t width, uint32_t height, 
                                                  TextureFormat preferred_format,
                                                  const std::vector<TextureFormat>& fallback_formats);
    
    BufferHandle get_aligned_buffer(size_t size_bytes, size_t alignment,
                                  BufferUsage usage = BufferUsage::CONSTANT);
    
    // Batch operations for performance
    std::vector<TextureHandle> get_texture_batch(const std::vector<std::tuple<uint32_t, uint32_t, TextureFormat>>& specs);
    std::vector<BufferHandle> get_buffer_batch(const std::vector<std::pair<size_t, BufferUsage>>& specs);
    
    void return_texture_batch(const std::vector<TextureHandle>& textures);
    void return_buffer_batch(const std::vector<BufferHandle>& buffers);
    
    // Memory management
    void force_garbage_collection();
    void defragment_memory();
    void trim_unused_resources();
    void clear_all_pools();
    
    // Configuration and tuning
    void set_allocation_strategy(AllocationStrategy strategy) { config_.allocation_strategy = strategy; }
    void set_gc_pressure_threshold(float threshold) { config_.gc_pressure_threshold = threshold; }
    void enable_detailed_tracking(bool enabled) { config_.enable_detailed_tracking = enabled; }
    
    // Statistics and monitoring
    const MemoryPoolStats& get_stats() const { return stats_; }
    void reset_stats() { stats_.reset(); }
    
    size_t get_total_memory_usage() const;
    size_t get_available_memory() const;
    float get_memory_utilization() const;
    float get_fragmentation_ratio() const;
    
    // Debug and diagnostics
    void dump_pool_state(std::ostream& out) const;
    void export_allocation_report(const std::string& filename) const;
    bool validate_pool_integrity() const;
    
    // Performance optimization
    void optimize_for_current_usage_pattern();
    void pre_allocate_common_resources();
    void enable_background_gc(bool enabled);

private:
    GraphicsDevice& device_;
    MemoryPoolConfig config_;
    mutable std::mutex pool_mutex_;
    
    // Resource pools
    std::vector<std::unique_ptr<PooledTexture>> texture_pool_;
    std::vector<std::unique_ptr<PooledBuffer>> buffer_pool_;
    
    // Fast lookup structures
    std::unordered_map<size_t, std::vector<size_t>> texture_hash_map_;
    std::unordered_map<size_t, std::vector<size_t>> buffer_hash_map_;
    
    // Size-based pools for better cache performance
    std::unordered_map<size_t, std::queue<size_t>> texture_size_pools_;
    std::unordered_map<size_t, std::queue<size_t>> buffer_size_pools_;
    
    // Statistics and monitoring
    mutable MemoryPoolStats stats_;
    std::atomic<size_t> next_texture_index_{0};
    std::atomic<size_t> next_buffer_index_{0};
    
    // Garbage collection
    std::chrono::high_resolution_clock::time_point last_gc_time_;
    std::thread gc_thread_;
    std::atomic<bool> gc_thread_running_{false};
    
    // Allocation strategies
    size_t find_texture_exact_match(uint32_t width, uint32_t height, uint32_t depth,
                                   TextureFormat format, uint32_t mips, uint32_t array, TextureUsage usage);
    size_t find_texture_best_fit(uint32_t width, uint32_t height, TextureFormat format);
    size_t find_texture_first_fit(uint32_t width, uint32_t height, TextureFormat format);
    
    size_t find_buffer_exact_match(size_t size, BufferUsage usage, BufferAccessPattern access);
    size_t find_buffer_best_fit(size_t size, BufferUsage usage);
    size_t find_buffer_first_fit(size_t size, BufferUsage usage);
    
    // Resource creation
    TextureHandle create_pooled_texture(uint32_t width, uint32_t height, uint32_t depth,
                                       TextureFormat format, uint32_t mips, uint32_t array, TextureUsage usage);
    BufferHandle create_pooled_buffer(size_t size, BufferUsage usage, BufferAccessPattern access);
    
    // Garbage collection implementation
    void run_garbage_collection();
    void gc_worker_thread();
    bool should_run_gc() const;
    void collect_unused_textures();
    void collect_unused_buffers();
    
    // Memory defragmentation
    void defragment_texture_pool();
    void defragment_buffer_pool();
    void compact_pool_structures();
    
    // Utility methods
    void update_allocation_stats(std::chrono::nanoseconds allocation_time);
    void update_deallocation_stats(std::chrono::nanoseconds deallocation_time);
    void update_cache_stats(bool cache_hit);
    
    size_t calculate_texture_size(uint32_t width, uint32_t height, uint32_t depth,
                                 TextureFormat format, uint32_t mips, uint32_t array) const;
    size_t calculate_texture_hash(uint32_t width, uint32_t height, uint32_t depth,
                                 TextureFormat format, uint32_t mips, uint32_t array, TextureUsage usage) const;
    size_t calculate_buffer_hash(size_t size, BufferUsage usage, BufferAccessPattern access) const;
    
    bool is_texture_expired(const PooledTexture& texture) const;
    bool is_buffer_expired(const PooledBuffer& buffer) const;
};

// RAII helper for automatic resource return
template<typename HandleType>
class PooledResource {
public:
    PooledResource(OptimizedMemoryPool& pool, HandleType handle)
        : pool_(pool), handle_(handle), valid_(true) {}
    
    PooledResource(PooledResource&& other) noexcept
        : pool_(other.pool_), handle_(other.handle_), valid_(other.valid_) {
        other.valid_ = false;
    }
    
    PooledResource& operator=(PooledResource&& other) noexcept {
        if (this != &other) {
            release();
            pool_ = other.pool_;
            handle_ = other.handle_;
            valid_ = other.valid_;
            other.valid_ = false;
        }
        return *this;
    }
    
    ~PooledResource() {
        release();
    }
    
    HandleType get() const { return handle_; }
    HandleType operator*() const { return handle_; }
    HandleType operator->() const { return handle_; }
    
    void release() {
        if (valid_) {
            if constexpr (std::is_same_v<HandleType, TextureHandle>) {
                pool_.return_texture(handle_);
            } else if constexpr (std::is_same_v<HandleType, BufferHandle>) {
                pool_.return_buffer(handle_);
            }
            valid_ = false;
        }
    }

private:
    OptimizedMemoryPool& pool_;
    HandleType handle_;
    bool valid_;
    
    // Prevent copying
    PooledResource(const PooledResource&) = delete;
    PooledResource& operator=(const PooledResource&) = delete;
};

using PooledTextureResource = PooledResource<TextureHandle>;
using PooledBufferResource = PooledResource<BufferHandle>;

// Factory functions for easy RAII resource management
PooledTextureResource make_pooled_texture(OptimizedMemoryPool& pool, 
                                        uint32_t width, uint32_t height, 
                                        TextureFormat format = TextureFormat::RGBA8_UNORM);

PooledBufferResource make_pooled_buffer(OptimizedMemoryPool& pool, 
                                      size_t size, 
                                      BufferUsage usage = BufferUsage::VERTEX);

} // namespace gfx
