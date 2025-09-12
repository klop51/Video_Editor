// Week 10: Compute Shader System
// Advanced GPU compute capabilities for sophisticated video processing

#pragma once

#include "../core/include/core/result.hpp"
#include "include/gfx/graphics_device.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

namespace video_editor::gfx {

// =============================================================================
// Forward Declarations
// =============================================================================

class ComputeShaderImpl;
class ComputeContext;
class ComputeBuffer;
class ComputeTexture;

// =============================================================================
// Compute Shader Resource Types
// =============================================================================

enum class ComputeResourceType {
    CONSTANT_BUFFER,
    STRUCTURED_BUFFER,
    TEXTURE_2D,
    TEXTURE_3D,
    UNORDERED_ACCESS_VIEW,
    SHADER_RESOURCE_VIEW
};

enum class ComputeBufferUsage {
    DEFAULT,
    IMMUTABLE,
    DYNAMIC,
    STAGING
};

enum class ComputeDataType {
    FLOAT,
    FLOAT2,
    FLOAT3,
    FLOAT4,
    INT,
    INT2,
    INT3,
    INT4,
    UINT,
    UINT2,
    UINT3,
    UINT4
};

// =============================================================================
// Compute Shader Parameters
// =============================================================================

struct ComputeDispatchParams {
    uint32_t thread_groups_x = 1;
    uint32_t thread_groups_y = 1;
    uint32_t thread_groups_z = 1;
    uint32_t threads_per_group_x = 1;
    uint32_t threads_per_group_y = 1;
    uint32_t threads_per_group_z = 1;
};

struct ComputeShaderSystemDesc {
    std::string shader_source;
    std::string entry_point = "cs_main";
    std::string shader_model = "cs_5_0";
    std::vector<std::string> defines;
    bool enable_debug = false;
};

struct ComputeBufferDesc {
    size_t element_size = 0;
    size_t element_count = 0;
    ComputeBufferUsage usage = ComputeBufferUsage::DEFAULT;
    ComputeDataType data_type = ComputeDataType::FLOAT;
    bool allow_raw_views = false;
    bool allow_unordered_access = false;
    bool cpu_accessible = false;
};

// =============================================================================
// Compute Performance Metrics
// =============================================================================

struct ComputePerformanceMetrics {
    float dispatch_time_ms = 0.0f;
    float gpu_execution_time_ms = 0.0f;
    float memory_bandwidth_gb_s = 0.0f;
    uint64_t operations_per_second = 0;
    size_t memory_used_bytes = 0;
    size_t memory_transferred_bytes = 0;
    int active_thread_groups = 0;
    float gpu_utilization_percent = 0.0f;
};

// =============================================================================
// Compute Buffer Class
// =============================================================================

class ComputeBuffer {
public:
    ComputeBuffer() = default;
    ~ComputeBuffer() = default;

    // Buffer management
    ve::core::Result<bool> create(GraphicsDevice* device, const ComputeBufferDesc& desc);
    ve::core::Result<bool> upload_data(const void* data, size_t size);
    ve::core::Result<bool> download_data(void* data, size_t size);
    void release();

    // Resource views
    ID3D11ShaderResourceView* get_srv() const { return srv_.Get(); }
    ID3D11UnorderedAccessView* get_uav() const { return uav_.Get(); }
    ID3D11Buffer* get_buffer() const { return buffer_.Get(); }

    // Properties
    size_t get_element_size() const { return desc_.element_size; }
    size_t get_element_count() const { return desc_.element_count; }
    size_t get_total_size() const { return desc_.element_size * desc_.element_count; }
    ComputeDataType get_data_type() const { return desc_.data_type; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> staging_buffer_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    ComputeBufferDesc desc_;
    GraphicsDevice* device_ = nullptr;
};

// =============================================================================
// Compute Texture Class
// =============================================================================

class ComputeTexture {
public:
    ComputeTexture() = default;
    ~ComputeTexture() = default;

    // Texture management
    ve::core::Result<bool> create_2d(GraphicsDevice* device, uint32_t width, uint32_t height, 
                                 DXGI_FORMAT format, bool allow_uav = true);
    ve::core::Result<bool> create_3d(GraphicsDevice* device, uint32_t width, uint32_t height, 
                                 uint32_t depth, DXGI_FORMAT format, bool allow_uav = true);
    void release();

