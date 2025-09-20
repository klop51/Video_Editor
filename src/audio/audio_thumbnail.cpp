/**
 * @file audio_thumbnail.cpp
 * @brief Implementation of Audio Thumbnail System
 */

#include "audio/audio_thumbnail.h"
#include "core/logging.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

namespace ve::audio {

//=============================================================================
// AudioThumbnail Implementation
//=============================================================================

float AudioThumbnail::get_peak_at_position(size_t channel, float position) const {
    if (channel >= peak_data.size() || peak_data[channel].empty()) {
        return 0.0f;
    }
    
    position = std::clamp(position, 0.0f, 1.0f);
    size_t index = static_cast<size_t>(position * (peak_data[channel].size() - 1));
    return peak_data[channel][index];
}

float AudioThumbnail::get_rms_at_position(size_t channel, float position) const {
    if (channel >= rms_data.size() || rms_data[channel].empty()) {
        return 0.0f;
    }
    
    position = std::clamp(position, 0.0f, 1.0f);
    size_t index = static_cast<size_t>(position * (rms_data[channel].size() - 1));
    return rms_data[channel][index];
}

//=============================================================================
// AudioThumbnailGenerator Implementation
//=============================================================================

class AudioThumbnailGeneratorImpl : public AudioThumbnailGenerator {
public:
    explicit AudioThumbnailGeneratorImpl(
        std::shared_ptr<WaveformGenerator> waveform_generator,
        std::shared_ptr<WaveformCache> waveform_cache,
        const ThumbnailConfig& config
    );
    
    ~AudioThumbnailGeneratorImpl() override;
    
    // AudioThumbnailGenerator interface
    std::future<std::shared_ptr<AudioThumbnail>> generate_thumbnail(
        const std::string& audio_source,
        ThumbnailSize size,
        int priority
    ) override;
    
    std::future<std::vector<std::shared_ptr<AudioThumbnail>>> generate_batch(
        const std::vector<std::string>& audio_sources,
        ThumbnailSize size,
        ThumbnailProgressCallback progress_callback,
        ThumbnailCompletionCallback completion_callback,
        BatchCompletionCallback batch_callback
    ) override;
    
    std::shared_ptr<AudioThumbnail> get_cached_thumbnail(
        const std::string& audio_source,
        ThumbnailSize size
    ) override;
    
    bool is_thumbnail_available(
        const std::string& audio_source,
        ThumbnailSize size
    ) override;
    
    bool cancel_generation(const std::string& audio_source) override;
    size_t cancel_all_generations() override;
    float get_generation_progress(const std::string& audio_source) override;
    size_t clear_cache(size_t older_than_hours) override;
    CacheStats get_cache_statistics() override;
    
    const ThumbnailConfig& get_config() const override { return config_; }
    void set_config(const ThumbnailConfig& config) override;

private:
    /**
     * @brief Thumbnail generation task
     */
    struct ThumbnailTask {
        std::string audio_source;
        ThumbnailSize size;
        int priority;
        std::promise<std::shared_ptr<AudioThumbnail>> promise;
        std::atomic<bool> cancelled{false};
        std::atomic<float> progress{0.0f};
        std::chrono::system_clock::time_point creation_time;
        
        ThumbnailTask(const std::string& source, ThumbnailSize thumbnail_size, int task_priority)
            : audio_source(source), size(thumbnail_size), priority(task_priority)
            , creation_time(std::chrono::system_clock::now()) {}
        
        bool operator<(const ThumbnailTask& other) const {
            // Higher priority tasks come first
            return priority < other.priority;
        }
    };
    
    /**
     * @brief Batch generation context
     */
    struct BatchContext {
        std::vector<std::string> audio_sources;
        ThumbnailSize size;
        ThumbnailProgressCallback progress_callback;
        ThumbnailCompletionCallback completion_callback;
        BatchCompletionCallback batch_callback;
        std::promise<std::vector<std::shared_ptr<AudioThumbnail>>> promise;
        std::atomic<size_t> completed_count{0};
        std::vector<std::shared_ptr<AudioThumbnail>> results;
        
        BatchContext(const std::vector<std::string>& sources, ThumbnailSize batch_size)
            : audio_sources(sources), size(batch_size) {
            results.resize(sources.size());
        }
    };
    
    /**
     * @brief Worker thread function
     */
    void worker_thread();
    
    /**
     * @brief Process a single thumbnail generation task
     */
    std::shared_ptr<AudioThumbnail> process_thumbnail_task(const ThumbnailTask& task);
    
    /**
     * @brief Generate thumbnail from waveform data
     */
    std::shared_ptr<AudioThumbnail> generate_from_waveform(
        const std::string& audio_source,
        const WaveformData& waveform_data,
        ThumbnailSize size
    );
    
    /**
     * @brief Load thumbnail from cache
     */
    std::shared_ptr<AudioThumbnail> load_from_cache(
        const std::string& audio_source,
        ThumbnailSize size
    );
    
    /**
     * @brief Save thumbnail to cache
     */
    void save_to_cache(
        const std::string& audio_source,
        ThumbnailSize size,
        std::shared_ptr<AudioThumbnail> thumbnail
    );
    
    /**
     * @brief Get cache file path for thumbnail
     */
    std::filesystem::path get_cache_file_path(
        const std::string& audio_source,
        ThumbnailSize size
    ) const;
    
