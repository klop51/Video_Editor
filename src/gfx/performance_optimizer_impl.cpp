#include "performance_optimizer.hpp"
#include "graphics_device.hpp"
#include "../core/logging.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <chrono>

namespace gfx {

// Quality Settings Implementation
void QualitySettings::clamp_to_valid_ranges() {
    render_scale = std::clamp(render_scale, 0.25f, 1.0f);
    ui_scale = std::clamp(ui_scale, 0.5f, 2.0f);
    shadow_quality = std::clamp(shadow_quality, 0, 4);
    reflection_quality = std::clamp(reflection_quality, 0, 3);
    particle_quality = std::clamp(particle_quality, 0, 3);
    effect_quality = std::clamp(effect_quality, 0, 4);
    target_fps = std::clamp(target_fps, 15.0f, 240.0f);
    min_acceptable_fps = std::clamp(min_acceptable_fps, 10.0f, target_fps);
    quality_scale_speed = std::clamp(quality_scale_speed, 0.01f, 1.0f);
    max_texture_memory_mb = std::clamp(max_texture_memory_mb, size_t(128), size_t(8192));
    max_buffer_memory_mb = std::clamp(max_buffer_memory_mb, size_t(64), size_t(2048));
}

void QualitySettings::reset_to_defaults() {
    *this = QualitySettings{};
}

bool QualitySettings::is_valid() const {
    return render_scale > 0.0f && render_scale <= 1.0f &&
           target_fps > 0.0f && min_acceptable_fps > 0.0f &&
           min_acceptable_fps <= target_fps &&
           shadow_quality >= 0 && shadow_quality <= 4 &&
           reflection_quality >= 0 && reflection_quality <= 3 &&
           particle_quality >= 0 && particle_quality <= 3 &&
           effect_quality >= 0 && effect_quality <= 4;
}

float QualitySettings::get_overall_quality_score() const {
    float score = 0.0f;
    score += render_scale * 0.3f;                                    // 30% weight
    score += (shadow_quality / 4.0f) * 0.2f;                       // 20% weight
    score += (reflection_quality / 3.0f) * 0.15f;                  // 15% weight
    score += (particle_quality / 3.0f) * 0.1f;                     // 10% weight
    score += (effect_quality / 4.0f) * 0.15f;                      // 15% weight
    score += (motion_blur ? 0.05f : 0.0f);                         // 5% weight
    score += (depth_of_field ? 0.05f : 0.0f);                      // 5% weight
    return std::clamp(score, 0.0f, 1.0f);
}

// Performance Optimizer Implementation
PerformanceOptimizer::PerformanceOptimizer(DetailedProfiler& profiler, GraphicsDevice& device)
    : profiler_(profiler), device_(device) {
    
    current_quality_.reset_to_defaults();
    default_quality_ = current_quality_;
    
    recent_frame_times_.reserve(120);  // 2 seconds at 60fps
    recent_cpu_usage_.reserve(120);
    recent_gpu_usage_.reserve(120);
    recent_memory_usage_.reserve(120);
    
    last_update_time_ = std::chrono::high_resolution_clock::now();
}

PerformanceOptimizer::~PerformanceOptimizer() = default;

void PerformanceOptimizer::update(float delta_time) {
    auto current_time = std::chrono::high_resolution_clock::now();
    auto time_since_update = std::chrono::duration<float>(current_time - last_update_time_).count();
    
    time_since_last_analysis_ += delta_time;
    time_since_quality_change_ += delta_time;
    
    // Update performance history every frame
    update_performance_history();
    
    // Run full analysis every 2 seconds
    if (time_since_last_analysis_ >= 2.0f) {
        analyze_bottlenecks();
        time_since_last_analysis_ = 0.0f;
        
        // Apply automatic optimizations if enabled
        if (adaptive_quality_enabled_) {
            apply_automatic_optimizations();
        }
    }
    
    // Check for emergency quality reduction
    float current_fps = profiler_.get_current_fps();
    if (current_fps < min_acceptable_fps_ * 0.5f && time_since_quality_change_ > 1.0f) {
        force_quality_adjustment(0.8f);  // Emergency 20% quality reduction
    }
    
    last_update_time_ = current_time;
}

void PerformanceOptimizer::analyze_bottlenecks() {
    last_analysis_ = BottleneckAnalysis{};
    
    if (recent_frame_times_.size() < 30) {
        return;  // Not enough data
    }
    
    // Calculate average metrics
    float avg_frame_time = std::accumulate(recent_frame_times_.begin(), recent_frame_times_.end(), 0.0f) / recent_frame_times_.size();
    float avg_cpu_usage = std::accumulate(recent_cpu_usage_.begin(), recent_cpu_usage_.end(), 0.0f) / recent_cpu_usage_.size();
    float avg_gpu_usage = std::accumulate(recent_gpu_usage_.begin(), recent_gpu_usage_.end(), 0.0f) / recent_gpu_usage_.size();
    
    last_analysis_.avg_frame_time_ms = avg_frame_time;
    last_analysis_.cpu_utilization = avg_cpu_usage;
    last_analysis_.gpu_utilization = avg_gpu_usage;
    
    if (!recent_memory_usage_.empty()) {
        last_analysis_.memory_usage_mb = recent_memory_usage_.back() / (1024 * 1024);
    }
    
    // Determine primary bottleneck
    last_analysis_.primary_bottleneck = analyze_primary_bottleneck();
    last_analysis_.secondary_bottlenecks = analyze_secondary_bottlenecks();
    last_analysis_.confidence_score = calculate_confidence_score(last_analysis_.primary_bottleneck);
    
    // Generate descriptions and suggestions
    switch (last_analysis_.primary_bottleneck) {
        case BottleneckType::CPU_BOUND:
            last_analysis_.detailed_description = "CPU is the primary bottleneck. High CPU utilization detected.";
            last_analysis_.suggested_fixes = generate_cpu_suggestions();
            break;
        case BottleneckType::GPU_BOUND:
            last_analysis_.detailed_description = "GPU is the primary bottleneck. High GPU utilization or expensive rendering.";
            last_analysis_.suggested_fixes = generate_gpu_suggestions();
            break;
        case BottleneckType::MEMORY_BOUND:
            last_analysis_.detailed_description = "Memory bandwidth or capacity is limiting performance.";
            last_analysis_.suggested_fixes = generate_memory_suggestions();
            break;
        case BottleneckType::SYNCHRONIZATION:
            last_analysis_.detailed_description = "CPU-GPU synchronization is causing stalls.";
            last_analysis_.suggested_fixes = {"Reduce CPU-GPU dependencies", "Use asynchronous operations", "Pipeline rendering work"};
            break;
        default:
            last_analysis_.detailed_description = "No significant bottlenecks detected.";
            break;
    }
    
    if (detailed_logging_) {
        log_bottleneck_analysis();
    }
    
    // Notify callback if set
    if (performance_callback_) {
        performance_callback_(profiler_.get_current_fps(), last_analysis_);
    }
}

BottleneckType PerformanceOptimizer::analyze_primary_bottleneck() const {
    if (recent_frame_times_.empty()) {
        return BottleneckType::NONE;
    }
    
    float avg_frame_time = std::accumulate(recent_frame_times_.begin(), recent_frame_times_.end(), 0.0f) / recent_frame_times_.size();
    
    // If we're hitting target framerate, no bottleneck
    if (avg_frame_time <= (1000.0f / target_fps_) * 1.1f) {
        return BottleneckType::NONE;
    }
    
    // Check CPU utilization
    if (!recent_cpu_usage_.empty()) {
        float avg_cpu = std::accumulate(recent_cpu_usage_.begin(), recent_cpu_usage_.end(), 0.0f) / recent_cpu_usage_.size();
        if (avg_cpu > 0.85f) {
            return BottleneckType::CPU_BOUND;
        }
    }
    
    // Check GPU utilization
    if (!recent_gpu_usage_.empty()) {
        float avg_gpu = std::accumulate(recent_gpu_usage_.begin(), recent_gpu_usage_.end(), 0.0f) / recent_gpu_usage_.size();
        if (avg_gpu > 0.85f) {
            return BottleneckType::GPU_BOUND;
        }
    }
    
    // Check memory pressure
    if (is_memory_pressure_high()) {
        return BottleneckType::MEMORY_BOUND;
    }
    
    // Check for synchronization issues
    if (is_synchronization_bottleneck()) {
        return BottleneckType::SYNCHRONIZATION;
    }
    
    // Default to GPU bound if frame time is high but utilization is unclear
    return BottleneckType::GPU_BOUND;
}

std::vector<BottleneckType> PerformanceOptimizer::analyze_secondary_bottlenecks() const {
    std::vector<BottleneckType> secondary;
    
    // Check for secondary issues
    if (is_gpu_memory_limited()) {
        secondary.push_back(BottleneckType::TEXTURE_BANDWIDTH);
    }
    
    if (is_gpu_compute_limited()) {
        secondary.push_back(BottleneckType::FRAGMENT_PROCESSING);
    }
    
    // Frame variance indicates synchronization issues
    if (!recent_frame_times_.empty()) {
        float variance = 0.0f;
        float mean = std::accumulate(recent_frame_times_.begin(), recent_frame_times_.end(), 0.0f) / recent_frame_times_.size();
        
        for (float time : recent_frame_times_) {
            variance += (time - mean) * (time - mean);
        }
        variance /= recent_frame_times_.size();
        
        if (variance > mean * 0.2f) {  // High variance
            secondary.push_back(BottleneckType::SYNCHRONIZATION);
        }
    }
    
    return secondary;
}

float PerformanceOptimizer::calculate_confidence_score(BottleneckType bottleneck) const {
    if (bottleneck == BottleneckType::NONE) {
        return 1.0f;
    }
    
    // Base confidence on data availability and consistency
    float confidence = 0.5f;
    
    if (recent_frame_times_.size() >= 60) {
        confidence += 0.2f;  // More data = higher confidence
    }
    
    if (!recent_cpu_usage_.empty() && !recent_gpu_usage_.empty()) {
        confidence += 0.2f;  // Have both CPU and GPU data
    }
    
    // Check consistency over time
    if (recent_frame_times_.size() >= 30) {
        float recent_avg = 0.0f;
        float older_avg = 0.0f;
        size_t half = recent_frame_times_.size() / 2;
        
        for (size_t i = half; i < recent_frame_times_.size(); ++i) {
            recent_avg += recent_frame_times_[i];
        }
        recent_avg /= half;
        
        for (size_t i = 0; i < half; ++i) {
            older_avg += recent_frame_times_[i];
        }
        older_avg /= half;
        
        float consistency = 1.0f - std::abs(recent_avg - older_avg) / std::max(recent_avg, older_avg);
        confidence += consistency * 0.1f;
    }
    
    return std::clamp(confidence, 0.0f, 1.0f);
}

void PerformanceOptimizer::apply_automatic_optimizations() {
    if (!should_adjust_quality()) {
        return;
    }
    
    float adjustment_factor = calculate_quality_adjustment_factor();
    
    switch (last_analysis_.primary_bottleneck) {
        case BottleneckType::GPU_BOUND:
            apply_gpu_optimizations();
            break;
        case BottleneckType::CPU_BOUND:
            apply_cpu_optimizations();
            break;
        case BottleneckType::MEMORY_BOUND:
            apply_memory_optimizations();
            break;
        default:
            apply_quality_optimizations();
            break;
    }
    
    // Mark that we recently changed quality
    quality_recently_changed_ = true;
    time_since_quality_change_ = 0.0f;
}

void PerformanceOptimizer::apply_gpu_optimizations() {
    float current_fps = profiler_.get_current_fps();
    
    if (current_fps < min_acceptable_fps_) {
        // Aggressive optimizations
        if (current_quality_.render_scale > 0.75f) {
            current_quality_.render_scale *= 0.9f;
        }
        
        if (current_quality_.shadow_quality > 1) {
            current_quality_.shadow_quality--;
        }
        
        if (current_quality_.effect_quality > 1) {
            current_quality_.effect_quality--;
        }
        
        // Disable expensive effects
        if (current_fps < min_acceptable_fps_ * 0.8f) {
            current_quality_.motion_blur = false;
            current_quality_.depth_of_field = false;
        }
    } else if (current_fps < target_fps_ * 0.9f) {
        // Gradual optimizations
        current_quality_.render_scale *= 0.95f;
    }
    
    current_quality_.clamp_to_valid_ranges();
}

void PerformanceOptimizer::apply_cpu_optimizations() {
    // CPU optimizations focus on reducing CPU workload
    current_quality_.aggressive_culling = true;
    current_quality_.enable_async_compute = false;  // Reduce CPU overhead
    
    if (current_quality_.particle_quality > 1) {
        current_quality_.particle_quality--;
    }
    
    float current_fps = profiler_.get_current_fps();
    if (current_fps < min_acceptable_fps_) {
        current_quality_.max_worker_threads = std::max(1, current_quality_.max_worker_threads - 1);
    }
}

void PerformanceOptimizer::apply_memory_optimizations() {
    // Reduce memory pressure
    current_quality_.max_texture_memory_mb = static_cast<size_t>(current_quality_.max_texture_memory_mb * 0.9f);
    current_quality_.max_buffer_memory_mb = static_cast<size_t>(current_quality_.max_buffer_memory_mb * 0.9f);
    
    // Reduce quality settings that use lots of memory
    if (current_quality_.shadow_quality > 2) {
        current_quality_.shadow_quality = 2;
    }
    
    if (current_quality_.reflection_quality > 1) {
        current_quality_.reflection_quality = 1;
    }
    
    current_quality_.clamp_to_valid_ranges();
}

void PerformanceOptimizer::apply_quality_optimizations() {
    float current_fps = profiler_.get_current_fps();
    float fps_ratio = current_fps / target_fps_;
    
    if (fps_ratio < 0.8f) {
        // Significant performance issues
        current_quality_.render_scale *= 0.9f;
        current_quality_.effect_quality = std::max(1, current_quality_.effect_quality - 1);
    } else if (fps_ratio < 0.95f) {
        // Minor performance issues
        current_quality_.render_scale *= 0.95f;
    } else if (fps_ratio > 1.2f && current_quality_.get_overall_quality_score() < default_quality_.get_overall_quality_score()) {
        // Performance headroom available, increase quality gradually
        current_quality_.render_scale = std::min(1.0f, current_quality_.render_scale * 1.02f);
        
        if (current_quality_.effect_quality < default_quality_.effect_quality) {
            current_quality_.effect_quality++;
        }
    }
    
    current_quality_.clamp_to_valid_ranges();
}

bool PerformanceOptimizer::should_adjust_quality() const {
    // Don't adjust too frequently
    if (time_since_quality_change_ < 2.0f) {
        return false;
    }
    
    float current_fps = profiler_.get_current_fps();
    
    // Always adjust if below minimum acceptable
    if (current_fps < min_acceptable_fps_) {
        return true;
    }
    
    // Adjust if significantly below target
    if (current_fps < target_fps_ * 0.9f) {
        return true;
    }
    
    // Increase quality if we have headroom
    if (current_fps > target_fps_ * 1.2f && 
        current_quality_.get_overall_quality_score() < default_quality_.get_overall_quality_score()) {
        return true;
    }
    
    return false;
}

float PerformanceOptimizer::calculate_quality_adjustment_factor() const {
    float current_fps = profiler_.get_current_fps();
    float fps_ratio = current_fps / target_fps_;
    
    if (fps_ratio < 0.5f) {
        return 0.7f;  // Aggressive reduction
    } else if (fps_ratio < 0.8f) {
        return 0.85f;  // Moderate reduction
    } else if (fps_ratio < 0.95f) {
        return 0.95f;  // Minor reduction
    } else if (fps_ratio > 1.2f) {
        return 1.05f;  // Minor increase
    }
    
    return 1.0f;  // No change
}

void PerformanceOptimizer::update_performance_history() {
    float current_fps = profiler_.get_current_fps();
    if (current_fps > 0.0f) {
        recent_frame_times_.push_back(1000.0f / current_fps);
    }
    
    // Estimate CPU and GPU utilization (simplified)
    float cpu_usage = estimate_cpu_utilization();
    float gpu_usage = estimate_gpu_utilization();
    
    recent_cpu_usage_.push_back(cpu_usage);
    recent_gpu_usage_.push_back(gpu_usage);
    recent_memory_usage_.push_back(profiler_.get_current_memory_usage());
    
    // Maintain rolling windows
    if (recent_frame_times_.size() > 120) {
        recent_frame_times_.erase(recent_frame_times_.begin());
        recent_cpu_usage_.erase(recent_cpu_usage_.begin());
        recent_gpu_usage_.erase(recent_gpu_usage_.begin());
        recent_memory_usage_.erase(recent_memory_usage_.begin());
    }
}

bool PerformanceOptimizer::is_memory_pressure_high() const {
    if (recent_memory_usage_.empty()) {
        return false;
    }
    
    size_t current_usage = recent_memory_usage_.back();
    size_t total_budget = (current_quality_.max_texture_memory_mb + current_quality_.max_buffer_memory_mb) * 1024 * 1024;
    
    return (float)current_usage / total_budget > memory_pressure_threshold_;
}

float PerformanceOptimizer::estimate_cpu_utilization() const {
    // Simplified CPU utilization estimation based on frame timing
    auto report = profiler_.generate_current_frame_report();
    
    if (report.total_frame_time_ms > 0.0f && report.cpu_time_ms > 0.0f) {
        return std::clamp(report.cpu_time_ms / report.total_frame_time_ms, 0.0f, 1.0f);
    }
    
    return 0.5f;  // Default estimate
}

float PerformanceOptimizer::estimate_gpu_utilization() const {
    // Simplified GPU utilization estimation
    auto report = profiler_.generate_current_frame_report();
    
    if (report.total_frame_time_ms > 0.0f && report.gpu_time_ms > 0.0f) {
        return std::clamp(report.gpu_time_ms / report.total_frame_time_ms, 0.0f, 1.0f);
    }
    
    return 0.5f;  // Default estimate
}

bool PerformanceOptimizer::is_synchronization_bottleneck() const {
    // Look for CPU-GPU sync issues in frame timing variance
    if (recent_frame_times_.size() < 30) {
        return false;
    }
    
    float mean = std::accumulate(recent_frame_times_.begin(), recent_frame_times_.end(), 0.0f) / recent_frame_times_.size();
    float variance = 0.0f;
    
    for (float time : recent_frame_times_) {
        variance += (time - mean) * (time - mean);
    }
    variance /= recent_frame_times_.size();
    
    // High variance relative to mean suggests sync issues
    return variance > mean * mean * 0.25f;
}

std::vector<std::string> PerformanceOptimizer::generate_cpu_suggestions() const {
    return {
        "Reduce draw call overhead by batching rendering operations",
        "Move expensive calculations to compute shaders",
        "Implement multithreading for CPU-intensive tasks",
        "Use object pooling to reduce allocation overhead",
        "Profile and optimize hot code paths in CPU profiler",
        "Reduce CPU-side validation and error checking in release builds"
    };
}

std::vector<std::string> PerformanceOptimizer::generate_gpu_suggestions() const {
    return {
        "Reduce rendering resolution or enable temporal upscaling",
        "Lower shadow quality or shadow map resolution",
        "Reduce particle count and complexity",
        "Implement level-of-detail (LOD) systems for complex geometry",
        "Optimize shaders to reduce ALU and texture operations",
        "Use occlusion culling to reduce overdraw",
        "Consider disabling expensive post-processing effects"
    };
}

std::vector<std::string> PerformanceOptimizer::generate_memory_suggestions() const {
    return {
        "Implement texture streaming to reduce memory usage",
        "Use compressed texture formats where possible",
        "Implement resource pooling for frequent allocations",
        "Reduce texture resolution for non-critical assets",
        "Use mipmapping and texture LOD to save memory",
        "Implement garbage collection for unused resources",
        "Consider using texture atlases to reduce memory fragmentation"
    };
}

void PerformanceOptimizer::force_quality_adjustment(float scale_factor) {
    current_quality_.render_scale *= scale_factor;
    current_quality_.shadow_quality = std::max(0, static_cast<int>(current_quality_.shadow_quality * scale_factor));
    current_quality_.effect_quality = std::max(0, static_cast<int>(current_quality_.effect_quality * scale_factor));
    current_quality_.particle_quality = std::max(0, static_cast<int>(current_quality_.particle_quality * scale_factor));
    
    if (scale_factor < 0.9f) {
        current_quality_.motion_blur = false;
        current_quality_.depth_of_field = false;
    }
    
    current_quality_.clamp_to_valid_ranges();
    time_since_quality_change_ = 0.0f;
    quality_recently_changed_ = true;
}

float PerformanceOptimizer::get_current_fps() const {
    return profiler_.get_current_fps();
}

bool PerformanceOptimizer::is_meeting_performance_targets() const {
    return get_current_fps() >= min_acceptable_fps_;
}

std::vector<std::string> PerformanceOptimizer::get_optimization_suggestions() const {
    std::vector<std::string> suggestions;
    
    switch (last_analysis_.primary_bottleneck) {
        case BottleneckType::CPU_BOUND:
            suggestions = generate_cpu_suggestions();
            break;
        case BottleneckType::GPU_BOUND:
            suggestions = generate_gpu_suggestions();
            break;
        case BottleneckType::MEMORY_BOUND:
            suggestions = generate_memory_suggestions();
            break;
        default:
            suggestions.push_back("Performance is within acceptable ranges");
            break;
    }
    
    return suggestions;
}

void PerformanceOptimizer::set_quality_settings(const QualitySettings& settings) {
    current_quality_ = settings;
    current_quality_.clamp_to_valid_ranges();
    time_since_quality_change_ = 0.0f;
}

void PerformanceOptimizer::reset_quality_to_defaults() {
    current_quality_ = default_quality_;
    time_since_quality_change_ = 0.0f;
}

void PerformanceOptimizer::log_bottleneck_analysis() const {
    CORE_INFO("=== Performance Analysis ===");
    CORE_INFO("Primary Bottleneck: {}", static_cast<int>(last_analysis_.primary_bottleneck));
    CORE_INFO("Confidence: {:.2f}", last_analysis_.confidence_score);
    CORE_INFO("Average Frame Time: {:.2f}ms", last_analysis_.avg_frame_time_ms);
    CORE_INFO("CPU Utilization: {:.1f}%", last_analysis_.cpu_utilization * 100.0f);
    CORE_INFO("GPU Utilization: {:.1f}%", last_analysis_.gpu_utilization * 100.0f);
    CORE_INFO("Memory Usage: {}MB", last_analysis_.memory_usage_mb);
    CORE_INFO("Current Quality Score: {:.2f}", current_quality_.get_overall_quality_score());
}

} // namespace gfx