    // Resource views
    ID3D11ShaderResourceView* get_srv() const { return srv_.Get(); }
    ID3D11UnorderedAccessView* get_uav() const { return uav_.Get(); }
    ID3D11Texture2D* get_texture_2d() const { return texture_2d_.Get(); }
    ID3D11Texture3D* get_texture_3d() const { return texture_3d_.Get(); }

    // Properties
    uint32_t get_width() const { return width_; }
    uint32_t get_height() const { return height_; }
    uint32_t get_depth() const { return depth_; }
    DXGI_FORMAT get_format() const { return format_; }
    bool is_3d() const { return is_3d_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_2d_;
    Microsoft::WRL::ComPtr<ID3D11Texture3D> texture_3d_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t depth_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    bool is_3d_ = false;
};

// =============================================================================
// Compute Shader Class
// =============================================================================

class ComputeShaderImpl {
public:
    ComputeShaderImpl() = default;
    ~ComputeShaderImpl() = default;

    // Shader compilation and management
    ve::core::Result<bool> create_from_source(GraphicsDevice* device, const ComputeShaderSystemDesc& desc);
    ve::core::Result<bool> create_from_file(GraphicsDevice* device, const std::string& file_path,
                                       const std::string& entry_point = "cs_main");
    void release();

    // Resource binding
    void bind_constant_buffer(uint32_t slot, ComputeBuffer* buffer);
    void bind_structured_buffer(uint32_t slot, ComputeBuffer* buffer);
    void bind_texture_srv(uint32_t slot, ComputeTexture* texture);
    void bind_texture_uav(uint32_t slot, ComputeTexture* texture);
    void bind_buffer_uav(uint32_t slot, ComputeBuffer* buffer);

    // Execution
    ve::core::Result<ComputePerformanceMetrics> dispatch(const ComputeDispatchParams& params);
    ve::core::Result<ComputePerformanceMetrics> dispatch_1d(uint32_t num_elements, uint32_t threads_per_group = 64);
    ve::core::Result<ComputePerformanceMetrics> dispatch_2d(uint32_t width, uint32_t height, 
                                                        uint32_t threads_x = 8, uint32_t threads_y = 8);
    ve::core::Result<ComputePerformanceMetrics> dispatch_3d(uint32_t width, uint32_t height, uint32_t depth,
                                                        uint32_t threads_x = 4, uint32_t threads_y = 4, uint32_t threads_z = 4);

    // Properties
    const std::string& get_entry_point() const { return entry_point_; }
    const std::vector<std::string>& get_defines() const { return defines_; }
    bool is_valid() const { return shader_ != nullptr; }

private:
    ve::core::Result<bool> compile_shader(const std::string& source, const ComputeShaderSystemDesc& desc);
    void clear_bindings();
    ComputePerformanceMetrics measure_performance(const ComputeDispatchParams& params);

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    GraphicsDevice* device_ = nullptr;

    std::string entry_point_;
    std::vector<std::string> defines_;
    
    // Resource bindings
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> bound_constant_buffers_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> bound_srvs_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> bound_uavs_;
    
    // Performance monitoring
    Microsoft::WRL::ComPtr<ID3D11Query> timestamp_start_;
    Microsoft::WRL::ComPtr<ID3D11Query> timestamp_end_;
    Microsoft::WRL::ComPtr<ID3D11Query> timestamp_disjoint_;
};

// =============================================================================
// Compute Context Class
// =============================================================================

class ComputeContext {
public:
    ComputeContext() = default;
    ~ComputeContext() = default;

    // Context management
    ve::core::Result<bool> initialize(GraphicsDevice* device);
    void shutdown();

    // Resource creation
    std::unique_ptr<ComputeShaderImpl> create_shader();
    std::unique_ptr<ComputeBuffer> create_buffer();
    std::unique_ptr<ComputeTexture> create_texture();

    // Batch operations
    void begin_batch();
    void end_batch();
    ve::core::Result<bool> execute_batch();

    // Memory management
    void flush_gpu_cache();
    size_t get_gpu_memory_usage() const;
    void cleanup_temporary_resources();

