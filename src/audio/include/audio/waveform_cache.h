/**
 * @file waveform_cache.h
 * @brief Intelligent Disk-Based Caching System for Waveform Data
 * 
 * Provides high-performance caching of waveform data with automatic eviction policies,
 * compression, and optimized storage for professional video editing workflows.
 * Supports fast timeline scrubbing and multi-resolution waveform management.
 */

#pragma once

#include "audio/waveform_generator.h"
#include "core/time.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <functional>
#include <filesystem>
#include <map>
#include <atomic>

namespace ve::audio {

/**
 * @brief Cache key for uniquely identifying waveform data
 */
struct WaveformCacheKey {
    std::string audio_source;              // Path to audio file or identifier
    ve::TimePoint start_time;              // Start time of cached segment
    ve::TimePoint duration;                // Duration of cached segment
    size_t samples_per_point;              // Zoom level (samples per waveform point)
    uint32_t channel_mask;                 // Channel selection mask
    
    // Generate unique hash for cache lookup
    size_t hash() const;
    
    // Equality comparison
    bool operator==(const WaveformCacheKey& other) const;
    
    // String representation for debugging
    std::string to_string() const;
};

/**
 * @brief Cache entry metadata
 */
struct WaveformCacheEntry {
    WaveformCacheKey key;                          // Cache key
    std::shared_ptr<WaveformData> data;            // Cached waveform data
    std::filesystem::path file_path;               // Path to cached file on disk
    size_t compressed_size;                        // Size of compressed data on disk
    size_t uncompressed_size;                      // Size of uncompressed data in memory
    std::chrono::system_clock::time_point created_time;    // When entry was created
    std::chrono::system_clock::time_point last_accessed;   // Last access time
    std::atomic<size_t> access_count{0};           // Number of times accessed
    bool is_persistent;                            // Should survive cache cleanup
    
    WaveformCacheEntry(const WaveformCacheKey& cache_key)
        : key(cache_key)
        , created_time(std::chrono::system_clock::now())
        , last_accessed(std::chrono::system_clock::now())
        , is_persistent(false) {}
};

/**
 * @brief Cache eviction policy
 */
enum class EvictionPolicy {
    LRU,           // Least Recently Used
    LFU,           // Least Frequently Used  
    SIZE_BASED,    // Largest entries first
    AGE_BASED,     // Oldest entries first
    HYBRID         // Combination of access patterns and size
};

/**
 * @brief Cache compression settings
 */
struct CompressionConfig {
    bool enable_compression = true;        // Enable data compression
    int compression_level = 6;             // Compression level (1-9, higher = better compression)
    size_t min_size_for_compression = 4096; // Minimum size to enable compression
    bool async_compression = true;         // Compress in background thread
};

/**
 * @brief Cache configuration
 */
struct WaveformCacheConfig {
    // Storage settings
    std::filesystem::path cache_directory = "waveform_cache";
    size_t max_disk_usage_mb = 2048;       // Maximum disk space for cache (2GB)
    size_t max_memory_usage_mb = 512;      // Maximum memory for loaded waveforms (512MB)
    size_t max_entries = 10000;            // Maximum number of cache entries
    
    // Eviction policy
    EvictionPolicy eviction_policy = EvictionPolicy::HYBRID;
    float memory_pressure_threshold = 0.8f;   // Trigger cleanup at 80% memory usage
    float disk_pressure_threshold = 0.9f;     // Trigger disk cleanup at 90% usage
    
    // Compression
    CompressionConfig compression;
    
    // Performance settings  
    size_t io_thread_count = 2;            // Number of background I/O threads
    bool enable_prefetching = true;        // Enable predictive cache loading
    size_t prefetch_window_seconds = 30;   // Prefetch waveforms within 30 seconds
    
    // Persistence
    bool enable_persistent_cache = true;   // Keep cache between application runs
    std::chrono::hours max_cache_age{168}; // Auto-cleanup after 7 days
    
