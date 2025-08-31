// Week 8: GPU System Coordinator Implementation
// Master controller for coordinating all GPU systems

#include "gfx/gpu_system_coordinator.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ve::gfx {

GPUSystemCoordinator::GPUSystemCoordinator(StreamingTextureUploader* uploader,
                                         GPUMemoryManager* memory_manager,
                                         AsyncRenderer* renderer,
                                         PerformanceMonitor* monitor,
                                         const Config& config)
    : config_(config)
    , texture_uploader_(uploader)
    , memory_manager_(memory_manager)
    , async_renderer_(renderer)
    , performance_monitor_(monitor)
    , coordination_enabled_(true)
    , last_optimization_time_(std::chrono::steady_clock::now())
    , last_performance_check_time_(std::chrono::steady_clock::now())
{
    // Initialize coordination stats
    coordination_stats_ = CoordinationStats{};
    
    // Start coordination thread if enabled
    if (config_.enable_automatic_coordination) {
        coordination_thread_ = std::thread(&GPUSystemCoordinator::coordination_thread_main, this);
    }
    
    VE_LOG_INFO("GPU System Coordinator initialized with automatic coordination: {}",
                config_.enable_automatic_coordination);
}

GPUSystemCoordinator::~GPUSystemCoordinator() {
    shutdown_requested_.store(true);
    
    if (coordination_thread_.joinable()) {
        coordination_thread_.join();
    }
    
    VE_LOG_INFO("GPU System Coordinator shutdown complete");
}

bool GPUSystemCoordinator::initialize() {
    if (!texture_uploader_ || !memory_manager_ || !async_renderer_ || !performance_monitor_) {
        VE_LOG_ERROR("GPU System Coordinator: Missing required subsystems");
        return false;
    }
    
    // Test connectivity to all subsystems
    bool all_systems_ready = true;
    
    try {
        // Check texture uploader
        auto upload_stats = texture_uploader_->get_upload_stats();
        VE_LOG_DEBUG("Texture uploader connected: {} pending uploads", upload_stats.uploads_pending);
        
        // Check memory manager
        auto memory_stats = memory_manager_->get_memory_stats();
        VE_LOG_DEBUG("Memory manager connected: {} MB used", memory_stats.used_memory_mb);
        
        // Check renderer
        auto render_stats = async_renderer_->get_render_stats();
        VE_LOG_DEBUG("Async renderer connected: {} active jobs", render_stats.active_jobs);
        
        // Check performance monitor
        auto perf_stats = performance_monitor_->get_current_performance();
        VE_LOG_DEBUG("Performance monitor connected: {:.1f} FPS", perf_stats.current_fps);
        
        initialized_.store(true);
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("GPU System Coordinator initialization failed: {}", e.what());
        all_systems_ready = false;
    }
    
    return all_systems_ready;
}

std::future<TextureHandle> GPUSystemCoordinator::upload_texture_smart(const void* image_data, int width, int height,
                                                                      TextureFormat format, const SmartUploadParams& params) {
    if (!coordination_enabled_.load()) {
        // Fallback to direct upload
        StreamingUploadJob job{};
        job.image_data = image_data;
        job.width = width;
        job.height = height;
        job.format = format;
        job.priority = static_cast<UploadPriority>(params.priority);
        
        return texture_uploader_->queue_upload(std::move(job));
    }
    
    // Smart upload with coordination
    auto promise = std::make_shared<std::promise<TextureHandle>>();
    auto future = promise->get_future();
    
    // Perform coordination analysis
    SmartUploadParams optimized_params = params;
    optimize_upload_parameters(optimized_params, width, height, format);
    
    // Execute smart upload
    execute_smart_upload(promise, image_data, width, height, format, optimized_params);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        coordination_stats_.total_smart_uploads++;
        coordination_stats_.total_coordination_events++;
    }
    
    return future;
}

