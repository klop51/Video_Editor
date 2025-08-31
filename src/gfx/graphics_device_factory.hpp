#pragma once

#include "graphics_device.hpp"
#include "vulkan_graphics_device.hpp"
#include "../core/types.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace gfx {

// Forward declarations
class D3D11GraphicsDevice;

// Graphics API enumeration
enum class GraphicsAPI {
    AUTOMATIC,          // Choose best available
    D3D11,             // Direct3D 11 (Windows)
    D3D12,             // Direct3D 12 (Windows, future)
    VULKAN,            // Vulkan (Cross-platform)
    OPENGL,            // OpenGL (Cross-platform, legacy)
    METAL,             // Metal (macOS/iOS, future)
    WEBGPU             // WebGPU (Web, future)
};

// Platform detection
enum class Platform {
    WINDOWS,
    LINUX,
    MACOS,
    ANDROID,
    IOS,
    WEB,
    UNKNOWN
};

// GPU vendor detection
enum class GPUVendor {
    UNKNOWN,
    NVIDIA,
    AMD,
    INTEL,
    QUALCOMM,
    ARM,
    APPLE
};

// Device preference configuration
struct DevicePreferences {
    GraphicsAPI preferred_api = GraphicsAPI::AUTOMATIC;
    GPUVendor preferred_vendor = GPUVendor::UNKNOWN;
    bool prefer_discrete_gpu = true;
    bool prefer_high_performance = true;
    bool require_compute_support = false;
    bool require_ray_tracing = false;
    bool enable_validation = false;
    uint32_t min_vram_mb = 512;
    
    // Feature requirements
    bool require_geometry_shaders = false;
    bool require_tessellation = false;
    bool require_multiview = false;
    bool require_variable_rate_shading = false;
    
    // Performance preferences
    bool prefer_low_latency = false;
    bool prefer_power_efficiency = false;
    uint32_t target_fps = 60;
    
    void set_gaming_preset();
    void set_content_creation_preset();
    void set_mobile_preset();
    void set_power_efficient_preset();
};

// Device capability information
struct DeviceInfo {
    std::string device_name;
    std::string driver_version;
    GPUVendor vendor = GPUVendor::UNKNOWN;
    GraphicsAPI api = GraphicsAPI::AUTOMATIC;
    Platform platform = Platform::UNKNOWN;
    
    // Memory information
    size_t dedicated_vram_mb = 0;
    size_t shared_vram_mb = 0;
    size_t total_vram_mb = 0;
    
    // Feature support
    bool supports_compute = false;
    bool supports_geometry_shaders = false;
    bool supports_tessellation = false;
    bool supports_ray_tracing = false;
    bool supports_variable_rate_shading = false;
    bool supports_mesh_shaders = false;
    bool supports_multiview = false;
    
    // Performance characteristics
    uint32_t max_texture_size = 0;
    uint32_t max_render_targets = 0;
    uint32_t max_compute_work_group_size = 0;
    float estimated_performance_score = 0.0f;
    
    // API-specific information
    std::string api_version;
    std::vector<std::string> supported_extensions;
    
    bool is_discrete_gpu() const;
    bool meets_requirements(const DevicePreferences& prefs) const;
    float calculate_suitability_score(const DevicePreferences& prefs) const;
};

// Cross-platform graphics device factory
class GraphicsDeviceFactory {
public:
    // Main device creation interface
    static std::unique_ptr<GraphicsDevice> create_best_device(const DevicePreferences& prefs = {});
    static std::unique_ptr<GraphicsDevice> create_device(GraphicsAPI api, const GraphicsDeviceConfig& config = {});
    
    // Specific API device creation
    static std::unique_ptr<GraphicsDevice> create_vulkan_device(const GraphicsDeviceConfig& config = {});
    static std::unique_ptr<GraphicsDevice> create_d3d11_device(const GraphicsDeviceConfig& config = {});
    static std::unique_ptr<GraphicsDevice> create_opengl_device(const GraphicsDeviceConfig& config = {});
    
    // Device enumeration and selection
    static std::vector<DeviceInfo> enumerate_devices();
    static std::vector<DeviceInfo> enumerate_vulkan_devices();
    static std::vector<DeviceInfo> enumerate_d3d11_devices();
    
