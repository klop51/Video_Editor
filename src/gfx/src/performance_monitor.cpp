// Week 8: Performance Monitor Implementation
// Real-time GPU performance tracking and analysis

#include "gfx/performance_monitor.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace ve::gfx {

// PerformanceTimeSeries Implementation
void PerformanceTimeSeries::add_point(float value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    data_points_.emplace_back(value);
    
    // Limit memory usage
    if (data_points_.size() > MAX_DATA_POINTS) {
        data_points_.erase(data_points_.begin(), data_points_.begin() + (data_points_.size() - MAX_DATA_POINTS));
    }
}

std::vector<PerformanceDataPoint> PerformanceTimeSeries::get_recent_data(std::chrono::milliseconds duration) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    std::vector<PerformanceDataPoint> result;
    
    for (const auto& point : data_points_) {
        if (point.timestamp >= cutoff_time) {
            result.push_back(point);
        }
    }
    
    return result;
}

float PerformanceTimeSeries::get_average(std::chrono::milliseconds duration) const {
    auto recent_data = get_recent_data(duration);
    
    if (recent_data.empty()) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (const auto& point : recent_data) {
        sum += point.value;
    }
    
    return sum / recent_data.size();
}

float PerformanceTimeSeries::get_minimum(std::chrono::milliseconds duration) const {
    auto recent_data = get_recent_data(duration);
    
    if (recent_data.empty()) {
        return 0.0f;
    }
    
    float min_val = recent_data[0].value;
    for (const auto& point : recent_data) {
        min_val = std::min(min_val, point.value);
    }
    
    return min_val;
}

float PerformanceTimeSeries::get_maximum(std::chrono::milliseconds duration) const {
    auto recent_data = get_recent_data(duration);
    
    if (recent_data.empty()) {
        return 0.0f;
    }
    
    float max_val = recent_data[0].value;
    for (const auto& point : recent_data) {
        max_val = std::max(max_val, point.value);
    }
    
    return max_val;
}

float PerformanceTimeSeries::get_latest() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (data_points_.empty()) {
        return 0.0f;
    }
    
    return data_points_.back().value;
}

void PerformanceTimeSeries::cleanup_old_data(std::chrono::milliseconds max_age) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    auto cutoff_time = std::chrono::steady_clock::now() - max_age;
    
    data_points_.erase(
        std::remove_if(data_points_.begin(), data_points_.end(),
                      [cutoff_time](const PerformanceDataPoint& point) {
                          return point.timestamp < cutoff_time;
                      }),
        data_points_.end()
    );
}

size_t PerformanceTimeSeries::size() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return data_points_.size();
}

// PerformanceMonitor Implementation
PerformanceMonitor::PerformanceMonitor(const Config& config) 
    : config_(config) {
    
    ve::log::info("Creating PerformanceMonitor");
    
    // Initialize timing
    last_update_time_ = std::chrono::high_resolution_clock::now();
    last_report_time_ = std::chrono::steady_clock::now();
    period_start_time_ = std::chrono::steady_clock::now();
    
    // Initialize time series for key metrics
    time_series_["frame_time_ms"] = PerformanceTimeSeries();
    time_series_["fps"] = PerformanceTimeSeries();
    time_series_["gpu_utilization"] = PerformanceTimeSeries();
    time_series_["memory_usage"] = PerformanceTimeSeries();
    time_series_["upload_bandwidth"] = PerformanceTimeSeries();
    time_series_["download_bandwidth"] = PerformanceTimeSeries();
    time_series_["cache_hit_rate"] = PerformanceTimeSeries();
    
    ve::log::debug("PerformanceMonitor initialized");
}

PerformanceMonitor::~PerformanceMonitor() {
    ve::log::info("PerformanceMonitor shutdown");
}

void PerformanceMonitor::begin_frame() {
    frame_start_time_ = std::chrono::high_resolution_clock::now();
    
    // Reset per-frame counters
    draw_calls_this_frame_.store(0);
    triangles_this_frame_.store(0);
    texture_switches_this_frame_.store(0);
    shader_switches_this_frame_.store(0);
    effects_this_frame_.store(0);
    cache_hits_this_frame_.store(0);
    cache_misses_this_frame_.store(0);
}

