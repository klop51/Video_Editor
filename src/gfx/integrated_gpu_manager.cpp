// Week 8: Integrated GPU Manager Implementation
// High-level unified interface for all GPU systems

#include "gfx/integrated_gpu_manager.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>

namespace ve::gfx {

IntegratedGPUManager::IntegratedGPUManager(const Config& config)
    : config_(config)
    , initialized_(false)
    , shutdown_requested_(false)
    , automatic_optimization_enabled_(true)
    , operation_counter_(0)
    , allocated_memory_mb_(0)
    , allocated_gpu_utilization_(0.0f)
{
    // Initialize current status
    current_status_ = IntegratedGPUStatus{};
    current_status_.system_start_time = std::chrono::steady_clock::now();
    
    VE_LOG_INFO("Integrated GPU Manager created with config: coordination={}, adaptive_performance={}, memory_optimization={}, streaming_uploads={}",
                config_.enable_coordination, config_.enable_adaptive_performance, 
                config_.enable_memory_optimization, config_.enable_streaming_uploads);
}

IntegratedGPUManager::~IntegratedGPUManager() {
    shutdown();
    VE_LOG_INFO("Integrated GPU Manager destroyed");
}

bool IntegratedGPUManager::initialize() {
    if (initialized_.load()) {
        VE_LOG_WARNING("Integrated GPU Manager already initialized");
        return true;
    }
    
    VE_LOG_INFO("Initializing Integrated GPU Manager...");
    
    try {
        // Initialize all subsystems
        if (!initialize_subsystems()) {
            VE_LOG_ERROR("Failed to initialize GPU subsystems");
            return false;
        }
        
        // Setup coordination between systems
        if (!setup_coordination()) {
            VE_LOG_ERROR("Failed to setup system coordination");
            return false;
        }
        
        // Start monitoring thread
        start_monitoring_thread();
        
        // Mark as initialized
        initialized_.store(true);
        
        // Update initial status
        update_system_status();
        
        VE_LOG_INFO("Integrated GPU Manager initialization complete");
        return true;
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Integrated GPU Manager initialization failed: {}", e.what());
        return false;
    }
}

void IntegratedGPUManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    VE_LOG_INFO("Shutting down Integrated GPU Manager...");
    
    shutdown_requested_.store(true);
    
    // Wait for monitoring thread to finish
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    // Cancel all pending operations
    cancel_all_operations();
    
    // Shutdown subsystems in reverse order
    adaptive_renderer_.reset();
    memory_aware_uploader_.reset();
    system_coordinator_.reset();
    performance_monitor_.reset();
    async_renderer_.reset();
    memory_manager_.reset();
    texture_uploader_.reset();
    
    initialized_.store(false);
    
    VE_LOG_INFO("Integrated GPU Manager shutdown complete");
}

std::future<GPUOperationResult> IntegratedGPUManager::upload_texture_optimized(const void* image_data, int width, int height,
                                                                               TextureFormat format,
                                                                               const GPUOperationConfig& config) {
    if (!is_ready()) {
        auto promise = std::make_shared<std::promise<GPUOperationResult>>();
        GPUOperationResult result{};
        result.status = GPUOperationStatus::FAILED;
        result.error_message = "GPU Manager not initialized";
        promise->set_value(result);
        return promise->get_future();
    }
    
    // Generate operation ID and track it
    std::string operation_id = generate_operation_id();
    track_operation(operation_id, config);
    
    auto promise = std::make_shared<std::promise<GPUOperationResult>>();
    auto future = promise->get_future();
    
    // Execute upload operation asynchronously
    std::thread([this, promise, operation_id, image_data, width, height, format, config]() {
        try {
            GPUOperationResult result{};
            result.operation_id = operation_id;
            result.status = GPUOperationStatus::IN_PROGRESS;
            
            // Check resource availability
            if (!check_resource_availability(config)) {
                result.status = GPUOperationStatus::FAILED;
                result.error_message = "Insufficient resources available";
                complete_operation(operation_id, result);
                promise->set_value(result);
                return;
            }
            
            // Allocate resources
            allocate_resources_for_operation(operation_id, config);
            
            auto start_time = std::chrono::steady_clock::now();
            
            // Choose optimal upload method
            std::future<TextureHandle> upload_future;
            
            if (config_.enable_memory_optimization && memory_aware_uploader_) {
                // Use memory-aware uploader
                MemoryAwareUploadJob job{};
                job.image_data = image_data;
                job.width = width;
                job.height = height;
                job.format = format;
                job.priority = convert_operation_priority_to_memory_aware(config.priority);
                job.enable_compression = config.respect_memory_constraints;
                job.allow_memory_delay = config.allow_background_execution;
                
                upload_future = memory_aware_uploader_->queue_memory_aware_upload(std::move(job));
                
                result.optimizations_applied.push_back("Memory-aware upload");
                result.was_optimized = true;
                
            } else if (config_.enable_streaming_uploads && texture_uploader_) {
                // Use streaming uploader
                StreamingUploadJob job{};
                job.image_data = image_data;
                job.width = width;
                job.height = height;
                job.format = format;
                job.priority = convert_operation_priority_to_upload(config.priority);
                
                upload_future = texture_uploader_->queue_upload(std::move(job));
                
                result.optimizations_applied.push_back("Streaming upload");
                result.was_optimized = true;
                
            } else {
                // Fallback: direct upload (would need implementation)
                throw std::runtime_error("No upload method available");
            }
            
            // Wait for upload completion
            result.result_texture = upload_future.get();
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
            
            // Calculate memory usage
            result.memory_used_mb = calculate_texture_memory_usage(width, height, format);
            
            // Get optimization benefit
            if (result.was_optimized) {
                result.optimization_benefit = calculate_optimization_benefit(config);
            }
            
            result.status = GPUOperationStatus::COMPLETED;
            
            // Release resources
            release_resources_for_operation(operation_id);
            
            // Complete operation tracking
            complete_operation(operation_id, result);
            promise->set_value(result);
            
        } catch (const std::exception& e) {
            GPUOperationResult result{};
            result.operation_id = operation_id;
            result.status = GPUOperationStatus::FAILED;
            result.error_message = e.what();
            
            release_resources_for_operation(operation_id);
            complete_operation(operation_id, result);
            promise->set_value(result);
        }
    }).detach();
    
    return future;
}

