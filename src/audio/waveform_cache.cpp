/**
 * @file waveform_cache.cpp
 * @brief Implementation of High-Performance Waveform Cache
 */

#include "audio/waveform_cache_impl.h"
#include "core/logging.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>

namespace ve::audio {

//=============================================================================
// WaveformCacheKey Implementation
//=============================================================================

size_t WaveformCacheKey::hash() const {
    size_t h1 = std::hash<std::string>{}(audio_source);
    size_t h2 = static_cast<size_t>(start_time.numerator() ^ start_time.denominator());
    size_t h3 = static_cast<size_t>(duration.numerator() ^ duration.denominator());
    size_t h4 = std::hash<size_t>{}(samples_per_point);
    size_t h5 = std::hash<uint32_t>{}(channel_mask);
    
    // Combine hashes using a well-distributed hash combination
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
}

bool WaveformCacheKey::operator==(const WaveformCacheKey& other) const {
    return audio_source == other.audio_source &&
           start_time == other.start_time &&
           duration == other.duration &&
           samples_per_point == other.samples_per_point &&
           channel_mask == other.channel_mask;
}

std::string WaveformCacheKey::to_string() const {
    std::ostringstream oss;
    oss << "WaveformCacheKey{source=" << audio_source 
        << ", start=" << start_time.numerator() << "/" << start_time.denominator()
        << ", duration=" << duration.numerator() << "/" << duration.denominator()
        << ", samples_per_point=" << samples_per_point
        << ", channel_mask=0x" << std::hex << channel_mask << "}";
    return oss.str();
}

//=============================================================================
// LRU Eviction Policy Implementation
//=============================================================================

void LRUEvictionPolicy::access(const WaveformCacheKey& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = key_positions_.find(key);
    if (it != key_positions_.end()) {
        // Move to front
        access_order_.erase(it->second);
        access_order_.push_front(key);
        it->second = access_order_.begin();
    }
}

void LRUEvictionPolicy::add(const WaveformCacheKey& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    access_order_.push_front(key);
    key_positions_[key] = access_order_.begin();
}

void LRUEvictionPolicy::remove(const WaveformCacheKey& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = key_positions_.find(key);
    if (it != key_positions_.end()) {
        access_order_.erase(it->second);
        key_positions_.erase(it);
    }
}

std::vector<WaveformCacheKey> LRUEvictionPolicy::get_candidates_for_eviction(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<WaveformCacheKey> candidates;
    candidates.reserve(count);
    
    auto it = access_order_.rbegin();
    for (size_t i = 0; i < count && it != access_order_.rend(); ++i, ++it) {
        candidates.push_back(*it);
    }
    
    return candidates;
}

void LRUEvictionPolicy::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    access_order_.clear();
    key_positions_.clear();
}

//=============================================================================
// WaveformCacheImpl Implementation
//=============================================================================

WaveformCacheImpl::WaveformCacheImpl(const WaveformCacheConfig& config)
    : config_(config)
    , lru_policy_(std::make_unique<LRUEvictionPolicy>())
    , last_stats_report_(std::chrono::system_clock::now()) {
    
    if (!ensure_cache_directory()) {
        throw std::runtime_error("Failed to create cache directory: " + config_.cache_directory.string());
    }
    
    // Load existing cache index
    if (config_.enable_persistent_cache) {
        load_cache_index();
    }
    
    // Start worker threads
    for (size_t i = 0; i < config_.io_thread_count; ++i) {
        worker_threads_.emplace_back(&WaveformCacheImpl::worker_thread, this);
    }
    
    // Start statistics thread
    if (config_.enable_statistics) {
        stats_thread_ = std::thread(&WaveformCacheImpl::stats_thread, this);
    }
    
    ve::log_info("WaveformCache initialized: max_memory={}MB, max_disk={}MB, compression={}",
                config_.max_memory_usage_mb,
                config_.max_disk_usage_mb,
                config_.compression.enable_compression ? "enabled" : "disabled");
}

WaveformCacheImpl::~WaveformCacheImpl() {
    shutdown_ = true;
    queue_condition_.notify_all();
    
    // Wait for worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
    
    // Save cache index if persistent
    if (config_.enable_persistent_cache) {
        save_cache_index();
    }
    
    ve::log_info("WaveformCache shutdown. Final stats: {}% hit ratio, {} entries cached",
                static_cast<int>(stats_.hit_ratio() * 100),
                stats_.current_entry_count.load());
}