void PerformanceMonitor::end_frame() {
    auto frame_end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate frame time
    float frame_time_ms = std::chrono::duration<float, std::milli>(frame_end_time - frame_start_time_).count();
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        // Update frame statistics
        current_stats_.frame_time_ms = frame_time_ms;
        current_stats_.draw_calls_per_frame = draw_calls_this_frame_.load();
        current_stats_.triangles_per_frame = triangles_this_frame_.load();
        current_stats_.texture_switches_per_frame = texture_switches_this_frame_.load();
        current_stats_.shader_switches_per_frame = shader_switches_this_frame_.load();
        current_stats_.effects_processed_per_frame = effects_this_frame_.load();
        current_stats_.cache_hits_per_frame = cache_hits_this_frame_.load();
        current_stats_.cache_misses_per_frame = cache_misses_this_frame_.load();
        
        // Update totals
        current_stats_.total_frames_processed++;
        frames_this_period_.fetch_add(1);
        
        // Calculate FPS
        current_stats_.fps = calculate_fps();
        current_stats_.effective_fps = current_stats_.fps; // Simplified for now
        
        // Add to time series
        time_series_["frame_time_ms"].add_point(frame_time_ms);
        time_series_["fps"].add_point(current_stats_.fps);
        
        current_stats_.measurement_time = std::chrono::steady_clock::now();
    }
    
    // Periodic updates
    auto now = std::chrono::high_resolution_clock::now();
    auto update_interval = std::chrono::milliseconds(config_.update_interval_ms);
    
    if (now - last_update_time_ >= update_interval) {
        update_statistics();
        last_update_time_ = now;
    }
    
    // Automatic reports
    if (config_.enable_automatic_reports) {
        auto report_interval = std::chrono::seconds(config_.report_interval_seconds);
        auto report_now = std::chrono::steady_clock::now();
        
        if (report_now - last_report_time_ >= report_interval) {
            generate_automatic_report();
            last_report_time_ = report_now;
        }
    }
}

uint32_t PerformanceMonitor::begin_event(const std::string& event_name) {
    if (!config_.enable_detailed_timing) {
        return 0;
    }
    
    uint32_t event_id = next_event_id_.fetch_add(1);
    
    ProfilingEvent event;
    event.name = event_name;
    event.start_time = std::chrono::high_resolution_clock::now();
    event.is_active = true;
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        active_events_[event_id] = event;
    }
    
    return event_id;
}

void PerformanceMonitor::end_event(uint32_t event_id) {
    if (!config_.enable_detailed_timing || event_id == 0) {
        return;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto it = active_events_.find(event_id);
    if (it != active_events_.end() && it->second.is_active) {
        float duration_ms = std::chrono::duration<float, std::milli>(end_time - it->second.start_time).count();
        
        ve::log::debug("Event", it->second.name, "took", duration_ms, "ms");
        
        // Add to time series if this is a recognized event type
        auto series_it = time_series_.find(it->second.name);
        if (series_it != time_series_.end()) {
            series_it->second.add_point(duration_ms);
        }
        
        active_events_.erase(it);
    }
}

void PerformanceMonitor::record_memory_allocation(size_t bytes) {
    if (!config_.enable_memory_tracking) {
        return;
    }
    
    current_gpu_memory_.fetch_add(bytes);
    allocations_this_period_.fetch_add(1);
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    current_stats_.gpu_memory_used = current_gpu_memory_.load();
    current_stats_.memory_allocations_per_second = allocations_this_period_.load(); // Simplified
}

void PerformanceMonitor::record_memory_deallocation(size_t bytes) {
    if (!config_.enable_memory_tracking) {
        return;
    }
    
    current_gpu_memory_.fetch_sub(bytes);
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    current_stats_.gpu_memory_used = current_gpu_memory_.load();
}

void PerformanceMonitor::record_draw_call(size_t triangle_count) {
    if (!config_.enable_pipeline_stats) {
        return;
    }
    
    draw_calls_this_frame_.fetch_add(1);
    triangles_this_frame_.fetch_add(triangle_count);
}

void PerformanceMonitor::record_texture_switch() {
    if (!config_.enable_pipeline_stats) {
        return;
    }
    
    texture_switches_this_frame_.fetch_add(1);
}

void PerformanceMonitor::record_shader_switch() {
    if (!config_.enable_pipeline_stats) {
        return;
    }
    
    shader_switches_this_frame_.fetch_add(1);
}

void PerformanceMonitor::record_effect(const std::string& effect_name, float processing_time_ms) {
    if (!config_.enable_effect_profiling) {
        return;
    }
    
    effects_this_frame_.fetch_add(1);
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        // Update average effect time
        size_t total_effects = current_stats_.total_frames_processed * current_stats_.effects_processed_per_frame;
        if (total_effects == 0) {
            current_stats_.average_effect_time_ms = processing_time_ms;
        } else {
            current_stats_.average_effect_time_ms = 
                (current_stats_.average_effect_time_ms * total_effects + processing_time_ms) / (total_effects + 1);
        }
        
        // Add to time series
        time_series_[effect_name].add_point(processing_time_ms);
    }
}

