#include "detailed_profiler.hpp"
#include "../core/logging.hpp"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

namespace gfx {

// Global profiler instance
DetailedProfiler g_profiler;

// GPU Timer Implementation
GPUTimer::GPUTimer(ID3D11Device* device, ID3D11DeviceContext* context)
    : device_(device), context_(context), next_query_index_(0), last_frame_time_ms_(0.0f) {
    
    // Pre-allocate query objects for common profiling scenarios
    queries_.resize(64);  // Support up to 64 concurrent GPU events
    
    for (auto& query : queries_) {
        create_query(query);
    }
}

GPUTimer::~GPUTimer() {
    for (auto& query : queries_) {
        if (query.start_query) query.start_query->Release();
        if (query.end_query) query.end_query->Release();
        if (query.disjoint_query) query.disjoint_query->Release();
    }
}

void GPUTimer::create_query(TimingQuery& query) {
    D3D11_QUERY_DESC desc = {};
    
    // Timestamp queries
    desc.Query = D3D11_QUERY_TIMESTAMP;
    device_->CreateQuery(&desc, &query.start_query);
    device_->CreateQuery(&desc, &query.end_query);
    
    // Disjoint query for frequency
    desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    device_->CreateQuery(&desc, &query.disjoint_query);
}

void GPUTimer::begin_frame() {
    next_query_index_ = 0;
    while (!active_queries_.empty()) {
        active_queries_.pop();
    }
    
    // Start frame timing
    if (next_query_index_ < queries_.size()) {
        auto& query = queries_[next_query_index_];
        query.name = "Frame";
        query.active = true;
        
        context_->Begin(query.disjoint_query);
        context_->End(query.start_query);
        
        active_queries_.push(next_query_index_);
        next_query_index_++;
    }
}

void GPUTimer::begin_event(const std::string& name) {
    if (next_query_index_ >= queries_.size()) {
        // Pool exhausted, skip this event
        return;
    }
    
    auto& query = queries_[next_query_index_];
    query.name = name;
    query.active = true;
    
    context_->Begin(query.disjoint_query);
    context_->End(query.start_query);
    
    active_queries_.push(next_query_index_);
    next_query_index_++;
}

void GPUTimer::end_event() {
    if (active_queries_.empty()) {
        return;
    }
    
    size_t query_index = active_queries_.top();
    active_queries_.pop();
    
    auto& query = queries_[query_index];
    context_->End(query.end_query);
    context_->End(query.disjoint_query);
}

void GPUTimer::end_frame() {
    // End frame timing
    if (!active_queries_.empty()) {
        end_event();  // End the frame event
    }
    
    // Resolve all completed queries
    resolve_queries();
}

void GPUTimer::resolve_queries() {
    event_times_.clear();
    
    for (size_t i = 0; i < next_query_index_; ++i) {
        auto& query = queries_[i];
        if (!query.active) continue;
        
        // Check if results are available
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
        if (context_->GetData(query.disjoint_query, &disjoint_data, sizeof(disjoint_data), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            continue;  // Data not ready yet
        }
        
        if (disjoint_data.Disjoint) {
            continue;  // Invalid timing data
        }
        
        UINT64 start_time, end_time;
        if (context_->GetData(query.start_query, &start_time, sizeof(start_time), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            context_->GetData(query.end_query, &end_time, sizeof(end_time), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            continue;  // Data not ready
        }
        
        // Calculate timing in milliseconds
        float time_ms = (float)(end_time - start_time) / (float)disjoint_data.Frequency * 1000.0f;
        event_times_[query.name] = time_ms;
        
        if (query.name == "Frame") {
            last_frame_time_ms_ = time_ms;
        }
        
        query.active = false;
    }
}

// Memory Profiler Implementation
std::string MemoryProfiler::capture_stack_trace() const {
    // Simplified stack trace capture for Windows
    void* stack[16];
    WORD frames = CaptureStackBackTrace(0, 16, stack, nullptr);
    
    std::ostringstream oss;
    for (WORD i = 0; i < frames; ++i) {
        oss << std::hex << stack[i];
        if (i < frames - 1) oss << " -> ";
    }
    return oss.str();
}

void MemoryProfiler::record_allocation(void* ptr, size_t size, const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AllocationInfo info;
    info.size = size;
    info.category = category;
    info.timestamp = std::chrono::high_resolution_clock::now();
    info.stack_trace = capture_stack_trace();
    
    allocations_[ptr] = info;
    category_totals_[category] += size;
    total_allocated_ += size;
    peak_allocation_ = std::max(peak_allocation_, total_allocated_);
}

void MemoryProfiler::record_deallocation(void* ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
        const auto& info = it->second;
        category_totals_[info.category] -= info.size;
        total_allocated_ -= info.size;
        allocations_.erase(it);
    }
}

size_t MemoryProfiler::get_total_allocated() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_allocated_;
}

size_t MemoryProfiler::get_peak_allocation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peak_allocation_;
}

size_t MemoryProfiler::get_allocation_by_category(const std::string& category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = category_totals_.find(category);
    return it != category_totals_.end() ? it->second : 0;
}

void MemoryProfiler::generate_memory_report(std::ostream& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    out << "=== Memory Profiling Report ===\n";
    out << "Total Allocated: " << total_allocated_ / (1024 * 1024) << " MB\n";
    out << "Peak Allocation: " << peak_allocation_ / (1024 * 1024) << " MB\n";
    out << "Active Allocations: " << allocations_.size() << "\n\n";
    
    out << "Memory by Category:\n";
    for (const auto& [category, size] : category_totals_) {
        if (size > 0) {
            out << "  " << category << ": " << size / (1024 * 1024) << " MB\n";
        }
    }
}

// Detailed Profiler Implementation
DetailedProfiler::DetailedProfiler() {
    memory_profiler_ = std::make_unique<MemoryProfiler>();
    frame_history_.reserve(max_frames_to_keep_);
    recent_frame_times_.reserve(300);  // 5 seconds at 60fps
}

DetailedProfiler::~DetailedProfiler() = default;

void DetailedProfiler::begin_frame() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    frame_start_time_ = HighResolutionTimer::now();
    current_frame_number_++;
    