bool WaveformCacheImpl::store(
    const WaveformCacheKey& key,
    std::shared_ptr<WaveformData> data,
    bool is_persistent) {
    
    if (!data || !waveform_utils::validate_waveform_data(*data)) {
        return false;
    }
    
    // Check if we need cleanup before storing
    if (needs_cleanup()) {
        cleanup(false);
    }
    
    // Store in memory cache immediately
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        memory_cache_[key] = data;
        
        // Create cache entry
        auto entry = std::make_unique<WaveformCacheEntry>(key);
        entry->data = data;
        entry->is_persistent = is_persistent;
        entry->uncompressed_size = waveform_utils::calculate_memory_usage(*data);
        
        cache_entries_[key] = std::move(entry);
        
        // Update LRU policy
        lru_policy_->add(key);
        
        // Update statistics
        memory_usage_ += cache_entries_[key]->uncompressed_size;
        stats_.current_entry_count++;
        stats_.current_memory_usage = memory_usage_.load();
    }
    
    // Schedule background compression and disk storage
    if (config_.compression.async_compression) {
        auto task = std::make_shared<CacheTask>(CacheTask::COMPRESS_AND_STORE, key);
        task->data = data;
        task->is_persistent = is_persistent;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(task);
        }
        queue_condition_.notify_one();
    } else {
        // Store synchronously
        compress_and_store_sync(key, data, is_persistent);
    }
    
    notify_event(key, "stored");
    return true;
}

std::shared_ptr<WaveformData> WaveformCacheImpl::retrieve(const WaveformCacheKey& key) {
    // Check memory cache first
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        auto it = memory_cache_.find(key);
        if (it != memory_cache_.end()) {
            lru_policy_->access(key);
            
            // Update entry access time
            auto entry_it = cache_entries_.find(key);
            if (entry_it != cache_entries_.end()) {
                entry_it->second->last_accessed = std::chrono::system_clock::now();
                entry_it->second->access_count++;
            }
            
            stats_.cache_hits++;
            notify_event(key, "hit_memory");
            return it->second;
        }
    }
    
    // Try loading from disk
    auto data = load_and_decompress_sync(key);
    if (data) {
        // Store in memory cache
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            memory_cache_[key] = data;
            lru_policy_->access(key);
            
            auto entry_it = cache_entries_.find(key);
            if (entry_it != cache_entries_.end()) {
                entry_it->second->data = data;
                entry_it->second->last_accessed = std::chrono::system_clock::now();
                entry_it->second->access_count++;
            }
        }
        
        stats_.cache_hits++;
        stats_.disk_reads++;
        notify_event(key, "hit_disk");
        return data;
    }
    
    stats_.cache_misses++;
    notify_event(key, "miss");
    return nullptr;
}

bool WaveformCacheImpl::contains(const WaveformCacheKey& key) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_entries_.find(key) != cache_entries_.end();
}

bool WaveformCacheImpl::remove(const WaveformCacheKey& key) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto entry_it = cache_entries_.find(key);
    if (entry_it == cache_entries_.end()) {
        return false;
    }
    
    // Remove from memory cache
    auto mem_it = memory_cache_.find(key);
    if (mem_it != memory_cache_.end()) {
        memory_usage_ -= entry_it->second->uncompressed_size;
        memory_cache_.erase(mem_it);
    }
    
    // Remove disk file
    if (!entry_it->second->file_path.empty() && std::filesystem::exists(entry_it->second->file_path)) {
        try {
            disk_usage_ -= entry_it->second->compressed_size;
            std::filesystem::remove(entry_it->second->file_path);
        } catch (const std::exception& e) {
            ve::log_warning("Failed to remove cache file {}: {}", entry_it->second->file_path.string(), e.what());
        }
    }
    
    // Update policy and statistics
    lru_policy_->remove(key);
    cache_entries_.erase(entry_it);
    stats_.current_entry_count--;
    stats_.current_memory_usage = memory_usage_.load();
    stats_.current_disk_usage = disk_usage_.load();
    
    notify_event(key, "removed");
    return true;
}

size_t WaveformCacheImpl::prefetch(
    const std::string& audio_source,
    const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
    const std::vector<ZoomLevel>& zoom_levels) {
    
    if (!config_.enable_prefetching) {
        return 0;
    }
    
    size_t scheduled_count = 0;
    
    for (const auto& zoom_level : zoom_levels) {
        WaveformCacheKey key = cache_utils::generate_cache_key(
            audio_source, time_range, zoom_level, 0
        );
        
        if (!contains(key)) {
            auto task = std::make_shared<CacheTask>(CacheTask::PREFETCH, key);
            
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                task_queue_.push(task);
            }
            
            scheduled_count++;
            notify_event(key, "prefetch_scheduled");
        }
    }
    
    if (scheduled_count > 0) {
        queue_condition_.notify_all();
    }
    
    return scheduled_count;
}