void PerformanceMonitor::record_cache_access(bool was_hit) {
    if (was_hit) {
        cache_hits_this_frame_.fetch_add(1);
    } else {
        cache_misses_this_frame_.fetch_add(1);
    }
}

void PerformanceMonitor::record_upload(size_t bytes, float duration_ms) {
    bytes_uploaded_this_period_.fetch_add(bytes);
    
    if (duration_ms > 0.0f) {
        float mbps = (bytes / (1024.0f * 1024.0f)) / (duration_ms / 1000.0f);
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        current_stats_.upload_bandwidth_mbps = mbps;
        time_series_["upload_bandwidth"].add_point(mbps);
    }
}

void PerformanceMonitor::record_download(size_t bytes, float duration_ms) {
    bytes_downloaded_this_period_.fetch_add(bytes);
    
    if (duration_ms > 0.0f) {
        float mbps = (bytes / (1024.0f * 1024.0f)) / (duration_ms / 1000.0f);
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        current_stats_.download_bandwidth_mbps = mbps;
        time_series_["download_bandwidth"].add_point(mbps);
    }
}

void PerformanceMonitor::record_dropped_frame() {
    dropped_frames_this_period_.fetch_add(1);
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    current_stats_.dropped_frames = dropped_frames_this_period_.load();
}

void PerformanceMonitor::record_quality_change(bool was_upgrade) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (was_upgrade) {
        current_stats_.quality_upgrades++;
    } else {
        current_stats_.quality_downgrades++;
    }
}

void PerformanceMonitor::update_gpu_utilization(float gpu_percent, float memory_percent) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    current_stats_.gpu_utilization_percent = gpu_percent;
    current_stats_.memory_usage_percent = memory_percent;
    
    time_series_["gpu_utilization"].add_point(gpu_percent);
    time_series_["memory_usage"].add_point(memory_percent);
}

GPUPerformanceStats PerformanceMonitor::get_current_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_stats_;
}

const PerformanceTimeSeries* PerformanceMonitor::get_time_series(const std::string& metric_name) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto it = time_series_.find(metric_name);
    return (it != time_series_.end()) ? &it->second : nullptr;
}