    /**
     * @brief Check if cached thumbnail is still valid
     */
    bool is_cache_valid(
        const std::string& audio_source,
        const std::filesystem::path& cache_file
    ) const;
    
    /**
     * @brief Clean up expired cache entries
     */
    void cleanup_cache();
    
    // Configuration and dependencies
    ThumbnailConfig config_;
    std::shared_ptr<WaveformGenerator> waveform_generator_;
    std::shared_ptr<WaveformCache> waveform_cache_;
    
    // Threading infrastructure
    std::vector<std::thread> worker_threads_;
    std::priority_queue<std::shared_ptr<ThumbnailTask>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::atomic<bool> shutdown_{false};
    
    // Task tracking
    std::unordered_map<std::string, std::shared_ptr<ThumbnailTask>> active_tasks_;
    std::unordered_map<std::string, std::shared_ptr<BatchContext>> active_batches_;
    mutable std::mutex tracking_mutex_;
    
    // Memory cache for recently generated thumbnails
    std::unordered_map<std::string, std::shared_ptr<AudioThumbnail>> memory_cache_;
    mutable std::mutex cache_mutex_;
    std::atomic<size_t> memory_usage_{0};
    
    // Cache directory
    std::filesystem::path cache_directory_;
    
    // Statistics
    std::atomic<size_t> thumbnails_generated_{0};
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_misses_{0};
};

AudioThumbnailGeneratorImpl::AudioThumbnailGeneratorImpl(
    std::shared_ptr<WaveformGenerator> waveform_generator,
    std::shared_ptr<WaveformCache> waveform_cache,
    const ThumbnailConfig& config)
    : config_(config)
    , waveform_generator_(waveform_generator)
    , waveform_cache_(waveform_cache)
    , cache_directory_("thumbnail_cache") {
    
    if (!waveform_generator_) {
        throw std::invalid_argument("WaveformGenerator is required for thumbnail generation");
    }
    
    // Create cache directory
    if (config_.enable_thumbnail_cache) {
        try {
            std::filesystem::create_directories(cache_directory_);
        } catch (const std::exception& e) {
            ve::log_warning("Failed to create thumbnail cache directory: {}", e.what());
        }
    }
    
    // Start worker threads
    for (size_t i = 0; i < config_.max_concurrent_thumbnails; ++i) {
        worker_threads_.emplace_back(&AudioThumbnailGeneratorImpl::worker_thread, this);
    }
    
    ve::log_info("AudioThumbnailGenerator initialized with {} workers, cache: {}",
                config_.max_concurrent_thumbnails,
                config_.enable_thumbnail_cache ? "enabled" : "disabled");
}

AudioThumbnailGeneratorImpl::~AudioThumbnailGeneratorImpl() {
    shutdown_ = true;
    queue_condition_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    ve::log_info("AudioThumbnailGenerator shutdown. Generated: {}, Cache hits: {}, Cache misses: {}",
                thumbnails_generated_.load(),
                cache_hits_.load(),
                cache_misses_.load());
}

std::future<std::shared_ptr<AudioThumbnail>> AudioThumbnailGeneratorImpl::generate_thumbnail(
    const std::string& audio_source,
    ThumbnailSize size,
    int priority) {
    
    // Check cache first
    auto cached = get_cached_thumbnail(audio_source, size);
    if (cached) {
        std::promise<std::shared_ptr<AudioThumbnail>> promise;
        auto future = promise.get_future();
        promise.set_value(cached);
        return future;
    }
    
    // Create and enqueue task
    auto task = std::make_shared<ThumbnailTask>(audio_source, size, priority);
    auto future = task->promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
        
        std::lock_guard<std::mutex> tracking_lock(tracking_mutex_);
        active_tasks_[audio_source] = task;
    }
    
    queue_condition_.notify_one();
    return future;
}

std::future<std::vector<std::shared_ptr<AudioThumbnail>>> AudioThumbnailGeneratorImpl::generate_batch(
    const std::vector<std::string>& audio_sources,
    ThumbnailSize size,
    ThumbnailProgressCallback progress_callback,
    ThumbnailCompletionCallback completion_callback,
    BatchCompletionCallback batch_callback) {
    
    auto batch_context = std::make_shared<BatchContext>(audio_sources, size);
    batch_context->progress_callback = progress_callback;
    batch_context->completion_callback = completion_callback;
    batch_context->batch_callback = batch_callback;
    
    auto future = batch_context->promise.get_future();
    
    // Generate batch ID and register context
    std::string batch_id = "batch_" + std::to_string(std::hash<const void*>{}(batch_context.get()));
    {
        std::lock_guard<std::mutex> lock(tracking_mutex_);
        active_batches_[batch_id] = batch_context;
    }
    
    // Schedule individual thumbnail tasks
    for (size_t i = 0; i < audio_sources.size(); ++i) {
        const auto& audio_source = audio_sources[i];
        
        // Check cache first
        auto cached = get_cached_thumbnail(audio_source, size);
        if (cached) {
            batch_context->results[i] = cached;
            batch_context->completed_count++;
            
            if (completion_callback) {
                completion_callback(cached, true);
            }
            
            continue;
        }
        
        // Create task with batch context
        auto task = std::make_shared<ThumbnailTask>(audio_source, size, 100); // High priority for batch
        
        // Wrap completion to update batch progress
        auto original_promise = std::move(task->promise);
        std::promise<std::shared_ptr<AudioThumbnail>> new_promise;
        task->promise = std::move(new_promise);
        
        auto task_future = task->promise.get_future();
        
        // Handle task completion asynchronously
        std::thread([=, &batch_context = batch_context, task_index = i]() {
            try {
                auto thumbnail = task_future.get();
                batch_context->results[task_index] = thumbnail;
                
                size_t completed = ++batch_context->completed_count;
                
                if (completion_callback) {
                    completion_callback(thumbnail, thumbnail != nullptr);
                }
                
                if (batch_callback) {
                    batch_callback(completed, audio_sources.size());
                }
                
                // Check if batch is complete
                if (completed == audio_sources.size()) {
                    batch_context->promise.set_value(batch_context->results);
                    
                    // Remove from active batches
                    std::lock_guard<std::mutex> lock(tracking_mutex_);
                    active_batches_.erase(batch_id);
                }
                
            } catch (const std::exception& e) {
                ve::log_error("Batch thumbnail generation failed for {}: {}", audio_source, e.what());
            }
        }).detach();
        
        // Queue the task
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(task);
        }
    }
    
    queue_condition_.notify_all();
    return future;
}

