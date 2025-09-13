// GPU Memory Optimizer Implementation
// Professional VRAM management for high-performance video editing

#include "gpu_memory_optimizer.hpp"
#include "graphics_device.hpp"
#include "graphics_device_bridge.hpp"
#include <algorithm>
#include <future>
#include <random>
#include <cmath>

namespace video_editor::gfx {

// ============================================================================
// IntelligentCache Implementation
// ============================================================================

IntelligentCache::IntelligentCache(const Config& config) 
    : config_(config), 
      optimization_thread_(&IntelligentCache::optimization_thread_func, this) {
    
    // Initialize statistics
    stats_ = {};
    
    // Reserve space for frame history
    frame_access_history_.reserve(1000);
    
    // Initialize access pattern analysis
    access_pattern_.pattern_type = AccessPattern::Type::Random;
    access_pattern_.confidence = 0.0f;
}

IntelligentCache::~IntelligentCache() {
    should_optimize_ = false;
    if (optimization_thread_.joinable()) {
        optimization_thread_.join();
    }
}

TextureHandle IntelligentCache::get_texture(uint64_t hash) {
    std::shared_lock lock(cache_mutex_);
    
    auto it = cache_entries_.find(hash);
    if (it != cache_entries_.end()) {
        // Cache hit - update access statistics
        auto& entry = *it->second;
        entry.last_access_time = std::chrono::steady_clock::now();
        entry.access_count++;
        entry.frame_last_used = current_frame_.load();
        
        ++stats_.cache_hits;
        stats_.update_hit_ratio();
        
        return entry.texture;
    }
    
    // Cache miss
    ++stats_.cache_misses;
    stats_.update_hit_ratio();
    
    return TextureHandle{};
}

bool IntelligentCache::put_texture(uint64_t hash, TextureHandle texture, float quality_score) {
    if (quality_score < config_.quality_threshold) {
        return false; // Don't cache low-quality textures
    }
    
    std::unique_lock lock(cache_mutex_);
    
    // Check if already cached
    if (cache_entries_.find(hash) != cache_entries_.end()) {
        return true;
    }
    
    // Create new cache entry
    auto entry = std::make_unique<CacheEntry>();
    entry->hash = hash;
    entry->texture = texture;
    entry->last_access_time = std::chrono::steady_clock::now();
    entry->creation_time = entry->last_access_time;
    entry->access_count = 1;
    entry->frame_last_used = current_frame_.load();
    entry->quality_score = quality_score;
    entry->memory_size = 1024 * 1024; // Default size, actual implementation would query GPU
    entry->is_critical = critical_hashes_.find(hash) != critical_hashes_.end();
    
    // Check if we need to evict entries
    size_t total_size = get_cache_size() + entry->memory_size;
    if (total_size > config_.max_cache_size || cache_entries_.size() >= config_.max_entries) {
        lock.unlock();
        ensure_free_memory(entry->memory_size);
        lock.lock();
    }
    
    // Add to cache
    cache_entries_[hash] = std::move(entry);
    stats_.used_vram += cache_entries_[hash]->memory_size;
    
    return true;
}

void IntelligentCache::remove_texture(uint64_t hash) {
    std::unique_lock lock(cache_mutex_);
    
    auto it = cache_entries_.find(hash);
    if (it != cache_entries_.end()) {
        stats_.used_vram -= it->second->memory_size;
        cache_entries_.erase(it);
    }
}

void IntelligentCache::mark_critical(uint64_t hash, bool critical) {
    std::unique_lock lock(cache_mutex_);
    
    if (critical) {
        critical_hashes_.insert(hash);
    } else {
        critical_hashes_.erase(hash);
    }
    
    // Update entry if it exists
    auto it = cache_entries_.find(hash);
    if (it != cache_entries_.end()) {
        it->second->is_critical = critical;
    }
}

void IntelligentCache::notify_frame_access(uint32_t frame_number) {
    current_frame_.store(frame_number);
    
    // Update frame access history for pattern analysis
    std::lock_guard lock(optimization_mutex_);
    frame_access_history_.push_back(frame_number);
    
    // Keep only recent history
    if (frame_access_history_.size() > 100) {
        frame_access_history_.erase(frame_access_history_.begin());
    }
    
    // Trigger pattern analysis
    update_access_patterns();
}

void IntelligentCache::update_access_patterns() {
    if (frame_access_history_.size() < 5) return;
    
    std::lock_guard lock(optimization_mutex_);
    
    // Analyze recent frame access pattern
    bool is_sequential = true;
    bool is_forward = true;
    bool is_backward = true;
    
    for (size_t i = 1; i < frame_access_history_.size(); ++i) {
        int32_t diff = static_cast<int32_t>(frame_access_history_[i]) - 
                      static_cast<int32_t>(frame_access_history_[i-1]);
        
        if (std::abs(diff) != 1) {
            is_sequential = false;
            break;
        }
        
        if (diff != 1) is_forward = false;
        if (diff != -1) is_backward = false;
    }
    
    // Update pattern type
    if (is_sequential && (is_forward || is_backward)) {
        access_pattern_.pattern_type = AccessPattern::Type::Sequential;
        access_pattern_.confidence = 0.9f;
    } else {
        // Check for burst pattern (multiple frames accessed quickly)
        // Note: Time-based analysis would be used in full implementation
        
        // Count recent accesses
        size_t recent_accesses = 0;
        for (size_t i = frame_access_history_.size(); i > 0; --i) {
            if (i < frame_access_history_.size()) {
                // Approximate time check (in real implementation, would track timestamps)
                ++recent_accesses;
                if (recent_accesses >= frame_access_history_.size() / 2) break;
            }
        }
        
        if (recent_accesses > 10) {
            access_pattern_.pattern_type = AccessPattern::Type::Burst;
            access_pattern_.confidence = 0.8f;
        } else {
            access_pattern_.pattern_type = AccessPattern::Type::Random;
            access_pattern_.confidence = 0.6f;
        }
    }
}

void IntelligentCache::predict_future_needs() {
    if (!config_.enable_prediction) return;
    
    std::lock_guard lock(optimization_mutex_);
    
    uint32_t current = current_frame_.load();
    
    switch (access_pattern_.pattern_type) {
        case AccessPattern::Type::Sequential: {
            // Predict linear progression
            bool is_forward = frame_access_history_.size() >= 2 &&
                frame_access_history_.back() > frame_access_history_[frame_access_history_.size()-2];
            
            for (uint32_t i = 1; i <= config_.prediction_lookahead; ++i) {
                uint32_t predicted_frame = is_forward ? current + i : current - i;
                predicted_frames_.push(predicted_frame);
            }
            break;
        }
        
        case AccessPattern::Type::Burst: {
            // Predict surrounding frames for burst access
            for (uint32_t i = 1; i <= config_.prediction_lookahead / 2; ++i) {
                predicted_frames_.push(current + i);
                if (current >= i) {
                    predicted_frames_.push(current - i);
                }
            }
            break;
        }
        
        case AccessPattern::Type::Random:
        default:
            // For random access, predict based on recently accessed frames
            std::unordered_set<uint32_t> recent_frames;
            for (auto frame : frame_access_history_) {
                recent_frames.insert(frame);
            }
            
            // Predict frames around recently accessed ones
            for (auto frame : recent_frames) {
                for (uint32_t offset = 1; offset <= 3; ++offset) {
                    predicted_frames_.push(frame + offset);
                    if (frame >= offset) {
                        predicted_frames_.push(frame - offset);
                    }
                }
            }
            break;
    }
}

void IntelligentCache::preload_likely_textures() {
    if (!config_.enable_prediction) return;
    
    std::lock_guard lock(optimization_mutex_);
    
    // Process predicted frames queue
    size_t preloaded_count = 0;
    const size_t max_preload = 10; // Limit preloading to avoid memory pressure
    
    while (!predicted_frames_.empty() && preloaded_count < max_preload) {
        uint32_t frame = predicted_frames_.front();
        predicted_frames_.pop();
        
        // Generate hash for this frame (simplified - real implementation would
        // consider different texture types, effects, etc.)
        uint64_t hash = generate_texture_hash(frame, "main");
        
        // Check if already cached or being preloaded
        if (cache_entries_.find(hash) == cache_entries_.end() &&
            preloaded_hashes_.find(hash) == preloaded_hashes_.end()) {
            
            preloaded_hashes_.insert(hash);
            ++preloaded_count;
            
            // In real implementation, would trigger async texture loading
            // For now, just mark as predicted needed
            auto it = cache_entries_.find(hash);
            if (it != cache_entries_.end()) {
                it->second->is_predicted_needed = true;
            }
        }
    }
}

bool IntelligentCache::ensure_free_memory(size_t required_bytes) {
    size_t current_size = get_cache_size();
    
    if (current_size + required_bytes <= config_.max_cache_size) {
        return true; // Already have enough space
    }
    
    size_t bytes_to_free = required_bytes > config_.max_cache_size ? 
                          config_.max_cache_size : required_bytes;
    size_t target_size = config_.max_cache_size - bytes_to_free;
    
    // If required_bytes is larger than max_cache_size, target 70% usage
    if (required_bytes >= config_.max_cache_size) {
        target_size = static_cast<size_t>(static_cast<double>(config_.max_cache_size) * 0.7);
    }
    
    evict_by_size(current_size - target_size);
    
    return get_cache_size() + required_bytes <= config_.max_cache_size;
}

void IntelligentCache::evict_by_size(size_t target_eviction_size) {
    std::unique_lock lock(cache_mutex_);
    
    // Get eviction candidates sorted by priority (lowest first)
    std::vector<std::pair<float, uint64_t>> candidates;
    uint32_t current = current_frame_.load();
    
    for (const auto& pair : cache_entries_) {
        const auto& entry = *pair.second;
        if (!entry.is_critical && entry.reference_count == 0) {
            float priority = entry.calculate_priority(current);
            candidates.emplace_back(priority, pair.first);
        }
    }
    
    // Sort by priority (ascending - lowest priority first)
    std::sort(candidates.begin(), candidates.end());
    
    // Evict textures until we reach target
    size_t evicted_size = 0;
    for (const auto& candidate : candidates) {
        if (evicted_size >= target_eviction_size) break;
        
        uint64_t hash = candidate.second;
        auto it = cache_entries_.find(hash);
        if (it != cache_entries_.end()) {
            evicted_size += it->second->memory_size;
            stats_.used_vram -= it->second->memory_size;
            cache_entries_.erase(it);
        }
    }
}

size_t IntelligentCache::get_cache_size() const {
    std::shared_lock lock(cache_mutex_);
    
    size_t total_size = 0;
    for (const auto& pair : cache_entries_) {
        total_size += pair.second->memory_size;
    }
    return total_size;
}

uint64_t IntelligentCache::generate_texture_hash(uint32_t frame, const std::string& identifier) const {
    // Simple hash generation - in real implementation would use more sophisticated hashing
    std::hash<std::string> hasher;
    return hasher(std::to_string(frame) + "_" + identifier);
}

void IntelligentCache::optimization_thread_func() {
    while (should_optimize_.load()) {
        {
            std::lock_guard lock(optimization_mutex_);
            
            // Update entry scores
            update_entry_scores();
            
            // Predict future needs
            predict_future_needs();
            
            // Preload likely textures
            preload_likely_textures();
            
            // Compress eligible textures
            compress_eligible_textures();
            
            // Cleanup expired entries
            cleanup_expired_entries();
        }
        
        // Sleep for optimization interval
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void IntelligentCache::update_entry_scores() {
    uint32_t current = current_frame_.load();
    
    for (auto& pair : cache_entries_) {
        auto& entry = *pair.second;
        entry.quality_score = entry.calculate_priority(current);
    }
}

void IntelligentCache::compress_eligible_textures() {
    if (!config_.enable_compression) return;
    
    // Find large, infrequently accessed textures for compression
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : cache_entries_) {
        auto& entry = *pair.second;
        
        // Criteria for compression:
        // 1. Not compressed already
        // 2. Large texture (>1MB)
        // 3. Not accessed recently (>5 seconds)
        // 4. Not critical
        if (!entry.is_compressed && 
            entry.memory_size > 1024 * 1024 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - entry.last_access_time).count() > 5 &&
            !entry.is_critical) {
            
            // Mark for compression (actual compression would happen in background)
            entry.is_compressed = true;
            // In real implementation: entry.texture = compress_texture(entry.texture);
        }
    }
}

void IntelligentCache::cleanup_expired_entries() {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> expired_hashes;
    
    for (const auto& pair : cache_entries_) {
        const auto& entry = *pair.second;
        
        // Remove entries not accessed for a long time and not critical
        auto time_since_access = std::chrono::duration_cast<std::chrono::minutes>(
            now - entry.last_access_time).count();
        
        if (time_since_access > 30 && !entry.is_critical && entry.reference_count == 0) {
            expired_hashes.push_back(pair.first);
        }
    }
    
    // Remove expired entries
    for (uint64_t hash : expired_hashes) {
        remove_texture(hash);
    }
}

MemoryStats IntelligentCache::get_statistics() const {
    std::shared_lock lock(cache_mutex_);
    return stats_;
}

void IntelligentCache::force_cleanup() {
    std::unique_lock lock(cache_mutex_);
    
    // Force immediate cleanup of all non-critical entries
    auto it = cache_entries_.begin();
    while (it != cache_entries_.end()) {
        if (!it->second->is_critical) {
            it = cache_entries_.erase(it);
        } else {
            ++it;
        }
    }
}

void IntelligentCache::trigger_garbage_collection() {
    std::unique_lock lock(cache_mutex_);
    
    // Trigger aggressive garbage collection
    // Note: cleanup_expired_entries is private, so we'll implement cleanup here
    auto now = std::chrono::steady_clock::now();
    auto it = cache_entries_.begin();
    while (it != cache_entries_.end()) {
        // Remove expired entries (older than 60 seconds without access)
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->last_access_time);
        if (age.count() > 60 && !it->second->is_critical) {
            it = cache_entries_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Remove least recently used entries if still over memory limit
    if (get_cache_size() > config_.max_cache_size) {
        size_t target_eviction = get_cache_size() - config_.max_cache_size;
        evict_by_size(target_eviction);
    }
}

// ============================================================================
// TextureCompression Implementation
// ============================================================================

TextureCompression::TextureCompression(GraphicsDevice* device) : device_(device) {
    initialize_default_profiles();
}

TextureHandle TextureCompression::compress_for_cache(TextureHandle input, CompressionLevel level) {
    if (level == CompressionLevel::None) {
        return input;
    }
    
    // Default to RGBA8 format for placeholder implementation
    TextureFormat input_format = TextureFormat::RGBA8;
    CompressionInfo best_compression = find_best_compression(input_format, level);
    
    if (best_compression.compressed_format == input_format) {
        return input; // No suitable compression found
    }
    
    // In real implementation, would perform actual texture compression
    // For now, return a placeholder that represents compressed texture
    return input; // Placeholder
}

float TextureCompression::get_compression_ratio(TextureFormat format, CompressionLevel level) const {
    std::shared_lock lock(profiles_mutex_);
    
    auto it = compression_profiles_.find(format);
    if (it != compression_profiles_.end()) {
        for (const auto& profile : it->second) {
            if (profile.level == level) {
                return profile.compression_ratio;
            }
        }
    }
    
    return 1.0f; // No compression
}

void TextureCompression::initialize_default_profiles() {
    // Add compression profiles for common formats
    
    // RGBA8 compression options
    CompressionInfo bc1_profile{};
    bc1_profile.level = CompressionLevel::Fast;
    bc1_profile.original_format = TextureFormat::RGBA8;
    bc1_profile.compressed_format = TextureFormat::R8; // Placeholder for BC1
    bc1_profile.compression_ratio = 6.0f;
    bc1_profile.is_lossy = true;
    
    CompressionInfo bc7_profile{};
    bc7_profile.level = CompressionLevel::Balanced;
    bc7_profile.original_format = TextureFormat::RGBA8;
    bc7_profile.compressed_format = TextureFormat::R32F; // Placeholder for BC7
    bc7_profile.compression_ratio = 4.0f;
    bc7_profile.is_lossy = true;
    
    std::unique_lock lock(profiles_mutex_);
    compression_profiles_[TextureFormat::RGBA8] = {bc1_profile, bc7_profile};
}

TextureCompression::CompressionInfo TextureCompression::find_best_compression(TextureFormat format, CompressionLevel level) const {
    std::shared_lock lock(profiles_mutex_);
    
    auto it = compression_profiles_.find(format);
    if (it != compression_profiles_.end()) {
        for (const auto& profile : it->second) {
            if (profile.level == level) {
                return profile;
            }
        }
    }
    
    // Return no-compression profile
    CompressionInfo no_compression{};
    no_compression.level = CompressionLevel::None;
    no_compression.original_format = format;
    no_compression.compressed_format = format;
    no_compression.compression_ratio = 1.0f;
    return no_compression;
}

// ============================================================================
// GPUMemoryOptimizer Implementation
// ============================================================================

GPUMemoryOptimizer::GPUMemoryOptimizer(GraphicsDevice* device, const OptimizerConfig& config)
    : device_(device), config_(config) {
    
    // Initialize components
    cache_ = std::make_unique<IntelligentCache>(config_.cache_config);
    compression_ = std::make_unique<TextureCompression>(device_);
    streaming_ = std::make_unique<StreamingOptimizer>(cache_.get(), device_, config_.streaming_config);
    
    // Setup VRAM monitor callbacks
    vram_monitor_.on_memory_pressure_changed = [this](float pressure) {
        handle_memory_pressure(pressure);
    };
    
    // Start monitoring thread
    if (config_.enable_background_optimization) {
        monitoring_thread_ = std::thread(&GPUMemoryOptimizer::monitoring_thread_func, this);
    }
}

GPUMemoryOptimizer::~GPUMemoryOptimizer() {
    should_monitor_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

TextureHandle GPUMemoryOptimizer::get_texture(uint64_t hash) {
    return cache_->get_texture(hash);
}

bool GPUMemoryOptimizer::cache_texture(uint64_t hash, TextureHandle texture, float quality) {
    return cache_->put_texture(hash, texture, quality);
}

void GPUMemoryOptimizer::notify_frame_change(uint32_t new_frame) {
    cache_->notify_frame_access(new_frame);
}

bool GPUMemoryOptimizer::ensure_memory_available(size_t required_bytes) {
    // Update VRAM status
    vram_monitor_.update_from_device(device_);
    
    if (vram_monitor_.available_vram >= required_bytes) {
        return true;
    }
    
    // Try cache cleanup
    cache_->ensure_free_memory(required_bytes);
    
    // Check again
    vram_monitor_.update_from_device(device_);
    return vram_monitor_.available_vram >= required_bytes;
}

void GPUMemoryOptimizer::monitoring_thread_func() {
    while (should_monitor_.load()) {
        // Update VRAM statistics
        vram_monitor_.update_from_device(device_);
        
        // Check for memory pressure and trigger cleanup if needed
        vram_monitor_.trigger_cleanup_if_needed(cache_.get());
        
        // Optimize based on current access patterns
        auto pattern = cache_->get_current_pattern();
        switch (pattern) {
            case AccessPattern::Type::Sequential:
                optimize_for_realtime_playback();
                break;
            case AccessPattern::Type::Random:
                optimize_for_scrubbing();
                break;
            case AccessPattern::Type::Predictable:
                optimize_for_scrubbing(); // Use scrubbing optimization for predictable patterns
                break;
            case AccessPattern::Type::Burst:
                optimize_for_rendering();
                break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.optimization_interval_ms));
    }
}

void GPUMemoryOptimizer::handle_memory_pressure(float pressure) {
    if (pressure > 0.9f) {
        // Critical memory pressure - aggressive cleanup
        cache_->force_cleanup();
        cache_->trigger_garbage_collection();
    } else if (pressure > 0.75f) {
        // High memory pressure - moderate cleanup
        cache_->ensure_free_memory(128ULL * 1024 * 1024); // Free 128MB
    }
}

MemoryStats GPUMemoryOptimizer::get_memory_statistics() const {
    return cache_->get_statistics();
}

void GPUMemoryOptimizer::optimize_for_realtime_playback() {
    // Optimize cache for real-time playback
    if (cache_) {
        // Increase cache size for smoother playback
        // Prioritize sequential frame access patterns
        cache_->update_access_patterns();
        cache_->predict_future_needs();
    }
}

void GPUMemoryOptimizer::optimize_for_scrubbing() {
    // Optimize cache for scrubbing operations
    if (cache_) {
        // Prepare for random access patterns
        // Keep more frames in memory for quick seeking
        // Note: Cannot call private cleanup_expired_entries, so trigger public cleanup instead
        cache_->trigger_garbage_collection();
    }
}

void GPUMemoryOptimizer::optimize_for_rendering() {
    // Optimize cache for rendering operations
    if (cache_) {
        // Free up memory for render targets
        // Compress textures more aggressively
        cache_->trigger_garbage_collection();
        cache_->force_cleanup();
    }
}

// ============================================================================
// StreamingOptimizer Implementation
// ============================================================================

StreamingOptimizer::StreamingOptimizer(IntelligentCache* cache, GraphicsDevice* device, 
                                      const StreamingConfig& config)
    : config_(config), cache_(cache), device_(device) {
    // Initialize streaming state
    is_streaming_.store(false);
    current_playhead_.store(0);
    stats_ = {};
    last_stats_update_ = std::chrono::steady_clock::now();
}

StreamingOptimizer::~StreamingOptimizer() {
    stop_streaming();
}

void StreamingOptimizer::start_streaming(uint32_t start_frame) {
    std::lock_guard<std::mutex> lock(streaming_mutex_);
    
    if (is_streaming_.load()) {
        return; // Already streaming
    }
    
    current_playhead_.store(start_frame);
    is_streaming_.store(true);
    
    // Start loader threads
    for (uint32_t i = 0; i < config_.max_concurrent_loads; ++i) {
        loader_threads_.emplace_back(&StreamingOptimizer::loader_thread_func, this);
    }
}

void StreamingOptimizer::stop_streaming() {
    is_streaming_.store(false);
    
    // Wait for all loader threads to finish
    for (auto& thread : loader_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    loader_threads_.clear();
    
    // Clear loading queue
    std::lock_guard<std::mutex> lock(streaming_mutex_);
    while (!loading_queue_.empty()) {
        loading_queue_.pop();
    }
}

void StreamingOptimizer::seek_to_frame(uint32_t frame) {
    current_playhead_.store(frame);
    // Clear existing queue and prioritize new frame
    std::lock_guard<std::mutex> lock(streaming_mutex_);
    while (!loading_queue_.empty()) {
        loading_queue_.pop();
    }
    // Add immediate frames to queue
    for (uint32_t i = 0; i < config_.read_ahead_frames; ++i) {
        loading_queue_.push(frame + i);
    }
}

void StreamingOptimizer::set_playback_speed(float speed) {
    // Adjust read-ahead based on playback speed
    std::lock_guard<std::mutex> lock(streaming_mutex_);
    // Higher speeds need more read-ahead
    config_.read_ahead_frames = static_cast<uint32_t>(30 * speed);
}

void StreamingOptimizer::analyze_access_patterns() {
    // Implementation for pattern analysis
    // This would analyze recent frame access patterns
}

void StreamingOptimizer::adjust_cache_size_dynamically() {
    // Implementation for dynamic cache adjustment
    // This would adjust cache size based on usage patterns
}

void StreamingOptimizer::prioritize_critical_textures() {
    // Implementation for texture prioritization
    // This would mark important textures as critical
}

void StreamingOptimizer::optimize_for_playback_mode(bool is_realtime) {
    if (is_realtime) {
        // Optimize for real-time playback
        config_.read_ahead_frames = 60; // More aggressive read-ahead
        config_.load_threshold = 0.5f;  // Start loading earlier
    } else {
        // Optimize for scrubbing/editing
        config_.read_ahead_frames = 10; // Less read-ahead
        config_.load_threshold = 0.8f;  // Start loading later
    }
}

void StreamingOptimizer::update_config(const StreamingConfig& new_config) {
    std::lock_guard<std::mutex> lock(streaming_mutex_);
    config_ = new_config;
}

StreamingOptimizer::StreamingStats StreamingOptimizer::get_statistics() const {
    std::lock_guard<std::mutex> lock(streaming_mutex_);
    return stats_;
}

bool StreamingOptimizer::is_buffer_healthy() const {
    return stats_.buffer_utilization > 0.3f && !stats_.is_underrun;
}

void StreamingOptimizer::loader_thread_func() {
    while (is_streaming_.load()) {
        uint32_t frame_to_load = 0;
        bool has_work = false;
        
        {
            std::lock_guard<std::mutex> lock(streaming_mutex_);
            if (!loading_queue_.empty()) {
                frame_to_load = loading_queue_.front();
                loading_queue_.pop();
                has_work = true;
            }
        }
        
        if (has_work) {
            load_frame_async(frame_to_load);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void StreamingOptimizer::load_frame_async(uint32_t frame) {
    // Implementation for asynchronous frame loading
    // This would load the texture for the specified frame
    
    // Ensure device is available for operations
    if (!device_) {
        return;
    }
    
    // Generate hash for the frame
    std::hash<uint32_t> hasher;
    uint64_t hash = hasher(frame);
    
    // Check if already cached
    if (cache_->get_texture(hash).is_valid()) {
        ++stats_.cache_hits;
        return;
    }
    
    ++stats_.cache_misses;
    // In real implementation, would load the actual texture
    // For now, just update statistics
    ++stats_.frames_streamed;
}

void StreamingOptimizer::update_loading_priorities() {
    // Implementation for priority updates
    // This would reorder the loading queue based on playhead position
}

void StreamingOptimizer::adjust_quality_based_on_performance() {
    // Implementation for quality adjustment
    // This would lower quality if performance is poor
}

// ============================================================================
// VRAMMonitor Implementation
// ============================================================================

void VRAMMonitor::update_from_device(GraphicsDevice* device) {
    if (!device) return;
    
    // Simulate VRAM information since we don't have actual device implementation
    // In real implementation, this would query the graphics device
    total_vram = 4ULL * 1024 * 1024 * 1024; // 4GB simulated
    used_vram = static_cast<size_t>(static_cast<float>(total_vram) * 0.6f); // 60% used
    available_vram = total_vram - used_vram;
    
    // Calculate memory pressure
    float old_pressure = memory_pressure;
    memory_pressure = static_cast<float>(used_vram) / static_cast<float>(total_vram);
    
    // Calculate fragmentation (simplified estimation)
    calculate_fragmentation();
    
    // Trigger callbacks if pressure changed significantly
    if (std::abs(memory_pressure - old_pressure) > 0.05f && on_memory_pressure_changed) {
        on_memory_pressure_changed(memory_pressure);
    }
    
    // Trigger warning/critical callbacks
    if (memory_pressure > thresholds.critical_threshold && on_memory_critical) {
        on_memory_critical();
    } else if (memory_pressure > thresholds.warning_threshold && on_memory_warning) {
        on_memory_warning();
    }
}

void VRAMMonitor::trigger_cleanup_if_needed(IntelligentCache* cache) {
    if (!cache) return;
    
    if (memory_pressure > thresholds.cleanup_threshold) {
        // Calculate how much memory to free
        size_t target_usage = static_cast<size_t>(static_cast<float>(total_vram) * (thresholds.cleanup_threshold - 0.1f));
        size_t memory_to_free = used_vram > target_usage ? used_vram - target_usage : 0;
        
        if (memory_to_free > 0) {
            cache->ensure_free_memory(memory_to_free);
        }
    }
    
    // Always ensure minimum free memory
    if (available_vram < thresholds.min_free_bytes) {
        size_t additional_free_needed = thresholds.min_free_bytes - available_vram;
        cache->ensure_free_memory(additional_free_needed);
    }
}

void VRAMMonitor::calculate_fragmentation() {
    // Simple fragmentation estimation
    // In real implementation, this would analyze memory layout
    fragmentation_ratio = memory_pressure * 0.1f; // Simplified calculation
}

bool VRAMMonitor::is_memory_available(size_t required_bytes) const {
    return available_vram >= required_bytes;
}

float VRAMMonitor::get_usage_ratio() const {
    return memory_pressure;
}

} // namespace video_editor::gfx
