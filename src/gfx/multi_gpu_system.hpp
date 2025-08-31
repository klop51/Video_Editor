// Week 11: Multi-GPU & Hardware Acceleration System
// Advanced multi-GPU management and hardware decode/encode integration

#pragma once

#include "graphics_device.hpp"
#include "core/result.hpp"
#include "core/frame.hpp"
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <thread>

namespace VideoEditor::GFX {

// =============================================================================
// Forward Declarations
// =============================================================================

class MultiGPUManager;
class HardwareDecoder;
class HardwareEncoder;
class CrossDeviceMemoryManager;

// =============================================================================
// Multi-GPU Types and Enums
// =============================================================================

enum class GPUVendor {
    NVIDIA,
    AMD,
    INTEL,
    UNKNOWN
};

enum class GPUType {
    DISCRETE,      // Dedicated GPU
    INTEGRATED,    // Integrated GPU
    EXTERNAL,      // External GPU (Thunderbolt, etc.)
    VIRTUAL        // Virtual/Cloud GPU
};

enum class TaskType {
    DECODE,        // Video decode operations
    EFFECTS,       // Compute/graphics effects
    ENCODE,        // Video encode operations
    DISPLAY,       // Display composition
    COMPUTE,       // General compute tasks
    COPY,          // Memory copy operations
    PRESENT        // Final presentation
};

enum class WorkloadPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3,
    REALTIME = 4
};

enum class MemorySyncMode {
    IMMEDIATE,     // Synchronous copy
    DEFERRED,      // Asynchronous copy
    LAZY,          // Copy on demand
    PERSISTENT     // Keep synchronized
};

// =============================================================================
// GPU Device Information
// =============================================================================

struct GPUCapabilities {
    // Basic capabilities
    bool supports_d3d11_1;
    bool supports_d3d12;
    bool supports_vulkan;
    
    // Hardware decode support
    bool supports_h264_decode;
    bool supports_h265_decode;
    bool supports_av1_decode;
    bool supports_vp9_decode;
    
    // Hardware encode support
    bool supports_h264_encode;
    bool supports_h265_encode;
    bool supports_av1_encode;
    
    // Compute capabilities
    uint32_t max_compute_units;
    uint32_t max_threads_per_group;
    size_t shared_memory_size;
    bool supports_fp16;
    bool supports_int8;
    
    // Memory information
    size_t dedicated_video_memory;
    size_t dedicated_system_memory;
    size_t shared_system_memory;
    
    // Performance characteristics
    uint32_t memory_bandwidth_gb_s;
    uint32_t shader_units;
    uint32_t base_clock_mhz;
    uint32_t boost_clock_mhz;
    
    // Multi-GPU features
    bool supports_sli_crossfire;
    bool supports_multi_adapter;
    bool supports_linked_adapter;
};

struct GPUDeviceInfo {
    uint32_t adapter_index;
    std::string device_name;
    std::string driver_version;
    GPUVendor vendor;
    GPUType type;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t subsystem_id;
    uint32_t revision;
    
    GPUCapabilities capabilities;
    
    // Performance scoring
    float compute_score;
    float graphics_score;
    float video_score;
    float memory_score;
    float overall_score;
    
    // Current status
    bool is_available;
    bool is_primary;
    float current_utilization;
    size_t current_memory_usage;
    
    // Thermal and power
    uint32_t temperature_celsius;
    uint32_t power_usage_watts;
    uint32_t tdp_watts;
};

// =============================================================================
// Task Distribution and Scheduling
// =============================================================================

struct TaskRequest {
    std::string task_id;
    TaskType type;
    WorkloadPriority priority;
    size_t estimated_memory_mb;
    float estimated_duration_ms;
    
    // Resource requirements
    bool requires_hardware_decode;
    bool requires_hardware_encode;
    bool requires_high_memory_bandwidth;
    bool requires_low_latency;
    
    // Callback for completion
    std::function<void(const std::string&, bool)> completion_callback;
    
    // Custom data
    std::unordered_map<std::string, std::string> metadata;
};

struct TaskAssignment {
    TaskRequest request;
    uint32_t assigned_device_index;
    std::chrono::steady_clock::time_point scheduled_time;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point completion_time;
    bool is_executing;
    bool is_completed;
    std::string error_message;
};

// =============================================================================
// Multi-GPU Manager
// =============================================================================

class MultiGPUManager {
public:
    MultiGPUManager() = default;
    ~MultiGPUManager() = default;

    // System initialization
    Core::Result<void> initialize();
    void shutdown();

    // Device enumeration and management
    Core::Result<void> enumerate_devices();
    size_t get_device_count() const { return gpu_devices_.size(); }
    const GPUDeviceInfo* get_device_info(uint32_t device_index) const;
    std::vector<GPUDeviceInfo> get_all_device_info() const;

    // Device selection and allocation
    Core::Result<uint32_t> get_best_device_for_task(const TaskRequest& request);
    Core::Result<uint32_t> get_device_for_task_type(TaskType type, WorkloadPriority priority = WorkloadPriority::NORMAL);
    GraphicsDevice* get_graphics_device(uint32_t device_index);