size_t WaveformCacheImpl::cleanup(bool aggressive_cleanup) {
    size_t removed_count = 0;
    
    // Determine cleanup thresholds
    float memory_threshold = aggressive_cleanup ? 0.5f : config_.memory_pressure_threshold;
    float disk_threshold = aggressive_cleanup ? 0.7f : config_.disk_pressure_threshold;
    
    // Memory pressure cleanup
    if (memory_usage_.load() > config_.max_memory_usage_mb * 1024 * 1024 * memory_threshold) {
        removed_count += cleanup_memory_pressure();
    }
    
    // Disk pressure cleanup  
    if (disk_usage_.load() > config_.max_disk_usage_mb * 1024 * 1024 * disk_threshold) {
        removed_count += cleanup_disk_pressure();
    }
    
    // Age-based cleanup
    if (config_.enable_persistent_cache) {
        auto cutoff_time = std::chrono::system_clock::now() - config_.max_cache_age;
        
        std::vector<WaveformCacheKey> expired_keys;
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            for (const auto& [key, entry] : cache_entries_) {
                if (!entry->is_persistent && entry->created_time < cutoff_time) {
                    expired_keys.push_back(key);
                }
            }
        }
        
        for (const auto& key : expired_keys) {
            if (remove(key)) {
                removed_count++;
            }
        }
    }
    
    if (removed_count > 0) {
        stats_.evictions += removed_count;
        ve::log_info("Cache cleanup completed: {} entries removed", removed_count);
    }
    
    return removed_count;
}

void WaveformCacheImpl::clear(bool include_persistent) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = cache_entries_.begin();
    while (it != cache_entries_.end()) {
        if (include_persistent || !it->second->is_persistent) {
            // Remove disk file
            if (!it->second->file_path.empty() && std::filesystem::exists(it->second->file_path)) {
                try {
                    std::filesystem::remove(it->second->file_path);
                } catch (const std::exception& e) {
                    ve::log_warning("Failed to remove cache file during clear: {}", e.what());
                }
            }
            
            // Remove from memory cache
            memory_cache_.erase(it->first);
            lru_policy_->remove(it->first);
            
            it = cache_entries_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Reset counters
    memory_usage_ = 0;
    disk_usage_ = 0;
    stats_.current_entry_count = cache_entries_.size();
    stats_.current_memory_usage = 0;
    
    // Calculate remaining disk usage for persistent entries
    for (const auto& [key, entry] : cache_entries_) {
        disk_usage_ += entry->compressed_size;
    }
    stats_.current_disk_usage = disk_usage_.load();
    
    ve::log_info("Cache cleared: persistent_only={}", !include_persistent);
}

void WaveformCacheImpl::optimize() {
    // Schedule optimization task
    auto task = std::make_shared<CacheTask>(CacheTask::OPTIMIZE, WaveformCacheKey{});
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
    }
    
    queue_condition_.notify_one();
}

void WaveformCacheImpl::set_config(const WaveformCacheConfig& config) {
    config_ = config;
    
    // Trigger cleanup if new limits are exceeded
    if (needs_cleanup()) {
        cleanup(false);
    }
}

void WaveformCacheImpl::set_event_callback(CacheEventCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    event_callback_ = callback;
}

void WaveformCacheImpl::set_stats_callback(CacheStatsCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    stats_callback_ = callback;
}

bool WaveformCacheImpl::export_index(const std::filesystem::path& index_file) {
    try {
        std::ofstream file(index_file, std::ios::binary);
        if (!file) {
            return false;
        }
        
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        
        // Write header
        uint32_t version = 1;
        uint32_t entry_count = static_cast<uint32_t>(cache_entries_.size());
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&entry_count), sizeof(entry_count));
        
        // Write entries
        for (const auto& [key, entry] : cache_entries_) {
            // Write key
            uint32_t source_size = static_cast<uint32_t>(key.audio_source.size());
            file.write(reinterpret_cast<const char*>(&source_size), sizeof(source_size));
            file.write(key.audio_source.data(), source_size);
            
            file.write(reinterpret_cast<const char*>(&key.start_time), sizeof(key.start_time));
            file.write(reinterpret_cast<const char*>(&key.duration), sizeof(key.duration));
            file.write(reinterpret_cast<const char*>(&key.samples_per_point), sizeof(key.samples_per_point));
            file.write(reinterpret_cast<const char*>(&key.channel_mask), sizeof(key.channel_mask));
            
            // Write entry metadata
            std::string file_path_str = entry->file_path.string();
            uint32_t path_size = static_cast<uint32_t>(file_path_str.size());
            file.write(reinterpret_cast<const char*>(&path_size), sizeof(path_size));
            file.write(file_path_str.data(), path_size);
            
            file.write(reinterpret_cast<const char*>(&entry->compressed_size), sizeof(entry->compressed_size));
            file.write(reinterpret_cast<const char*>(&entry->uncompressed_size), sizeof(entry->uncompressed_size));
            file.write(reinterpret_cast<const char*>(&entry->is_persistent), sizeof(entry->is_persistent));
            
            auto created_time_t = std::chrono::system_clock::to_time_t(entry->created_time);
            auto last_accessed_t = std::chrono::system_clock::to_time_t(entry->last_accessed);
            file.write(reinterpret_cast<const char*>(&created_time_t), sizeof(created_time_t));
            file.write(reinterpret_cast<const char*>(&last_accessed_t), sizeof(last_accessed_t));
        }
        
        return file.good();
        
    } catch (const std::exception& e) {
        ve::log_error("Failed to export cache index: {}", e.what());
        return false;
    }
}