    // Statistics and monitoring
    bool enable_statistics = true;         // Collect cache performance statistics
    std::chrono::minutes stats_report_interval{60}; // Report stats every hour
};

/**
 * @brief Cache performance statistics
 */
struct WaveformCacheStats {
    std::atomic<size_t> cache_hits{0};
    std::atomic<size_t> cache_misses{0};
    std::atomic<size_t> evictions{0};
    std::atomic<size_t> compressions{0};
    std::atomic<size_t> decompressions{0};
    std::atomic<size_t> disk_reads{0};
    std::atomic<size_t> disk_writes{0};
    std::atomic<size_t> total_bytes_cached{0};
    std::atomic<size_t> total_bytes_compressed{0};
    
    // Performance metrics
    std::atomic<std::chrono::microseconds> avg_read_time{std::chrono::microseconds(0)};
    std::atomic<std::chrono::microseconds> avg_write_time{std::chrono::microseconds(0)};
    std::atomic<std::chrono::microseconds> avg_compression_time{std::chrono::microseconds(0)};
    
    // Current state
    std::atomic<size_t> current_memory_usage{0};
    std::atomic<size_t> current_disk_usage{0};
    std::atomic<size_t> current_entry_count{0};
    
    // Calculate hit ratio
    double hit_ratio() const {
        const size_t total = cache_hits.load() + cache_misses.load();
        return total > 0 ? static_cast<double>(cache_hits.load()) / total : 0.0;
    }
    
    // Calculate compression ratio
    double compression_ratio() const {
        const size_t compressed = total_bytes_compressed.load();
        const size_t original = total_bytes_cached.load();
        return original > 0 ? static_cast<double>(compressed) / original : 1.0;
    }
};

/**
 * @brief Cache event callback types
 */
using CacheEventCallback = std::function<void(const WaveformCacheKey& key, const std::string& event)>;
using CacheStatsCallback = std::function<void(const WaveformCacheStats& stats)>;

/**
 * @brief Intelligent waveform cache interface
 */
class WaveformCache {
public:
    virtual ~WaveformCache() = default;
    
    /**
     * @brief Store waveform data in cache
     * @param key Unique cache key
     * @param data Waveform data to cache
     * @param is_persistent Should entry survive cleanup
     * @return True if successfully cached
     */
    virtual bool store(
        const WaveformCacheKey& key,
        std::shared_ptr<WaveformData> data,
        bool is_persistent = false
    ) = 0;
    
    /**
     * @brief Retrieve waveform data from cache
     * @param key Cache key to lookup
     * @return Cached waveform data or nullptr if not found
     */
    virtual std::shared_ptr<WaveformData> retrieve(const WaveformCacheKey& key) = 0;
    
    /**
     * @brief Check if waveform data exists in cache
     * @param key Cache key to check
     * @return True if data is cached
     */
    virtual bool contains(const WaveformCacheKey& key) const = 0;
    
    /**
     * @brief Remove specific entry from cache
     * @param key Cache key to remove
     * @return True if entry was removed
     */
    virtual bool remove(const WaveformCacheKey& key) = 0;
    
    /**
     * @brief Prefetch waveform data for upcoming timeline access
     * @param audio_source Audio source identifier
     * @param time_range Time range to prefetch
     * @param zoom_levels Zoom levels to prefetch
     * @return Number of items scheduled for prefetching
     */
    virtual size_t prefetch(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const std::vector<ZoomLevel>& zoom_levels
    ) = 0;
    
    /**
     * @brief Force cleanup of cache based on configured policies
     * @param aggressive_cleanup Use more aggressive cleanup thresholds
     * @return Number of entries removed
     */
    virtual size_t cleanup(bool aggressive_cleanup = false) = 0;
    
    /**
     * @brief Clear all cache entries
     * @param include_persistent Also remove persistent entries
     */
    virtual void clear(bool include_persistent = false) = 0;
    
    /**
     * @brief Optimize cache for better performance
     * (e.g., defragment, recompress, reorganize)
     */
    virtual void optimize() = 0;
    
    /**
     * @brief Get current cache statistics
     */
    virtual WaveformCacheStats get_statistics() const = 0;
    
    /**
     * @brief Get current configuration
     */
    virtual const WaveformCacheConfig& get_config() const = 0;
    
    /**
     * @brief Update cache configuration
     * @param config New configuration
     */
    virtual void set_config(const WaveformCacheConfig& config) = 0;
    
    /**
     * @brief Set event callback for cache operations
     */
    virtual void set_event_callback(CacheEventCallback callback) = 0;
    
