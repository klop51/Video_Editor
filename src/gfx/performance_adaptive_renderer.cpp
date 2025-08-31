// Week 8: Performance Adaptive Renderer Implementation
// Intelligent renderer with performance-based quality adaptation

#include "gfx/performance_adaptive_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ve::gfx {

PerformanceAdaptiveRenderer::PerformanceAdaptiveRenderer(AsyncRenderer* base_renderer,
                                                       PerformanceMonitor* performance_monitor,
                                                       const Config& config)
    : config_(config)
    , base_renderer_(base_renderer)
    , performance_monitor_(performance_monitor)
    , adaptive_quality_enabled_(true)
    , current_quality_level_(config.default_quality)
    , forced_quality_active_(false)
    , shutdown_requested_(false)
    , last_adaptation_time_(std::chrono::steady_clock::now())
    , last_emergency_adaptation_time_(std::chrono::steady_clock::now())
{
    // Initialize statistics
    adaptation_stats_ = QualityAdaptationStats{};
    
    // Start performance monitoring thread
    monitoring_thread_ = std::thread(&PerformanceAdaptiveRenderer::performance_monitoring_thread, this);
    
    VE_LOG_INFO("Performance Adaptive Renderer initialized with target FPS: {:.1f}, default quality: {}",
                config_.target_fps, static_cast<int>(config_.default_quality));
}

PerformanceAdaptiveRenderer::~PerformanceAdaptiveRenderer() {
    shutdown_requested_.store(true);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    VE_LOG_INFO("Performance Adaptive Renderer shutdown complete");
}

std::future<TextureHandle> PerformanceAdaptiveRenderer::render_adaptive(AdaptiveRenderJob job) {
    if (!adaptive_quality_enabled_.load()) {
        // Fallback to direct rendering without adaptation
        RenderJob basic_job{};
        basic_job.effect_type = job.effect_type;
        basic_job.parameters = std::move(job.parameters);
        basic_job.input_textures = std::move(job.input_textures);
        basic_job.priority = job.priority;
        
        return base_renderer_->submit_job(std::move(basic_job));
    }
    
    // Adaptive rendering with quality management
    auto promise = std::make_shared<std::promise<TextureHandle>>();
    auto future = promise->get_future();
    
    // Determine optimal quality for this job
    DetailedQualityLevel optimal_quality = calculate_optimal_quality_for_job(job);
    
    // Execute adaptive render
    execute_adaptive_render(promise, std::move(job), optimal_quality);
    
    return future;
}

std::future<TextureHandle> PerformanceAdaptiveRenderer::apply_effect_adaptive(int effect_type, const void* parameters, size_t param_size,
                                                                              TextureHandle input_texture,
                                                                              DetailedQualityLevel target_quality) {
    // Create adaptive render job
    AdaptiveRenderJob job{};
    job.effect_type = effect_type;
    job.parameters.resize(param_size);
    std::memcpy(job.parameters.data(), parameters, param_size);
    job.input_textures.push_back(input_texture);
    job.max_quality = target_quality;
    job.target_fps = config_.target_fps;
    job.max_frame_time_ms = config_.max_frame_time_ms;
    
    return render_adaptive(std::move(job));
}

void PerformanceAdaptiveRenderer::set_adaptive_quality_mode(bool enabled) {
    adaptive_quality_enabled_.store(enabled);
    VE_LOG_INFO("Adaptive quality mode {}", enabled ? "enabled" : "disabled");
    
    if (enabled) {
        // Reset to default quality when enabling
        current_quality_level_.store(config_.default_quality);
    }
}

void PerformanceAdaptiveRenderer::update_quality_based_on_fps(float current_fps, bool force_update) {
    if (!adaptive_quality_enabled_.load() || forced_quality_active_.load()) {
        return;
    }
    
    // Check if adaptation is allowed
    if (!force_update && !is_adaptation_allowed()) {
        return;
    }
    
    auto current_quality = current_quality_level_.load();
    auto new_quality = calculate_quality_for_fps(current_fps);
    
    if (new_quality != current_quality) {
        VE_LOG_DEBUG("FPS-based quality adaptation: {:.1f} FPS -> quality level {}",
                     current_fps, static_cast<int>(new_quality));
        
        apply_quality_adaptation(new_quality, "FPS-based adaptation");
    }
}