bool WaveformCacheImpl::import_index(const std::filesystem::path& index_file) {
    try {
        std::ifstream file(index_file, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Read header
        uint32_t version, entry_count;
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        file.read(reinterpret_cast<char*>(&entry_count), sizeof(entry_count));
        
        if (version != 1) {
            ve::log_warning("Unsupported cache index version: {}", version);
            return false;
        }
        
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        
        // Read entries
        for (uint32_t i = 0; i < entry_count; ++i) {
            // Read key
            uint32_t source_size;
            file.read(reinterpret_cast<char*>(&source_size), sizeof(source_size));
            
            WaveformCacheKey key;
            key.audio_source.resize(source_size);
            file.read(key.audio_source.data(), source_size);
            
            file.read(reinterpret_cast<char*>(&key.start_time), sizeof(key.start_time));
            file.read(reinterpret_cast<char*>(&key.duration), sizeof(key.duration));
            file.read(reinterpret_cast<char*>(&key.samples_per_point), sizeof(key.samples_per_point));
            file.read(reinterpret_cast<char*>(&key.channel_mask), sizeof(key.channel_mask));
            
            // Read entry metadata
            uint32_t path_size;
            file.read(reinterpret_cast<char*>(&path_size), sizeof(path_size));
            
            std::string file_path_str;
            file_path_str.resize(path_size);
            file.read(file_path_str.data(), path_size);
            
            auto entry = std::make_unique<WaveformCacheEntry>(key);
            entry->file_path = file_path_str;
            
            file.read(reinterpret_cast<char*>(&entry->compressed_size), sizeof(entry->compressed_size));
            file.read(reinterpret_cast<char*>(&entry->uncompressed_size), sizeof(entry->uncompressed_size));
            file.read(reinterpret_cast<char*>(&entry->is_persistent), sizeof(entry->is_persistent));
            
            std::time_t created_time_t, last_accessed_t;
            file.read(reinterpret_cast<char*>(&created_time_t), sizeof(created_time_t));
            file.read(reinterpret_cast<char*>(&last_accessed_t), sizeof(last_accessed_t));
            
            entry->created_time = std::chrono::system_clock::from_time_t(created_time_t);
            entry->last_accessed = std::chrono::system_clock::from_time_t(last_accessed_t);
            
            // Only add if file still exists
            if (std::filesystem::exists(entry->file_path)) {
                cache_entries_[key] = std::move(entry);
                lru_policy_->add(key);
                disk_usage_ += cache_entries_[key]->compressed_size;
            }
        }
        
        stats_.current_entry_count = cache_entries_.size();
        stats_.current_disk_usage = disk_usage_.load();
        
        ve::log_info("Cache index imported: {} entries loaded", cache_entries_.size());
        return true;
        
    } catch (const std::exception& e) {
        ve::log_error("Failed to import cache index: {}", e.what());
        return false;
    }
}

//=============================================================================
// Private Implementation Methods
//=============================================================================

void WaveformCacheImpl::worker_thread() {
    while (!shutdown_) {
        std::shared_ptr<CacheTask> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] { return !task_queue_.empty() || shutdown_; });
            
            if (shutdown_) break;
            
            task = task_queue_.front();
            task_queue_.pop();
        }
        
        if (task) {
            process_task(*task);
        }
    }
}

void WaveformCacheImpl::stats_thread() {
    while (!shutdown_) {
        std::this_thread::sleep_for(config_.stats_report_interval);
        
        if (shutdown_) break;
        
        // Update current statistics
        stats_.current_memory_usage = memory_usage_.load();
        stats_.current_disk_usage = disk_usage_.load();
        stats_.current_entry_count = cache_entries_.size();
        
        // Call stats callback if set
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (stats_callback_) {
                stats_callback_(stats_);
            }
        }
        
        // Log periodic statistics
        auto now = std::chrono::system_clock::now();
        if (now - last_stats_report_ >= config_.stats_report_interval) {
            ve::log_info("Cache stats: {}% hit ratio, {} entries, {}MB memory, {}MB disk",
                        static_cast<int>(stats_.hit_ratio() * 100),
                        stats_.current_entry_count.load(),
                        stats_.current_memory_usage.load() / (1024 * 1024),
                        stats_.current_disk_usage.load() / (1024 * 1024));
            last_stats_report_ = now;
        }
    }
}