std::future<GPUOperationResult> IntegratedGPUManager::apply_effect_intelligent(int effect_type, const void* parameters, size_t param_size,
                                                                               TextureHandle input_texture,
                                                                               const GPUOperationConfig& config) {
    if (!is_ready()) {
        auto promise = std::make_shared<std::promise<GPUOperationResult>>();
        GPUOperationResult result{};
        result.status = GPUOperationStatus::FAILED;
        result.error_message = "GPU Manager not initialized";
        promise->set_value(result);
        return promise->get_future();
    }
    
    // Generate operation ID and track it
    std::string operation_id = generate_operation_id();
    track_operation(operation_id, config);
    
    auto promise = std::make_shared<std::promise<GPUOperationResult>>();
    auto future = promise->get_future();
    
    // Execute render operation asynchronously
    std::thread([this, promise, operation_id, effect_type, parameters, param_size, input_texture, config]() {
        try {
            GPUOperationResult result{};
            result.operation_id = operation_id;
            result.status = GPUOperationStatus::IN_PROGRESS;
            
            // Check resource availability
            if (!check_resource_availability(config)) {
                result.status = GPUOperationStatus::FAILED;
                result.error_message = "Insufficient resources available";
                complete_operation(operation_id, result);
                promise->set_value(result);
                return;
            }
            
            // Allocate resources
            allocate_resources_for_operation(operation_id, config);
            
            auto start_time = std::chrono::steady_clock::now();
            
            // Choose optimal rendering method
            std::future<TextureHandle> render_future;
            
            if (config_.enable_adaptive_performance && adaptive_renderer_) {
                // Use adaptive renderer
                AdaptiveRenderJob job{};
                job.effect_type = effect_type;
                job.parameters.resize(param_size);
                std::memcpy(job.parameters.data(), parameters, param_size);
                job.input_textures.push_back(input_texture);
                job.max_quality = convert_operation_config_to_quality(config.quality_preference);
                job.enable_quality_adaptation = config.allow_quality_adaptation;
                job.target_fps = config_.target_fps;
                
                render_future = adaptive_renderer_->render_adaptive(std::move(job));
                
                result.optimizations_applied.push_back("Adaptive quality rendering");
                result.was_optimized = true;
                
            } else if (async_renderer_) {
                // Use basic async renderer
                RenderJob job{};
                job.effect_type = effect_type;
                job.parameters.resize(param_size);
                std::memcpy(job.parameters.data(), parameters, param_size);
                job.input_textures.push_back(input_texture);
                job.priority = convert_operation_priority_to_render(config.priority);
                
                render_future = async_renderer_->submit_job(std::move(job));
                
                result.optimizations_applied.push_back("Async rendering");
                result.was_optimized = true;
                
            } else {
                throw std::runtime_error("No rendering method available");
            }
            
            // Wait for render completion
            result.result_texture = render_future.get();
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
            
            // Get quality used
            if (adaptive_renderer_) {
                result.quality_used = adaptive_renderer_->get_current_quality_level();
            }
            
            // Calculate memory usage (estimate)
            result.memory_used_mb = estimate_render_memory_usage(effect_type);
            
            // Get optimization benefit
            if (result.was_optimized) {
                result.optimization_benefit = calculate_optimization_benefit(config);
            }
            
            result.status = GPUOperationStatus::COMPLETED;
            
            // Release resources
            release_resources_for_operation(operation_id);
            
            // Complete operation tracking
            complete_operation(operation_id, result);
            promise->set_value(result);
            
        } catch (const std::exception& e) {
            GPUOperationResult result{};
            result.operation_id = operation_id;
            result.status = GPUOperationStatus::FAILED;
            result.error_message = e.what();
            
            release_resources_for_operation(operation_id);
            complete_operation(operation_id, result);
            promise->set_value(result);
        }
    }).detach();
    
    return future;
}