DetailedQualityLevel PerformanceAdaptiveRenderer::calculate_optimal_quality(const GPUPerformanceStats& stats, float job_complexity) const {
    if (!adaptive_quality_enabled_.load()) {
        return config_.default_quality;
    }
    
    // Get current quality
    auto current_quality = current_quality_level_.load();
    
    // Apply strategy-specific calculation
    DetailedQualityLevel optimal_quality;
    
    switch (config_.strategy) {
        case AdaptationStrategy::CONSERVATIVE:
            optimal_quality = apply_conservative_strategy(current_quality, stats);
            break;
        case AdaptationStrategy::BALANCED:
            optimal_quality = apply_balanced_strategy(current_quality, stats);
            break;
        case AdaptationStrategy::AGGRESSIVE:
            optimal_quality = apply_aggressive_strategy(current_quality, stats);
            break;
        case AdaptationStrategy::CUSTOM:
            // Use balanced as fallback for custom
            optimal_quality = apply_balanced_strategy(current_quality, stats);
            break;
    }
    
    // Adjust for job complexity
    if (job_complexity > 0.8f) {
        // High complexity jobs get reduced quality
        if (static_cast<int>(optimal_quality) > 0) {
            optimal_quality = static_cast<DetailedQualityLevel>(static_cast<int>(optimal_quality) - 1);
        }
    } else if (job_complexity < 0.3f) {
        // Low complexity jobs can have higher quality
        if (static_cast<int>(optimal_quality) < static_cast<int>(DetailedQualityLevel::ULTRA_HIGH)) {
            optimal_quality = static_cast<DetailedQualityLevel>(static_cast<int>(optimal_quality) + 1);
        }
    }
    
    // Ensure quality is within configured bounds
    optimal_quality = std::clamp(optimal_quality, 
                                static_cast<DetailedQualityLevel>(0),
                                DetailedQualityLevel::ULTRA_HIGH);
    
    // Check emergency conditions
    if (stats.current_fps < config_.target_fps * config_.emergency_fps_threshold) {
        optimal_quality = config_.emergency_quality;
        if (config_.allow_ultra_low_quality && stats.current_fps < config_.target_fps * 0.3f) {
            optimal_quality = DetailedQualityLevel::ULTRA_LOW;
        }
    }
    
    return optimal_quality;
}

void PerformanceAdaptiveRenderer::set_performance_targets(float target_fps, float max_frame_time_ms) {
    config_.target_fps = target_fps;
    config_.max_frame_time_ms = max_frame_time_ms;
    
    VE_LOG_INFO("Performance targets updated: {:.1f} FPS, {:.1f}ms max frame time",
                target_fps, max_frame_time_ms);
}

void PerformanceAdaptiveRenderer::force_quality_level(DetailedQualityLevel quality, uint32_t duration_ms) {
    current_quality_level_.store(quality);
    forced_quality_active_.store(true);
    
    if (duration_ms > 0) {
        forced_quality_end_time_ = std::chrono::steady_clock::now() + 
                                  std::chrono::milliseconds(duration_ms);
    } else {
        forced_quality_end_time_ = std::chrono::steady_clock::time_point::max();
    }
    
    VE_LOG_INFO("Quality level forced to {} for {}ms", 
                static_cast<int>(quality), 
                duration_ms == 0 ? 0 : duration_ms);
}

QualityAdaptationStats PerformanceAdaptiveRenderer::get_adaptation_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return adaptation_stats_;
}

void PerformanceAdaptiveRenderer::reset_adaptation_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    adaptation_stats_ = QualityAdaptationStats{};
    VE_LOG_DEBUG("Quality adaptation statistics reset");
}

void PerformanceAdaptiveRenderer::update_config(const Config& new_config) {
    config_ = new_config;
    VE_LOG_INFO("Performance Adaptive Renderer configuration updated");
}

std::vector<std::pair<std::chrono::steady_clock::time_point, DetailedQualityLevel>> 
PerformanceAdaptiveRenderer::get_adaptation_history() const {
    if (!config_.enable_adaptation_history) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    return adaptation_history_;
}

bool PerformanceAdaptiveRenderer::is_performance_degraded() const {
    if (!performance_monitor_) {
        return false;
    }
    
    try {
        auto stats = performance_monitor_->get_current_performance();
        return stats.current_fps < config_.min_acceptable_fps ||
               stats.average_frame_time_ms > config_.max_frame_time_ms;
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to check performance degradation: {}", e.what());
        return false;
    }
}