    // Task scheduling and distribution
    Core::Result<std::string> schedule_task(const TaskRequest& request);
    Core::Result<void> execute_task(const std::string& task_id);
    Core::Result<void> cancel_task(const std::string& task_id);
    bool is_task_completed(const std::string& task_id) const;

    // Load balancing
    void enable_load_balancing(bool enabled) { load_balancing_enabled_ = enabled; }
    void set_load_balancing_strategy(const std::string& strategy) { load_balancing_strategy_ = strategy; }
    float get_device_utilization(uint32_t device_index) const;
    void balance_workload();

    // Multi-GPU coordination
    Core::Result<void> create_device_group(const std::vector<uint32_t>& device_indices, const std::string& group_name);
    Core::Result<void> destroy_device_group(const std::string& group_name);
    std::vector<std::string> get_device_groups() const;

    // Performance monitoring
    struct PerformanceMetrics {
        float total_gpu_utilization;
        size_t total_memory_usage_mb;
        uint32_t active_tasks;
        uint32_t queued_tasks;
        float average_task_completion_time_ms;
        std::vector<float> per_device_utilization;
        std::unordered_map<std::string, float> task_type_performance;
    };

    PerformanceMetrics get_performance_metrics() const;
    void enable_performance_monitoring(bool enabled) { performance_monitoring_enabled_ = enabled; }
    void reset_performance_counters();

    // Configuration and preferences
    void set_device_preference(TaskType task_type, uint32_t preferred_device_index);
    void set_memory_allocation_strategy(const std::string& strategy);
    void set_power_management_mode(const std::string& mode);

private:
    struct DeviceGroup {
        std::string name;
        std::vector<uint32_t> device_indices;
        bool linked_mode;
        float combined_score;
    };

    Core::Result<void> initialize_dxgi();
    Core::Result<void> query_device_capabilities(uint32_t adapter_index, GPUDeviceInfo& info);
    Core::Result<void> create_graphics_devices();
    
    float calculate_device_score(const GPUDeviceInfo& info, const TaskRequest& request) const;
    float calculate_task_affinity(const GPUDeviceInfo& info, TaskType task_type) const;
    
    void update_device_utilization();
    void process_task_queue();
    void monitor_device_performance();

    // DXGI and device management
    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory_;
    std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter4>> adapters_;
    std::vector<std::unique_ptr<GraphicsDevice>> graphics_devices_;
    std::vector<GPUDeviceInfo> gpu_devices_;

    // Task management
    std::queue<TaskAssignment> task_queue_;
    std::unordered_map<std::string, TaskAssignment> active_tasks_;
    std::unordered_map<std::string, TaskAssignment> completed_tasks_;
    std::mutex task_mutex_;

    // Device groups and load balancing
    std::unordered_map<std::string, DeviceGroup> device_groups_;
    std::unordered_map<TaskType, uint32_t> task_preferences_;
    bool load_balancing_enabled_ = true;
    std::string load_balancing_strategy_ = "performance";

    // Background processing
    std::thread task_processing_thread_;
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};

    // Performance tracking
    bool performance_monitoring_enabled_ = true;
    mutable std::mutex metrics_mutex_;
    PerformanceMetrics current_metrics_;
    std::chrono::steady_clock::time_point last_metrics_update_;

    // Configuration
    std::string memory_allocation_strategy_ = "balanced";
    std::string power_management_mode_ = "performance";
    uint32_t primary_device_index_ = 0;
};

// =============================================================================
// Cross-Device Memory Manager
// =============================================================================

class CrossDeviceMemoryManager {
public:
    CrossDeviceMemoryManager() = default;
    ~CrossDeviceMemoryManager() = default;

    // Initialization
    Core::Result<void> initialize(MultiGPUManager* gpu_manager);
    void shutdown();

    // Cross-device texture management
    class CrossDeviceTexture {
    public:
        CrossDeviceTexture() = default;
        ~CrossDeviceTexture();

        // Texture creation and management
        Core::Result<void> create(uint32_t width, uint32_t height, DXGI_FORMAT format, 
                                 const std::vector<uint32_t>& device_indices);
        void release();

        // Device-specific access
        ID3D11Texture2D* get_texture_for_device(uint32_t device_index);
        Core::Result<void> sync_to_device(uint32_t target_device_index, MemorySyncMode mode = MemorySyncMode::IMMEDIATE);
        Core::Result<void> sync_from_device(uint32_t source_device_index, MemorySyncMode mode = MemorySyncMode::IMMEDIATE);

        // Synchronization management
        Core::Result<void> sync_all_devices(uint32_t source_device_index);
        bool is_device_current(uint32_t device_index) const;
        void mark_device_dirty(uint32_t device_index);

        // Properties
        uint32_t get_width() const { return width_; }
        uint32_t get_height() const { return height_; }
        DXGI_FORMAT get_format() const { return format_; }
        const std::vector<uint32_t>& get_device_indices() const { return device_indices_; }