std::shared_ptr<AudioThumbnail> AudioThumbnailGeneratorImpl::get_cached_thumbnail(
    const std::string& audio_source,
    ThumbnailSize size) {
    
    if (!config_.enable_thumbnail_cache) {
        return nullptr;
    }
    
    // Check memory cache first
    std::string cache_key = thumbnail_utils::generate_thumbnail_cache_key(
        audio_source, size, std::chrono::system_clock::now() // File time would be better
    );
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = memory_cache_.find(cache_key);
        if (it != memory_cache_.end()) {
            cache_hits_++;
            return it->second;
        }
    }
    
    // Check disk cache
    auto thumbnail = load_from_cache(audio_source, size);
    if (thumbnail) {
        // Store in memory cache
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            memory_cache_[cache_key] = thumbnail;
            memory_usage_ += thumbnail_utils::calculate_thumbnail_memory_usage(*thumbnail);
        }
        
        cache_hits_++;
        return thumbnail;
    }
    
    cache_misses_++;
    return nullptr;
}

bool AudioThumbnailGeneratorImpl::is_thumbnail_available(
    const std::string& audio_source,
    ThumbnailSize size) {
    
    return get_cached_thumbnail(audio_source, size) != nullptr;
}

bool AudioThumbnailGeneratorImpl::cancel_generation(const std::string& audio_source) {
    std::lock_guard<std::mutex> lock(tracking_mutex_);
    
    auto it = active_tasks_.find(audio_source);
    if (it != active_tasks_.end()) {
        it->second->cancelled = true;
        active_tasks_.erase(it);
        return true;
    }
    
    return false;
}

size_t AudioThumbnailGeneratorImpl::cancel_all_generations() {
    std::lock_guard<std::mutex> lock(tracking_mutex_);
    
    size_t cancelled_count = 0;
    for (auto& [source, task] : active_tasks_) {
        task->cancelled = true;
        cancelled_count++;
    }
    
    active_tasks_.clear();
    return cancelled_count;
}

float AudioThumbnailGeneratorImpl::get_generation_progress(const std::string& audio_source) {
    std::lock_guard<std::mutex> lock(tracking_mutex_);
    
    auto it = active_tasks_.find(audio_source);
    return (it != active_tasks_.end()) ? it->second->progress.load() : -1.0f;
}

size_t AudioThumbnailGeneratorImpl::clear_cache(size_t older_than_hours) {
    size_t cleared_count = 0;
    
    // Clear memory cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cleared_count += memory_cache_.size();
        memory_cache_.clear();
        memory_usage_ = 0;
    }
    
    // Clear disk cache
    if (config_.enable_thumbnail_cache && std::filesystem::exists(cache_directory_)) {
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(older_than_hours);
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_directory_)) {
                if (entry.is_regular_file()) {
                    if (older_than_hours == 0 || 
                        std::filesystem::last_write_time(entry) < std::filesystem::file_time_type::clock::now() - std::chrono::hours(older_than_hours)) {
                        std::filesystem::remove(entry);
                        cleared_count++;
                    }
                }
            }
        } catch (const std::exception& e) {
            ve::log_warning("Error clearing thumbnail cache: {}", e.what());
        }
    }
    
    return cleared_count;
}

AudioThumbnailGenerator::CacheStats AudioThumbnailGeneratorImpl::get_cache_statistics() {
    CacheStats stats{};
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        stats.total_thumbnails = memory_cache_.size();
        stats.memory_usage_bytes = memory_usage_.load();
    }
    
    // Calculate disk usage
    if (config_.enable_thumbnail_cache && std::filesystem::exists(cache_directory_)) {
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_directory_)) {
                if (entry.is_regular_file()) {
                    stats.disk_usage_bytes += entry.file_size();
                    
                    auto write_time = std::filesystem::last_write_time(entry);
                    auto system_time = std::chrono::file_clock::to_sys(write_time);
                    
                    if (stats.oldest_thumbnail == std::chrono::system_clock::time_point{} ||
                        system_time < stats.oldest_thumbnail) {
                        stats.oldest_thumbnail = system_time;
                    }
                    
                    if (system_time > stats.newest_thumbnail) {
                        stats.newest_thumbnail = system_time;
                    }
                }
            }
        } catch (const std::exception& e) {
            ve::log_warning("Error calculating cache statistics: {}", e.what());
        }
    }
    
    // Calculate hit ratio
    size_t total_requests = cache_hits_.load() + cache_misses_.load();
    stats.hit_ratio = total_requests > 0 ? static_cast<float>(cache_hits_.load()) / total_requests : 0.0f;
    
    return stats;
}

