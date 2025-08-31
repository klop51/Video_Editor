#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <stack>
#include <fstream>
#include <atomic>

namespace gfx {

// High-resolution timing utilities
class HighResolutionTimer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;

    static TimePoint now() {
        return Clock::now();
    }

    static uint64_t to_nanoseconds(const Duration& duration) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }

    static uint64_t to_microseconds(const Duration& duration) {
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }

    static uint64_t to_milliseconds(const Duration& duration) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

// GPU timing with D3D11 queries
class GPUTimer {
public:
    GPUTimer(ID3D11Device* device, ID3D11DeviceContext* context);
    ~GPUTimer();

    void begin_frame();
    void begin_event(const std::string& name);
    void end_event();
    void end_frame();

    float get_last_frame_time_ms() const { return last_frame_time_ms_; }
    const std::unordered_map<std::string, float>& get_event_times() const { return event_times_; }

private:
    struct TimingQuery {
        ID3D11Query* start_query = nullptr;
        ID3D11Query* end_query = nullptr;
        ID3D11Query* disjoint_query = nullptr;
        std::string name;
        bool active = false;
    };

    ID3D11Device* device_;
    ID3D11DeviceContext* context_;
    std::vector<TimingQuery> queries_;
    std::stack<size_t> active_queries_;
    size_t next_query_index_;
    float last_frame_time_ms_;
    std::unordered_map<std::string, float> event_times_;

    void create_query(TimingQuery& query);
    void resolve_queries();
};

// Memory tracking and profiling
class MemoryProfiler {
public:
    struct AllocationInfo {
        size_t size;
        std::string category;
        std::chrono::high_resolution_clock::time_point timestamp;
        std::string stack_trace;
    };

    void record_allocation(void* ptr, size_t size, const std::string& category);
    void record_deallocation(void* ptr);
    
    size_t get_total_allocated() const;
    size_t get_peak_allocation() const;
    size_t get_allocation_by_category(const std::string& category) const;
    
    void generate_memory_report(std::ostream& out) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<void*, AllocationInfo> allocations_;
    std::unordered_map<std::string, size_t> category_totals_;
    size_t total_allocated_ = 0;
    size_t peak_allocation_ = 0;
    
    std::string capture_stack_trace() const;
};

// Comprehensive profiling block with CPU, GPU, and memory tracking
struct ProfileBlock {
    std::string name;
    std::string category;
    uint64_t cpu_start_ns;
    uint64_t cpu_end_ns;
    float gpu_time_ms;
    size_t memory_before;
    size_t memory_after;
    size_t memory_peak;
    std::thread::id thread_id;
    uint32_t frame_number;
    std::vector<ProfileBlock> children;
    
    // Performance metrics
    uint64_t get_cpu_duration_ns() const { return cpu_end_ns - cpu_start_ns; }
    float get_cpu_duration_ms() const { return get_cpu_duration_ns() / 1000000.0f; }
    size_t get_memory_delta() const { return memory_after - memory_before; }
    
    // Utility functions
    void add_child(const ProfileBlock& child) { children.push_back(child); }
    bool has_children() const { return !children.empty(); }
};

// Main profiling report with comprehensive analysis
struct ProfileReport {
    uint32_t frame_number;
    float total_frame_time_ms;
    float cpu_time_ms;
    float gpu_time_ms;
    size_t peak_memory_usage;
    std::vector<ProfileBlock> root_blocks;
    
    // Performance statistics
    struct PerformanceStats {
        float avg_frame_time_ms;
        float min_frame_time_ms;
        float max_frame_time_ms;
        float avg_cpu_utilization;
        float avg_gpu_utilization;
        size_t avg_memory_usage;
        
        // Bottleneck analysis
        std::string primary_bottleneck;  // "CPU", "GPU", "Memory"
        std::vector<std::string> expensive_operations;
        std::vector<std::string> optimization_suggestions;
    } stats;
    
    // Export functionality
    void export_to_json(const std::string& filename) const;
    void export_to_csv(const std::string& filename) const;
    void export_chrome_trace(const std::string& filename) const;
};