    // Clear any remaining blocks from previous frame
    while (!active_blocks_.empty()) {
        active_blocks_.pop();
    }
    
    if (gpu_timer_) {
        gpu_timer_->begin_frame();
    }
}

void DetailedProfiler::end_frame() {
    auto frame_end_time = HighResolutionTimer::now();
    auto frame_duration = frame_end_time - frame_start_time_;
    float frame_time_ms = HighResolutionTimer::to_milliseconds(frame_duration);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (gpu_timer_) {
        gpu_timer_->end_frame();
    }
    
    // Store frame data
    FrameData frame_data;
    frame_data.frame_number = current_frame_number_;
    frame_data.total_time_ms = frame_time_ms;
    frame_data.peak_memory = memory_profiler_->get_total_allocated();
    frame_data.timestamp = frame_end_time;
    
    // Move any remaining blocks to frame data
    std::vector<ProfileBlock> frame_blocks;
    while (!active_blocks_.empty()) {
        frame_blocks.push_back(std::move(active_blocks_.top()));
        active_blocks_.pop();
    }
    frame_data.blocks = std::move(frame_blocks);
    
    // Add to history
    frame_history_.push_back(std::move(frame_data));
    
    // Maintain history size limit
    if (frame_history_.size() > max_frames_to_keep_) {
        frame_history_.erase(frame_history_.begin());
    }
    
    // Update frame rate tracking
    update_frame_rate_tracking(frame_time_ms);
    last_frame_time_ = frame_end_time;
}

void DetailedProfiler::begin_block(const std::string& name, const std::string& category) {
    auto start_time = HighResolutionTimer::now();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    ProfileBlock block;
    block.name = name;
    block.category = category;
    block.cpu_start_ns = HighResolutionTimer::to_nanoseconds(start_time.time_since_epoch());
    block.thread_id = std::this_thread::get_id();
    block.frame_number = current_frame_number_;
    
    if (memory_profiling_enabled_) {
        block.memory_before = memory_profiler_->get_total_allocated();
    }
    
    active_blocks_.push(std::move(block));
    
    if (gpu_timer_ && gpu_profiling_enabled_) {
        gpu_timer_->begin_event(name);
    }
}

void DetailedProfiler::end_block() {
    auto end_time = HighResolutionTimer::now();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (active_blocks_.empty()) {
        return;  // Mismatched begin/end calls
    }
    
    ProfileBlock block = std::move(active_blocks_.top());
    active_blocks_.pop();
    
    block.cpu_end_ns = HighResolutionTimer::to_nanoseconds(end_time.time_since_epoch());
    
    if (memory_profiling_enabled_) {
        block.memory_after = memory_profiler_->get_total_allocated();
        block.memory_peak = memory_profiler_->get_peak_allocation();
    }
    
    if (gpu_timer_ && gpu_profiling_enabled_) {
        gpu_timer_->end_event();
        const auto& gpu_times = gpu_timer_->get_event_times();
        auto gpu_it = gpu_times.find(block.name);
        if (gpu_it != gpu_times.end()) {
            block.gpu_time_ms = gpu_it->second;
        }
    }
    
    // Add to parent block or frame data
    if (!active_blocks_.empty()) {
        active_blocks_.top().add_child(block);
    } else {
        // This is a root block, store it in current frame
        // For now, just store in the stack until frame ends
        active_blocks_.push(std::move(block));
    }
}