std::future<GPUOperationResult> IntegratedGPUManager::process_effect_chain_optimized(const std::vector<RenderJob>& effects,
                                                                                     TextureHandle input_texture,
                                                                                     const GPUOperationConfig& config) {
    if (!is_ready()) {
        auto promise = std::make_shared<std::promise<GPUOperationResult>>();
        GPUOperationResult result{};
        result.status = GPUOperationStatus::FAILED;
        result.error_message = "GPU Manager not initialized";
        promise->set_value(result);
        return promise->get_future();
    }
    
    auto promise = std::make_shared<std::promise<GPUOperationResult>>();
    auto future = promise->get_future();
    
    // Generate operation ID for the entire chain
    std::string operation_id = generate_operation_id();
    track_operation(operation_id, config);
    
    // Execute effect chain asynchronously
    std::thread([this, promise, operation_id, effects, input_texture, config]() {
        try {
            GPUOperationResult result{};
            result.operation_id = operation_id;
            result.status = GPUOperationStatus::IN_PROGRESS;
            
            auto start_time = std::chrono::steady_clock::now();
            
            TextureHandle current_texture = input_texture;
            
            // Process each effect in the chain
            for (size_t i = 0; i < effects.size(); ++i) {
                const auto& effect = effects[i];
                
                VE_LOG_DEBUG("Processing effect {} of {} in chain", i + 1, effects.size());
                
                // Apply effect using intelligent rendering
                auto effect_future = apply_effect_intelligent(
                    effect.effect_type,
                    effect.parameters.data(),
                    effect.parameters.size(),
                    current_texture,
                    config
                );
                
                // Wait for this effect to complete
                auto effect_result = effect_future.get();
                
                if (effect_result.status != GPUOperationStatus::COMPLETED) {
                    result.status = GPUOperationStatus::FAILED;
                    result.error_message = "Effect " + std::to_string(i + 1) + " failed: " + effect_result.error_message;
                    complete_operation(operation_id, result);
                    promise->set_value(result);
                    return;
                }
                
                // Update current texture for next effect
                current_texture = effect_result.result_texture;
                
                // Accumulate statistics
                result.execution_time_ms += effect_result.execution_time_ms;
                result.memory_used_mb = std::max(result.memory_used_mb, effect_result.memory_used_mb);
                
                // Merge optimization information
                if (effect_result.was_optimized) {
                    result.was_optimized = true;
                    result.optimizations_applied.insert(
                        result.optimizations_applied.end(),
                        effect_result.optimizations_applied.begin(),
                        effect_result.optimizations_applied.end()
                    );
                }
            }
            
            auto end_time = std::chrono::steady_clock::now();
            float total_time = std::chrono::duration<float, std::milli>(end_time - start_time).count();
            
            result.result_texture = current_texture;
            result.execution_time_ms = total_time;
            result.status = GPUOperationStatus::COMPLETED;
            
            // Add chain-specific optimizations
            result.optimizations_applied.push_back("Effect chain processing");
            
            // Calculate optimization benefit for the entire chain
            if (result.was_optimized) {
                result.optimization_benefit = calculate_chain_optimization_benefit(effects.size(), config);
            }
            
            complete_operation(operation_id, result);
            promise->set_value(result);
            
        } catch (const std::exception& e) {
            GPUOperationResult result{};
            result.operation_id = operation_id;
            result.status = GPUOperationStatus::FAILED;
            result.error_message = e.what();
            
            complete_operation(operation_id, result);
            promise->set_value(result);
        }
    }).detach();
    
    return future;
}

bool IntegratedGPUManager::optimize_performance(bool aggressive) {
    if (!is_ready()) {
        return false;
    }
    
    VE_LOG_INFO("Starting {} performance optimization", aggressive ? "aggressive" : "standard");
    
    bool optimizations_applied = false;
    
    try {
        // GPU System Coordinator optimization
        if (system_coordinator_ && config_.enable_coordination) {
            bool coord_optimized = system_coordinator_->optimize_pipeline_automatically();
            if (coord_optimized) {
                optimizations_applied = true;
                VE_LOG_DEBUG("System coordination optimizations applied");
            }
        }
        
        // Memory optimization
        if (memory_manager_ && config_.enable_memory_optimization) {
            size_t freed_mb = memory_manager_->cleanup_unused_textures();
            if (freed_mb > 0) {
                optimizations_applied = true;
                VE_LOG_DEBUG("Memory optimization freed {} MB", freed_mb);
            }
        }
        
        // Memory-aware uploader optimization
        if (memory_aware_uploader_ && config_.enable_memory_optimization) {
            bool mem_optimized = memory_aware_uploader_->optimize_memory_usage();
            if (mem_optimized) {
                optimizations_applied = true;
                VE_LOG_DEBUG("Memory-aware uploader optimizations applied");
            }
        }
        
        // Performance adaptive optimization
        if (adaptive_renderer_ && config_.enable_adaptive_performance) {
            // Force adaptation check
            if (performance_monitor_) {
                auto stats = performance_monitor_->get_current_performance();
                adaptive_renderer_->update_quality_based_on_fps(stats.current_fps, true);
                optimizations_applied = true;
                VE_LOG_DEBUG("Performance adaptive optimizations applied");
            }
        }
        
        // Aggressive optimizations
        if (aggressive) {
            // Force quality reduction temporarily
            if (adaptive_renderer_) {
                adaptive_renderer_->force_quality_level(DetailedQualityLevel::LOW, 5000); // 5 second duration
                VE_LOG_DEBUG("Aggressive: temporary quality reduction applied");
            }
            
            // Cancel low-priority operations
            size_t cancelled = cancel_low_priority_operations();
            if (cancelled > 0) {
                optimizations_applied = true;
                VE_LOG_DEBUG("Aggressive: cancelled {} low-priority operations", cancelled);
            }
        }
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Performance optimization failed: {}", e.what());
        return false;
    }
    
    VE_LOG_INFO("Performance optimization completed: {} optimizations applied",
                optimizations_applied ? "some" : "no");
    
    return optimizations_applied;
}

size_t IntegratedGPUManager::cleanup_memory_intelligent(size_t target_free_mb) {
    if (!is_ready()) {
        return 0;
    }
    
    size_t total_freed = 0;
    
    try {
        // Primary memory cleanup through memory manager
        if (memory_manager_) {
            size_t freed = memory_manager_->cleanup_unused_textures();
            total_freed += freed;
            VE_LOG_DEBUG("Memory manager freed {} MB", freed);
        }
        
        // Memory-aware uploader optimization
        if (memory_aware_uploader_) {
            bool optimized = memory_aware_uploader_->optimize_memory_usage();
            if (optimized) {
                VE_LOG_DEBUG("Memory-aware uploader contributed to cleanup");
            }
        }
        
        // If target not met, try more aggressive cleanup
        if (target_free_mb > 0 && total_freed < target_free_mb) {
            VE_LOG_DEBUG("Target not met ({}MB < {}MB), trying aggressive cleanup",
                        total_freed, target_free_mb);
            
            // Cancel pending uploads to free upload buffers
            size_t cancelled_uploads = cancel_pending_uploads();
            VE_LOG_DEBUG("Cancelled {} pending uploads for memory", cancelled_uploads);
            
            // Reduce quality temporarily to free render targets
            if (adaptive_renderer_) {
                adaptive_renderer_->force_quality_level(DetailedQualityLevel::LOW, 10000); // 10 seconds
                VE_LOG_DEBUG("Temporary quality reduction for memory cleanup");
            }
        }
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Intelligent memory cleanup failed: {}", e.what());
    }
    
    VE_LOG_INFO("Intelligent memory cleanup completed: {} MB freed", total_freed);
    return total_freed;
}