    /**
     * @brief Set statistics callback for periodic reporting
     */
    virtual void set_stats_callback(CacheStatsCallback callback) = 0;
    
    /**
     * @brief Export cache index for backup/restore
     * @param index_file Path to export index file
     * @return True if export successful
     */
    virtual bool export_index(const std::filesystem::path& index_file) = 0;
    
    /**
     * @brief Import cache index from backup
     * @param index_file Path to index file
     * @return True if import successful
     */
    virtual bool import_index(const std::filesystem::path& index_file) = 0;
    
    /**
     * @brief Create cache instance
     * @param config Cache configuration
     * @return Unique pointer to cache instance
     */
    static std::unique_ptr<WaveformCache> create(const WaveformCacheConfig& config = {});
};

/**
 * @brief Advanced cache query interface for complex lookups
 */
class WaveformCacheQuery {
public:
    explicit WaveformCacheQuery(WaveformCache& cache) : cache_(cache) {}
    
    /**
     * @brief Find all cached entries for an audio source
     */
    std::vector<WaveformCacheKey> find_by_source(const std::string& audio_source) const;
    
    /**
     * @brief Find cached entries within a time range
     */
    std::vector<WaveformCacheKey> find_by_time_range(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range
    ) const;
    
    /**
     * @brief Find cached entries for specific zoom levels
     */
    std::vector<WaveformCacheKey> find_by_zoom_level(
        const std::string& audio_source,
        const std::vector<size_t>& samples_per_point
    ) const;
    
    /**
     * @brief Find the best available cached entry for a request
     * (may return higher resolution that can be downsampled)
     */
    WaveformCacheKey find_best_match(
        const std::string& audio_source,
        const ve::TimePoint& start_time,
        const ve::TimePoint& duration,
        size_t preferred_samples_per_point
    ) const;
    
    /**
     * @brief Get cache coverage for an audio source
     * (which time ranges and zoom levels are cached)
     */
    struct CoverageMaps {
        std::map<size_t, std::vector<std::pair<ve::TimePoint, ve::TimePoint>>> zoom_coverage;
        double total_coverage_percentage;
    };
    
    CoverageMaps get_coverage(const std::string& audio_source) const;

private:
    WaveformCache& cache_;
};

/**
 * @brief Utility functions for cache management
 */
namespace cache_utils {
    
    /**
     * @brief Generate optimal cache key for a waveform request
     */
    WaveformCacheKey generate_cache_key(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const ZoomLevel& zoom_level,
        uint32_t channel_mask = 0
    );
    
    /**
     * @brief Calculate cache storage requirements for waveform data
     */
    size_t calculate_storage_size(
        const WaveformData& data,
        bool with_compression = true,
        int compression_level = 6
    );
    
    /**
     * @brief Validate cache directory structure and permissions
     */
    bool validate_cache_directory(const std::filesystem::path& cache_dir);
    
    /**
     * @brief Clean up orphaned cache files
     */
    size_t cleanup_orphaned_files(const std::filesystem::path& cache_dir);
    
    /**
     * @brief Calculate optimal prefetch strategy for timeline position
     */
    std::vector<WaveformCacheKey> calculate_prefetch_keys(
        const std::string& audio_source,
        const ve::TimePoint& current_position,
        const ve::TimePoint& playback_speed,
        const std::vector<ZoomLevel>& active_zoom_levels,
        size_t prefetch_window_seconds
    );
    
    /**
     * @brief Compress waveform data for storage
     */
    std::vector<uint8_t> compress_waveform_data(
        const WaveformData& data,
        int compression_level = 6
    );
    
    /**
     * @brief Decompress waveform data from storage
     */
    std::shared_ptr<WaveformData> decompress_waveform_data(
        const std::vector<uint8_t>& compressed_data
    );
    
    /**
     * @brief Merge cache statistics from multiple sources
     */
    WaveformCacheStats merge_statistics(
        const std::vector<WaveformCacheStats>& stats_list
    );
}

} // namespace ve::audio

// Hash function for WaveformCacheKey to use in unordered containers
template<>
struct std::hash<ve::audio::WaveformCacheKey> {
    size_t operator()(const ve::audio::WaveformCacheKey& key) const noexcept {
        return key.hash();
    }
};