void DetailedProfiler::initialize_gpu_profiling(ID3D11Device* device, ID3D11DeviceContext* context) {
    std::lock_guard<std::mutex> lock(mutex_);
    gpu_timer_ = std::make_unique<GPUTimer>(device, context);
    gpu_profiling_enabled_ = true;
}

void DetailedProfiler::begin_gpu_event(const std::string& name) {
    if (gpu_timer_ && gpu_profiling_enabled_) {
        gpu_timer_->begin_event(name);
    }
}

void DetailedProfiler::end_gpu_event() {
    if (gpu_timer_ && gpu_profiling_enabled_) {
        gpu_timer_->end_event();
    }
}

void DetailedProfiler::record_allocation(void* ptr, size_t size, const std::string& category) {
    if (memory_profiling_enabled_) {
        memory_profiler_->record_allocation(ptr, size, category);
    }
}

void DetailedProfiler::record_deallocation(void* ptr) {
    if (memory_profiling_enabled_) {
        memory_profiler_->record_deallocation(ptr);
    }
}

float DetailedProfiler::get_current_fps() const {
    return current_fps_.load();
}

float DetailedProfiler::get_average_fps(size_t frame_count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (recent_frame_times_.empty()) {
        return 0.0f;
    }
    
    size_t count = std::min(frame_count, recent_frame_times_.size());
    float sum = std::accumulate(recent_frame_times_.end() - count, recent_frame_times_.end(), 0.0f);
    float avg_frame_time = sum / count;
    
    return avg_frame_time > 0.0f ? 1000.0f / avg_frame_time : 0.0f;
}

size_t DetailedProfiler::get_current_memory_usage() const {
    return memory_profiler_->get_total_allocated();
}

bool DetailedProfiler::is_performance_acceptable(float min_fps) const {
    return get_current_fps() >= min_fps;
}

ProfileReport DetailedProfiler::generate_current_frame_report() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ProfileReport report;
    
    if (!frame_history_.empty()) {
        const auto& last_frame = frame_history_.back();
        report.frame_number = last_frame.frame_number;
        report.total_frame_time_ms = last_frame.total_time_ms;
        report.peak_memory_usage = last_frame.peak_memory;
        report.root_blocks = last_frame.blocks;
        
        // Calculate CPU and GPU times
        float total_cpu_time = 0.0f;
        float total_gpu_time = 0.0f;
        
        for (const auto& block : last_frame.blocks) {
            total_cpu_time += block.get_cpu_duration_ms();
            total_gpu_time += block.gpu_time_ms;
        }
        
        report.cpu_time_ms = total_cpu_time;
        report.gpu_time_ms = total_gpu_time;
    }
    
    return report;
}

ProfileReport DetailedProfiler::generate_aggregate_report(size_t frame_count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ProfileReport report;
    
    if (frame_history_.empty()) {
        return report;
    }
    
    size_t count = std::min(frame_count, frame_history_.size());
    auto start_it = frame_history_.end() - count;
    
    // Aggregate statistics
    float total_frame_time = 0.0f;
    float total_cpu_time = 0.0f;
    float total_gpu_time = 0.0f;
    size_t max_memory = 0;
    
    for (auto it = start_it; it != frame_history_.end(); ++it) {
        total_frame_time += it->total_time_ms;
        max_memory = std::max(max_memory, it->peak_memory);
        
        for (const auto& block : it->blocks) {
            total_cpu_time += block.get_cpu_duration_ms();
            total_gpu_time += block.gpu_time_ms;
        }
    }
    
    report.total_frame_time_ms = total_frame_time / count;
    report.cpu_time_ms = total_cpu_time / count;
    report.gpu_time_ms = total_gpu_time / count;
    report.peak_memory_usage = max_memory;
    
    // Generate performance statistics
    report.stats.avg_frame_time_ms = report.total_frame_time_ms;
    report.stats.avg_cpu_utilization = (total_cpu_time / count) / (total_frame_time / count);
    report.stats.avg_gpu_utilization = (total_gpu_time / count) / (total_frame_time / count);
    report.stats.avg_memory_usage = max_memory;
    
    return report;
}