std::future<TextureHandle> GPUSystemCoordinator::render_effect_adaptive(int effect_type, const void* parameters, size_t param_size,
                                                                        TextureHandle input_texture, const AdaptiveRenderParams& params) {
    if (!coordination_enabled_.load()) {
        // Fallback to direct rendering
        RenderJob job{};
        job.effect_type = effect_type;
        job.parameters.resize(param_size);
        std::memcpy(job.parameters.data(), parameters, param_size);
        job.input_textures.push_back(input_texture);
        job.priority = static_cast<RenderPriority>(params.priority);
        
        return async_renderer_->submit_job(std::move(job));
    }
    
    // Adaptive rendering with coordination
    auto promise = std::make_shared<std::promise<TextureHandle>>();
    auto future = promise->get_future();
    
    // Perform coordination analysis
    AdaptiveRenderParams optimized_params = params;
    optimize_render_parameters(optimized_params, effect_type);
    
    // Execute adaptive rendering
    execute_adaptive_render(promise, effect_type, parameters, param_size, input_texture, optimized_params);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        coordination_stats_.total_adaptive_renders++;
        coordination_stats_.total_coordination_events++;
    }
    
    return future;
}

bool GPUSystemCoordinator::optimize_pipeline_automatically() {
    if (!coordination_enabled_.load() || !initialized_.load()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_optimization = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_optimization_time_).count();
    
    // Check if enough time has passed since last optimization
    if (time_since_last_optimization < config_.optimization_interval_ms) {
        return false;
    }
    
    VE_LOG_DEBUG("Starting automatic pipeline optimization");
    
    bool optimizations_applied = false;
    
    try {
        // Gather current system state
        auto upload_stats = texture_uploader_->get_upload_stats();
        auto memory_stats = memory_manager_->get_memory_stats();
        auto render_stats = async_renderer_->get_render_stats();
        auto perf_stats = performance_monitor_->get_current_performance();
        
        // Memory optimization
        if (memory_stats.memory_pressure > config_.memory_pressure_threshold) {
            VE_LOG_INFO("High memory pressure ({:.1f}%), triggering memory optimization",
                       memory_stats.memory_pressure * 100.0f);
            
            // Trigger memory cleanup
            size_t freed_mb = memory_manager_->cleanup_unused_textures();
            if (freed_mb > 0) {
                VE_LOG_INFO("Memory optimization freed {} MB", freed_mb);
                optimizations_applied = true;
                
                std::lock_guard<std::mutex> lock(stats_mutex_);
                coordination_stats_.memory_optimizations_triggered++;
            }
        }
        
        // Upload queue optimization
        if (upload_stats.uploads_pending > config_.max_pending_uploads) {
            VE_LOG_INFO("High upload queue pressure ({} pending), optimizing upload strategy",
                       upload_stats.uploads_pending);
            
            optimize_upload_queue();
            optimizations_applied = true;
            
            std::lock_guard<std::mutex> lock(stats_mutex_);
            coordination_stats_.upload_optimizations_triggered++;
        }
        
        // Render queue optimization
        if (render_stats.active_jobs > config_.max_active_render_jobs) {
            VE_LOG_INFO("High render queue pressure ({} active jobs), optimizing render strategy",
                       render_stats.active_jobs);
            
            optimize_render_queue();
            optimizations_applied = true;
            
            std::lock_guard<std::mutex> lock(stats_mutex_);
            coordination_stats_.render_optimizations_triggered++;
        }
        
        // Performance-based optimization
        if (perf_stats.current_fps < config_.min_acceptable_fps) {
            VE_LOG_INFO("Low performance ({:.1f} FPS), triggering performance optimization",
                       perf_stats.current_fps);
            
            optimize_for_performance();
            optimizations_applied = true;
            
            std::lock_guard<std::mutex> lock(stats_mutex_);
            coordination_stats_.performance_optimizations_triggered++;
        }
        
        last_optimization_time_ = now;
        
        if (optimizations_applied) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            coordination_stats_.total_optimizations_applied++;
            coordination_stats_.optimization_success_rate = 
                static_cast<float>(coordination_stats_.total_optimizations_applied) / 
                static_cast<float>(coordination_stats_.total_coordination_events + 1);
        }
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Pipeline optimization failed: {}", e.what());
        return false;
    }
    
    VE_LOG_DEBUG("Automatic pipeline optimization completed: {} optimizations applied",
                 optimizations_applied ? "some" : "no");
    
    return optimizations_applied;
}