std::string PerformanceMonitor::generate_report(std::chrono::milliseconds duration) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::ostringstream report;
    report << std::fixed << std::setprecision(2);
    
    report << "=== GPU Performance Report ===\n";
    report << "Report Duration: " << duration.count() << " ms\n\n";
    
    // Frame performance
    report << "Frame Performance:\n";
    report << "  Current FPS: " << current_stats_.fps << "\n";
    report << "  Frame Time: " << current_stats_.frame_time_ms << " ms\n";
    report << "  Dropped Frames: " << current_stats_.dropped_frames << "\n";
    
    // Get historical data
    auto frame_time_avg = time_series_.at("frame_time_ms").get_average(duration);
    auto frame_time_max = time_series_.at("frame_time_ms").get_maximum(duration);
    auto fps_avg = time_series_.at("fps").get_average(duration);
    auto fps_min = time_series_.at("fps").get_minimum(duration);
    
    report << "  Average Frame Time: " << frame_time_avg << " ms\n";
    report << "  Peak Frame Time: " << frame_time_max << " ms\n";
    report << "  Average FPS: " << fps_avg << "\n";
    report << "  Minimum FPS: " << fps_min << "\n\n";
    
    // GPU utilization
    report << "GPU Utilization:\n";
    report << "  Current GPU: " << current_stats_.gpu_utilization_percent << "%\n";
    report << "  Current Memory: " << current_stats_.memory_usage_percent << "%\n";
    
    auto gpu_util_avg = time_series_.at("gpu_utilization").get_average(duration);
    auto memory_util_avg = time_series_.at("memory_usage").get_average(duration);
    
    report << "  Average GPU: " << gpu_util_avg << "%\n";
    report << "  Average Memory: " << memory_util_avg << "%\n\n";
    
    // Memory statistics
    report << "Memory Usage:\n";
    report << "  Used: " << (current_stats_.gpu_memory_used / (1024*1024)) << " MB\n";
    report << "  Available: " << (current_stats_.gpu_memory_available / (1024*1024)) << " MB\n";
    report << "  Total: " << (current_stats_.gpu_memory_total / (1024*1024)) << " MB\n";
    report << "  Allocations/sec: " << current_stats_.memory_allocations_per_second << "\n\n";
    
    // Pipeline statistics
    report << "Rendering Pipeline:\n";
    report << "  Draw Calls/Frame: " << current_stats_.draw_calls_per_frame << "\n";
    report << "  Triangles/Frame: " << current_stats_.triangles_per_frame << "\n";
    report << "  Texture Switches/Frame: " << current_stats_.texture_switches_per_frame << "\n";
    report << "  Shader Switches/Frame: " << current_stats_.shader_switches_per_frame << "\n\n";
    
    // Effect processing
    report << "Effect Processing:\n";
    report << "  Effects/Frame: " << current_stats_.effects_processed_per_frame << "\n";
    report << "  Average Effect Time: " << current_stats_.average_effect_time_ms << " ms\n";
    report << "  Cache Hits/Frame: " << current_stats_.cache_hits_per_frame << "\n";
    report << "  Cache Misses/Frame: " << current_stats_.cache_misses_per_frame << "\n";
    
    size_t total_cache_accesses = current_stats_.cache_hits_per_frame + current_stats_.cache_misses_per_frame;
    if (total_cache_accesses > 0) {
        float hit_rate = (float(current_stats_.cache_hits_per_frame) / total_cache_accesses) * 100.0f;
        report << "  Cache Hit Rate: " << hit_rate << "%\n";
    }
    report << "\n";
    
    // Bandwidth
    report << "Bandwidth:\n";
    report << "  Upload: " << current_stats_.upload_bandwidth_mbps << " MB/s\n";
    report << "  Download: " << current_stats_.download_bandwidth_mbps << " MB/s\n";
    report << "  Total Uploaded: " << (current_stats_.total_bytes_uploaded / (1024*1024)) << " MB\n";
    report << "  Total Downloaded: " << (current_stats_.total_bytes_downloaded / (1024*1024)) << " MB\n\n";
    
    // Quality metrics
    report << "Quality Management:\n";
    report << "  Quality Score: " << current_stats_.average_quality_score << "\n";
    report << "  Quality Downgrades: " << current_stats_.quality_downgrades << "\n";
    report << "  Quality Upgrades: " << current_stats_.quality_upgrades << "\n";
    
    return report.str();
}

bool PerformanceMonitor::is_performance_degraded() const {
    return detect_performance_issues();
}

std::vector<std::string> PerformanceMonitor::get_performance_recommendations() const {
    return analyze_bottlenecks();
}

void PerformanceMonitor::reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    current_stats_.reset();
    frames_this_period_.store(0);
    dropped_frames_this_period_.store(0);
    current_gpu_memory_.store(0);
    allocations_this_period_.store(0);
    bytes_uploaded_this_period_.store(0);
    bytes_downloaded_this_period_.store(0);
    
    // Clear time series
    for (auto& [name, series] : time_series_) {
        series = PerformanceTimeSeries();
    }
    
    ve::log::info("Performance monitor statistics reset");
}

void PerformanceMonitor::update_config(const Config& new_config) {
    config_ = new_config;
    ve::log::info("Performance monitor configuration updated");
}

void PerformanceMonitor::update_statistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Update bandwidth stats
    update_bandwidth_stats();
    
    // Update cache hit rate
    size_t total_cache_accesses = current_stats_.cache_hits_per_frame + current_stats_.cache_misses_per_frame;
    if (total_cache_accesses > 0) {
        float hit_rate = (float(current_stats_.cache_hits_per_frame) / total_cache_accesses) * 100.0f;
        time_series_["cache_hit_rate"].add_point(hit_rate);
    }
    
    // Cleanup old data
    auto max_age = std::chrono::seconds(config_.history_duration_seconds);
    for (auto& [name, series] : time_series_) {
        series.cleanup_old_data(max_age);
    }
}

