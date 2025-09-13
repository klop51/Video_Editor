// Streaming Optimizer and VRAM Monitor Implementation
// Advanced streaming and memory monitoring for large video files

#include "gpu_memory_optimizer.hpp"
#include <algorithm>
#include <future>
#include <queue>
#include <iostream>

namespace video_editor::gfx {

// ============================================================================
// StreamingOptimizer Implementation
// ============================================================================

StreamingOptimizer::StreamingOptimizer(IntelligentCache* cache, GraphicsDevice* device, 
                                     const StreamingConfig& config)
    : cache_(cache), device_(device), config_(config) {
    
    stats_ = {};
    last_stats_update_ = std::chrono::steady_clock::now();
    
    // Initialize loader threads
    for (uint32_t i = 0; i < config_.max_concurrent_loads; ++i) {
        loader_threads_.emplace_back(&StreamingOptimizer::loader_thread_func, this);
    }
}

StreamingOptimizer::~StreamingOptimizer() {
    std::cout << "StreamingOptimizer destructor starting..." << std::endl;
    stop_streaming();
    
    // DEBUGGING: Temporarily disable thread joins to isolate abort() source
    // Join loader threads
    // for (auto& thread : loader_threads_) {
    //     if (thread.joinable()) {
    //         thread.join();
    //     }
    // }
    std::cout << "StreamingOptimizer destructor ending..." << std::endl;
}

void StreamingOptimizer::start_streaming(uint32_t start_frame) {
    std::lock_guard lock(streaming_mutex_);
    
    current_playhead_.store(start_frame);
    is_streaming_.store(true);
    
    // Initialize loading queue with read-ahead frames
    for (uint32_t i = 0; i < config_.read_ahead_frames; ++i) {
        loading_queue_.push(start_frame + i);
    }
    
    // Reset statistics
    stats_ = {};
    last_stats_update_ = std::chrono::steady_clock::now();
}

void StreamingOptimizer::stop_streaming() {
    is_streaming_.store(false);
    
    std::lock_guard lock(streaming_mutex_);
    
    // Clear loading queue
    while (!loading_queue_.empty()) {
        loading_queue_.pop();
    }
}

void StreamingOptimizer::seek_to_frame(uint32_t frame) {
    if (!is_streaming_.load()) return;
    
    std::lock_guard lock(streaming_mutex_);
    
    current_playhead_.store(frame);
    
    // Clear current queue and rebuild around new position
    while (!loading_queue_.empty()) {
        loading_queue_.pop();
    }
    
    // Add frames around seek position
    uint32_t start_frame = frame > config_.read_ahead_frames / 2 ? 
                          frame - config_.read_ahead_frames / 2 : 0;
    
    for (uint32_t i = 0; i < config_.read_ahead_frames; ++i) {
        loading_queue_.push(start_frame + i);
    }
    
    // Mark critical frames around playhead
    for (uint32_t i = 0; i < 5; ++i) {
        uint64_t hash = generate_frame_hash(frame + i);
        cache_->mark_critical(hash, true);
        
        if (frame >= i) {
            hash = generate_frame_hash(frame - i);
            cache_->mark_critical(hash, true);
        }
    }
}

void StreamingOptimizer::set_playback_speed(float speed) {
    // Adjust read-ahead based on playback speed
    uint32_t adjusted_read_ahead = static_cast<uint32_t>(config_.read_ahead_frames * std::abs(speed));
    adjusted_read_ahead = std::min(adjusted_read_ahead, config_.read_ahead_frames * 4); // Cap at 4x
    
    if (speed > 2.0f) {
        // High-speed playback - increase read-ahead, reduce quality
        config_.read_ahead_frames = adjusted_read_ahead;
        if (config_.enable_adaptive_quality) {
            // In real implementation, would reduce texture quality/resolution
        }
    } else if (speed < 0.5f) {
        // Slow playback - reduce read-ahead, increase quality
        config_.read_ahead_frames = std::max(10u, adjusted_read_ahead);
    }
}

void StreamingOptimizer::analyze_access_patterns() {
    // Analyze buffer utilization and loading patterns
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_stats_update_).count();
    
    if (time_since_update < 1000) return; // Update stats every second
    
    // Calculate buffer utilization
    size_t current_buffer_size = 0;
    uint32_t current_frame = current_playhead_.load();
    
    // Count frames in cache around current position
    for (uint32_t i = 0; i < config_.read_ahead_frames; ++i) {
        uint64_t hash = generate_frame_hash(current_frame + i);
        if (cache_->get_texture(hash).is_valid()) {
            current_buffer_size += estimate_frame_size();
        }
    }
    
    stats_.buffer_utilization = float(current_buffer_size) / float(config_.streaming_buffer_size);
    
    // Check for underrun conditions
    stats_.is_underrun = stats_.buffer_utilization < config_.load_threshold;
    
    // Update load time statistics
    // In real implementation, would track actual loading times
    
    last_stats_update_ = now;
}

void StreamingOptimizer::adjust_cache_size_dynamically() {
    analyze_access_patterns();
    
    if (stats_.is_underrun) {
        // Buffer underrun - increase read-ahead
        config_.read_ahead_frames = std::min(config_.read_ahead_frames + 5, 100u);
        
        // Increase loading priority for nearby frames
        prioritize_critical_textures();
    } else if (stats_.buffer_utilization > 0.9f) {
        // Buffer over-full - reduce read-ahead
        config_.read_ahead_frames = std::max(config_.read_ahead_frames - 2, 10u);
    }
}