    // Performance monitoring
    ComputePerformanceMetrics get_accumulated_metrics() const { return accumulated_metrics_; }
    void reset_performance_metrics();
    void enable_profiling(bool enabled) { profiling_enabled_ = enabled; }

private:
    GraphicsDevice* device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context_;

    // Batch processing
    bool batch_mode_ = false;
    std::vector<std::function<void()>> batched_operations_;

    // Performance tracking
    bool profiling_enabled_ = false;
    ComputePerformanceMetrics accumulated_metrics_;
    std::vector<std::unique_ptr<ComputeShaderImpl>> active_shaders_;
    std::vector<std::unique_ptr<ComputeBuffer>> active_buffers_;
    std::vector<std::unique_ptr<ComputeTexture>> active_textures_;
};

// =============================================================================
// Compute Shader System Class
// =============================================================================

class ComputeShaderSystem {
public:
    ComputeShaderSystem() = default;
    ~ComputeShaderSystem() = default;

    // System initialization
    ve::core::Result<bool> initialize(GraphicsDevice* device);
    void shutdown();

    // Context management
    ComputeContext* get_primary_context() { return primary_context_.get(); }
    std::unique_ptr<ComputeContext> create_context();

    // Shader library management
    ve::core::Result<ComputeShaderImpl*> load_shader(const std::string& name, const std::string& file_path);
    ComputeShaderImpl* get_shader(const std::string& name);
    void precompile_common_shaders();

    // System capabilities
    struct ComputeCapabilities {
        uint32_t max_thread_groups_x;
        uint32_t max_thread_groups_y;
        uint32_t max_thread_groups_z;
        uint32_t max_threads_per_group;
        uint32_t max_shared_memory_size;
        bool supports_double_precision;
        bool supports_atomic_operations;
        bool supports_wave_intrinsics;
        uint32_t wave_size;
    };

    const ComputeCapabilities& get_capabilities() const { return capabilities_; }

    // Performance and monitoring
    ComputePerformanceMetrics get_system_metrics() const;
    void enable_system_profiling(bool enabled);
    void log_performance_summary() const;

private:
    void query_compute_capabilities();
    void setup_performance_monitoring();

    GraphicsDevice* device_ = nullptr;
    std::unique_ptr<ComputeContext> primary_context_;
    
    // Shader library
    std::unordered_map<std::string, std::unique_ptr<ComputeShaderImpl>> shader_library_;
    
    // System info
    ComputeCapabilities capabilities_;
    bool system_profiling_enabled_ = false;
    
    // Performance tracking
    mutable ComputePerformanceMetrics system_metrics_;
    std::chrono::steady_clock::time_point last_metrics_update_;
};

// =============================================================================
// Compute Shader Helper Macros
// =============================================================================

#define COMPUTE_SHADER_ENTRY(name) \
    extern "C" void name(uint3 dispatch_thread_id : SV_DispatchThreadID, \
                        uint3 group_thread_id : SV_GroupThreadID, \
                        uint3 group_id : SV_GroupID, \
                        uint group_index : SV_GroupIndex)

#define COMPUTE_THREADS(x, y, z) [numthreads(x, y, z)]

// =============================================================================
// Common Compute Shader Utilities
// =============================================================================

namespace ComputeUtils {
    // Calculate optimal thread group dimensions
    ComputeDispatchParams calculate_dispatch_params(uint32_t width, uint32_t height = 1, uint32_t depth = 1,
                                                   uint32_t preferred_group_size_x = 8,
                                                   uint32_t preferred_group_size_y = 8,
                                                   uint32_t preferred_group_size_z = 1);

    // Calculate required shared memory size
    size_t calculate_shared_memory_size(ComputeDataType data_type, uint32_t element_count);

    // Validate compute shader parameters
    bool validate_dispatch_params(const ComputeDispatchParams& params, const ComputeShaderSystem::ComputeCapabilities& caps);

    // Performance estimation
    float estimate_execution_time_ms(uint32_t operations, float gpu_gflops);
    float estimate_memory_bandwidth_gb_s(size_t data_size_bytes, float execution_time_ms);
}

} // namespace video_editor::gfx