float PerformanceMonitor::calculate_fps() const {
    auto now = std::chrono::steady_clock::now();
    auto period_duration = std::chrono::duration<float>(now - period_start_time_).count();
    
    if (period_duration > 0.0f) {
        return frames_this_period_.load() / period_duration;
    }
    
    return 0.0f;
}

void PerformanceMonitor::update_bandwidth_stats() {
    auto now = std::chrono::steady_clock::now();
    auto period_duration = std::chrono::duration<float>(now - period_start_time_).count();
    
    if (period_duration > 0.0f) {
        float upload_mbps = (bytes_uploaded_this_period_.load() / (1024.0f * 1024.0f)) / period_duration;
        float download_mbps = (bytes_downloaded_this_period_.load() / (1024.0f * 1024.0f)) / period_duration;
        
        current_stats_.upload_bandwidth_mbps = upload_mbps;
        current_stats_.download_bandwidth_mbps = download_mbps;
        current_stats_.total_bytes_uploaded += bytes_uploaded_this_period_.exchange(0);
        current_stats_.total_bytes_downloaded += bytes_downloaded_this_period_.exchange(0);
    }
}

bool PerformanceMonitor::detect_performance_issues() const {
    // Check for common performance problems
    
    // Low FPS
    if (current_stats_.fps < 30.0f) {
        return true;
    }
    
    // High frame time variance
    auto frame_time_avg = time_series_.at("frame_time_ms").get_average(std::chrono::seconds(5));
    auto frame_time_max = time_series_.at("frame_time_ms").get_maximum(std::chrono::seconds(5));
    if (frame_time_max > frame_time_avg * 2.0f) {
        return true;  // High variance indicates stuttering
    }
    
    // High GPU memory usage
    if (current_stats_.memory_usage_percent > 90.0f) {
        return true;
    }
    
    // High dropped frame rate
    int total_frames = frames_this_period_.load();
    if (total_frames > 0 && (float(current_stats_.dropped_frames) / total_frames) > 0.05f) {
        return true;  // More than 5% dropped frames
    }
    
    return false;
}

std::vector<std::string> PerformanceMonitor::analyze_bottlenecks() const {
    std::vector<std::string> recommendations;
    
    // Frame rate analysis
    if (current_stats_.fps < 30.0f) {
        recommendations.push_back("Low FPS detected. Consider reducing effect quality or resolution.");
    }
    
    // Memory analysis
    if (current_stats_.memory_usage_percent > 80.0f) {
        recommendations.push_back("High GPU memory usage. Enable automatic eviction or reduce texture quality.");
    }
    
    // Pipeline analysis
    if (current_stats_.draw_calls_per_frame > 1000) {
        recommendations.push_back("High draw call count. Consider batching operations.");
    }
    
    if (current_stats_.texture_switches_per_frame > 50) {
        recommendations.push_back("High texture switching. Consider texture atlasing.");
    }
    
    // Cache analysis
    size_t total_cache_accesses = current_stats_.cache_hits_per_frame + current_stats_.cache_misses_per_frame;
    if (total_cache_accesses > 0) {
        float hit_rate = (float(current_stats_.cache_hits_per_frame) / total_cache_accesses) * 100.0f;
        if (hit_rate < 70.0f) {
            recommendations.push_back("Low cache hit rate. Increase cache size or improve cache locality.");
        }
    }
    
    // Bandwidth analysis
    if (current_stats_.upload_bandwidth_mbps < 100.0f) {  // Less than 100 MB/s
        recommendations.push_back("Low upload bandwidth. Consider async uploads or data compression.");
    }
    
    return recommendations;
}

void PerformanceMonitor::generate_automatic_report() {
    if (!config_.enable_automatic_reports) {
        return;
    }
    
    auto report = generate_report(std::chrono::minutes(1));
    ve::log::info("Performance Report:\n" + report);
    
    // Check for issues and log recommendations
    if (detect_performance_issues()) {
        auto recommendations = analyze_bottlenecks();
        ve::log::warning("Performance issues detected:");
        for (const auto& rec : recommendations) {
            ve::log::warning("  - " + rec);
        }
    }
}

} // namespace ve::gfx