    // Platform and API detection
    static Platform detect_platform();
    static std::vector<GraphicsAPI> get_supported_apis();
    static bool is_api_supported(GraphicsAPI api);
    
    // Device selection helpers
    static DeviceInfo select_best_device(const std::vector<DeviceInfo>& devices, const DevicePreferences& prefs);
    static GraphicsAPI choose_best_api(const DevicePreferences& prefs);
    
    // Configuration and testing
    static bool test_device_compatibility(GraphicsAPI api, const GraphicsDeviceConfig& config);
    static GraphicsDeviceConfig get_recommended_config(const DeviceInfo& device_info);
    
    // Debug and diagnostics
    static void print_device_info(const DeviceInfo& info);
    static void print_all_devices();
    static std::string get_device_info_json(const DeviceInfo& info);

private:
    // Device detection implementation
    static std::vector<DeviceInfo> detect_vulkan_devices();
    static std::vector<DeviceInfo> detect_d3d11_devices();
    static std::vector<DeviceInfo> detect_opengl_devices();
    
    // Platform-specific detection
    static std::vector<DeviceInfo> detect_windows_devices();
    static std::vector<DeviceInfo> detect_linux_devices();
    static std::vector<DeviceInfo> detect_macos_devices();
    
    // Vendor detection
    static GPUVendor detect_vendor_from_name(const std::string& device_name);
    static GPUVendor detect_vendor_from_vendor_id(uint32_t vendor_id);
    
    // Performance estimation
    static float estimate_device_performance(const DeviceInfo& info);
    static float get_vendor_performance_modifier(GPUVendor vendor);
    static float get_api_performance_modifier(GraphicsAPI api, GPUVendor vendor);
    
    // Compatibility checking
    static bool check_api_compatibility(GraphicsAPI api, Platform platform);
    static bool check_feature_support(const DeviceInfo& info, const DevicePreferences& prefs);
};

// Cross-platform shader compilation and management
class ShaderCompiler {
public:
    ShaderCompiler();
    ~ShaderCompiler();

    // Shader compilation interface
    struct CompilationResult {
        bool success = false;
        std::vector<uint8_t> bytecode;
        std::string error_message;
        std::vector<std::string> warnings;
        size_t compilation_time_ms = 0;
    };
    
    // High-level shader compilation
    CompilationResult compile_shader(const std::string& source, ShaderStage stage, GraphicsAPI target_api);
    CompilationResult compile_from_file(const std::string& filename, ShaderStage stage, GraphicsAPI target_api);
    
    // Cross-compilation between APIs
    CompilationResult compile_hlsl_to_spirv(const std::string& hlsl_source, ShaderStage stage);
    CompilationResult compile_spirv_to_hlsl(const std::vector<uint32_t>& spirv_code);
    CompilationResult compile_spirv_to_glsl(const std::vector<uint32_t>& spirv_code, int glsl_version = 450);
    
    // Shader optimization and validation
    CompilationResult optimize_spirv(const std::vector<uint32_t>& spirv_code);
    bool validate_spirv(const std::vector<uint32_t>& spirv_code, std::string& error_message);
    
    // Shader reflection and analysis
    struct ShaderReflection {
        std::vector<DescriptorSetLayoutBinding> descriptor_bindings;
        std::vector<PushConstantRange> push_constant_ranges;
        std::vector<VertexInputBinding> vertex_inputs;
        ShaderStageFlags stage_flags;
        
        // Resource usage analysis
        uint32_t texture_slots_used = 0;
        uint32_t buffer_slots_used = 0;
        uint32_t constant_buffer_size = 0;
        
        // Performance characteristics
        uint32_t estimated_alu_instructions = 0;
        uint32_t estimated_texture_samples = 0;
        float estimated_complexity_score = 0.0f;
    };
    
    ShaderReflection reflect_shader(const std::vector<uint32_t>& spirv_code);
    ShaderReflection reflect_hlsl_shader(const std::string& hlsl_source, ShaderStage stage);
    
    // Compilation configuration
    struct CompilerOptions {
        bool enable_debug_info = false;
        bool enable_optimization = true;
        bool enable_fast_math = false;
        bool enable_warnings_as_errors = false;
        std::vector<std::string> include_directories;
        std::vector<std::pair<std::string, std::string>> macro_definitions;
        