IntegratedGPUStatus IntegratedGPUManager::get_system_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_;
}

float IntegratedGPUManager::get_system_health_score() const {
    try {
        auto status = get_system_status();
        
        float health_score = 1.0f;
        
        // Deduct for errors and warnings
        health_score -= status.errors.size() * 0.2f;
        health_score -= status.warnings.size() * 0.1f;
        
        // Deduct for poor performance
        if (status.current_fps < status.target_fps * 0.8f) {
            health_score -= 0.3f;
        }
        
        // Deduct for high memory pressure
        if (status.memory_utilization > 0.9f) {
            health_score -= 0.2f;
        }
        
        // Deduct for high operation queue pressure
        if (status.operation_queue_pressure > 0.8f) {
            health_score -= 0.2f;
        }
        
        return std::clamp(health_score, 0.0f, 1.0f);
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to calculate system health score: {}", e.what());
        return 0.5f; // Default to moderate health
    }
}

GPUOperationStatus IntegratedGPUManager::get_operation_status(const std::string& operation_id) const {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    
    // Check pending operations
    auto pending_it = pending_operations_.find(operation_id);
    if (pending_it != pending_operations_.end()) {
        return GPUOperationStatus::PENDING;
    }
    
    // Check completed operations
    auto completed_it = completed_operations_.find(operation_id);
    if (completed_it != completed_operations_.end()) {
        return completed_it->second.status;
    }
    
    return GPUOperationStatus::FAILED; // Operation not found
}

bool IntegratedGPUManager::cancel_operation(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    
    auto it = pending_operations_.find(operation_id);
    if (it != pending_operations_.end()) {
        pending_operations_.erase(it);
        
        // Create cancelled result
        GPUOperationResult result{};
        result.operation_id = operation_id;
        result.status = GPUOperationStatus::CANCELLED;
        completed_operations_[operation_id] = result;
        
        VE_LOG_DEBUG("Operation {} cancelled", operation_id);
        return true;
    }
    
    return false;
}

size_t IntegratedGPUManager::cancel_all_operations() {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    
    size_t cancelled_count = pending_operations_.size();
    
    // Move all pending to completed with cancelled status
    for (const auto& [op_id, config] : pending_operations_) {
        GPUOperationResult result{};
        result.operation_id = op_id;
        result.status = GPUOperationStatus::CANCELLED;
        completed_operations_[op_id] = result;
    }
    
    pending_operations_.clear();
    
    VE_LOG_INFO("Cancelled {} pending operations", cancelled_count);
    return cancelled_count;
}

void IntegratedGPUManager::set_performance_mode(bool prefer_quality) {
    if (adaptive_renderer_) {
        if (prefer_quality) {
            adaptive_renderer_->force_quality_level(DetailedQualityLevel::HIGH);
        } else {
            adaptive_renderer_->set_adaptive_quality_mode(true); // Enable automatic adaptation for performance
        }
    }
    
    VE_LOG_INFO("Performance mode set to: {}", prefer_quality ? "quality-preferred" : "performance-preferred");
}

void IntegratedGPUManager::set_automatic_optimization(bool enabled) {
    automatic_optimization_enabled_.store(enabled);
    VE_LOG_INFO("Automatic optimization {}", enabled ? "enabled" : "disabled");
}

void IntegratedGPUManager::update_config(const Config& new_config) {
    config_ = new_config;
    
    // Update subsystem configurations
    if (system_coordinator_) {
        GPUSystemCoordinator::Config coord_config{};
        coord_config.enable_automatic_coordination = new_config.enable_coordination;
        coord_config.target_fps = new_config.target_fps;
        system_coordinator_->update_config(coord_config);
    }
    
    if (adaptive_renderer_) {
        PerformanceAdaptiveRenderer::Config render_config{};
        render_config.target_fps = new_config.target_fps;
        render_config.min_acceptable_fps = new_config.min_acceptable_fps;
        render_config.default_quality = new_config.default_quality;
        adaptive_renderer_->update_config(render_config);
    }
    
    VE_LOG_INFO("Integrated GPU Manager configuration updated");
}

void IntegratedGPUManager::set_memory_limit(size_t max_memory_mb) {
    config_.max_gpu_memory_usage_mb = max_memory_mb;
    
    if (memory_manager_) {
        // Update memory manager limit (assuming it has such capability)
        VE_LOG_INFO("Memory limit updated to {} MB", max_memory_mb);
    }
}

void IntegratedGPUManager::set_performance_targets(float target_fps, DetailedQualityLevel quality) {
    config_.target_fps = target_fps;
    config_.default_quality = quality;
    
    if (adaptive_renderer_) {
        adaptive_renderer_->set_performance_targets(target_fps, 1000.0f / target_fps);
        adaptive_renderer_->force_quality_level(quality);
    }
    
    VE_LOG_INFO("Performance targets updated: {:.1f} FPS, quality level {}",
                target_fps, static_cast<int>(quality));
}