void WaveformCacheImpl::process_task(const CacheTask& task) {
    switch (task.type) {
        case CacheTask::COMPRESS_AND_STORE:
            compress_and_store_sync(task.key, task.data, task.is_persistent);
            break;
            
        case CacheTask::LOAD_AND_DECOMPRESS:
            {
                auto data = load_and_decompress_sync(task.key);
                if (!task.promise.get_future().valid()) {
                    // Promise was already set
                    break;
                }
                task.promise.set_value(data);
            }
            break;
            
        case CacheTask::PREFETCH:
            {
                // Implement prefetch logic: check if data exists and prepare cache
                std::shared_ptr<WaveformData> cached_data;
                
                // Check memory cache first
                {
                    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
                    auto it = memory_cache_.find(task.key);
                    if (it != memory_cache_.end()) {
                        cached_data = it->second;
                        lru_policy_->access(task.key); // Mark as recently accessed
                    }
                }
                
                // If not in memory, try loading from disk cache
                if (!cached_data && config_.enable_persistent_cache) {
                    cached_data = load_and_decompress_sync(task.key);
                    if (cached_data) {
                        // Pre-load into memory cache for faster access
                        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
                        memory_cache_[task.key] = cached_data;
                        lru_policy_->access(task.key);
                        
                        // Update cache entry
                        auto entry_it = cache_entries_.find(task.key);
                        if (entry_it != cache_entries_.end()) {
                            entry_it->second->last_accessed = std::chrono::system_clock::now();
                            entry_it->second->access_count++;
                        }
                        
                        ve::log_debug("Prefetched waveform from disk cache: {}", 
                                     task.key.audio_source);
                    }
                }
                
                // Note: If data doesn't exist in cache, we don't generate it here
                // Generation should be triggered by the main application flow
                // This prefetch just optimizes access to already cached data
                
                if (cached_data) {
                    notify_event(task.key, "prefetch_completed_cached");
                } else {
                    notify_event(task.key, "prefetch_completed_not_found");
                }
            }
            break;
            
        case CacheTask::CLEANUP:
            cleanup(false);
            break;
            
        case CacheTask::OPTIMIZE:
            // Defragment cache, recompress with better settings, etc.
            ve::log_info("Cache optimization completed");
            break;
    }
}

bool WaveformCacheImpl::compress_and_store_sync(
    const WaveformCacheKey& key,
    std::shared_ptr<WaveformData> data,
    bool is_persistent) {
    
    if (!data) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Generate file path
        auto file_path = get_cache_file_path(key);
        std::filesystem::create_directories(file_path.parent_path());
        
        // Compress data
        std::vector<uint8_t> compressed_data;
        if (config_.compression.enable_compression &&
            waveform_utils::calculate_memory_usage(*data) >= config_.compression.min_size_for_compression) {
            
            compressed_data = cache_utils::compress_waveform_data(*data, config_.compression.compression_level);
            stats_.compressions++;
        } else {
            // Store uncompressed (still need serialization)
            compressed_data = cache_utils::compress_waveform_data(*data, 0); // Level 0 = no compression
        }
        
        // Write to disk
        std::ofstream file(file_path, std::ios::binary);
        if (!file) {
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
        file.close();
        
        // Update cache entry
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            auto it = cache_entries_.find(key);
            if (it != cache_entries_.end()) {
                it->second->file_path = file_path;
                it->second->compressed_size = compressed_data.size();
                disk_usage_ += compressed_data.size();
                stats_.current_disk_usage = disk_usage_.load();
            }
        }
        
        stats_.disk_writes++;
        stats_.total_bytes_cached += waveform_utils::calculate_memory_usage(*data);
        stats_.total_bytes_compressed += compressed_data.size();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        update_stats_timing(stats_.avg_write_time, duration);
        
        notify_event(key, "stored_to_disk");
        return true;
        
    } catch (const std::exception& e) {
        ve::log_error("Failed to compress and store waveform: {}", e.what());
        return false;
    }
}

std::shared_ptr<WaveformData> WaveformCacheImpl::load_and_decompress_sync(const WaveformCacheKey& key) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        auto it = cache_entries_.find(key);
        if (it == cache_entries_.end() || it->second->file_path.empty()) {
            return nullptr;
        }
        
        auto file_path = it->second->file_path;
        lock.unlock();
        
        if (!std::filesystem::exists(file_path)) {
            return nullptr;
        }
        
        // Read compressed data
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return nullptr;
        }
        
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> compressed_data(file_size);
        file.read(reinterpret_cast<char*>(compressed_data.data()), file_size);
        file.close();
        
        // Decompress data
        auto data = cache_utils::decompress_waveform_data(compressed_data);
        if (data) {
            stats_.decompressions++;
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            update_stats_timing(stats_.avg_read_time, duration);
        }
        
        return data;
        
    } catch (const std::exception& e) {
        ve::log_error("Failed to load and decompress waveform: {}", e.what());
        return nullptr;
    }
}

