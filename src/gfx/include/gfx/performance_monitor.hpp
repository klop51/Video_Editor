// Week 8: Performance Monitoring Dashboard
// Real-time GPU performance tracking and analysis

#pragma once

#include <chrono>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>

namespace ve::gfx {

/**
 * @brief Detailed GPU performance metrics
 */
struct GPUPerformanceStats {
    // Frame timing
    float frame_time_ms = 0.0f;                  // Total frame processing time
    float render_time_ms = 0.0f;                 // GPU rendering time
    float upload_time_ms = 0.0f;                 // Texture upload time
    float download_time_ms = 0.0f;               // Texture download time
    float present_time_ms = 0.0f;                // Present/swap time
    
    // Throughput metrics
    float fps = 0.0f;                            // Frames per second
    float effective_fps = 0.0f;                  // Accounting for dropped frames
    int dropped_frames = 0;                      // Frames dropped this period
    int total_frames_processed = 0;              // Total frames processed
    
    // GPU utilization
    float gpu_utilization_percent = 0.0f;       // GPU busy percentage
    float shader_utilization_percent = 0.0f;    // Shader unit utilization
    float memory_bandwidth_utilization = 0.0f;  // Memory bandwidth usage
    
    // Memory statistics
    size_t gpu_memory_used = 0;                  // Currently used VRAM
    size_t gpu_memory_available = 0;             // Available VRAM
    size_t gpu_memory_total = 0;                 // Total VRAM
    float memory_usage_percent = 0.0f;           // Memory utilization
    size_t memory_allocations_per_second = 0;    // Allocation rate
    
    // Pipeline statistics
    size_t draw_calls_per_frame = 0;             // Draw calls issued
    size_t triangles_per_frame = 0;              // Triangles rendered
    size_t texture_switches_per_frame = 0;       // Texture binding changes
    size_t shader_switches_per_frame = 0;        // Shader program changes
    
    // Effect processing
    size_t effects_processed_per_frame = 0;      // Effects applied per frame
    float average_effect_time_ms = 0.0f;         // Average effect processing time
    size_t cache_hits_per_frame = 0;             // Effect cache hits
    size_t cache_misses_per_frame = 0;           // Effect cache misses
    
    // Bandwidth metrics
    float upload_bandwidth_mbps = 0.0f;          // Upload bandwidth (MB/s)
    float download_bandwidth_mbps = 0.0f;        // Download bandwidth (MB/s)
    size_t total_bytes_uploaded = 0;             // Total upload volume
    size_t total_bytes_downloaded = 0;           // Total download volume
    
    // Quality metrics
    float average_quality_score = 0.0f;          // Rendering quality metric
    int quality_downgrades = 0;                  // Times quality was reduced
    int quality_upgrades = 0;                    // Times quality was increased
    
    // Timestamps
    std::chrono::steady_clock::time_point measurement_time;
    std::chrono::steady_clock::time_point period_start;
    
    void reset() {
        *this = GPUPerformanceStats{};
        measurement_time = std::chrono::steady_clock::now();
        period_start = measurement_time;
    }
};

/**
 * @brief Historical performance data point
 */
struct PerformanceDataPoint {
    std::chrono::steady_clock::time_point timestamp;
    float value;
    
    PerformanceDataPoint() = default;
    PerformanceDataPoint(float val) : timestamp(std::chrono::steady_clock::now()), value(val) {}
};

/**
 * @brief Time series data for performance metrics
 */
class PerformanceTimeSeries {
public:
    /**
     * @brief Add data point to time series
     */
    void add_point(float value);
    
    /**
     * @brief Get data points within time range
     * @param duration Look-back duration
     * @return Vector of data points
     */
    std::vector<PerformanceDataPoint> get_recent_data(std::chrono::milliseconds duration) const;
    
    /**
     * @brief Get average value over time period
     */
    float get_average(std::chrono::milliseconds duration) const;
    
    /**
     * @brief Get minimum value over time period
     */
    float get_minimum(std::chrono::milliseconds duration) const;
    
    /**
     * @brief Get maximum value over time period
     */
    float get_maximum(std::chrono::milliseconds duration) const;
    
    /**
     * @brief Get latest value
     */
    float get_latest() const;
    
    /**
     * @brief Clear old data points
     * @param max_age Maximum age to keep
     */
    void cleanup_old_data(std::chrono::milliseconds max_age);
    
    /**
     * @brief Get number of data points
     */
    size_t size() const;

private:
    mutable std::mutex data_mutex_;
    std::vector<PerformanceDataPoint> data_points_;
    static constexpr size_t MAX_DATA_POINTS = 10000;  // Limit memory usage
};

/**
 * @brief Performance monitoring and analysis system
 */
class PerformanceMonitor {
public:
    /**
     * @brief Configuration for performance monitoring
     */
    struct Config {
        bool enable_detailed_timing = true;       // Track detailed frame timing
        bool enable_gpu_profiling = true;         // Use GPU timestamp queries
        bool enable_memory_tracking = true;       // Track memory allocations
        bool enable_pipeline_stats = true;        // Track draw calls, etc.
        bool enable_effect_profiling = true;      // Profile individual effects
        uint32_t update_interval_ms = 16;         // Update every 16ms (60 FPS)
        uint32_t history_duration_seconds = 300;  // Keep 5 minutes of history
        bool enable_automatic_reports = true;     // Generate periodic reports
        uint32_t report_interval_seconds = 60;    // Report every minute
    };
    