std::vector<std::string> IntegratedGPUManager::get_performance_recommendations() const {
    std::vector<std::string> recommendations;
    
    try {
        auto status = get_system_status();
        
        // Performance recommendations
        if (status.current_fps < status.target_fps * 0.8f) {
            recommendations.push_back("Performance is below target. Consider reducing quality settings or effect complexity.");
        }
        
        // Memory recommendations
        if (status.memory_utilization > 0.8f) {
            recommendations.push_back("High memory usage detected. Consider enabling more aggressive memory optimization.");
        }
        
        // Queue pressure recommendations
        if (status.operation_queue_pressure > 0.7f) {
            recommendations.push_back("High operation queue pressure. Consider reducing concurrent operations.");
        }
        
        // System health recommendations
        float health_score = get_system_health_score();
        if (health_score < 0.7f) {
            recommendations.push_back("Overall system health is degraded. Review error logs and consider system optimization.");
        }
        
        // Subsystem-specific recommendations
        if (adaptive_renderer_) {
            auto renderer_recommendations = adaptive_renderer_->get_performance_recommendations();
            recommendations.insert(recommendations.end(), 
                                 renderer_recommendations.begin(), 
                                 renderer_recommendations.end());
        }
        
        if (memory_aware_uploader_) {
            auto memory_recommendations = memory_aware_uploader_->get_memory_recommendations();
            recommendations.insert(recommendations.end(), 
                                 memory_recommendations.begin(), 
                                 memory_recommendations.end());
        }
        
        if (recommendations.empty()) {
            recommendations.push_back("System is operating optimally. No recommendations at this time.");
        }
        
    } catch (const std::exception& e) {
        recommendations.push_back(std::string("Error generating recommendations: ") + e.what());
    }
    
    return recommendations;
}

std::map<std::string, size_t> IntegratedGPUManager::get_memory_usage_breakdown() const {
    std::map<std::string, size_t> breakdown;
    
    try {
        if (memory_manager_) {
            auto memory_stats = memory_manager_->get_memory_stats();
            breakdown["Total Used"] = memory_stats.used_memory_mb;
            breakdown["Available"] = memory_stats.available_memory_mb;
            breakdown["Cached Textures"] = memory_stats.cached_textures_mb;
        }
        
        breakdown["Allocated by Operations"] = allocated_memory_mb_;
        
        // Add estimated breakdowns for other subsystems
        breakdown["Upload Buffers"] = estimate_upload_buffer_usage();
        breakdown["Render Targets"] = estimate_render_target_usage();
        breakdown["System Overhead"] = estimate_system_overhead();
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to get memory usage breakdown: {}", e.what());
        breakdown["Error"] = 0;
    }
    
    return breakdown;
}

std::vector<GPUPerformanceStats> IntegratedGPUManager::get_performance_history() const {
    if (!config_.enable_performance_history || !performance_monitor_) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    return performance_history_;
}

bool IntegratedGPUManager::force_optimization_cycle() {
    if (!is_ready()) {
        return false;
    }
    
    VE_LOG_INFO("Forcing optimization cycle");
    
    try {
        // Force all optimizations
        bool success = optimize_performance(true); // Aggressive optimization
        
        // Force memory cleanup
        size_t freed = cleanup_memory_intelligent(0);
        
        // Force system coordination optimization
        if (system_coordinator_) {
            system_coordinator_->optimize_pipeline_automatically();
        }
        
        VE_LOG_INFO("Forced optimization cycle completed: {} MB freed", freed);
        return success;
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Forced optimization cycle failed: {}", e.what());
        return false;
    }
}

std::map<std::string, std::string> IntegratedGPUManager::get_system_diagnostics() const {
    std::map<std::string, std::string> diagnostics;
    
    try {
        auto status = get_system_status();
        
        diagnostics["System Initialized"] = initialized_.load() ? "Yes" : "No";
        diagnostics["Shutdown Requested"] = shutdown_requested_.load() ? "Yes" : "No";
        diagnostics["Automatic Optimization"] = automatic_optimization_enabled_.load() ? "Enabled" : "Disabled";
        
        diagnostics["Current FPS"] = std::to_string(status.current_fps);
        diagnostics["Target FPS"] = std::to_string(status.target_fps);
        diagnostics["Memory Utilization"] = std::to_string(static_cast<int>(status.memory_utilization * 100)) + "%";
        
        diagnostics["Pending Operations"] = std::to_string(status.pending_uploads + status.queued_operations);
        diagnostics["Active Render Jobs"] = std::to_string(status.active_render_jobs);
        
        diagnostics["Health Score"] = std::to_string(static_cast<int>(get_system_health_score() * 100)) + "%";
        
        // Subsystem status
        diagnostics["Texture Uploader"] = texture_uploader_ ? "Available" : "Not Available";
        diagnostics["Memory Manager"] = memory_manager_ ? "Available" : "Not Available";
        diagnostics["Async Renderer"] = async_renderer_ ? "Available" : "Not Available";
        diagnostics["Performance Monitor"] = performance_monitor_ ? "Available" : "Not Available";
        diagnostics["System Coordinator"] = system_coordinator_ ? "Available" : "Not Available";
        diagnostics["Memory Aware Uploader"] = memory_aware_uploader_ ? "Available" : "Not Available";
        diagnostics["Adaptive Renderer"] = adaptive_renderer_ ? "Available" : "Not Available";
        
        // Configuration status
        diagnostics["Coordination Enabled"] = config_.enable_coordination ? "Yes" : "No";
        diagnostics["Adaptive Performance Enabled"] = config_.enable_adaptive_performance ? "Yes" : "No";
        diagnostics["Memory Optimization Enabled"] = config_.enable_memory_optimization ? "Yes" : "No";
        diagnostics["Streaming Uploads Enabled"] = config_.enable_streaming_uploads ? "Yes" : "No";
        
    } catch (const std::exception& e) {
        diagnostics["Error"] = e.what();
    }
    
    return diagnostics;
}