std::vector<std::string> PerformanceAdaptiveRenderer::get_performance_recommendations() const {
    std::vector<std::string> recommendations;
    
    try {
        if (!performance_monitor_) {
            recommendations.push_back("Performance monitor not available");
            return recommendations;
        }
        
        auto stats = performance_monitor_->get_current_performance();
        auto adaptation_stats = get_adaptation_stats();
        
        // Low FPS recommendations
        if (stats.current_fps < config_.min_acceptable_fps) {
            recommendations.push_back("Performance is below acceptable levels. Consider reducing quality settings or optimizing effects.");
        }
        
        // High frame time recommendations
        if (stats.average_frame_time_ms > config_.max_frame_time_ms) {
            recommendations.push_back("Frame times are too high. Consider reducing effect complexity or resolution.");
        }
        
        // Adaptation frequency recommendations
        if (adaptation_stats.total_quality_adaptations > 100) {
            float adaptation_rate = static_cast<float>(adaptation_stats.total_quality_adaptations) / 
                                   static_cast<float>(adaptation_stats.total_quality_adaptations + 1);
            if (adaptation_rate > 0.5f) {
                recommendations.push_back("Quality is being adapted frequently. Consider adjusting performance targets or system resources.");
            }
        }
        
        // Quality level recommendations
        auto current_quality = current_quality_level_.load();
        if (current_quality == DetailedQualityLevel::ULTRA_LOW) {
            recommendations.push_back("Quality is at ultra-low level. System resources may be insufficient for current workload.");
        } else if (current_quality == DetailedQualityLevel::ULTRA_HIGH && stats.current_fps > config_.target_fps * 1.5f) {
            recommendations.push_back("Performance headroom available. Quality could potentially be increased further.");
        }
        
        // Emergency adaptations
        if (adaptation_stats.emergency_quality_drops > 5) {
            recommendations.push_back("Multiple emergency quality drops detected. Consider optimizing system configuration or reducing workload.");
        }
        
        if (recommendations.empty()) {
            recommendations.push_back("Performance is within acceptable parameters. No immediate recommendations.");
        }
        
    } catch (const std::exception& e) {
        recommendations.push_back(std::string("Error generating recommendations: ") + e.what());
    }
    
    return recommendations;
}

// Private implementation methods