void AudioThumbnailGeneratorImpl::set_config(const ThumbnailConfig& config) {
    config_ = config;
    
    // Clean up cache if needed
    if (!config_.enable_thumbnail_cache) {
        clear_cache(0);
    }
}

//=============================================================================
// Private Implementation Methods
//=============================================================================

void AudioThumbnailGeneratorImpl::worker_thread() {
    while (!shutdown_) {
        std::shared_ptr<ThumbnailTask> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] { return !task_queue_.empty() || shutdown_; });
            
            if (shutdown_) break;
            
            task = task_queue_.top();
            task_queue_.pop();
        }
        
        if (task && !task->cancelled) {
            auto thumbnail = process_thumbnail_task(*task);
            
            if (!task->cancelled) {
                task->promise.set_value(thumbnail);
                
                if (thumbnail) {
                    thumbnails_generated_++;
                    save_to_cache(task->audio_source, task->size, thumbnail);
                }
            }
            
            // Remove from active tasks
            {
                std::lock_guard<std::mutex> lock(tracking_mutex_);
                active_tasks_.erase(task->audio_source);
            }
        }
    }
}

std::shared_ptr<AudioThumbnail> AudioThumbnailGeneratorImpl::process_thumbnail_task(const ThumbnailTask& task) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Check for timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            start_time - task.creation_time
        );
        
        if (elapsed > config_.generation_timeout) {
            ve::log_warning("Thumbnail generation timeout for {}", task.audio_source);
            return nullptr;
        }
        
        if (task.cancelled) {
            return nullptr;
        }
        
        task.progress = 0.1f;
        
        // Get file duration for time range
        ve::TimePoint total_duration(0, 1);
        try {
            if (std::filesystem::exists(task.audio_source)) {
                // TODO: Get actual audio duration - for now use placeholder
                total_duration = ve::TimePoint(300, 1); // 5 minutes default
            }
        } catch (const std::exception&) {
            // Use default duration
        }
        
        task.progress = 0.3f;
        
        // Generate appropriate zoom level for thumbnail
        size_t thumbnail_width = static_cast<size_t>(task.size);
        size_t samples_per_point = 1000; // Good default for thumbnails
        
        if (config_.enable_fast_mode) {
            samples_per_point *= 4; // Lower resolution for speed
        }
        
        ZoomLevel zoom_level(samples_per_point, "thumbnail");
        
        task.progress = 0.5f;
        
        // Generate waveform data
        auto waveform_data = waveform_generator_->generate_waveform(
            task.audio_source,
            {ve::TimePoint(0, 1), total_duration},
            zoom_level
        );
        
        if (!waveform_data || task.cancelled) {
            return nullptr;
        }
        
        task.progress = 0.8f;
        
        // Convert to thumbnail
        auto thumbnail = generate_from_waveform(task.audio_source, *waveform_data, task.size);
        
        task.progress = 1.0f;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        ve::log_debug("Generated thumbnail for {} in {}ms", task.audio_source, duration.count());
        
        return thumbnail;
        
    } catch (const std::exception& e) {
        ve::log_error("Thumbnail generation failed for {}: {}", task.audio_source, e.what());
        return nullptr;
    }
}

std::shared_ptr<AudioThumbnail> AudioThumbnailGeneratorImpl::generate_from_waveform(
    const std::string& audio_source,
    const WaveformData& waveform_data,
    ThumbnailSize size) {
    
    // Convert waveform to thumbnail format
    auto thumbnail = thumbnail_utils::convert_waveform_to_thumbnail(
        waveform_data,
        static_cast<size_t>(size),
        config_.generate_rms
    );
    
    if (!thumbnail) {
        return nullptr;
    }
    
    // Set basic metadata
    thumbnail->audio_source = audio_source;
    thumbnail->size = size;
    thumbnail->total_duration = waveform_data.duration;
    thumbnail->sample_rate = waveform_data.sample_rate;
    thumbnail->channel_count = waveform_data.channel_count();
    thumbnail->generated_time = std::chrono::system_clock::now();
    
    // Analyze audio characteristics
    auto analysis = thumbnail_utils::analyze_audio_characteristics(
        waveform_data,
        config_.silence_threshold_db,
        config_.clipping_threshold
    );
    
    thumbnail->is_silent = analysis.is_silent;
    thumbnail->is_clipped = analysis.is_clipped;
    thumbnail->dynamic_range_db = analysis.dynamic_range_db;
    thumbnail->max_amplitude = analysis.max_amplitude;
    thumbnail->average_rms = analysis.average_rms;
    
    return thumbnail;
}