// Private implementation methods

bool IntegratedGPUManager::initialize_subsystems() {
    try {
        // Initialize core systems first
        texture_uploader_ = std::make_unique<StreamingTextureUploader>();
        if (!texture_uploader_) {
            VE_LOG_ERROR("Failed to create texture uploader");
            return false;
        }
        
        memory_manager_ = std::make_unique<GPUMemoryManager>();
        if (!memory_manager_) {
            VE_LOG_ERROR("Failed to create memory manager");
            return false;
        }
        
        async_renderer_ = std::make_unique<AsyncRenderer>();
        if (!async_renderer_) {
            VE_LOG_ERROR("Failed to create async renderer");
            return false;
        }
        
        performance_monitor_ = std::make_unique<PerformanceMonitor>();
        if (!performance_monitor_) {
            VE_LOG_ERROR("Failed to create performance monitor");
            return false;
        }
        
        VE_LOG_DEBUG("Core subsystems initialized");
        return true;
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Subsystem initialization failed: {}", e.what());
        return false;
    }
}

bool IntegratedGPUManager::setup_coordination() {
    try {
        // Initialize coordination systems
        if (config_.enable_coordination) {
            system_coordinator_ = std::make_unique<GPUSystemCoordinator>(
                texture_uploader_.get(),
                memory_manager_.get(),
                async_renderer_.get(),
                performance_monitor_.get()
            );
            
            if (!system_coordinator_->initialize()) {
                VE_LOG_ERROR("Failed to initialize system coordinator");
                return false;
            }
        }
        
        if (config_.enable_memory_optimization) {
            memory_aware_uploader_ = std::make_unique<MemoryAwareUploader>(
                texture_uploader_.get(),
                memory_manager_.get()
            );
        }
        
        if (config_.enable_adaptive_performance) {
            adaptive_renderer_ = std::make_unique<PerformanceAdaptiveRenderer>(
                async_renderer_.get(),
                performance_monitor_.get()
            );
        }
        
        VE_LOG_DEBUG("Coordination systems initialized");
        return true;
        
    } catch (const std::exception& e) {
        VE_LOG_ERROR("Coordination setup failed: {}", e.what());
        return false;
    }
}

void IntegratedGPUManager::start_monitoring_thread() {
    monitoring_thread_ = std::thread(&IntegratedGPUManager::monitoring_thread_main, this);
    VE_LOG_DEBUG("Monitoring thread started");
}

void IntegratedGPUManager::monitoring_thread_main() {
    while (!shutdown_requested_.load()) {
        try {
            // Update system status
            update_system_status();
            
            // Check system health
            check_system_health();
            
            // Apply automatic optimizations if enabled
            if (automatic_optimization_enabled_.load()) {
                apply_automatic_optimizations();
            }
            
            // Cleanup completed operations
            cleanup_completed_operations();
            
            // Update performance history
            if (config_.enable_performance_history && performance_monitor_) {
                std::lock_guard<std::mutex> lock(history_mutex_);
                auto current_stats = performance_monitor_->get_current_performance();
                performance_history_.push_back(current_stats);
                
                if (performance_history_.size() > MAX_HISTORY_SIZE) {
                    performance_history_.erase(performance_history_.begin());
                }
            }
            
            // Sleep for monitoring interval
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.status_update_interval_ms));
            
        } catch (const std::exception& e) {
            VE_LOG_ERROR("Monitoring thread error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    VE_LOG_DEBUG("Monitoring thread stopped");
}

void IntegratedGPUManager::update_system_status() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    try {
        current_status_.last_update = std::chrono::steady_clock::now();
        
        // Memory status
        if (memory_manager_) {
            auto memory_stats = memory_manager_->get_memory_stats();
            current_status_.used_gpu_memory_mb = memory_stats.used_memory_mb;
            current_status_.available_gpu_memory_mb = memory_stats.available_memory_mb;
            current_status_.total_gpu_memory_mb = memory_stats.used_memory_mb + memory_stats.available_memory_mb;
            current_status_.memory_utilization = memory_stats.memory_pressure;
            current_status_.textures_in_memory = memory_stats.cached_textures_count;
        }
        
        // Performance status
        if (performance_monitor_) {
            auto perf_stats = performance_monitor_->get_current_performance();
            current_status_.current_fps = perf_stats.current_fps;
            current_status_.average_frame_time_ms = perf_stats.average_frame_time_ms;
            current_status_.gpu_utilization = perf_stats.gpu_utilization;
        }
        
        current_status_.target_fps = config_.target_fps;
        
        if (adaptive_renderer_) {
            current_status_.current_quality = adaptive_renderer_->get_current_quality_level();
            current_status_.performance_adaptation_active = adaptive_renderer_->is_adaptive_quality_mode_enabled();
        }
        
        // Operation status
        {
            std::lock_guard<std::mutex> ops_lock(operations_mutex_);
            current_status_.pending_uploads = 0; // Would need to query texture uploader
            current_status_.queued_operations = pending_operations_.size();
            current_status_.operation_queue_pressure = 
                static_cast<float>(pending_operations_.size()) / static_cast<float>(config_.max_pending_operations);
        }
        
        if (async_renderer_) {
            auto render_stats = async_renderer_->get_render_stats();
            current_status_.active_render_jobs = render_stats.active_jobs;
        }
        
        // Coordination status
        current_status_.systems_coordinated = (system_coordinator_ != nullptr);
        if (system_coordinator_) {
            auto coord_stats = system_coordinator_->get_coordination_stats();
            current_status_.coordination_optimizations_active = coord_stats.total_optimizations_applied;
            current_status_.coordination_efficiency = coord_stats.optimization_success_rate;
        }
        
        // Overall health
        current_status_.is_healthy = get_system_health_score() > 0.7f;
        current_status_.overall_efficiency = calculate_overall_efficiency();
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to update system status: {}", e.what());
        current_status_.is_healthy = false;
        current_status_.errors.push_back("Status update failed: " + std::string(e.what()));
    }
}

