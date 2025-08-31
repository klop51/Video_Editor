// Week 8: Memory Aware Uploader Implementation
// Enhanced texture uploader with memory pressure coordination

#include "gfx/memory_aware_uploader.hpp"
#include <algorithm>
#include <cmath>
#include <thread>

namespace ve::gfx {

MemoryAwareUploader::MemoryAwareUploader(StreamingTextureUploader* base_uploader,
                                       GPUMemoryManager* memory_manager,
                                       const Config& config)
    : config_(config)
    , base_uploader_(base_uploader)
    , memory_manager_(memory_manager)
    , memory_aware_mode_enabled_(true)
    , last_memory_check_time_(std::chrono::steady_clock::now())
    , last_optimization_time_(std::chrono::steady_clock::now())
{
    // Initialize statistics
    memory_aware_stats_ = MemoryAwareStats{};
    
    // Start memory monitoring thread if enabled
    if (config_.enable_continuous_monitoring) {
        monitoring_thread_ = std::thread(&MemoryAwareUploader::memory_monitoring_thread, this);
    }
    
    VE_LOG_INFO("Memory Aware Uploader initialized with monitoring: {}",
                config_.enable_continuous_monitoring);
}

MemoryAwareUploader::~MemoryAwareUploader() {
    shutdown_requested_.store(true);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    VE_LOG_INFO("Memory Aware Uploader shutdown complete");
}

std::future<TextureHandle> MemoryAwareUploader::queue_memory_aware_upload(MemoryAwareUploadJob job) {
    if (!memory_aware_mode_enabled_.load()) {
        // Fallback to direct upload
        StreamingUploadJob basic_job{};
        basic_job.image_data = job.image_data;
        basic_job.width = job.width;
        basic_job.height = job.height;
        basic_job.format = job.format;
        basic_job.priority = static_cast<UploadPriority>(job.priority);
        
        return base_uploader_->queue_upload(std::move(basic_job));
    }
    
    // Memory-aware upload processing
    auto promise = std::make_shared<std::promise<TextureHandle>>();
    auto future = promise->get_future();
    
    // Check memory pressure and optimize job
    check_memory_pressure_and_optimize(job);
    
    // Execute upload with memory awareness
    execute_memory_aware_upload(promise, std::move(job));
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        memory_aware_stats_.total_memory_aware_uploads++;
    }
    
    return future;
}

bool MemoryAwareUploader::check_memory_pressure() {
    if (!memory_manager_) {
        return false;
    }
    
    try {
        auto memory_stats = memory_manager_->get_memory_stats();
        float memory_pressure = memory_stats.memory_pressure;
        
        // Update memory pressure history
        {
            std::lock_guard<std::mutex> lock(pressure_history_mutex_);
            memory_pressure_history_.push_back({
                std::chrono::steady_clock::now(),
                memory_pressure
            });
            
            // Keep only recent history
            auto cutoff_time = std::chrono::steady_clock::now() - 
                               std::chrono::seconds(config_.memory_pressure_history_seconds);
            
            memory_pressure_history_.erase(
                std::remove_if(memory_pressure_history_.begin(), memory_pressure_history_.end(),
                    [cutoff_time](const auto& entry) {
                        return entry.first < cutoff_time;
                    }),
                memory_pressure_history_.end()
            );
        }
        
        // Check if memory pressure exceeds threshold
        bool high_pressure = memory_pressure > config_.memory_pressure_threshold;
        
        if (high_pressure) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            memory_aware_stats_.memory_pressure_events++;
        }
        
        return high_pressure;
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Memory pressure check failed: {}", e.what());
        return false;
    }
}

float MemoryAwareUploader::get_current_memory_pressure() const {
    if (!memory_manager_) {
        return 0.0f;
    }
    
    try {
        auto memory_stats = memory_manager_->get_memory_stats();
        return memory_stats.memory_pressure;
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to get memory pressure: {}", e.what());
        return 0.0f;
    }
}