        // Target-specific options
        int hlsl_shader_model = 50;  // Shader Model 5.0
        int glsl_version = 450;      // GLSL 4.50
        bool vulkan_semantics = true;
    };
    
    void set_compiler_options(const CompilerOptions& options) { compiler_options_ = options; }
    const CompilerOptions& get_compiler_options() const { return compiler_options_; }
    
    // Caching and performance
    void enable_shader_cache(const std::string& cache_directory);
    void clear_shader_cache();
    size_t get_cache_size() const;
    
    // Batch compilation for improved performance
    std::vector<CompilationResult> compile_shader_batch(
        const std::vector<std::tuple<std::string, ShaderStage, GraphicsAPI>>& shaders);

private:
    CompilerOptions compiler_options_;
    std::string cache_directory_;
    bool cache_enabled_ = false;
    
    // Compilation backend implementations
    CompilationResult compile_hlsl_dxc(const std::string& source, ShaderStage stage);
    CompilationResult compile_hlsl_d3dcompile(const std::string& source, ShaderStage stage);
    CompilationResult compile_glsl_glslang(const std::string& source, ShaderStage stage);
    
    // Cross-compilation utilities
    std::string convert_hlsl_to_glsl(const std::string& hlsl_source, ShaderStage stage);
    std::vector<uint32_t> compile_glsl_to_spirv(const std::string& glsl_source, ShaderStage stage);
    
    // Cache management
    std::string generate_shader_hash(const std::string& source, const CompilerOptions& options);
    bool load_from_cache(const std::string& hash, CompilationResult& result);
    void save_to_cache(const std::string& hash, const CompilationResult& result);
    
    // Error handling and diagnostics
    std::string format_compilation_error(const std::string& raw_error);
    void log_compilation_statistics(const CompilationResult& result, const std::string& shader_name);
};

// Global device management singleton
class GraphicsDeviceManager {
public:
    static GraphicsDeviceManager& instance();
    
    // Device lifecycle management
    bool initialize(const DevicePreferences& prefs = {});
    void shutdown();
    bool is_initialized() const { return current_device_ != nullptr; }
    
    // Current device access
    GraphicsDevice* get_current_device() const { return current_device_.get(); }
    const DeviceInfo& get_current_device_info() const { return current_device_info_; }
    
    // Device switching for runtime API changes
    bool switch_device(GraphicsAPI new_api);
    bool switch_device(const DeviceInfo& device_info);
    
    // Resource management across device switches
    void register_resource_recreation_callback(std::function<void()> callback);
    void recreate_device_resources();
    
    // Performance monitoring
    void update_performance_metrics();
    float get_average_frame_time() const { return average_frame_time_; }
    bool is_performance_acceptable() const;
    
    // Device capabilities queries
    bool supports_feature(const std::string& feature) const;
    size_t get_available_vram() const;
    
private:
    GraphicsDeviceManager() = default;
    ~GraphicsDeviceManager() = default;
    
    std::unique_ptr<GraphicsDevice> current_device_;
    DeviceInfo current_device_info_;
    DevicePreferences current_preferences_;
    
    std::vector<std::function<void()>> resource_recreation_callbacks_;
    
    // Performance tracking
    std::vector<float> recent_frame_times_;
    float average_frame_time_ = 16.67f;  // 60 FPS target
    
    void cleanup_current_device();
    bool validate_device_switch(GraphicsAPI new_api);
};

// Convenience macros for device access
#define GFX_DEVICE() (GraphicsDeviceManager::instance().get_current_device())
#define GFX_DEVICE_INFO() (GraphicsDeviceManager::instance().get_current_device_info())

// Platform-specific utilities
namespace platform_utils {
    Platform get_current_platform();
    std::string get_platform_name(Platform platform);
    std::string get_platform_version();
    
    bool is_running_on_wine();
    bool is_running_in_vm();
    bool has_dedicated_gpu();
    
    // Windows-specific
    #ifdef _WIN32
    std::string get_windows_version();
    bool is_windows_10_or_later();
    bool supports_directx_12();
    #endif
    
    // Linux-specific
    #ifdef __linux__
    std::string get_linux_distribution();
    bool has_vulkan_support();
    bool has_wayland_support();
    #endif
}

} // namespace gfx