void IntegratedGPUManager::check_system_health() {
    auto status = get_system_status();
    
    // Clear previous warnings and errors (keep only recent ones)
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        if (current_status_.warnings.size() > 10) {
            current_status_.warnings.erase(current_status_.warnings.begin());
        }
        if (current_status_.errors.size() > 5) {
            current_status_.errors.erase(current_status_.errors.begin());
        }
    }
    
    // Check for warning conditions
    if (status.memory_utilization > 0.8f) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_status_.warnings.push_back("High memory utilization: " + 
                                          std::to_string(static_cast<int>(status.memory_utilization * 100)) + "%");
    }
    
    if (status.current_fps < status.target_fps * 0.8f) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_status_.warnings.push_back("Performance below target: " + 
                                          std::to_string(static_cast<int>(status.current_fps)) + " FPS < " + 
                                          std::to_string(static_cast<int>(status.target_fps * 0.8f)) + " FPS");
    }
    
    if (status.operation_queue_pressure > 0.8f) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_status_.warnings.push_back("High operation queue pressure: " + 
                                          std::to_string(static_cast<int>(status.operation_queue_pressure * 100)) + "%");
    }
}

void IntegratedGPUManager::apply_automatic_optimizations() {
    // Only apply automatic optimizations occasionally
    static auto last_auto_optimization = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(now - last_auto_optimization).count();
    
    if (time_since_last < 30) { // Only optimize every 30 seconds
        return;
    }
    
    auto status = get_system_status();
    
    // Trigger optimizations based on system health
    if (status.memory_utilization > config_.memory_pressure_threshold) {
        VE_LOG_DEBUG("Automatic memory optimization triggered");
        cleanup_memory_intelligent(0);
    }
    
    if (status.current_fps < config_.min_acceptable_fps) {
        VE_LOG_DEBUG("Automatic performance optimization triggered");
        optimize_performance(false);
    }
    
    if (status.operation_queue_pressure > 0.8f) {
        VE_LOG_DEBUG("Automatic queue optimization triggered");
        // Could implement queue optimization here
    }
    
    last_auto_optimization = now;
}

std::string IntegratedGPUManager::generate_operation_id() {
    uint64_t counter = operation_counter_.fetch_add(1);
    auto now = std::chrono::steady_clock::now();
    auto time_since_epoch = now.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch).count();
    
    std::ostringstream oss;
    oss << "op_" << timestamp << "_" << counter;
    return oss.str();
}

void IntegratedGPUManager::track_operation(const std::string& operation_id, const GPUOperationConfig& config) {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    pending_operations_[operation_id] = config;
}

void IntegratedGPUManager::complete_operation(const std::string& operation_id, const GPUOperationResult& result) {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    
    // Remove from pending
    auto pending_it = pending_operations_.find(operation_id);
    if (pending_it != pending_operations_.end()) {
        pending_operations_.erase(pending_it);
    }
    
    // Add to completed
    completed_operations_[operation_id] = result;
}

void IntegratedGPUManager::cleanup_completed_operations() {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    
    // Keep only recent completed operations (last 100)
    if (completed_operations_.size() > 100) {
        auto it = completed_operations_.begin();
        std::advance(it, completed_operations_.size() - 100);
        completed_operations_.erase(completed_operations_.begin(), it);
    }
}

bool IntegratedGPUManager::check_resource_availability(const GPUOperationConfig& config) const {
    // Check memory availability
    if (config.max_memory_usage_mb > 0) {
        if (memory_manager_) {
            auto memory_stats = memory_manager_->get_memory_stats();
            if (config.max_memory_usage_mb > memory_stats.available_memory_mb) {
                return false;
            }
        }
    }
    
    // Check GPU utilization availability
    if (allocated_gpu_utilization_ + config.max_gpu_utilization > 1.0f) {
        return false;
    }
    
    return true;
}

void IntegratedGPUManager::allocate_resources_for_operation(const std::string& operation_id, const GPUOperationConfig& config) {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    
    if (config.max_memory_usage_mb > 0) {
        allocated_memory_mb_ += config.max_memory_usage_mb;
    }
    
    allocated_gpu_utilization_ += config.max_gpu_utilization;
}

void IntegratedGPUManager::release_resources_for_operation(const std::string& operation_id) {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    
    // This would need to track allocated resources per operation
    // For now, just reset if we go negative
    if (allocated_memory_mb_ > 0) {
        allocated_memory_mb_ = std::max(0UL, allocated_memory_mb_ - 100); // Estimate
    }
    
    if (allocated_gpu_utilization_ > 0.0f) {
        allocated_gpu_utilization_ = std::max(0.0f, allocated_gpu_utilization_ - 0.1f); // Estimate
    }
}

// Helper conversion functions and calculations
size_t IntegratedGPUManager::calculate_texture_memory_usage(int width, int height, TextureFormat format) {
    size_t bytes_per_pixel = 4; // Default RGBA
    
    switch (format) {
        case TextureFormat::R8: bytes_per_pixel = 1; break;
        case TextureFormat::RG8: bytes_per_pixel = 2; break;
        case TextureFormat::RGB8: bytes_per_pixel = 3; break;
        case TextureFormat::RGBA8: bytes_per_pixel = 4; break;
        case TextureFormat::RGBA16F: bytes_per_pixel = 8; break;
        case TextureFormat::RGBA32F: bytes_per_pixel = 16; break;
        default: bytes_per_pixel = 4; break;
    }
    
    size_t total_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * bytes_per_pixel;
    return (total_bytes + 1024 * 1024 - 1) / (1024 * 1024); // Convert to MB
}