bool MemoryAwareUploader::optimize_memory_usage() {
    if (!memory_manager_) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_optimization = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_optimization_time_).count();
    
    // Check if enough time has passed since last optimization
    if (time_since_last_optimization < config_.optimization_cooldown_ms) {
        return false;
    }
    
    VE_LOG_DEBUG("Starting memory usage optimization");
    
    bool optimizations_applied = false;
    
    try {
        auto memory_stats = memory_manager_->get_memory_stats();
        
        // If memory pressure is high, try to free memory
        if (memory_stats.memory_pressure > config_.memory_pressure_threshold) {
            size_t freed_mb = memory_manager_->cleanup_unused_textures();
            
            if (freed_mb > 0) {
                VE_LOG_INFO("Memory optimization freed {} MB", freed_mb);
                optimizations_applied = true;
                
                std::lock_guard<std::mutex> lock(stats_mutex_);
                memory_aware_stats_.memory_optimizations_triggered++;
                memory_aware_stats_.total_memory_freed_mb += freed_mb;
            }
        }
        
        // Update optimization time
        last_optimization_time_ = now;
        
        if (optimizations_applied) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            memory_aware_stats_.successful_optimizations++;
        }
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Memory optimization failed: {}", e.what());
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        memory_aware_stats_.failed_optimizations++;
        
        return false;
    }
    
    return optimizations_applied;
}

void MemoryAwareUploader::set_memory_pressure_threshold(float threshold) {
    config_.memory_pressure_threshold = std::clamp(threshold, 0.0f, 1.0f);
    VE_LOG_INFO("Memory pressure threshold updated to {:.1f}%", threshold * 100.0f);
}

MemoryAwareStats MemoryAwareUploader::get_memory_aware_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return memory_aware_stats_;
}

void MemoryAwareUploader::reset_memory_aware_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    memory_aware_stats_ = MemoryAwareStats{};
    VE_LOG_DEBUG("Memory aware statistics reset");
}

std::vector<std::pair<std::chrono::steady_clock::time_point, float>> 
MemoryAwareUploader::get_memory_pressure_history() const {
    std::lock_guard<std::mutex> lock(pressure_history_mutex_);
    return memory_pressure_history_;
}

void MemoryAwareUploader::update_config(const Config& new_config) {
    config_ = new_config;
    VE_LOG_INFO("Memory Aware Uploader configuration updated");
}

std::vector<std::string> MemoryAwareUploader::get_memory_recommendations() const {
    std::vector<std::string> recommendations;
    
    try {
        if (!memory_manager_) {
            recommendations.push_back("Memory manager not available");
            return recommendations;
        }
        
        auto memory_stats = memory_manager_->get_memory_stats();
        auto stats = get_memory_aware_stats();
        
        // High memory pressure
        if (memory_stats.memory_pressure > 0.8f) {
            recommendations.push_back("Memory pressure is very high (>80%). Consider reducing texture quality or freeing unused textures.");
        } else if (memory_stats.memory_pressure > 0.6f) {
            recommendations.push_back("Memory pressure is elevated (>60%). Monitor memory usage closely.");
        }
        
        // Frequent memory pressure events
        if (stats.memory_pressure_events > 10) {
            recommendations.push_back("Frequent memory pressure events detected. Consider increasing memory limits or optimizing texture usage.");
        }
        
        // Low optimization success rate
        if (stats.successful_optimizations > 0 && stats.failed_optimizations > 0) {
            float success_rate = static_cast<float>(stats.successful_optimizations) / 
                                static_cast<float>(stats.successful_optimizations + stats.failed_optimizations);
            if (success_rate < 0.7f) {
                recommendations.push_back("Memory optimization success rate is low. Check for memory fragmentation or insufficient available memory.");
            }
        }
        
        // Compression recommendations
        if (stats.uploads_with_compression > 0) {
            float compression_ratio = static_cast<float>(stats.uploads_with_compression) / 
                                    static_cast<float>(stats.total_memory_aware_uploads);
            if (compression_ratio < 0.3f && memory_stats.memory_pressure > 0.5f) {
                recommendations.push_back("Consider enabling compression for more uploads to reduce memory usage.");
            }
        }
        
        // Delay recommendations
        if (stats.uploads_delayed > 0) {
            float delay_ratio = static_cast<float>(stats.uploads_delayed) / 
                               static_cast<float>(stats.total_memory_aware_uploads);
            if (delay_ratio > 0.5f) {
                recommendations.push_back("Many uploads are being delayed due to memory pressure. Consider optimizing upload scheduling or increasing memory limits.");
            }
        }
        
        if (recommendations.empty()) {
            recommendations.push_back("Memory usage is optimal. No recommendations at this time.");
        }
        
    } catch (const std::exception& e) {
        recommendations.push_back(std::string("Error generating recommendations: ") + e.what());
    }
    
    return recommendations;
}