void PerformanceAdaptiveRenderer::performance_monitoring_thread() {
    VE_LOG_DEBUG("Performance monitoring thread started");
    
    while (!shutdown_requested_.load()) {
        try {
            // Check for forced quality timeout
            if (forced_quality_active_.load() && 
                std::chrono::steady_clock::now() > forced_quality_end_time_) {
                forced_quality_active_.store(false);
                VE_LOG_DEBUG("Forced quality level expired, resuming adaptive quality");
            }
            
            // Analyze performance and adapt if needed
            if (config_.enable_automatic_coordination && adaptive_quality_enabled_.load()) {
                analyze_current_performance();
            }
            
            // Sleep for monitoring interval
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(config_.performance_check_interval_ms)));
            
        } catch (const std::exception& e) {
            VE_LOG_ERROR("Performance monitoring thread error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    VE_LOG_DEBUG("Performance monitoring thread stopped");
}

void PerformanceAdaptiveRenderer::analyze_current_performance() {
    if (!performance_monitor_) {
        return;
    }
    
    try {
        auto stats = performance_monitor_->get_current_performance();
        
        // Track recent performance
        {
            std::lock_guard<std::mutex> lock(performance_tracking_mutex_);
            recent_fps_measurements_.push_back(stats.current_fps);
            recent_frame_times_.push_back(stats.average_frame_time_ms);
            
            // Keep only recent samples
            if (recent_fps_measurements_.size() > MAX_PERFORMANCE_SAMPLES) {
                recent_fps_measurements_.erase(recent_fps_measurements_.begin());
            }
            if (recent_frame_times_.size() > MAX_PERFORMANCE_SAMPLES) {
                recent_frame_times_.erase(recent_frame_times_.begin());
            }
        }
        
        // Make adaptation decision
        make_adaptation_decision(stats);
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Performance analysis failed: {}", e.what());
    }
}

void PerformanceAdaptiveRenderer::make_adaptation_decision(const GPUPerformanceStats& stats) {
    if (!adaptive_quality_enabled_.load() || forced_quality_active_.load()) {
        return;
    }
    
    auto current_quality = current_quality_level_.load();
    auto optimal_quality = calculate_optimal_quality(stats);
    
    if (optimal_quality != current_quality) {
        // Check if adaptation is allowed
        bool emergency_adaptation = stats.current_fps < config_.target_fps * config_.emergency_fps_threshold;
        
        if (emergency_adaptation && is_emergency_adaptation_allowed()) {
            apply_quality_adaptation(optimal_quality, "Emergency adaptation");
        } else if (!emergency_adaptation && is_adaptation_allowed()) {
            apply_quality_adaptation(optimal_quality, "Performance adaptation");
        }
    }
}

void PerformanceAdaptiveRenderer::apply_quality_adaptation(DetailedQualityLevel new_quality, const std::string& reason) {
    auto old_quality = current_quality_level_.load();
    
    if (old_quality == new_quality) {
        return;
    }
    
    // Get performance stats before adaptation
    GPUPerformanceStats before_stats{};
    if (performance_monitor_) {
        before_stats = performance_monitor_->get_current_performance();
    }
    
    // Apply the quality change
    current_quality_level_.store(new_quality);
    
    // Update adaptation timing
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(timing_mutex_);
        last_adaptation_time_ = now;
        
        if (reason.find("Emergency") != std::string::npos) {
            last_emergency_adaptation_time_ = now;
        }
    }
    
    // Record in history
    if (config_.enable_adaptation_history) {
        record_adaptation_in_history(new_quality);
    }
    
    // Update statistics (will be done later with after_stats)
    VE_LOG_INFO("Quality adapted: {} -> {} ({})", 
                static_cast<int>(old_quality), 
                static_cast<int>(new_quality), 
                reason);
    
    // Schedule statistics update for later (after some rendering with new quality)
    std::thread([this, old_quality, new_quality, before_stats]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for adaptation to take effect
        
        GPUPerformanceStats after_stats{};
        if (performance_monitor_) {
            try {
                after_stats = performance_monitor_->get_current_performance();
            } catch (...) {
                // Ignore errors in delayed stats collection
            }
        }
        
        update_adaptation_statistics(old_quality, new_quality, before_stats, after_stats);
    }).detach();
}

DetailedQualityLevel PerformanceAdaptiveRenderer::calculate_quality_for_fps(float fps) const {
    float fps_ratio = fps / config_.target_fps;
    
    if (fps_ratio < config_.emergency_fps_threshold) {
        return config_.allow_ultra_low_quality ? DetailedQualityLevel::ULTRA_LOW : DetailedQualityLevel::LOW;
    } else if (fps_ratio < config_.fps_reduction_threshold) {
        // Performance is below target, reduce quality
        auto current = current_quality_level_.load();
        if (static_cast<int>(current) > 0) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) - 1);
        }
        return current;
    } else if (fps_ratio > config_.fps_increase_threshold) {
        // Performance is well above target, increase quality
        auto current = current_quality_level_.load();
        if (static_cast<int>(current) < static_cast<int>(DetailedQualityLevel::ULTRA_HIGH)) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) + 1);
        }
        return current;
    }
    
    // Performance is acceptable, maintain current quality
    return current_quality_level_.load();
}

DetailedQualityLevel PerformanceAdaptiveRenderer::calculate_quality_for_frame_time(float frame_time_ms) const {
    if (frame_time_ms > config_.max_frame_time_ms * 1.5f) {
        // Very high frame time, emergency reduction
        return config_.emergency_quality;
    } else if (frame_time_ms > config_.max_frame_time_ms) {
        // High frame time, reduce quality
        auto current = current_quality_level_.load();
        if (static_cast<int>(current) > 0) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) - 1);
        }
        return current;
    } else if (frame_time_ms < config_.max_frame_time_ms * 0.7f) {
        // Low frame time, can increase quality
        auto current = current_quality_level_.load();
        if (static_cast<int>(current) < static_cast<int>(DetailedQualityLevel::ULTRA_HIGH)) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) + 1);
        }
        return current;
    }
    
    return current_quality_level_.load();
}