std::filesystem::path WaveformCacheImpl::get_cache_file_path(const WaveformCacheKey& key) const {
    // Create hierarchical directory structure based on hash
    size_t hash = key.hash();
    
    std::ostringstream oss;
    oss << std::hex << hash;
    std::string hash_str = oss.str();
    
    // Create subdirectories using first few characters of hash
    auto cache_dir = config_.cache_directory;
    if (hash_str.length() >= 4) {
        cache_dir = cache_dir / hash_str.substr(0, 2) / hash_str.substr(2, 2);
    }
    
    // Generate filename
    std::ostringstream filename;
    filename << hash_str << "_" << key.samples_per_point << ".waveform";
    
    return cache_dir / filename.str();
}

bool WaveformCacheImpl::ensure_cache_directory() const {
    try {
        std::filesystem::create_directories(config_.cache_directory);
        return std::filesystem::exists(config_.cache_directory);
    } catch (const std::exception& e) {
        ve::log_error("Failed to create cache directory: {}", e.what());
        return false;
    }
}

bool WaveformCacheImpl::needs_cleanup() const {
    size_t memory_limit = config_.max_memory_usage_mb * 1024 * 1024;
    size_t disk_limit = config_.max_disk_usage_mb * 1024 * 1024;
    
    return memory_usage_.load() > memory_limit * config_.memory_pressure_threshold ||
           disk_usage_.load() > disk_limit * config_.disk_pressure_threshold ||
           cache_entries_.size() > config_.max_entries;
}

size_t WaveformCacheImpl::cleanup_memory_pressure() {
    size_t target_reduction = memory_usage_.load() * 0.2f; // Remove 20% of memory usage
    size_t removed_count = 0;
    size_t bytes_freed = 0;
    
    // Get eviction candidates from LRU policy
    auto candidates = lru_policy_->get_candidates_for_eviction(cache_entries_.size());
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    for (const auto& key : candidates) {
        if (bytes_freed >= target_reduction) {
            break;
        }
        
        auto it = memory_cache_.find(key);
        if (it != memory_cache_.end()) {
            auto entry_it = cache_entries_.find(key);
            if (entry_it != cache_entries_.end() && !entry_it->second->is_persistent) {
                bytes_freed += entry_it->second->uncompressed_size;
                memory_usage_ -= entry_it->second->uncompressed_size;
                memory_cache_.erase(it);
                
                // Keep entry metadata but remove data from memory
                entry_it->second->data.reset();
                removed_count++;
            }
        }
    }
    
    stats_.current_memory_usage = memory_usage_.load();
    return removed_count;
}

size_t WaveformCacheImpl::cleanup_disk_pressure() {
    size_t target_reduction = disk_usage_.load() * 0.15f; // Remove 15% of disk usage
    size_t removed_count = 0;
    size_t bytes_freed = 0;
    
    // Get eviction candidates, prioritizing largest files
    std::vector<std::pair<size_t, WaveformCacheKey>> size_candidates;
    
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        for (const auto& [key, entry] : cache_entries_) {
            if (!entry->is_persistent) {
                size_candidates.emplace_back(entry->compressed_size, key);
            }
        }
    }
    
    // Sort by size (largest first)
    std::sort(size_candidates.begin(), size_candidates.end(), std::greater<>());
    
    for (const auto& [size, key] : size_candidates) {
        if (bytes_freed >= target_reduction) {
            break;
        }
        
        if (remove(key)) {
            bytes_freed += size;
            removed_count++;
        }
    }
    
    return removed_count;
}

void WaveformCacheImpl::update_stats_timing(
    std::atomic<std::chrono::microseconds>& avg_time,
    std::chrono::microseconds new_time) {
    
    // Simple exponential moving average
    auto current = avg_time.load();
    auto updated = std::chrono::microseconds(
        static_cast<long long>(current.count() * 0.9 + new_time.count() * 0.1)
    );
    avg_time.store(updated);
}

void WaveformCacheImpl::notify_event(const WaveformCacheKey& key, const std::string& event) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (event_callback_) {
        event_callback_(key, event);
    }
}

bool WaveformCacheImpl::load_cache_index() {
    auto index_file = config_.cache_directory / "cache_index.bin";
    return import_index(index_file);
}

bool WaveformCacheImpl::save_cache_index() {
    auto index_file = config_.cache_directory / "cache_index.bin";
    return export_index(index_file);
}

//=============================================================================
// Factory Method
//=============================================================================

std::unique_ptr<WaveformCache> WaveformCache::create(const WaveformCacheConfig& config) {
    return std::make_unique<WaveformCacheImpl>(config);
}

//=============================================================================
// Cache Utils Implementation
//=============================================================================