std::shared_ptr<AudioThumbnail> AudioThumbnailGeneratorImpl::load_from_cache(
    const std::string& audio_source,
    ThumbnailSize size) {
    
    auto cache_file = get_cache_file_path(audio_source, size);
    if (!std::filesystem::exists(cache_file)) {
        return nullptr;
    }
    
    // Check if cache is still valid
    if (!is_cache_valid(audio_source, cache_file)) {
        std::filesystem::remove(cache_file);
        return nullptr;
    }
    
    try {
        std::ifstream file(cache_file, std::ios::binary);
        if (!file) {
            return nullptr;
        }
        
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(file_size);
        file.read(reinterpret_cast<char*>(data.data()), file_size);
        file.close();
        
        return thumbnail_utils::deserialize_thumbnail(data);
        
    } catch (const std::exception& e) {
        ve::log_warning("Failed to load thumbnail from cache: {}", e.what());
        return nullptr;
    }
}

void AudioThumbnailGeneratorImpl::save_to_cache(
    const std::string& audio_source,
    ThumbnailSize size,
    std::shared_ptr<AudioThumbnail> thumbnail) {
    
    if (!config_.enable_thumbnail_cache || !thumbnail) {
        return;
    }
    
    try {
        auto cache_file = get_cache_file_path(audio_source, size);
        std::filesystem::create_directories(cache_file.parent_path());
        
        auto data = thumbnail_utils::serialize_thumbnail(*thumbnail);
        
        std::ofstream file(cache_file, std::ios::binary);
        if (file) {
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
        }
        
    } catch (const std::exception& e) {
        ve::log_warning("Failed to save thumbnail to cache: {}", e.what());
    }
}

std::filesystem::path AudioThumbnailGeneratorImpl::get_cache_file_path(
    const std::string& audio_source,
    ThumbnailSize size) const {
    
    // Create hash-based subdirectory structure
    size_t hash = std::hash<std::string>{}(audio_source);
    
    std::ostringstream oss;
    oss << std::hex << hash;
    std::string hash_str = oss.str();
    
    auto cache_dir = cache_directory_;
    if (hash_str.length() >= 4) {
        cache_dir = cache_dir / hash_str.substr(0, 2) / hash_str.substr(2, 2);
    }
    
    // Generate filename
    std::ostringstream filename;
    filename << hash_str << "_" << static_cast<int>(size) << ".thumbnail";
    
    return cache_dir / filename.str();
}

bool AudioThumbnailGeneratorImpl::is_cache_valid(
    const std::string& audio_source,
    const std::filesystem::path& cache_file) const {
    
    try {
        auto cache_time = std::filesystem::last_write_time(cache_file);
        auto source_time = std::filesystem::last_write_time(audio_source);
        
        // Cache is valid if it's newer than the source file
        if (cache_time < source_time) {
            return false;
        }
        
        // Check age
        auto system_cache_time = std::chrono::file_clock::to_sys(cache_time);
        auto age = std::chrono::system_clock::now() - system_cache_time;
        
        return age < config_.cache_duration;
        
    } catch (const std::exception&) {
        return false;
    }
}

void AudioThumbnailGeneratorImpl::cleanup_cache() {
    if (!config_.enable_thumbnail_cache) {
        return;
    }
    
    auto cutoff_time = std::chrono::system_clock::now() - config_.cache_duration;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_directory_)) {
            if (entry.is_regular_file()) {
                auto file_time = std::chrono::file_clock::to_sys(std::filesystem::last_write_time(entry));
                if (file_time < cutoff_time) {
                    std::filesystem::remove(entry);
                }
            }
        }
    } catch (const std::exception& e) {
        ve::log_warning("Cache cleanup failed: {}", e.what());
    }
}

//=============================================================================
// Factory Method
//=============================================================================