DetailedQualityLevel PerformanceAdaptiveRenderer::apply_conservative_strategy(DetailedQualityLevel current, const GPUPerformanceStats& stats) const {
    // Conservative: small, careful adjustments
    float fps_ratio = stats.current_fps / config_.target_fps;
    
    if (fps_ratio < 0.8f) {
        // Only reduce if significantly below target
        if (static_cast<int>(current) > 0) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) - 1);
        }
    } else if (fps_ratio > 1.3f) {
        // Only increase if significantly above target
        if (static_cast<int>(current) < static_cast<int>(DetailedQualityLevel::ULTRA_HIGH)) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) + 1);
        }
    }
    
    return current;
}

DetailedQualityLevel PerformanceAdaptiveRenderer::apply_balanced_strategy(DetailedQualityLevel current, const GPUPerformanceStats& stats) const {
    // Balanced: moderate adjustments based on multiple factors
    float fps_ratio = stats.current_fps / config_.target_fps;
    float frame_time_ratio = stats.average_frame_time_ms / config_.max_frame_time_ms;
    
    // Weight both FPS and frame time
    float performance_score = (2.0f / fps_ratio + frame_time_ratio) / 3.0f;
    
    if (performance_score > 1.2f) {
        // Performance issues, reduce quality
        if (static_cast<int>(current) > 0) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) - 1);
        }
    } else if (performance_score < 0.8f) {
        // Good performance, increase quality
        if (static_cast<int>(current) < static_cast<int>(DetailedQualityLevel::ULTRA_HIGH)) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) + 1);
        }
    }
    
    return current;
}

DetailedQualityLevel PerformanceAdaptiveRenderer::apply_aggressive_strategy(DetailedQualityLevel current, const GPUPerformanceStats& stats) const {
    // Aggressive: quick, responsive adjustments
    float fps_ratio = stats.current_fps / config_.target_fps;
    
    if (fps_ratio < 0.95f) {
        // Quick reduction at even small FPS drops
        if (static_cast<int>(current) > 0) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) - 1);
        }
    } else if (fps_ratio > 1.1f) {
        // Quick increase when performance allows
        if (static_cast<int>(current) < static_cast<int>(DetailedQualityLevel::ULTRA_HIGH)) {
            return static_cast<DetailedQualityLevel>(static_cast<int>(current) + 1);
        }
    }
    
    return current;
}

bool PerformanceAdaptiveRenderer::is_adaptation_allowed() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_adaptation_time_).count();
    
    return time_since_last >= config_.adaptation_cooldown_ms;
}

bool PerformanceAdaptiveRenderer::is_emergency_adaptation_allowed() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_emergency_adaptation_time_).count();
    
    return time_since_last >= config_.emergency_cooldown_ms;
}

void PerformanceAdaptiveRenderer::update_adaptation_statistics(DetailedQualityLevel old_quality, DetailedQualityLevel new_quality,
                                                              const GPUPerformanceStats& before_stats, const GPUPerformanceStats& after_stats) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    adaptation_stats_.total_quality_adaptations++;
    
    if (static_cast<int>(new_quality) < static_cast<int>(old_quality)) {
        adaptation_stats_.quality_reductions++;
    } else if (static_cast<int>(new_quality) > static_cast<int>(old_quality)) {
        adaptation_stats_.quality_increases++;
    }
    
    // Update quality level usage
    int quality_index = static_cast<int>(new_quality);
    if (quality_index >= 0 && quality_index < 7) {
        adaptation_stats_.quality_level_usage[quality_index]++;
    }
    
    // Update performance tracking
    adaptation_stats_.average_fps_before_adaptation = 
        (adaptation_stats_.average_fps_before_adaptation * (adaptation_stats_.total_quality_adaptations - 1) + 
         before_stats.current_fps) / adaptation_stats_.total_quality_adaptations;
    
    adaptation_stats_.average_fps_after_adaptation = 
        (adaptation_stats_.average_fps_after_adaptation * (adaptation_stats_.total_quality_adaptations - 1) + 
         after_stats.current_fps) / adaptation_stats_.total_quality_adaptations;
    
    // Calculate performance improvement
    if (adaptation_stats_.average_fps_before_adaptation > 0.0f) {
        adaptation_stats_.performance_improvement_ratio = 
            adaptation_stats_.average_fps_after_adaptation / adaptation_stats_.average_fps_before_adaptation;
    }
    
    // Update success metrics
    if (after_stats.current_fps > before_stats.current_fps) {
        adaptation_stats_.successful_performance_recoveries++;
    } else {
        adaptation_stats_.failed_performance_recoveries++;
    }
    
    adaptation_stats_.adaptation_success_rate = 
        static_cast<float>(adaptation_stats_.successful_performance_recoveries) / 
        static_cast<float>(adaptation_stats_.total_quality_adaptations);
}