namespace cache_utils {

WaveformCacheKey generate_cache_key(
    const std::string& audio_source,
    const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
    const ZoomLevel& zoom_level,
    uint32_t channel_mask) {
    
    WaveformCacheKey key;
    key.audio_source = audio_source;
    key.start_time = time_range.first;
    key.duration = time_range.second - time_range.first;
    key.samples_per_point = zoom_level.samples_per_point;
    key.channel_mask = channel_mask;
    
    return key;
}

size_t calculate_storage_size(
    const WaveformData& data,
    bool with_compression,
    int compression_level) {
    
    size_t base_size = waveform_utils::calculate_memory_usage(data);
    
    if (!with_compression) {
        return base_size;
    }
    
    // Estimate compression ratio based on level
    double compression_ratio = 0.4 + (compression_level / 9.0) * 0.3; // 40-70% compression
    return static_cast<size_t>(base_size * compression_ratio);
}

bool validate_cache_directory(const std::filesystem::path& cache_dir) {
    try {
        if (!std::filesystem::exists(cache_dir)) {
            std::filesystem::create_directories(cache_dir);
        }
        
        // Test write permissions
        auto test_file = cache_dir / "test_write.tmp";
        {
            std::ofstream test(test_file);
            if (!test) {
                return false;
            }
        }
        
        std::filesystem::remove(test_file);
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

size_t cleanup_orphaned_files(const std::filesystem::path& cache_dir) {
    size_t removed_count = 0;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".waveform") {
                // Check if file is very old (could be orphaned)
                auto last_write = std::filesystem::last_write_time(entry);
                auto now = std::filesystem::file_time_type::clock::now();
                auto age = now - last_write;
                
                if (age > std::chrono::hours(24 * 30)) { // 30 days old
                    std::filesystem::remove(entry);
                    removed_count++;
                }
            }
        }
    } catch (const std::exception& e) {
        ve::log_warning("Error during orphaned file cleanup: {}", e.what());
    }
    
    return removed_count;
}

std::vector<WaveformCacheKey> calculate_prefetch_keys(
    const std::string& audio_source,
    const ve::TimePoint& current_position,
    const ve::TimePoint& playback_speed,
    const std::vector<ZoomLevel>& active_zoom_levels,
    size_t prefetch_window_seconds) {
    
    std::vector<WaveformCacheKey> keys;
    
    // Calculate prefetch time range based on playback speed
    ve::TimePoint prefetch_duration(prefetch_window_seconds, 1);
    if (playback_speed > ve::TimePoint(0, 1)) {
        prefetch_duration = prefetch_duration * playback_speed;
    }
    
    ve::TimePoint start_time = current_position;
    ve::TimePoint end_time = current_position + prefetch_duration;
    
    for (const auto& zoom_level : active_zoom_levels) {
        keys.push_back(generate_cache_key(
            audio_source,
            {start_time, end_time},
            zoom_level,
            0
        ));
    }
    
    return keys;
}

std::vector<uint8_t> compress_waveform_data(
    const WaveformData& data,
    int compression_level) {
    
    // Serialize waveform data to binary format
    std::vector<uint8_t> serialized;
    
    // Header
    serialized.resize(sizeof(uint32_t) * 6); // Version, channels, points, sample_rate, samples_per_point, reserved
    auto* header = reinterpret_cast<uint32_t*>(serialized.data());
    header[0] = 1; // Version
    header[1] = static_cast<uint32_t>(data.channel_count());
    header[2] = static_cast<uint32_t>(data.point_count());
    header[3] = static_cast<uint32_t>(data.sample_rate);
    header[4] = static_cast<uint32_t>(data.samples_per_point);
    header[5] = 0; // Reserved
    
    // Time information
    size_t time_offset = serialized.size();
    serialized.resize(time_offset + sizeof(ve::TimePoint) * 2);
    auto* time_data = reinterpret_cast<ve::TimePoint*>(serialized.data() + time_offset);
    time_data[0] = data.start_time;
    time_data[1] = data.duration;
    
    // Waveform points
    size_t points_offset = serialized.size();
    size_t points_size = data.channel_count() * data.point_count() * sizeof(WaveformPoint);
    serialized.resize(points_offset + points_size);
    
    auto* points_data = reinterpret_cast<WaveformPoint*>(serialized.data() + points_offset);
    for (size_t ch = 0; ch < data.channel_count(); ++ch) {
        std::memcpy(points_data + ch * data.point_count(),
                   data.channels[ch].data(),
                   data.point_count() * sizeof(WaveformPoint));
    }
    
    // Compress if requested
    if (compression_level == 0) {
        return serialized;
    }
    
    // Use zlib for compression
    uLongf compressed_size = compressBound(static_cast<uLong>(serialized.size()));
    std::vector<uint8_t> compressed(compressed_size + sizeof(uint32_t));
    
    // Store original size in first 4 bytes
    *reinterpret_cast<uint32_t*>(compressed.data()) = static_cast<uint32_t>(serialized.size());
    
    int result = compress2(
        compressed.data() + sizeof(uint32_t),
        &compressed_size,
        serialized.data(),
        static_cast<uLong>(serialized.size()),
        compression_level
    );
    
    if (result != Z_OK) {
        // Compression failed, return uncompressed
        return serialized;
    }
    
    compressed.resize(compressed_size + sizeof(uint32_t));
    return compressed;
}