    private:
        struct DeviceTexture {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
            bool is_current;
            std::chrono::steady_clock::time_point last_update;
        };

        uint32_t width_ = 0;
        uint32_t height_ = 0;
        DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
        std::vector<uint32_t> device_indices_;
        std::unordered_map<uint32_t, DeviceTexture> device_textures_;
        MultiGPUManager* gpu_manager_ = nullptr;
        mutable std::mutex sync_mutex_;
    };

    // Cross-device buffer management
    class CrossDeviceBuffer {
    public:
        CrossDeviceBuffer() = default;
        ~CrossDeviceBuffer();

        Core::Result<void> create(size_t size, D3D11_USAGE usage, UINT bind_flags,
                                 const std::vector<uint32_t>& device_indices);
        void release();

        ID3D11Buffer* get_buffer_for_device(uint32_t device_index);
        Core::Result<void> sync_to_device(uint32_t target_device_index, MemorySyncMode mode = MemorySyncMode::IMMEDIATE);
        Core::Result<void> sync_from_device(uint32_t source_device_index, MemorySyncMode mode = MemorySyncMode::IMMEDIATE);

        size_t get_size() const { return size_; }
        const std::vector<uint32_t>& get_device_indices() const { return device_indices_; }

    private:
        struct DeviceBuffer {
            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
            bool is_current;
        };

        size_t size_ = 0;
        D3D11_USAGE usage_ = D3D11_USAGE_DEFAULT;
        UINT bind_flags_ = 0;
        std::vector<uint32_t> device_indices_;
        std::unordered_map<uint32_t, DeviceBuffer> device_buffers_;
        MultiGPUManager* gpu_manager_ = nullptr;
        mutable std::mutex sync_mutex_;
    };

    // Factory methods
    std::unique_ptr<CrossDeviceTexture> create_cross_device_texture();
    std::unique_ptr<CrossDeviceBuffer> create_cross_device_buffer();

    // Memory pool management
    void enable_memory_pooling(bool enabled) { memory_pooling_enabled_ = enabled; }
    void set_pool_size_limit_mb(size_t limit_mb) { pool_size_limit_mb_ = limit_mb; }
    void cleanup_unused_resources();
    size_t get_total_memory_usage_mb() const;

    // Performance monitoring
    struct MemoryMetrics {
        size_t total_cross_device_memory_mb;
        size_t cross_device_copies_per_second;
        float average_copy_bandwidth_gb_s;
        uint32_t active_cross_device_resources;
        std::unordered_map<uint32_t, size_t> per_device_memory_usage_mb;
    };

    MemoryMetrics get_memory_metrics() const;
    void enable_memory_profiling(bool enabled) { memory_profiling_enabled_ = enabled; }

private:
    Core::Result<void> perform_device_copy(uint32_t source_device, uint32_t target_device,
                                          ID3D11Resource* source_resource, ID3D11Resource* target_resource);
    
    MultiGPUManager* gpu_manager_ = nullptr;
    
    // Memory management
    bool memory_pooling_enabled_ = true;
    size_t pool_size_limit_mb_ = 1024;
    std::vector<std::unique_ptr<CrossDeviceTexture>> texture_pool_;
    std::vector<std::unique_ptr<CrossDeviceBuffer>> buffer_pool_;
    std::mutex pool_mutex_;

    // Performance tracking
    bool memory_profiling_enabled_ = false;
    mutable MemoryMetrics current_memory_metrics_;
    std::mutex metrics_mutex_;
};

// =============================================================================
// Multi-GPU Utility Functions
// =============================================================================

namespace MultiGPUUtils {
    // Device scoring and selection
    float calculate_performance_score(const GPUDeviceInfo& info);
    float calculate_task_compatibility_score(const GPUDeviceInfo& info, const TaskRequest& request);
    std::vector<uint32_t> rank_devices_for_task(const std::vector<GPUDeviceInfo>& devices, const TaskRequest& request);

    // Load balancing algorithms
    uint32_t select_device_round_robin(const std::vector<GPUDeviceInfo>& devices, uint32_t& last_selected);
    uint32_t select_device_lowest_utilization(const std::vector<GPUDeviceInfo>& devices);
    uint32_t select_device_best_fit(const std::vector<GPUDeviceInfo>& devices, const TaskRequest& request);

    // Performance estimation
    float estimate_task_duration(const TaskRequest& request, const GPUDeviceInfo& device);
    size_t estimate_memory_usage(const TaskRequest& request, const GPUDeviceInfo& device);
    float estimate_power_consumption(const TaskRequest& request, const GPUDeviceInfo& device);

    // Configuration helpers
    std::string get_vendor_name(GPUVendor vendor);
    std::string get_gpu_type_name(GPUType type);
    std::string get_task_type_name(TaskType type);
    bool is_nvidia_gpu(const GPUDeviceInfo& info);
    bool is_amd_gpu(const GPUDeviceInfo& info);
    bool is_intel_gpu(const GPUDeviceInfo& info);
}

} // namespace VideoEditor::GFX