// Advanced profiler with hierarchical timing and automatic analysis
class DetailedProfiler {
public:
    DetailedProfiler();
    ~DetailedProfiler();

    // Basic profiling interface
    void begin_frame();
    void end_frame();
    void begin_block(const std::string& name, const std::string& category = "General");
    void end_block();
    
    // Advanced profiling features
    void set_gpu_profiling_enabled(bool enabled) { gpu_profiling_enabled_ = enabled; }
    void set_memory_profiling_enabled(bool enabled) { memory_profiling_enabled_ = enabled; }
    void set_max_frames_to_keep(size_t max_frames) { max_frames_to_keep_ = max_frames; }
    
    // Report generation
    ProfileReport generate_current_frame_report() const;
    ProfileReport generate_aggregate_report(size_t frame_count = 60) const;
    void export_detailed_report(const std::string& filename) const;
    
    // Performance analysis
    std::vector<std::string> analyze_bottlenecks() const;
    std::vector<std::string> suggest_optimizations() const;
    bool is_frame_rate_stable(float target_fps = 60.0f) const;
    
    // GPU integration
    void initialize_gpu_profiling(ID3D11Device* device, ID3D11DeviceContext* context);
    void begin_gpu_event(const std::string& name);
    void end_gpu_event();
    
    // Memory integration
    void record_allocation(void* ptr, size_t size, const std::string& category);
    void record_deallocation(void* ptr);
    
    // Real-time monitoring
    float get_current_fps() const;
    float get_average_fps(size_t frame_count = 60) const;
    size_t get_current_memory_usage() const;
    bool is_performance_acceptable(float min_fps = 30.0f) const;

private:
    struct FrameData {
        uint32_t frame_number;
        std::vector<ProfileBlock> blocks;
        float total_time_ms;
        size_t peak_memory;
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    mutable std::mutex mutex_;
    std::stack<ProfileBlock> active_blocks_;
    std::vector<FrameData> frame_history_;
    std::unique_ptr<GPUTimer> gpu_timer_;
    std::unique_ptr<MemoryProfiler> memory_profiler_;
    
    // Configuration
    bool gpu_profiling_enabled_ = false;
    bool memory_profiling_enabled_ = true;
    size_t max_frames_to_keep_ = 300;  // 5 seconds at 60fps
    
    // Frame tracking
    uint32_t current_frame_number_ = 0;
    std::chrono::high_resolution_clock::time_point frame_start_time_;
    std::chrono::high_resolution_clock::time_point last_frame_time_;
    
    // Performance tracking
    mutable std::vector<float> recent_frame_times_;
    mutable std::atomic<float> current_fps_{0.0f};
    
    // Analysis helpers
    void update_frame_rate_tracking(float frame_time_ms);
    ProfileBlock create_block_from_stack() const;
    void analyze_frame_data(const FrameData& frame_data, ProfileReport::PerformanceStats& stats) const;
    std::string identify_bottleneck(const ProfileReport& report) const;
    std::vector<std::string> generate_optimization_suggestions(const ProfileReport& report) const;
};

// RAII profiling scope for automatic timing
class ProfileScope {
public:
    ProfileScope(DetailedProfiler& profiler, const std::string& name, const std::string& category = "General")
        : profiler_(profiler) {
        profiler_.begin_block(name, category);
    }
    
    ~ProfileScope() {
        profiler_.end_block();
    }

private:
    DetailedProfiler& profiler_;
};

// Convenient macros for profiling
#define PROFILE_SCOPE(profiler, name) \
    ProfileScope _profile_scope(profiler, name)

#define PROFILE_SCOPE_CATEGORY(profiler, name, category) \
    ProfileScope _profile_scope(profiler, name, category)

#define PROFILE_FUNCTION(profiler) \
    PROFILE_SCOPE(profiler, __FUNCTION__)

#define PROFILE_GPU_EVENT(profiler, name) \
    profiler.begin_gpu_event(name); \
    auto _gpu_event_guard = [&]() { profiler.end_gpu_event(); }; \
    (void)_gpu_event_guard

// Global profiler instance for easy access
extern DetailedProfiler g_profiler;

} // namespace gfx