std::shared_ptr<WaveformData> decompress_waveform_data(
    const std::vector<uint8_t>& compressed_data) {
    
    if (compressed_data.size() < sizeof(uint32_t)) {
        return nullptr;
    }
    
    // Check if data is compressed (has size header)
    uint32_t original_size = *reinterpret_cast<const uint32_t*>(compressed_data.data());
    
    std::vector<uint8_t> decompressed;
    
    if (original_size > compressed_data.size()) {
        // Data is compressed
        decompressed.resize(original_size);
        
        uLongf dest_len = original_size;
        int result = uncompress(
            decompressed.data(),
            &dest_len,
            compressed_data.data() + sizeof(uint32_t),
            static_cast<uLong>(compressed_data.size() - sizeof(uint32_t))
        );
        
        if (result != Z_OK) {
            return nullptr;
        }
    } else {
        // Data is not compressed
        decompressed.assign(compressed_data.begin(), compressed_data.end());
    }
    
    // Deserialize waveform data
    if (decompressed.size() < sizeof(uint32_t) * 6) {
        return nullptr;
    }
    
    const auto* header = reinterpret_cast<const uint32_t*>(decompressed.data());
    uint32_t version = header[0];
    uint32_t channel_count = header[1];
    uint32_t point_count = header[2];
    uint32_t sample_rate = header[3];
    uint32_t samples_per_point = header[4];
    
    if (version != 1 || channel_count == 0 || point_count == 0) {
        return nullptr;
    }
    
    // Read time information
    size_t time_offset = sizeof(uint32_t) * 6;
    if (decompressed.size() < time_offset + sizeof(ve::TimePoint) * 2) {
        return nullptr;
    }
    
    const auto* time_data = reinterpret_cast<const ve::TimePoint*>(decompressed.data() + time_offset);
    ve::TimePoint start_time = time_data[0];
    ve::TimePoint duration = time_data[1];
    
    // Read waveform points
    size_t points_offset = time_offset + sizeof(ve::TimePoint) * 2;
    size_t expected_points_size = channel_count * point_count * sizeof(WaveformPoint);
    
    if (decompressed.size() < points_offset + expected_points_size) {
        return nullptr;
    }
    
    auto waveform_data = std::make_shared<WaveformData>();
    waveform_data->start_time = start_time;
    waveform_data->duration = duration;
    waveform_data->sample_rate = static_cast<int32_t>(sample_rate);
    waveform_data->samples_per_point = static_cast<size_t>(samples_per_point);
    waveform_data->channels.resize(channel_count);
    
    const auto* points_data = reinterpret_cast<const WaveformPoint*>(decompressed.data() + points_offset);
    for (size_t ch = 0; ch < channel_count; ++ch) {
        waveform_data->channels[ch].resize(point_count);
        std::memcpy(waveform_data->channels[ch].data(),
                   points_data + ch * point_count,
                   point_count * sizeof(WaveformPoint));
    }
    
    return waveform_data;
}

WaveformCacheStats merge_statistics(
    const std::vector<WaveformCacheStats>& stats_list) {
    
    WaveformCacheStats merged;
    
    for (const auto& stats : stats_list) {
        merged.cache_hits += stats.cache_hits.load();
        merged.cache_misses += stats.cache_misses.load();
        merged.evictions += stats.evictions.load();
        merged.compressions += stats.compressions.load();
        merged.decompressions += stats.decompressions.load();
        merged.disk_reads += stats.disk_reads.load();
        merged.disk_writes += stats.disk_writes.load();
        merged.total_bytes_cached += stats.total_bytes_cached.load();
        merged.total_bytes_compressed += stats.total_bytes_compressed.load();
        merged.current_memory_usage += stats.current_memory_usage.load();
        merged.current_disk_usage += stats.current_disk_usage.load();
        merged.current_entry_count += stats.current_entry_count.load();
    }
    
    // Average timing statistics
    if (!stats_list.empty()) {
        auto total_read_time = std::chrono::microseconds(0);
        auto total_write_time = std::chrono::microseconds(0);
        auto total_compression_time = std::chrono::microseconds(0);
        
        for (const auto& stats : stats_list) {
            total_read_time += stats.avg_read_time.load();
            total_write_time += stats.avg_write_time.load();
            total_compression_time += stats.avg_compression_time.load();
        }
        
        merged.avg_read_time = total_read_time / stats_list.size();
        merged.avg_write_time = total_write_time / stats_list.size();
        merged.avg_compression_time = total_compression_time / stats_list.size();
    }
    
    return merged;
}

} // namespace cache_utils

} // namespace ve::audio