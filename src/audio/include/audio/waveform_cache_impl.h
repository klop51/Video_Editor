/**
 * @file waveform_cache_impl.h
 * @brief High-Performance Waveform Cache Implementation
 */

#pragma once

#include "audio/waveform_cache.h"
#include <thread>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <zlib.h>

namespace ve::audio {

/**
 * @brief Background task for cache operations
 */
struct CacheTask {
    enum Type {
        COMPRESS_AND_STORE,
        LOAD_AND_DECOMPRESS,
        PREFETCH,
        CLEANUP,
        OPTIMIZE
    };
    
    Type type;
    WaveformCacheKey key;
    std::shared_ptr<WaveformData> data;
    std::promise<std::shared_ptr<WaveformData>> promise;
    bool is_persistent = false;
    
    CacheTask(Type task_type, const WaveformCacheKey& cache_key)
        : type(task_type), key(cache_key) {}
};

/**
 * @brief LRU (Least Recently Used) eviction policy implementation
 */
class LRUEvictionPolicy {
public:
    void access(const WaveformCacheKey& key);
    void add(const WaveformCacheKey& key);
    void remove(const WaveformCacheKey& key);
    std::vector<WaveformCacheKey> get_candidates_for_eviction(size_t count) const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::list<WaveformCacheKey> access_order_;
    std::unordered_map<WaveformCacheKey, std::list<WaveformCacheKey>::iterator> key_positions_;
};

/**
 * @brief High-performance waveform cache implementation
 */
class WaveformCacheImpl : public WaveformCache {
public:
    explicit WaveformCacheImpl(const WaveformCacheConfig& config);
    ~WaveformCacheImpl() override;
    
    // WaveformCache interface implementation
    bool store(
        const WaveformCacheKey& key,
        std::shared_ptr<WaveformData> data,
        bool is_persistent = false
    ) override;
    
    std::shared_ptr<WaveformData> retrieve(const WaveformCacheKey& key) override;
    bool contains(const WaveformCacheKey& key) const override;
    bool remove(const WaveformCacheKey& key) override;
    
    size_t prefetch(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const std::vector<ZoomLevel>& zoom_levels
    ) override;
    
    size_t cleanup(bool aggressive_cleanup = false) override;
    void clear(bool include_persistent = false) override;
    void optimize() override;
    
    WaveformCacheStats get_statistics() const override { return stats_; }
    const WaveformCacheConfig& get_config() const override { return config_; }
    void set_config(const WaveformCacheConfig& config) override;
    
    void set_event_callback(CacheEventCallback callback) override;
    void set_stats_callback(CacheStatsCallback callback) override;
    
    bool export_index(const std::filesystem::path& index_file) override;
    bool import_index(const std::filesystem::path& index_file) override;

private:
    /**
     * @brief Worker thread for background operations
     */
    void worker_thread();
    
    /**
     * @brief Statistics monitoring thread
     */
    void stats_thread();
    
    /**
     * @brief Process background tasks
     */
    void process_task(const CacheTask& task);
    
    /**
     * @brief Compress and store waveform data to disk
     */
    bool compress_and_store_sync(
        const WaveformCacheKey& key,
        std::shared_ptr<WaveformData> data,
        bool is_persistent
    );
    
    /**
     * @brief Load and decompress waveform data from disk
     */
    std::shared_ptr<WaveformData> load_and_decompress_sync(const WaveformCacheKey& key);
    
    /**
     * @brief Get file path for cache entry
     */
    std::filesystem::path get_cache_file_path(const WaveformCacheKey& key) const;
    
    /**
     * @brief Ensure cache directory exists
     */
    bool ensure_cache_directory() const;
    
    /**
     * @brief Check if cleanup is needed based on memory/disk pressure
     */
    bool needs_cleanup() const;
    
    /**
     * @brief Perform memory pressure cleanup
     */
    size_t cleanup_memory_pressure();
    
    /**
     * @brief Perform disk pressure cleanup
     */
    size_t cleanup_disk_pressure();
    
    /**
     * @brief Update cache statistics
     */
    void update_stats_timing(
        std::atomic<std::chrono::microseconds>& avg_time,
        std::chrono::microseconds new_time
    );
    
    /**
     * @brief Send cache event notification
     */
    void notify_event(const WaveformCacheKey& key, const std::string& event);
    
    /**
     * @brief Load cache index from disk
     */
    bool load_cache_index();
    
    /**
     * @brief Save cache index to disk
     */
    bool save_cache_index();
    
    // Configuration and state
    WaveformCacheConfig config_;
    mutable WaveformCacheStats stats_;
    
    // Cache storage
    std::unordered_map<WaveformCacheKey, std::unique_ptr<WaveformCacheEntry>> cache_entries_;
    std::unordered_map<WaveformCacheKey, std::shared_ptr<WaveformData>> memory_cache_;
    mutable std::shared_mutex cache_mutex_;
    
    // Eviction policy
    std::unique_ptr<LRUEvictionPolicy> lru_policy_;
    
    // Background processing
    std::vector<std::thread> worker_threads_;
    std::thread stats_thread_;
    std::queue<std::shared_ptr<CacheTask>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::atomic<bool> shutdown_{false};
    
    // Event callbacks
    CacheEventCallback event_callback_;
    CacheStatsCallback stats_callback_;
    std::mutex callback_mutex_;
    
    // Performance tracking
    std::chrono::system_clock::time_point last_stats_report_;
    std::atomic<size_t> memory_usage_{0};
    std::atomic<size_t> disk_usage_{0};
};

} // namespace ve::audio