void PerformanceAdaptiveRenderer::record_adaptation_in_history(DetailedQualityLevel quality) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    adaptation_history_.emplace_back(std::chrono::steady_clock::now(), quality);
    
    // Keep history size manageable
    if (adaptation_history_.size() > config_.adaptation_history_size) {
        adaptation_history_.erase(adaptation_history_.begin());
    }
}

DetailedQualityLevel PerformanceAdaptiveRenderer::calculate_optimal_quality_for_job(const AdaptiveRenderJob& job) {
    if (!performance_monitor_) {
        return job.max_quality;
    }
    
    try {
        auto stats = performance_monitor_->get_current_performance();
        float job_complexity = estimate_job_complexity(job);
        
        auto optimal_quality = calculate_optimal_quality(stats, job_complexity);
        
        // Respect job constraints
        optimal_quality = std::clamp(optimal_quality, job.min_quality, job.max_quality);
        
        return optimal_quality;
        
    } catch (const std::exception& e) {
        VE_LOG_WARNING("Failed to calculate optimal quality for job: {}", e.what());
        return job.max_quality;
    }
}

float PerformanceAdaptiveRenderer::estimate_job_complexity(const AdaptiveRenderJob& job) const {
    // Estimate based on effect type and input texture count
    float complexity = 0.5f; // Base complexity
    
    // Add complexity based on effect type (this would be effect-specific)
    if (job.effect_type >= 100) { // Assume high-numbered effects are more complex
        complexity += 0.3f;
    }
    
    // Add complexity based on number of input textures
    complexity += job.input_textures.size() * 0.1f;
    
    // Add complexity based on parameters size (rough estimate)
    complexity += std::min(0.3f, job.parameters.size() / 1000.0f);
    
    return std::clamp(complexity, 0.0f, 1.0f);
}

void PerformanceAdaptiveRenderer::execute_adaptive_render(std::shared_ptr<std::promise<TextureHandle>> promise,
                                                         AdaptiveRenderJob job, DetailedQualityLevel quality) {
    try {
        // Create render job with adapted quality
        RenderJob render_job{};
        render_job.effect_type = job.effect_type;
        render_job.parameters = std::move(job.parameters);
        render_job.input_textures = std::move(job.input_textures);
        render_job.priority = job.priority;
        
        // Apply quality-specific modifications to the job
        apply_quality_modifications(render_job, quality);
        
        // Submit to base renderer
        auto render_future = base_renderer_->submit_job(std::move(render_job));
        
        // Handle result with quality tracking
        std::thread([this, promise, render_future = std::move(render_future), job, quality]() mutable {
            try {
                auto result = render_future.get();
                
                // Invoke quality change callback if provided
                if (job.quality_change_callback && quality != job.max_quality) {
                    job.quality_change_callback(job.max_quality, quality);
                }
                
                promise->set_value(result);
                
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
    } catch (...) {
        promise->set_exception(std::current_exception());
    }
}

void PerformanceAdaptiveRenderer::apply_quality_modifications(RenderJob& job, DetailedQualityLevel quality) {
    // Apply quality-specific modifications to the render job
    // This is a simplified implementation - real implementation would be effect-specific
    
    switch (quality) {
        case DetailedQualityLevel::ULTRA_LOW:
            // Minimal quality modifications
            VE_LOG_DEBUG("Applying ultra-low quality modifications");
            break;
            
        case DetailedQualityLevel::LOW:
            // Low quality modifications
            VE_LOG_DEBUG("Applying low quality modifications");
            break;
            
        case DetailedQualityLevel::MEDIUM_LOW:
            // Medium-low quality modifications
            VE_LOG_DEBUG("Applying medium-low quality modifications");
            break;
            
        case DetailedQualityLevel::MEDIUM:
            // Standard quality - no modifications needed
            break;
            
        case DetailedQualityLevel::MEDIUM_HIGH:
        case DetailedQualityLevel::HIGH:
        case DetailedQualityLevel::ULTRA_HIGH:
            // High quality - might enable additional features
            VE_LOG_DEBUG("Applying high quality modifications");
            break;
    }
}

} // namespace ve::gfx