std::unique_ptr<AudioThumbnailGenerator> AudioThumbnailGenerator::create(
    std::shared_ptr<WaveformGenerator> waveform_generator,
    std::shared_ptr<WaveformCache> waveform_cache,
    const ThumbnailConfig& config) {
    
    return std::make_unique<AudioThumbnailGeneratorImpl>(
        waveform_generator, waveform_cache, config
    );
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

namespace thumbnail_utils {

ThumbnailSize calculate_optimal_size(size_t display_width, size_t display_height) {
    size_t target_width = std::min(display_width, display_height * 2);
    
    if (target_width <= 64) {
        return ThumbnailSize::TINY;
    } else if (target_width <= 128) {
        return ThumbnailSize::SMALL;
    } else if (target_width <= 256) {
        return ThumbnailSize::MEDIUM;
    } else {
        return ThumbnailSize::LARGE;
    }
}

std::shared_ptr<AudioThumbnail> convert_waveform_to_thumbnail(
    const WaveformData& waveform_data,
    size_t target_width,
    bool include_rms) {
    
    if (!waveform_utils::validate_waveform_data(waveform_data) || target_width == 0) {
        return nullptr;
    }
    
    auto thumbnail = std::make_shared<AudioThumbnail>();
    thumbnail->peak_data.resize(waveform_data.channel_count());
    
    if (include_rms) {
        thumbnail->rms_data.resize(waveform_data.channel_count());
    }
    
    // Downsample waveform data to thumbnail width
    for (size_t ch = 0; ch < waveform_data.channel_count(); ++ch) {
        const auto& channel_data = waveform_data.channels[ch];
        auto& peak_channel = thumbnail->peak_data[ch];
        peak_channel.resize(target_width);
        
        if (include_rms) {
            auto& rms_channel = thumbnail->rms_data[ch];
            rms_channel.resize(target_width);
        }
        
        // Calculate samples per pixel
        float samples_per_pixel = static_cast<float>(channel_data.size()) / target_width;
        
        for (size_t x = 0; x < target_width; ++x) {
            float start_sample = x * samples_per_pixel;
            float end_sample = (x + 1) * samples_per_pixel;
            
            size_t start_idx = static_cast<size_t>(start_sample);
            size_t end_idx = std::min(static_cast<size_t>(end_sample), channel_data.size());
            
            if (start_idx >= channel_data.size()) {
                break;
            }
            
            // Find peak and RMS values in this range
            float max_peak = 0.0f;
            float rms_sum = 0.0f;
            size_t sample_count = 0;
            
            for (size_t i = start_idx; i < end_idx; ++i) {
                const auto& point = channel_data[i];
                max_peak = std::max(max_peak, point.peak_amplitude());
                
                if (include_rms) {
                    rms_sum += point.rms_value * point.rms_value;
                }
                sample_count++;
            }
            
            // Normalize to 0.0-1.0 range
            peak_channel[x] = std::clamp(max_peak, 0.0f, 1.0f);
            
            if (include_rms && sample_count > 0) {
                float rms_value = std::sqrt(rms_sum / sample_count);
                thumbnail->rms_data[ch][x] = std::clamp(rms_value, 0.0f, 1.0f);
            }
        }
    }
    
    return thumbnail;
}

std::shared_ptr<AudioThumbnail> downsample_thumbnail(
    const AudioThumbnail& source_thumbnail,
    ThumbnailSize target_size) {
    
    size_t target_width = static_cast<size_t>(target_size);
    size_t source_width = source_thumbnail.width();
    
    if (target_width >= source_width) {
        // Can't downsample to larger size
        return nullptr;
    }
    
    auto thumbnail = std::make_shared<AudioThumbnail>();
    *thumbnail = source_thumbnail; // Copy metadata
    thumbnail->size = target_size;
    
    // Downsample data
    thumbnail->peak_data.resize(source_thumbnail.channel_count);
    if (!source_thumbnail.rms_data.empty()) {
        thumbnail->rms_data.resize(source_thumbnail.channel_count);
    }
    
    float downsample_ratio = static_cast<float>(source_width) / target_width;
    
    for (size_t ch = 0; ch < source_thumbnail.channel_count; ++ch) {
        const auto& source_peaks = source_thumbnail.peak_data[ch];
        auto& target_peaks = thumbnail->peak_data[ch];
        target_peaks.resize(target_width);
        
        if (!source_thumbnail.rms_data.empty()) {
            const auto& source_rms = source_thumbnail.rms_data[ch];
            auto& target_rms = thumbnail->rms_data[ch];
            target_rms.resize(target_width);
            
            for (size_t x = 0; x < target_width; ++x) {
                float start_pos = x * downsample_ratio;
                float end_pos = (x + 1) * downsample_ratio;
                
                size_t start_idx = static_cast<size_t>(start_pos);
                size_t end_idx = std::min(static_cast<size_t>(end_pos), source_peaks.size());
                
                float max_peak = 0.0f;
                float rms_sum = 0.0f;
                size_t count = 0;
                
                for (size_t i = start_idx; i < end_idx; ++i) {
                    max_peak = std::max(max_peak, source_peaks[i]);
                    rms_sum += source_rms[i] * source_rms[i];
                    count++;
                }
                
                target_peaks[x] = max_peak;
                target_rms[x] = count > 0 ? std::sqrt(rms_sum / count) : 0.0f;
            }
        } else {
            for (size_t x = 0; x < target_width; ++x) {
                float start_pos = x * downsample_ratio;
                float end_pos = (x + 1) * downsample_ratio;
                
                size_t start_idx = static_cast<size_t>(start_pos);
                size_t end_idx = std::min(static_cast<size_t>(end_pos), source_peaks.size());
                
                float max_peak = 0.0f;
                for (size_t i = start_idx; i < end_idx; ++i) {
                    max_peak = std::max(max_peak, source_peaks[i]);
                }
                
                target_peaks[x] = max_peak;
            }
        }
    }
    
    return thumbnail;
}

AudioAnalysis analyze_audio_characteristics(
    const WaveformData& waveform_data,
    float silence_threshold_db,
    float clipping_threshold) {
    
    AudioAnalysis analysis{};
    
    if (waveform_data.channels.empty()) {
        return analysis;
    }
    
    float silence_linear = std::pow(10.0f, silence_threshold_db / 20.0f);
    
    float total_max_amplitude = 0.0f;
    float total_rms_sum = 0.0f;
    size_t total_points = 0;
    size_t silent_points = 0;
    size_t clipped_points = 0;
    
    std::vector<float> all_amplitudes;
    
    for (const auto& channel : waveform_data.channels) {
        for (const auto& point : channel) {
            float amplitude = point.peak_amplitude();
            float rms = point.rms_value;
            
            total_max_amplitude = std::max(total_max_amplitude, amplitude);
            total_rms_sum += rms * rms;
            total_points++;
            
            if (rms < silence_linear) {
                silent_points++;
            }
            
            if (amplitude >= clipping_threshold) {
                clipped_points++;
            }
            
            all_amplitudes.push_back(amplitude);
        }
    }
    
    // Set analysis results
    analysis.max_amplitude = total_max_amplitude;
    analysis.average_rms = total_points > 0 ? std::sqrt(total_rms_sum / total_points) : 0.0f;
    analysis.is_silent = (silent_points > total_points * 0.9f); // 90% silent
    analysis.is_clipped = (clipped_points > 0);
    
    // Calculate dynamic range
    if (!all_amplitudes.empty()) {
        std::sort(all_amplitudes.begin(), all_amplitudes.end());
        
        // Use 1st and 99th percentiles for dynamic range
        size_t low_idx = static_cast<size_t>(all_amplitudes.size() * 0.01f);
        size_t high_idx = static_cast<size_t>(all_amplitudes.size() * 0.99f);
        
        float low_amplitude = all_amplitudes[low_idx];
        float high_amplitude = all_amplitudes[high_idx];
        
        if (low_amplitude > 0.0f && high_amplitude > 0.0f) {
            analysis.dynamic_range_db = 20.0f * std::log10(high_amplitude / low_amplitude);
        } else {
            analysis.dynamic_range_db = 0.0f;
        }
    }
    
    return analysis;
}

std::string generate_thumbnail_cache_key(
    const std::string& audio_source,
    ThumbnailSize size,
    std::chrono::system_clock::time_point file_modification_time) {
    
    auto time_t = std::chrono::system_clock::to_time_t(file_modification_time);
    
    std::ostringstream oss;
    oss << audio_source << "_" << static_cast<int>(size) << "_" << time_t;
    
    return oss.str();
}

bool validate_thumbnail(const AudioThumbnail& thumbnail) {
    if (thumbnail.peak_data.empty() || thumbnail.channel_count == 0) {
        return false;
    }
    
    size_t expected_width = thumbnail.width();
    
    for (const auto& channel_peaks : thumbnail.peak_data) {
        if (channel_peaks.size() != expected_width) {
            return false;
        }
        
        for (float peak : channel_peaks) {
            if (std::isnan(peak) || peak < 0.0f || peak > 1.0f) {
                return false;
            }
        }
    }
    
    if (!thumbnail.rms_data.empty()) {
        for (const auto& channel_rms : thumbnail.rms_data) {
            if (channel_rms.size() != expected_width) {
                return false;
            }
            
            for (float rms : channel_rms) {
                if (std::isnan(rms) || rms < 0.0f || rms > 1.0f) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

size_t calculate_thumbnail_memory_usage(const AudioThumbnail& thumbnail) {
    size_t total_size = sizeof(AudioThumbnail);
    
    for (const auto& channel : thumbnail.peak_data) {
        total_size += channel.size() * sizeof(float);
    }
    
    for (const auto& channel : thumbnail.rms_data) {
        total_size += channel.size() * sizeof(float);
    }
    
    total_size += thumbnail.audio_source.size();
    
    return total_size;
}

std::vector<uint8_t> serialize_thumbnail(const AudioThumbnail& thumbnail) {
    std::vector<uint8_t> data;
    
    // Write header
    size_t header_size = sizeof(uint32_t) * 8; // Version, size, duration, sample_rate, channel_count, etc.
    data.resize(header_size);
    
    auto* header = reinterpret_cast<uint32_t*>(data.data());
    header[0] = 1; // Version
    header[1] = static_cast<uint32_t>(thumbnail.size);
    header[2] = static_cast<uint32_t>(thumbnail.total_duration.numerator());
    header[3] = static_cast<uint32_t>(thumbnail.total_duration.denominator());
    header[4] = static_cast<uint32_t>(thumbnail.sample_rate);
    header[5] = static_cast<uint32_t>(thumbnail.channel_count);
    header[6] = static_cast<uint32_t>(thumbnail.width());
    header[7] = static_cast<uint32_t>(thumbnail.rms_data.empty() ? 0 : 1);
    
    // Write source path
    size_t source_offset = data.size();
    data.resize(source_offset + sizeof(uint32_t) + thumbnail.audio_source.size());
    
    *reinterpret_cast<uint32_t*>(data.data() + source_offset) = static_cast<uint32_t>(thumbnail.audio_source.size());
    std::memcpy(data.data() + source_offset + sizeof(uint32_t), 
                thumbnail.audio_source.data(), 
                thumbnail.audio_source.size());
    
    // Write peak data
    size_t peaks_offset = data.size();
    size_t peaks_size = thumbnail.channel_count * thumbnail.width() * sizeof(float);
    data.resize(peaks_offset + peaks_size);
    
    auto* peaks_data = reinterpret_cast<float*>(data.data() + peaks_offset);
    for (size_t ch = 0; ch < thumbnail.channel_count; ++ch) {
        std::memcpy(peaks_data + ch * thumbnail.width(),
                   thumbnail.peak_data[ch].data(),
                   thumbnail.width() * sizeof(float));
    }
    
    // Write RMS data if present
    if (!thumbnail.rms_data.empty()) {
        size_t rms_offset = data.size();
        size_t rms_size = thumbnail.channel_count * thumbnail.width() * sizeof(float);
        data.resize(rms_offset + rms_size);
        
        auto* rms_data_ptr = reinterpret_cast<float*>(data.data() + rms_offset);
        for (size_t ch = 0; ch < thumbnail.channel_count; ++ch) {
            std::memcpy(rms_data_ptr + ch * thumbnail.width(),
                       thumbnail.rms_data[ch].data(),
                       thumbnail.width() * sizeof(float));
        }
    }
    
    // Write metadata
    size_t metadata_offset = data.size();
    data.resize(metadata_offset + sizeof(float) * 4 + sizeof(uint8_t) * 2);
    
    auto* metadata = reinterpret_cast<float*>(data.data() + metadata_offset);
    metadata[0] = thumbnail.max_amplitude;
    metadata[1] = thumbnail.average_rms;
    metadata[2] = thumbnail.dynamic_range_db;
    metadata[3] = 0.0f; // Reserved
    
    auto* flags = reinterpret_cast<uint8_t*>(data.data() + metadata_offset + sizeof(float) * 4);
    flags[0] = thumbnail.is_silent ? 1 : 0;
    flags[1] = thumbnail.is_clipped ? 1 : 0;
    
    return data;
}

std::shared_ptr<AudioThumbnail> deserialize_thumbnail(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(uint32_t) * 8) {
        return nullptr;
    }
    
    const auto* header = reinterpret_cast<const uint32_t*>(data.data());
    uint32_t version = header[0];
    
    if (version != 1) {
        return nullptr;
    }
    
    auto thumbnail = std::make_shared<AudioThumbnail>();
    thumbnail->size = static_cast<ThumbnailSize>(header[1]);
    thumbnail->total_duration = ve::TimePoint(header[2], header[3]);
    thumbnail->sample_rate = static_cast<int32_t>(header[4]);
    thumbnail->channel_count = static_cast<size_t>(header[5]);
    size_t width = header[6];
    bool has_rms = header[7] != 0;
    
    if (thumbnail->channel_count == 0 || width == 0) {
        return nullptr;
    }
    
    // Read source path
    size_t source_offset = sizeof(uint32_t) * 8;
    if (data.size() < source_offset + sizeof(uint32_t)) {
        return nullptr;
    }
    
    uint32_t source_size = *reinterpret_cast<const uint32_t*>(data.data() + source_offset);
    source_offset += sizeof(uint32_t);
    
    if (data.size() < source_offset + source_size) {
        return nullptr;
    }
    
    thumbnail->audio_source.assign(
        reinterpret_cast<const char*>(data.data() + source_offset),
        source_size
    );
    
    // Read peak data
    size_t peaks_offset = source_offset + source_size;
    size_t peaks_size = thumbnail->channel_count * width * sizeof(float);
    
    if (data.size() < peaks_offset + peaks_size) {
        return nullptr;
    }
    
    thumbnail->peak_data.resize(thumbnail->channel_count);
    const auto* peaks_data = reinterpret_cast<const float*>(data.data() + peaks_offset);
    
    for (size_t ch = 0; ch < thumbnail->channel_count; ++ch) {
        thumbnail->peak_data[ch].resize(width);
        std::memcpy(thumbnail->peak_data[ch].data(),
                   peaks_data + ch * width,
                   width * sizeof(float));
    }
    
    // Read RMS data if present
    size_t rms_offset = peaks_offset + peaks_size;
    if (has_rms) {
        size_t rms_size = thumbnail->channel_count * width * sizeof(float);
        
        if (data.size() < rms_offset + rms_size) {
            return nullptr;
        }
        
        thumbnail->rms_data.resize(thumbnail->channel_count);
        const auto* rms_data_ptr = reinterpret_cast<const float*>(data.data() + rms_offset);
        
        for (size_t ch = 0; ch < thumbnail->channel_count; ++ch) {
            thumbnail->rms_data[ch].resize(width);
            std::memcpy(thumbnail->rms_data[ch].data(),
                       rms_data_ptr + ch * width,
                       width * sizeof(float));
        }
        
        rms_offset += rms_size;
    }
    
    // Read metadata
    if (data.size() >= rms_offset + sizeof(float) * 4 + sizeof(uint8_t) * 2) {
        const auto* metadata = reinterpret_cast<const float*>(data.data() + rms_offset);
        thumbnail->max_amplitude = metadata[0];
        thumbnail->average_rms = metadata[1];
        thumbnail->dynamic_range_db = metadata[2];
        
        const auto* flags = reinterpret_cast<const uint8_t*>(data.data() + rms_offset + sizeof(float) * 4);
        thumbnail->is_silent = flags[0] != 0;
        thumbnail->is_clipped = flags[1] != 0;
    }
    
    thumbnail->generated_time = std::chrono::system_clock::now();
    
    return thumbnail;
}

std::vector<std::string> get_supported_audio_extensions() {
    return {
        ".wav", ".wave",
        ".mp3",
        ".m4a", ".aac",
        ".flac",
        ".ogg", ".oga",
        ".wma",
        ".aiff", ".aif",
        ".au", ".snd"
    };
}

bool is_supported_audio_file(const std::filesystem::path& file_path) {
    auto extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    auto supported = get_supported_audio_extensions();
    return std::find(supported.begin(), supported.end(), extension) != supported.end();
}

} // namespace thumbnail_utils

} // namespace ve::audio