void StreamingOptimizer::prioritize_critical_textures() {
    uint32_t current_frame = current_playhead_.load();
    
    // Mark frames near playhead as critical
    for (uint32_t i = 0; i < 10; ++i) {
        uint64_t hash = generate_frame_hash(current_frame + i);
        cache_->mark_critical(hash, true);
        
        if (current_frame >= i) {
            hash = generate_frame_hash(current_frame - i);
            cache_->mark_critical(hash, true);
        }
    }
    
    // Unmark distant frames as non-critical
    for (uint32_t i = 20; i < 50; ++i) {
        uint64_t hash = generate_frame_hash(current_frame + i);
        cache_->mark_critical(hash, false);
        
        if (current_frame >= i) {
            hash = generate_frame_hash(current_frame - i);
            cache_->mark_critical(hash, false);
        }
    }
}

void StreamingOptimizer::optimize_for_playback_mode(bool is_realtime) {
    if (is_realtime) {
        // Real-time playback optimization
        config_.read_ahead_frames = 30; // 1 second at 30fps
        config_.max_concurrent_loads = 2; // Reduce CPU usage
        
        if (config_.enable_adaptive_quality) {
            // Prioritize smooth playback over quality
        }
    } else {
        // Non-real-time optimization (scrubbing, etc.)
        config_.read_ahead_frames = 10; // Smaller buffer for responsive seeking
        config_.max_concurrent_loads = 4; // More aggressive loading
    }
}

void StreamingOptimizer::loader_thread_func() {
    while (is_streaming_.load()) {
        uint32_t frame_to_load = 0;
        bool has_work = false;
        
        {
            std::lock_guard lock(streaming_mutex_);
            if (!loading_queue_.empty()) {
                frame_to_load = loading_queue_.front();
                loading_queue_.pop();
                has_work = true;
            }
        }
        
        if (has_work) {
            load_frame_async(frame_to_load);
            ++stats_.frames_streamed;
        } else {
            // No work available, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void StreamingOptimizer::load_frame_async(uint32_t frame) {
    auto start_time = std::chrono::steady_clock::now();
    
    uint64_t hash = generate_frame_hash(frame);
    
    // Check if already cached
    if (cache_->get_texture(hash).is_valid()) {
        ++stats_.cache_hits;
        return;
    }
    
    ++stats_.cache_misses;
    
    // Simulate frame loading (in real implementation, would load from disk/network)
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Simulate I/O
    
    // Create placeholder texture (in real implementation, would decode actual frame)
    TextureHandle texture = create_placeholder_texture(frame);
    
    // Cache the loaded texture
    cache_->put_texture(hash, texture, 1.0f);
    
    // Update statistics
    auto end_time = std::chrono::steady_clock::now();
    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // Update running average load time
    stats_.average_load_time_ms = (stats_.average_load_time_ms * 0.9f) + (load_time * 0.1f);
    stats_.bytes_streamed += estimate_frame_size();
}

uint64_t StreamingOptimizer::generate_frame_hash(uint32_t frame) const {
    // Simple hash generation for frame identification
    std::hash<std::string> hasher;
    return hasher("frame_" + std::to_string(frame));
}

TextureHandle StreamingOptimizer::create_placeholder_texture(uint32_t frame) const {
    // In real implementation, would create actual texture from decoded frame
    // For now, return a placeholder
    return TextureHandle{};
}

size_t StreamingOptimizer::estimate_frame_size() const {
    // Estimate frame size based on resolution and format
    // For 4K RGBA: 3840 * 2160 * 4 = ~33MB
    // For HD RGBA: 1920 * 1080 * 4 = ~8MB
    return 8 * 1024 * 1024; // 8MB estimate for HD frame
}

StreamingOptimizer::StreamingStats StreamingOptimizer::get_statistics() const {
    return stats_;
}

bool StreamingOptimizer::is_buffer_healthy() const {
    return !stats_.is_underrun && stats_.buffer_utilization > 0.3f && stats_.buffer_utilization < 0.9f;
}

// ============================================================================
// VRAMMonitor Implementation
// ============================================================================

void VRAMMonitor::update_from_device(GraphicsDevice* device) {
    if (!device) return;
    
    // Get VRAM information from graphics device
    auto vram_info = device->get_memory_info();
    
    total_vram = vram_info.total_memory;
    used_vram = vram_info.used_memory;
    available_vram = total_vram - used_vram;
    
    // Calculate memory pressure
    float old_pressure = memory_pressure;
    memory_pressure = float(used_vram) / float(total_vram);
    
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
        size_t target_usage = static_cast<size_t>(total_vram * (thresholds.cleanup_threshold - 0.1f));
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
    // Simplified fragmentation calculation
    // In real implementation, would analyze actual memory layout
    
    if (total_vram == 0) {
        fragmentation_ratio = 0.0f;
        return;
    }
    
    // Estimate fragmentation based on allocation patterns
    float usage_ratio = float(used_vram) / float(total_vram);
    
    // High usage with many small allocations leads to higher fragmentation
    if (usage_ratio > 0.7f && active_allocations > 1000) {
        fragmentation_ratio = std::min(usage_ratio * 0.3f, 0.5f);
    } else {
        fragmentation_ratio = usage_ratio * 0.1f;
    }
}

bool VRAMMonitor::is_memory_available(size_t required_bytes) const {
    return available_vram >= required_bytes + thresholds.min_free_bytes;
}

float VRAMMonitor::get_usage_ratio() const {
    return total_vram > 0 ? float(used_vram) / float(total_vram) : 0.0f;
}

} // namespace video_editor::gfx