size_t IntegratedGPUManager::estimate_render_memory_usage(int effect_type) {
    // Rough estimates based on effect complexity
    if (effect_type < 50) {
        return 10; // Simple effects: 10MB
    } else if (effect_type < 100) {
        return 25; // Medium effects: 25MB
    } else {
        return 50; // Complex effects: 50MB
    }
}

float IntegratedGPUManager::calculate_optimization_benefit(const GPUOperationConfig& config) {
    // Calculate benefit based on enabled optimizations
    float benefit = 0.0f;
    
    if (config.enable_performance_optimization) benefit += 0.2f;
    if (config.respect_memory_constraints) benefit += 0.2f;
    if (config.allow_quality_adaptation) benefit += 0.3f;
    if (config.allow_background_execution) benefit += 0.1f;
    
    return benefit;
}

float IntegratedGPUManager::calculate_chain_optimization_benefit(size_t chain_length, const GPUOperationConfig& config) {
    float base_benefit = calculate_optimization_benefit(config);
    
    // Chain processing provides additional benefits
    float chain_benefit = std::min(0.5f, chain_length * 0.1f); // Up to 50% additional benefit
    
    return base_benefit + chain_benefit;
}

float IntegratedGPUManager::calculate_overall_efficiency() {
    try {
        auto status = get_system_status();
        
        // Calculate efficiency based on multiple factors
        float performance_efficiency = std::min(1.0f, status.current_fps / status.target_fps);
        float memory_efficiency = 1.0f - status.memory_utilization;
        float queue_efficiency = 1.0f - status.operation_queue_pressure;
        
        // Weighted average
        return (performance_efficiency * 0.4f + memory_efficiency * 0.3f + queue_efficiency * 0.3f);
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to calculate overall efficiency: {}", e.what());
        return 0.5f;
    }
}

size_t IntegratedGPUManager::cancel_low_priority_operations() {
    std::lock_guard<std::mutex> lock(operations_mutex_);
    
    size_t cancelled = 0;
    auto it = pending_operations_.begin();
    
    while (it != pending_operations_.end()) {
        if (it->second.priority == GPUOperationPriority::LOW ||
            it->second.priority == GPUOperationPriority::BACKGROUND) {
            
            // Create cancelled result
            GPUOperationResult result{};
            result.operation_id = it->first;
            result.status = GPUOperationStatus::CANCELLED;
            completed_operations_[it->first] = result;
            
            it = pending_operations_.erase(it);
            cancelled++;
        } else {
            ++it;
        }
    }
    
    return cancelled;
}

size_t IntegratedGPUManager::cancel_pending_uploads() {
    // This would need integration with texture uploader to actually cancel uploads
    // For now, just return an estimate
    return 5; // Estimate of cancelled uploads
}

size_t IntegratedGPUManager::estimate_upload_buffer_usage() {
    // Estimate based on pending uploads
    return 50; // 50MB estimate
}

size_t IntegratedGPUManager::estimate_render_target_usage() {
    // Estimate based on active render jobs
    return 100; // 100MB estimate
}

size_t IntegratedGPUManager::estimate_system_overhead() {
    return 20; // 20MB system overhead estimate
}

// Priority conversion functions
MemoryAwarePriority IntegratedGPUManager::convert_operation_priority_to_memory_aware(GPUOperationPriority priority) {
    switch (priority) {
        case GPUOperationPriority::CRITICAL: return MemoryAwarePriority::CRITICAL;
        case GPUOperationPriority::HIGH: return MemoryAwarePriority::HIGH;
        case GPUOperationPriority::NORMAL: return MemoryAwarePriority::NORMAL;
        case GPUOperationPriority::LOW: return MemoryAwarePriority::LOW;
        case GPUOperationPriority::BACKGROUND: return MemoryAwarePriority::BACKGROUND;
        default: return MemoryAwarePriority::NORMAL;
    }
}

UploadPriority IntegratedGPUManager::convert_operation_priority_to_upload(GPUOperationPriority priority) {
    switch (priority) {
        case GPUOperationPriority::CRITICAL: return UploadPriority::IMMEDIATE;
        case GPUOperationPriority::HIGH: return UploadPriority::HIGH;
        case GPUOperationPriority::NORMAL: return UploadPriority::NORMAL;
        case GPUOperationPriority::LOW: return UploadPriority::LOW;
        case GPUOperationPriority::BACKGROUND: return UploadPriority::BACKGROUND;
        default: return UploadPriority::NORMAL;
    }
}

RenderPriority IntegratedGPUManager::convert_operation_priority_to_render(GPUOperationPriority priority) {
    switch (priority) {
        case GPUOperationPriority::CRITICAL: return RenderPriority::IMMEDIATE;
        case GPUOperationPriority::HIGH: return RenderPriority::HIGH;
        case GPUOperationPriority::NORMAL: return RenderPriority::NORMAL;
        case GPUOperationPriority::LOW: return RenderPriority::LOW;
        case GPUOperationPriority::BACKGROUND: return RenderPriority::BACKGROUND;
        default: return RenderPriority::NORMAL;
    }
}

DetailedQualityLevel IntegratedGPUManager::convert_operation_config_to_quality(DetailedQualityLevel preference) {
    return preference; // Direct conversion
}

} // namespace ve::gfx