CoordinationStats GPUSystemCoordinator::get_coordination_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return coordination_stats_;
}

void GPUSystemCoordinator::reset_coordination_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    coordination_stats_ = CoordinationStats{};
    VE_LOG_DEBUG("Coordination statistics reset");
}

void GPUSystemCoordinator::set_coordination_enabled(bool enabled) {
    coordination_enabled_.store(enabled);
    VE_LOG_INFO("GPU System Coordination {}", enabled ? "enabled" : "disabled");
}

void GPUSystemCoordinator::update_config(const Config& new_config) {
    config_ = new_config;
    VE_LOG_INFO("GPU System Coordinator configuration updated");
}

// Private implementation methods

void GPUSystemCoordinator::coordination_thread_main() {
    VE_LOG_DEBUG("Coordination thread started");
    
    while (!shutdown_requested_.load()) {
        try {
            // Check if it's time for automatic optimization
            auto now = std::chrono::steady_clock::now();
            auto time_since_last_check = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_performance_check_time_).count();
            
            if (time_since_last_check >= config_.coordination_check_interval_ms) {
                if (config_.enable_automatic_coordination) {
                    optimize_pipeline_automatically();
                }
                last_performance_check_time_ = now;
            }
            
            // Sleep for a short interval
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            VE_LOG_ERROR("Coordination thread error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    VE_LOG_DEBUG("Coordination thread stopped");
}

void GPUSystemCoordinator::optimize_upload_parameters(SmartUploadParams& params, int width, int height, TextureFormat format) {
    // Get current system state
    auto memory_stats = memory_manager_->get_memory_stats();
    auto upload_stats = texture_uploader_->get_upload_stats();
    
    // Calculate texture size
    size_t texture_size_mb = calculate_texture_size_mb(width, height, format);
    
    // Adjust compression based on memory pressure
    if (memory_stats.memory_pressure > 0.8f) {
        params.enable_compression = true;
        params.compression_quality = std::min(params.compression_quality, 0.7f);
        VE_LOG_DEBUG("High memory pressure: enabling compression (quality: {:.1f})",
                     params.compression_quality);
    }
    
    // Adjust priority based on queue pressure
    if (upload_stats.uploads_pending > config_.max_pending_uploads / 2) {
        // Reduce priority for non-critical uploads
        if (params.priority == CoordinatedPriority::NORMAL) {
            params.priority = CoordinatedPriority::LOW;
        }
        VE_LOG_DEBUG("High upload queue pressure: reducing upload priority");
    }
    
    // Enable background processing for large textures
    if (texture_size_mb > config_.large_texture_threshold_mb) {
        params.enable_background_processing = true;
        VE_LOG_DEBUG("Large texture ({}MB): enabling background processing", texture_size_mb);
    }
    
    // Adjust deadline based on performance
    auto perf_stats = performance_monitor_->get_current_performance();
    if (perf_stats.current_fps < config_.min_acceptable_fps && params.deadline_ms > 0) {
        // Extend deadline during performance issues
        params.deadline_ms = static_cast<uint32_t>(params.deadline_ms * 1.5f);
        VE_LOG_DEBUG("Low performance: extending upload deadline to {}ms", params.deadline_ms);
    }
}

void GPUSystemCoordinator::optimize_render_parameters(AdaptiveRenderParams& params, int effect_type) {
    // Get current system state
    auto memory_stats = memory_manager_->get_memory_stats();
    auto render_stats = async_renderer_->get_render_stats();
    auto perf_stats = performance_monitor_->get_current_performance();
    
    // Adjust quality based on performance
    if (perf_stats.current_fps < config_.min_acceptable_fps * 0.8f) {
        // Significant performance issues: reduce quality
        params.adaptive_quality = std::min(params.adaptive_quality, 0.6f);
        params.enable_performance_scaling = true;
        VE_LOG_DEBUG("Poor performance: reducing render quality to {:.1f}", params.adaptive_quality);
    }
    
    // Adjust priority based on queue pressure
    if (render_stats.active_jobs > config_.max_active_render_jobs / 2) {
        if (params.priority == CoordinatedPriority::NORMAL) {
            params.priority = CoordinatedPriority::LOW;
        }
        VE_LOG_DEBUG("High render queue pressure: reducing render priority");
    }
    
    // Enable memory optimization during memory pressure
    if (memory_stats.memory_pressure > 0.7f) {
        params.enable_memory_optimization = true;
        params.max_memory_usage_mb = static_cast<uint32_t>(
            memory_stats.available_memory_mb * 0.5f);
        VE_LOG_DEBUG("High memory pressure: enabling memory optimization (limit: {}MB)",
                     params.max_memory_usage_mb);
    }
    
    // Adjust timeout based on system load
    float system_load = calculate_system_load();
    if (system_load > 0.8f && params.timeout_ms > 0) {
        params.timeout_ms = static_cast<uint32_t>(params.timeout_ms * 1.5f);
        VE_LOG_DEBUG("High system load: extending render timeout to {}ms", params.timeout_ms);
    }
}

void GPUSystemCoordinator::execute_smart_upload(std::shared_ptr<std::promise<TextureHandle>> promise,
                                                const void* image_data, int width, int height,
                                                TextureFormat format, const SmartUploadParams& params) {
    try {
        // Create optimized upload job
        StreamingUploadJob job{};
        job.image_data = image_data;
        job.width = width;
        job.height = height;
        job.format = format;
        job.priority = static_cast<UploadPriority>(params.priority);
        
        // Apply compression if enabled
        if (params.enable_compression) {
            // TODO: Implement texture compression
            VE_LOG_DEBUG("Texture compression not yet implemented");
        }
        
        // Apply memory checks
        if (params.enable_memory_checks) {
            size_t texture_size_mb = calculate_texture_size_mb(width, height, format);
            auto memory_stats = memory_manager_->get_memory_stats();
            
            if (texture_size_mb > memory_stats.available_memory_mb) {
                // Try to free some memory
                size_t freed_mb = memory_manager_->cleanup_unused_textures();
                VE_LOG_INFO("Freed {}MB for texture upload", freed_mb);
                
                // Update coordination stats
                std::lock_guard<std::mutex> lock(stats_mutex_);
                coordination_stats_.memory_pressure_events++;
            }
        }
        
        // Submit upload job
        auto upload_future = texture_uploader_->queue_upload(std::move(job));
        
        // Handle the result asynchronously
        std::thread([promise, upload_future = std::move(upload_future)]() mutable {
            try {
                auto result = upload_future.get();
                promise->set_value(result);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
    } catch (...) {
        promise->set_exception(std::current_exception());
    }
}

void GPUSystemCoordinator::execute_adaptive_render(std::shared_ptr<std::promise<TextureHandle>> promise,
                                                   int effect_type, const void* parameters, size_t param_size,
                                                   TextureHandle input_texture, const AdaptiveRenderParams& params) {
    try {
        // Create optimized render job
        RenderJob job{};
        job.effect_type = effect_type;
        job.parameters.resize(param_size);
        std::memcpy(job.parameters.data(), parameters, param_size);
        job.input_textures.push_back(input_texture);
        job.priority = static_cast<RenderPriority>(params.priority);
        
        // Apply performance scaling
        if (params.enable_performance_scaling) {
            auto perf_stats = performance_monitor_->get_current_performance();
            if (perf_stats.current_fps < config_.min_acceptable_fps) {
                // Reduce quality parameters
                // TODO: Implement quality reduction based on effect type
                VE_LOG_DEBUG("Performance scaling applied to render job");
            }
        }
        
        // Apply memory optimization
        if (params.enable_memory_optimization) {
            // Check memory usage before rendering
            auto memory_stats = memory_manager_->get_memory_stats();
            if (memory_stats.memory_pressure > 0.8f) {
                // Free some memory before rendering
                size_t freed_mb = memory_manager_->cleanup_unused_textures();
                VE_LOG_DEBUG("Freed {}MB before render job", freed_mb);
                
                // Update coordination stats
                std::lock_guard<std::mutex> lock(stats_mutex_);
                coordination_stats_.memory_pressure_events++;
            }
        }
        
        // Submit render job
        auto render_future = async_renderer_->submit_job(std::move(job));
        
        // Handle the result asynchronously
        std::thread([promise, render_future = std::move(render_future)]() mutable {
            try {
                auto result = render_future.get();
                promise->set_value(result);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
    } catch (...) {
        promise->set_exception(std::current_exception());
    }
}

void GPUSystemCoordinator::optimize_upload_queue() {
    // Implementation would coordinate with texture uploader to optimize queue
    VE_LOG_DEBUG("Upload queue optimization triggered");
    
    // Example optimizations:
    // - Reorder uploads by priority and size
    // - Batch similar uploads together
    // - Delay non-critical uploads during high load
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    coordination_stats_.upload_queue_optimizations++;
}

void GPUSystemCoordinator::optimize_render_queue() {
    // Implementation would coordinate with async renderer to optimize queue
    VE_LOG_DEBUG("Render queue optimization triggered");
    
    // Example optimizations:
    // - Reorder jobs by priority and estimated execution time
    // - Batch compatible render operations
    // - Reduce quality for queued jobs during high load
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    coordination_stats_.render_queue_optimizations++;
}

void GPUSystemCoordinator::optimize_for_performance() {
    VE_LOG_DEBUG("Performance optimization triggered");
    
    // Coordinate all systems for performance improvement
    
    // 1. Memory optimization
    memory_manager_->cleanup_unused_textures();
    
    // 2. Reduce upload queue pressure
    // TODO: Implement upload throttling
    
    // 3. Reduce render quality temporarily
    // TODO: Implement temporary quality reduction
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    coordination_stats_.performance_recoveries_attempted++;
}

size_t GPUSystemCoordinator::calculate_texture_size_mb(int width, int height, TextureFormat format) {
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
    return (total_bytes + 1024 * 1024 - 1) / (1024 * 1024); // Round up to MB
}

float GPUSystemCoordinator::calculate_system_load() {
    // Calculate overall system load based on all subsystems
    float load = 0.0f;
    
    try {
        auto upload_stats = texture_uploader_->get_upload_stats();
        auto memory_stats = memory_manager_->get_memory_stats();
        auto render_stats = async_renderer_->get_render_stats();
        auto perf_stats = performance_monitor_->get_current_performance();
        
        // Upload load (0.0 - 1.0)
        float upload_load = std::min(1.0f, 
            static_cast<float>(upload_stats.uploads_pending) / static_cast<float>(config_.max_pending_uploads));
        
        // Memory load
        float memory_load = memory_stats.memory_pressure;
        
        // Render load
        float render_load = std::min(1.0f,
            static_cast<float>(render_stats.active_jobs) / static_cast<float>(config_.max_active_render_jobs));
        
        // Performance load (inverse of FPS ratio)
        float performance_load = std::max(0.0f, 1.0f - (perf_stats.current_fps / config_.target_fps));
        
        // Weighted average
        load = (upload_load * 0.25f + memory_load * 0.3f + render_load * 0.25f + performance_load * 0.2f);
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to calculate system load: {}", e.what());
        load = 0.5f; // Default to moderate load
    }
    
    return std::clamp(load, 0.0f, 1.0f);
}

} // namespace ve::gfx