std::vector<std::string> DetailedProfiler::analyze_bottlenecks() const {
    auto report = generate_aggregate_report(60);
    std::vector<std::string> bottlenecks;
    
    // Simple bottleneck analysis
    if (report.stats.avg_cpu_utilization > 0.8f) {
        bottlenecks.push_back("CPU bound - high CPU utilization detected");
    }
    
    if (report.stats.avg_gpu_utilization > 0.8f) {
        bottlenecks.push_back("GPU bound - high GPU utilization detected");
    }
    
    if (report.stats.avg_memory_usage > 1024 * 1024 * 1024) {  // 1GB
        bottlenecks.push_back("Memory pressure - high memory usage detected");
    }
    
    if (report.stats.avg_frame_time_ms > 16.67f) {  // 60fps threshold
        bottlenecks.push_back("Frame rate below 60fps target");
    }
    
    return bottlenecks;
}

std::vector<std::string> DetailedProfiler::suggest_optimizations() const {
    std::vector<std::string> suggestions;
    auto bottlenecks = analyze_bottlenecks();
    
    for (const auto& bottleneck : bottlenecks) {
        if (bottleneck.find("CPU bound") != std::string::npos) {
            suggestions.push_back("Consider multithreading expensive operations");
            suggestions.push_back("Profile and optimize hot code paths");
            suggestions.push_back("Reduce CPU-side validation and calculations");
        }
        
        if (bottleneck.find("GPU bound") != std::string::npos) {
            suggestions.push_back("Reduce rendering resolution or quality");
            suggestions.push_back("Optimize shaders and reduce draw calls");
            suggestions.push_back("Use LOD systems for complex geometry");
        }
        
        if (bottleneck.find("Memory pressure") != std::string::npos) {
            suggestions.push_back("Implement texture and buffer pooling");
            suggestions.push_back("Use compression for large assets");
            suggestions.push_back("Implement aggressive garbage collection");
        }
    }
    
    return suggestions;
}

bool DetailedProfiler::is_frame_rate_stable(float target_fps) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (recent_frame_times_.size() < 60) {
        return false;  // Not enough data
    }
    
    float target_frame_time = 1000.0f / target_fps;
    size_t stable_frames = 0;
    
    for (size_t i = recent_frame_times_.size() - 60; i < recent_frame_times_.size(); ++i) {
        if (std::abs(recent_frame_times_[i] - target_frame_time) < target_frame_time * 0.1f) {
            stable_frames++;
        }
    }
    
    return (float)stable_frames / 60.0f > 0.9f;  // 90% of frames within 10% of target
}

void DetailedProfiler::update_frame_rate_tracking(float frame_time_ms) {
    recent_frame_times_.push_back(frame_time_ms);
    
    // Maintain rolling window
    if (recent_frame_times_.size() > 300) {
        recent_frame_times_.erase(recent_frame_times_.begin());
    }
    
    // Update current FPS
    if (frame_time_ms > 0.0f) {
        current_fps_.store(1000.0f / frame_time_ms);
    }
}

void DetailedProfiler::export_detailed_report(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return;
    }
    
    auto report = generate_aggregate_report();
    
    file << "=== Detailed Performance Report ===\n";
    file << "Average Frame Time: " << report.stats.avg_frame_time_ms << " ms\n";
    file << "Average FPS: " << (1000.0f / report.stats.avg_frame_time_ms) << "\n";
    file << "CPU Utilization: " << (report.stats.avg_cpu_utilization * 100.0f) << "%\n";
    file << "GPU Utilization: " << (report.stats.avg_gpu_utilization * 100.0f) << "%\n";
    file << "Peak Memory Usage: " << (report.peak_memory_usage / (1024 * 1024)) << " MB\n\n";
    
    auto bottlenecks = analyze_bottlenecks();
    if (!bottlenecks.empty()) {
        file << "Detected Bottlenecks:\n";
        for (const auto& bottleneck : bottlenecks) {
            file << "  - " << bottleneck << "\n";
        }
        file << "\n";
    }
    
    auto suggestions = suggest_optimizations();
    if (!suggestions.empty()) {
        file << "Optimization Suggestions:\n";
        for (const auto& suggestion : suggestions) {
            file << "  - " << suggestion << "\n";
        }
    }
    
    if (memory_profiling_enabled_) {
        file << "\n";
        memory_profiler_->generate_memory_report(file);
    }
}

} // namespace gfx