    /**
     * @brief Create performance monitor
     * @param config Configuration options
     */
    explicit PerformanceMonitor(const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~PerformanceMonitor();
    
    // Non-copyable
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
    
    /**
     * @brief Begin frame timing measurement
     */
    void begin_frame();
    
    /**
     * @brief End frame timing measurement
     */
    void end_frame();
    
    /**
     * @brief Begin profiling event
     * @param event_name Name of the event
     * @return Event ID for ending the event
     */
    uint32_t begin_event(const std::string& event_name);
    
    /**
     * @brief End profiling event
     * @param event_id Event ID returned by begin_event
     */
    void end_event(uint32_t event_id);
    
    /**
     * @brief Record GPU memory allocation
     * @param bytes Size of allocation
     */
    void record_memory_allocation(size_t bytes);
    
    /**
     * @brief Record GPU memory deallocation
     * @param bytes Size of deallocation
     */
    void record_memory_deallocation(size_t bytes);
    
    /**
     * @brief Record draw call
     * @param triangle_count Number of triangles drawn
     */
    void record_draw_call(size_t triangle_count = 0);
    
    /**
     * @brief Record texture binding change
     */
    void record_texture_switch();
    
    /**
     * @brief Record shader program change
     */
    void record_shader_switch();
    
    /**
     * @brief Record effect processing
     * @param effect_name Name of the effect
     * @param processing_time_ms Time taken to process
     */
    void record_effect(const std::string& effect_name, float processing_time_ms);
    
    /**
     * @brief Record cache hit/miss
     * @param was_hit True for cache hit, false for miss
     */
    void record_cache_access(bool was_hit);
    
    /**
     * @brief Record data upload
     * @param bytes Number of bytes uploaded
     * @param duration_ms Time taken for upload
     */
    void record_upload(size_t bytes, float duration_ms);
    
    /**
     * @brief Record data download
     * @param bytes Number of bytes downloaded
     * @param duration_ms Time taken for download
     */
    void record_download(size_t bytes, float duration_ms);
    
    /**
     * @brief Record frame drop
     */
    void record_dropped_frame();
    
    /**
     * @brief Record quality change
     * @param was_upgrade True if quality increased, false if decreased
     */
    void record_quality_change(bool was_upgrade);
    
    /**
     * @brief Update GPU utilization metrics
     * @param gpu_percent GPU utilization percentage
     * @param memory_percent Memory utilization percentage
     */
    void update_gpu_utilization(float gpu_percent, float memory_percent);
    
    /**
     * @brief Get current performance statistics
     */
    GPUPerformanceStats get_current_stats() const;
    
    /**
     * @brief Get performance time series for metric
     * @param metric_name Name of the metric
     * @return Time series data
     */
    const PerformanceTimeSeries* get_time_series(const std::string& metric_name) const;
    
    /**
     * @brief Generate performance report
     * @param duration Report duration
     * @return Formatted performance report
     */
    std::string generate_report(std::chrono::milliseconds duration = std::chrono::minutes(5)) const;
    
    /**
     * @brief Check if performance is degraded
     * @return True if performance issues detected
     */
    bool is_performance_degraded() const;
    
    /**
     * @brief Get performance recommendations
     * @return List of optimization suggestions
     */
    std::vector<std::string> get_performance_recommendations() const;
    
    /**
     * @brief Reset all statistics and history
     */
    void reset();
    
    /**
     * @brief Update configuration
     * @param new_config New configuration
     */
    void update_config(const Config& new_config);
    
    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }

private:
    /**
     * @brief Active profiling event
     */
    struct ProfilingEvent {
        std::string name;
        std::chrono::high_resolution_clock::time_point start_time;
        bool is_active;
    };
    
    // Core functionality
    void update_statistics();
    void cleanup_old_data();
    void generate_automatic_report();
    float calculate_fps() const;
    float calculate_frame_time() const;
    void update_bandwidth_stats();
    
    // Analysis
    bool detect_performance_issues() const;
    std::vector<std::string> analyze_bottlenecks() const;
    
    // Configuration
    Config config_;
    
    // Timing and synchronization
    mutable std::mutex stats_mutex_;
    std::chrono::high_resolution_clock::time_point frame_start_time_;
    std::chrono::high_resolution_clock::time_point last_update_time_;
    std::chrono::steady_clock::time_point last_report_time_;
    
    // Current statistics
    GPUPerformanceStats current_stats_;
    
    // Historical data
    std::unordered_map<std::string, PerformanceTimeSeries> time_series_;
    
    // Event tracking
    std::unordered_map<uint32_t, ProfilingEvent> active_events_;
    std::atomic<uint32_t> next_event_id_{1};
    
    // Frame counting
    std::atomic<int> frames_this_period_{0};
    std::atomic<int> dropped_frames_this_period_{0};
    
    // Memory tracking
    std::atomic<size_t> current_gpu_memory_{0};
    std::atomic<size_t> allocations_this_period_{0};
    
    // Pipeline statistics (reset each frame)
    std::atomic<size_t> draw_calls_this_frame_{0};
    std::atomic<size_t> triangles_this_frame_{0};
    std::atomic<size_t> texture_switches_this_frame_{0};
    std::atomic<size_t> shader_switches_this_frame_{0};
    
    // Effect statistics
    std::atomic<size_t> effects_this_frame_{0};
    std::atomic<size_t> cache_hits_this_frame_{0};
    std::atomic<size_t> cache_misses_this_frame_{0};
    
    // Bandwidth tracking
    std::atomic<size_t> bytes_uploaded_this_period_{0};
    std::atomic<size_t> bytes_downloaded_this_period_{0};
    std::chrono::steady_clock::time_point period_start_time_;
};

} // namespace ve::gfx