// Private implementation methods

void MemoryAwareUploader::memory_monitoring_thread() {
    VE_LOG_DEBUG("Memory monitoring thread started");
    
    while (!shutdown_requested_.load()) {
        try {
            auto now = std::chrono::steady_clock::now();
            auto time_since_last_check = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_memory_check_time_).count();
            
            // Check memory pressure at regular intervals
            if (time_since_last_check >= config_.memory_check_interval_ms) {
                check_memory_pressure();
                
                // Trigger optimization if needed
                if (config_.enable_automatic_optimization) {
                    optimize_memory_usage();
                }
                
                last_memory_check_time_ = now;
            }
            
            // Sleep for monitoring interval
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.monitoring_interval_ms));
            
        } catch (const std::exception& e) {
            VE_LOG_ERROR("Memory monitoring thread error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    VE_LOG_DEBUG("Memory monitoring thread stopped");
}

void MemoryAwareUploader::check_memory_pressure_and_optimize(MemoryAwareUploadJob& job) {
    if (!memory_manager_) {
        return;
    }
    
    try {
        auto memory_stats = memory_manager_->get_memory_stats();
        float current_pressure = memory_stats.memory_pressure;
        
        // Calculate upload size
        size_t upload_size_mb = calculate_upload_size_mb(job.width, job.height, job.format);
        
        VE_LOG_DEBUG("Memory pressure check: {:.1f}%, upload size: {}MB", 
                     current_pressure * 100.0f, upload_size_mb);
        
        // High memory pressure optimizations
        if (current_pressure > config_.memory_pressure_threshold) {
            VE_LOG_INFO("High memory pressure ({:.1f}%), applying optimizations",
                       current_pressure * 100.0f);
            
            // Enable compression if available and beneficial
            if (job.enable_compression && !job.compression_applied) {
                job.compression_applied = true;
                job.compression_quality = std::min(job.compression_quality, 
                                                  config_.emergency_compression_quality);
                
                std::lock_guard<std::mutex> lock(stats_mutex_);
                memory_aware_stats_.uploads_with_compression++;
                
                VE_LOG_DEBUG("Compression enabled (quality: {:.1f})", job.compression_quality);
            }
            
            // Consider delaying upload if not critical
            if (job.allow_memory_delay && job.priority != MemoryAwarePriority::CRITICAL) {
                // Calculate delay based on memory pressure
                uint32_t delay_ms = static_cast<uint32_t>(
                    (current_pressure - config_.memory_pressure_threshold) * 
                    config_.max_memory_delay_ms / (1.0f - config_.memory_pressure_threshold)
                );
                
                if (delay_ms > 0) {
                    job.memory_delay_ms = std::min(delay_ms, config_.max_memory_delay_ms);
                    
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    memory_aware_stats_.uploads_delayed++;
                    memory_aware_stats_.total_delay_time_ms += delay_ms;
                    
                    VE_LOG_DEBUG("Upload delayed by {}ms due to memory pressure", delay_ms);
                }
            }
            
            // Try to free some memory before upload
            if (config_.enable_automatic_optimization && upload_size_mb > config_.large_upload_threshold_mb) {
                size_t freed_mb = memory_manager_->cleanup_unused_textures();
                if (freed_mb > 0) {
                    VE_LOG_INFO("Freed {}MB before large upload", freed_mb);
                    
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    memory_aware_stats_.memory_freed_for_uploads_mb += freed_mb;
                }
            }
        }
        
        // Validate memory availability after optimizations
        if (upload_size_mb > memory_stats.available_memory_mb) {
            VE_LOG_WARNING("Upload size ({}MB) exceeds available memory ({}MB)",
                          upload_size_mb, memory_stats.available_memory_mb);
            
            // Try emergency cleanup
            size_t emergency_freed = memory_manager_->cleanup_unused_textures();
            if (emergency_freed > 0) {
                VE_LOG_INFO("Emergency cleanup freed {}MB", emergency_freed);
                
                std::lock_guard<std::mutex> lock(stats_mutex_);
                memory_aware_stats_.emergency_cleanups_triggered++;
            }
        }
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Memory pressure check failed: {}", e.what());
    }
}

void MemoryAwareUploader::execute_memory_aware_upload(std::shared_ptr<std::promise<TextureHandle>> promise,
                                                     MemoryAwareUploadJob job) {
    try {
        // Apply memory delay if specified
        if (job.memory_delay_ms > 0) {
            VE_LOG_DEBUG("Delaying upload by {}ms due to memory pressure", job.memory_delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(job.memory_delay_ms));
        }
        
        // Create base upload job
        StreamingUploadJob base_job{};
        base_job.image_data = job.image_data;
        base_job.width = job.width;
        base_job.height = job.height;
        base_job.format = job.format;
        base_job.priority = convert_memory_aware_priority(job.priority);
        
        // Apply compression if enabled (placeholder for actual implementation)
        if (job.compression_applied) {
            VE_LOG_DEBUG("Applying texture compression (quality: {:.1f})", job.compression_quality);
            // TODO: Implement actual texture compression
        }
        
        // Monitor memory during upload
        auto memory_before = get_current_memory_pressure();
        
        // Execute the upload
        auto upload_future = base_uploader_->queue_upload(std::move(base_job));
        
        // Handle result asynchronously with memory tracking
        std::thread([this, promise, upload_future = std::move(upload_future), memory_before, job]() mutable {
            try {
                auto result = upload_future.get();
                
                // Track memory impact
                auto memory_after = get_current_memory_pressure();
                float memory_impact = memory_after - memory_before;
                
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    memory_aware_stats_.successful_uploads++;
                    memory_aware_stats_.average_memory_impact = 
                        (memory_aware_stats_.average_memory_impact * (memory_aware_stats_.successful_uploads - 1) + 
                         memory_impact) / memory_aware_stats_.successful_uploads;
                }
                
                VE_LOG_DEBUG("Upload completed, memory impact: {:.3f}", memory_impact);
                promise->set_value(result);
                
            } catch (const std::exception& e) {
                VE_LOG_ERROR("Memory-aware upload failed: {}", e.what());
                
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    memory_aware_stats_.failed_uploads++;
                }
                
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
    } catch (...) {
        promise->set_exception(std::current_exception());
    }
}

size_t MemoryAwareUploader::calculate_upload_size_mb(int width, int height, TextureFormat format) {
    size_t bytes_per_pixel = 4; // Default to RGBA
    
    switch (format) {
        case TextureFormat::R8:
            bytes_per_pixel = 1;
            break;
        case TextureFormat::RG8:
            bytes_per_pixel = 2;
            break;
        case TextureFormat::RGB8:
            bytes_per_pixel = 3;
            break;
        case TextureFormat::RGBA8:
            bytes_per_pixel = 4;
            break;
        case TextureFormat::R16F:
            bytes_per_pixel = 2;
            break;
        case TextureFormat::RG16F:
            bytes_per_pixel = 4;
            break;
        case TextureFormat::RGBA16F:
            bytes_per_pixel = 8;
            break;
        case TextureFormat::R32F:
            bytes_per_pixel = 4;
            break;
        case TextureFormat::RG32F:
            bytes_per_pixel = 8;
            break;
        case TextureFormat::RGBA32F:
            bytes_per_pixel = 16;
            break;
    }
    
    size_t total_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * bytes_per_pixel;
    
    // Add overhead for mipmaps (approximately 33% more)
    total_bytes = static_cast<size_t>(total_bytes * 1.33f);
    
    // Convert to MB (round up)
    return (total_bytes + 1024 * 1024 - 1) / (1024 * 1024);
}

UploadPriority MemoryAwareUploader::convert_memory_aware_priority(MemoryAwarePriority priority) {
    switch (priority) {
        case MemoryAwarePriority::CRITICAL:
            return UploadPriority::IMMEDIATE;
        case MemoryAwarePriority::HIGH:
            return UploadPriority::HIGH;
        case MemoryAwarePriority::NORMAL:
            return UploadPriority::NORMAL;
        case MemoryAwarePriority::LOW:
            return UploadPriority::LOW;
        case MemoryAwarePriority::BACKGROUND:
            return UploadPriority::BACKGROUND;
        default:
            return UploadPriority::NORMAL;
    }
}

} // namespace ve::gfx
