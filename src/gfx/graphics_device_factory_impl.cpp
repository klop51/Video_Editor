#include "graphics_device_factory.hpp"
#include "vulkan_graphics_device.hpp"
#include "../core/logging.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

// Platform-specific includes
#ifdef _WIN32
    #include <Windows.h>
    #include <dxgi1_6.h>
    #include <d3d11_4.h>
    #pragma comment(lib, "dxgi.lib")
    #pragma comment(lib, "d3d11.lib")
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #include <vulkan/vulkan.h>
#elif defined(__APPLE__)
    #include <Metal/Metal.h>
    #include <vulkan/vulkan.h>
#endif

namespace gfx {

// Device Preferences Implementation
void DevicePreferences::set_gaming_preset() {
    preferred_api = GraphicsAPI::AUTOMATIC;
    prefer_discrete_gpu = true;
    prefer_high_performance = true;
    require_compute_support = false;
    require_ray_tracing = false;
    min_vram_mb = 1024;
    target_fps = 60;
    prefer_low_latency = true;
    prefer_power_efficiency = false;
}

void DevicePreferences::set_content_creation_preset() {
    preferred_api = GraphicsAPI::AUTOMATIC;
    prefer_discrete_gpu = true;
    prefer_high_performance = true;
    require_compute_support = true;
    require_ray_tracing = false;
    min_vram_mb = 2048;
    target_fps = 30;
    prefer_low_latency = false;
    prefer_power_efficiency = false;
}

void DevicePreferences::set_mobile_preset() {
    preferred_api = GraphicsAPI::VULKAN;
    prefer_discrete_gpu = false;
    prefer_high_performance = false;
    require_compute_support = false;
    require_ray_tracing = false;
    min_vram_mb = 256;
    target_fps = 30;
    prefer_low_latency = false;
    prefer_power_efficiency = true;
}

void DevicePreferences::set_power_efficient_preset() {
    preferred_api = GraphicsAPI::AUTOMATIC;
    prefer_discrete_gpu = false;
    prefer_high_performance = false;
    require_compute_support = false;
    require_ray_tracing = false;
    min_vram_mb = 512;
    target_fps = 30;
    prefer_low_latency = false;
    prefer_power_efficiency = true;
}

// Device Info Implementation
bool DeviceInfo::is_discrete_gpu() const {
    return dedicated_vram_mb > 0 && shared_vram_mb < dedicated_vram_mb;
}

bool DeviceInfo::meets_requirements(const DevicePreferences& prefs) const {
    // Check basic requirements
    if (total_vram_mb < prefs.min_vram_mb) {
        return false;
    }
    
    if (prefs.prefer_discrete_gpu && !is_discrete_gpu()) {
        return false;
    }
    
    // Check feature requirements
    if (prefs.require_compute_support && !supports_compute) {
        return false;
    }
    
    if (prefs.require_geometry_shaders && !supports_geometry_shaders) {
        return false;
    }
    
    if (prefs.require_tessellation && !supports_tessellation) {
        return false;
    }
    
    if (prefs.require_ray_tracing && !supports_ray_tracing) {
        return false;
    }
    
    if (prefs.require_variable_rate_shading && !supports_variable_rate_shading) {
        return false;
    }
    
    if (prefs.require_multiview && !supports_multiview) {
        return false;
    }
    
    return true;
}

float DeviceInfo::calculate_suitability_score(const DevicePreferences& prefs) const {
    if (!meets_requirements(prefs)) {
        return 0.0f;
    }
    
    float score = 0.0f;
    
    // Base performance score
    score += estimated_performance_score * 0.4f;
    
    // VRAM scoring
    float vram_ratio = static_cast<float>(total_vram_mb) / std::max(prefs.min_vram_mb, 1u);
    score += std::min(vram_ratio, 4.0f) * 0.2f;  // Cap at 4x required VRAM
    
    // Discrete GPU preference
    if (prefs.prefer_discrete_gpu && is_discrete_gpu()) {
        score += 0.15f;
    } else if (!prefs.prefer_discrete_gpu && !is_discrete_gpu()) {
        score += 0.1f;  // Slight bonus for integrated when preferred
    }
    
    // Vendor preference
    if (prefs.preferred_vendor != GPUVendor::UNKNOWN && vendor == prefs.preferred_vendor) {
        score += 0.1f;
    }
    
    // API preference
    if (prefs.preferred_api != GraphicsAPI::AUTOMATIC && api == prefs.preferred_api) {
        score += 0.05f;
    }
    
    // Feature bonuses
    if (supports_ray_tracing) score += 0.05f;
    if (supports_mesh_shaders) score += 0.03f;
    if (supports_variable_rate_shading) score += 0.02f;
    
    return std::clamp(score, 0.0f, 1.0f);
}

// Graphics Device Factory Implementation
std::unique_ptr<GraphicsDevice> GraphicsDeviceFactory::create_best_device(const DevicePreferences& prefs) {
    CORE_INFO("Creating best graphics device with preferences...");
    
    // Enumerate all available devices
    auto devices = enumerate_devices();
    if (devices.empty()) {
        CORE_ERROR("No graphics devices found!");
        return nullptr;
    }
    
    // Select the best device based on preferences
    DeviceInfo best_device = select_best_device(devices, prefs);
    
    CORE_INFO("Selected device: {} (API: {}, Score: {:.2f})", 
              best_device.device_name, 
              static_cast<int>(best_device.api),
              best_device.calculate_suitability_score(prefs));
    
    // Create device with recommended configuration
    GraphicsDeviceConfig config = get_recommended_config(best_device);
    return create_device(best_device.api, config);
}

std::unique_ptr<GraphicsDevice> GraphicsDeviceFactory::create_device(GraphicsAPI api, const GraphicsDeviceConfig& config) {
    switch (api) {
        case GraphicsAPI::VULKAN:
            return create_vulkan_device(config);
        case GraphicsAPI::D3D11:
            return create_d3d11_device(config);
        case GraphicsAPI::OPENGL:
            return create_opengl_device(config);
        case GraphicsAPI::AUTOMATIC:
            return create_best_device();
        default:
            CORE_ERROR("Unsupported graphics API: {}", static_cast<int>(api));
            return nullptr;
    }
}

std::unique_ptr<GraphicsDevice> GraphicsDeviceFactory::create_vulkan_device(const GraphicsDeviceConfig& config) {
    if (!is_api_supported(GraphicsAPI::VULKAN)) {
        CORE_ERROR("Vulkan is not supported on this platform");
        return nullptr;
    }
    
    auto device = gfx::create_vulkan_device(config);
    if (!device) {
        CORE_ERROR("Failed to create Vulkan device");
        return nullptr;
    }
    
    CORE_INFO("Vulkan device created successfully");
    return std::move(device);
}

std::unique_ptr<GraphicsDevice> GraphicsDeviceFactory::create_d3d11_device(const GraphicsDeviceConfig& config) {
#ifdef _WIN32
    if (!is_api_supported(GraphicsAPI::D3D11)) {
        CORE_ERROR("D3D11 is not supported on this platform");
        return nullptr;
    }
    
    // D3D11 device creation would go here
    CORE_WARN("D3D11 device creation not yet implemented");
    return nullptr;
#else
    CORE_ERROR("D3D11 is not available on this platform");
    return nullptr;
#endif
}

std::unique_ptr<GraphicsDevice> GraphicsDeviceFactory::create_opengl_device(const GraphicsDeviceConfig& config) {
    CORE_WARN("OpenGL device creation not yet implemented");
    return nullptr;
}

std::vector<DeviceInfo> GraphicsDeviceFactory::enumerate_devices() {
    std::vector<DeviceInfo> all_devices;
    
    // Try Vulkan devices first
    if (is_api_supported(GraphicsAPI::VULKAN)) {
        auto vulkan_devices = enumerate_vulkan_devices();
        all_devices.insert(all_devices.end(), vulkan_devices.begin(), vulkan_devices.end());
    }
    
    // Try D3D11 devices on Windows
    if (is_api_supported(GraphicsAPI::D3D11)) {
        auto d3d11_devices = enumerate_d3d11_devices();
        all_devices.insert(all_devices.end(), d3d11_devices.begin(), d3d11_devices.end());
    }
    
    CORE_INFO("Found {} total graphics devices", all_devices.size());
    return all_devices;
}

std::vector<DeviceInfo> GraphicsDeviceFactory::enumerate_vulkan_devices() {
    std::vector<DeviceInfo> devices;
    
    try {
        // Create temporary Vulkan instance for enumeration
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Video Editor";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Video Editor Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;
        
        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;
        
        // Get required extensions
        std::vector<const char*> extensions = {"VK_KHR_surface"};
        
#ifdef _WIN32
        extensions.push_back("VK_KHR_win32_surface");
#elif defined(__linux__)
        extensions.push_back("VK_KHR_xlib_surface");
#elif defined(__APPLE__)
        extensions.push_back("VK_EXT_metal_surface");
#endif
        
        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();
        
        VkInstance instance;
        VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
        if (result != VK_SUCCESS) {
            CORE_WARN("Failed to create Vulkan instance for enumeration: {}", vulkan_utils::vk_result_to_string(result));
            return devices;
        }
        
        // Enumerate physical devices
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        
        if (device_count == 0) {
            CORE_WARN("No Vulkan-compatible devices found");
            vkDestroyInstance(instance, nullptr);
            return devices;
        }
        
        std::vector<VkPhysicalDevice> physical_devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());
        
        // Convert to DeviceInfo
        for (VkPhysicalDevice physical_device : physical_devices) {
            DeviceInfo info;
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceFeatures features;
            VkPhysicalDeviceMemoryProperties memory_properties;
            
            vkGetPhysicalDeviceProperties(physical_device, &properties);
            vkGetPhysicalDeviceFeatures(physical_device, &features);
            vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
            
            info.device_name = properties.deviceName;
            info.driver_version = std::to_string(properties.driverVersion);
            info.vendor = detect_vendor_from_vendor_id(properties.vendorID);
            info.api = GraphicsAPI::VULKAN;
            info.platform = detect_platform();
            
            // Calculate VRAM
            for (uint32_t i = 0; i < memory_properties.memoryHeapCount; i++) {
                if (memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    info.dedicated_vram_mb += static_cast<size_t>(memory_properties.memoryHeaps[i].size / (1024 * 1024));
                }
            }
            info.total_vram_mb = info.dedicated_vram_mb;
            
            // Feature detection
            info.supports_compute = true;  // Vulkan guarantees compute support
            info.supports_geometry_shaders = features.geometryShader;
            info.supports_tessellation = features.tessellationShader;
            
            // Check for extensions
            uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> available_extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());
            
            for (const auto& extension : available_extensions) {
                info.supported_extensions.push_back(extension.extensionName);
                
                if (strcmp(extension.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
                    info.supports_ray_tracing = true;
                }
                if (strcmp(extension.extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0) {
                    info.supports_variable_rate_shading = true;
                }
                if (strcmp(extension.extensionName, VK_NV_MESH_SHADER_EXTENSION_NAME) == 0) {
                    info.supports_mesh_shaders = true;
                }
            }
            
            // Performance estimation
            info.estimated_performance_score = estimate_device_performance(info);
            info.max_texture_size = properties.limits.maxImageDimension2D;
            info.max_render_targets = properties.limits.maxColorAttachments;
            info.max_compute_work_group_size = properties.limits.maxComputeWorkGroupSize[0];
            
            info.api_version = std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + "." +
                              std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + "." +
                              std::to_string(VK_VERSION_PATCH(properties.apiVersion));
            
            devices.push_back(info);
        }
        
        vkDestroyInstance(instance, nullptr);
        
    } catch (const std::exception& e) {
        CORE_ERROR("Exception during Vulkan device enumeration: {}", e.what());
    }
    
    CORE_INFO("Found {} Vulkan devices", devices.size());
    return devices;
}

std::vector<DeviceInfo> GraphicsDeviceFactory::enumerate_d3d11_devices() {
    std::vector<DeviceInfo> devices;
    
#ifdef _WIN32
    try {
        ComPtr<IDXGIFactory1> factory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            CORE_WARN("Failed to create DXGI factory for D3D11 enumeration");
            return devices;
        }
        
        UINT adapter_index = 0;
        ComPtr<IDXGIAdapter1> adapter;
        
        while (factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            
            DeviceInfo info;
            
            // Convert wide string to regular string
            char device_name[256];
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, device_name, sizeof(device_name), nullptr, nullptr);
            info.device_name = device_name;
            
            info.vendor = detect_vendor_from_vendor_id(desc.VendorId);
            info.api = GraphicsAPI::D3D11;
            info.platform = Platform::WINDOWS;
            
            info.dedicated_vram_mb = static_cast<size_t>(desc.DedicatedVideoMemory / (1024 * 1024));
            info.shared_vram_mb = static_cast<size_t>(desc.SharedSystemMemory / (1024 * 1024));
            info.total_vram_mb = info.dedicated_vram_mb + info.shared_vram_mb;
            
            // Try to create D3D11 device to check feature support
            D3D_FEATURE_LEVEL feature_levels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0
            };
            
            ComPtr<ID3D11Device> device;
            ComPtr<ID3D11DeviceContext> context;
            D3D_FEATURE_LEVEL selected_feature_level;
            
            hr = D3D11CreateDevice(
                adapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                0,
                feature_levels,
                ARRAYSIZE(feature_levels),
                D3D11_SDK_VERSION,
                &device,
                &selected_feature_level,
                &context
            );
            
            if (SUCCEEDED(hr)) {
                info.supports_compute = (selected_feature_level >= D3D_FEATURE_LEVEL_11_0);
                info.supports_geometry_shaders = (selected_feature_level >= D3D_FEATURE_LEVEL_10_0);
                info.supports_tessellation = (selected_feature_level >= D3D_FEATURE_LEVEL_11_0);
                
                // Check for advanced features
                ComPtr<ID3D11Device3> device3;
                if (SUCCEEDED(device.As(&device3))) {
                    // D3D11.3 features available
                }
                
                info.api_version = "11.0";
                if (selected_feature_level >= D3D_FEATURE_LEVEL_11_1) {
                    info.api_version = "11.1";
                }
            }
            
            info.estimated_performance_score = estimate_device_performance(info);
            devices.push_back(info);
            
            adapter_index++;
        }
        
    } catch (const std::exception& e) {
        CORE_ERROR("Exception during D3D11 device enumeration: {}", e.what());
    }
#endif
    
    CORE_INFO("Found {} D3D11 devices", devices.size());
    return devices;
}

Platform GraphicsDeviceFactory::detect_platform() {
#ifdef _WIN32
    return Platform::WINDOWS;
#elif defined(__linux__)
    return Platform::LINUX;
#elif defined(__APPLE__)
    return Platform::MACOS;
#elif defined(__ANDROID__)
    return Platform::ANDROID;
#elif defined(__EMSCRIPTEN__)
    return Platform::WEB;
#else
    return Platform::UNKNOWN;
#endif
}

std::vector<GraphicsAPI> GraphicsDeviceFactory::get_supported_apis() {
    std::vector<GraphicsAPI> supported_apis;
    
    if (is_api_supported(GraphicsAPI::VULKAN)) {
        supported_apis.push_back(GraphicsAPI::VULKAN);
    }
    
    if (is_api_supported(GraphicsAPI::D3D11)) {
        supported_apis.push_back(GraphicsAPI::D3D11);
    }
    
    if (is_api_supported(GraphicsAPI::OPENGL)) {
        supported_apis.push_back(GraphicsAPI::OPENGL);
    }
    
    return supported_apis;
}

bool GraphicsDeviceFactory::is_api_supported(GraphicsAPI api) {
    Platform platform = detect_platform();
    
    switch (api) {
        case GraphicsAPI::VULKAN:
            return check_api_compatibility(api, platform);
        case GraphicsAPI::D3D11:
            return platform == Platform::WINDOWS;
        case GraphicsAPI::D3D12:
            return platform == Platform::WINDOWS;
        case GraphicsAPI::OPENGL:
            return platform != Platform::UNKNOWN;
        case GraphicsAPI::METAL:
            return platform == Platform::MACOS || platform == Platform::IOS;
        case GraphicsAPI::WEBGPU:
            return platform == Platform::WEB;
        default:
            return false;
    }
}

DeviceInfo GraphicsDeviceFactory::select_best_device(const std::vector<DeviceInfo>& devices, const DevicePreferences& prefs) {
    if (devices.empty()) {
        return DeviceInfo{};
    }
    
    // Calculate scores for all devices
    std::vector<std::pair<float, size_t>> scored_devices;
    for (size_t i = 0; i < devices.size(); i++) {
        float score = devices[i].calculate_suitability_score(prefs);
        if (score > 0.0f) {
            scored_devices.emplace_back(score, i);
        }
    }
    
    if (scored_devices.empty()) {
        CORE_WARN("No devices meet the specified requirements, selecting first available");
        return devices[0];
    }
    
    // Sort by score (highest first)
    std::sort(scored_devices.begin(), scored_devices.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    return devices[scored_devices[0].second];
}

GraphicsAPI GraphicsDeviceFactory::choose_best_api(const DevicePreferences& prefs) {
    if (prefs.preferred_api != GraphicsAPI::AUTOMATIC) {
        if (is_api_supported(prefs.preferred_api)) {
            return prefs.preferred_api;
        } else {
            CORE_WARN("Preferred API not supported, falling back to automatic selection");
        }
    }
    
    Platform platform = detect_platform();
    
    // Platform-specific preferences
    switch (platform) {
        case Platform::WINDOWS:
            if (prefs.prefer_high_performance && is_api_supported(GraphicsAPI::VULKAN)) {
                return GraphicsAPI::VULKAN;
            }
            return GraphicsAPI::D3D11;  // Fallback for Windows
            
        case Platform::LINUX:
        case Platform::ANDROID:
            return GraphicsAPI::VULKAN;
            
        case Platform::MACOS:
        case Platform::IOS:
            if (is_api_supported(GraphicsAPI::METAL)) {
                return GraphicsAPI::METAL;
            }
            return GraphicsAPI::VULKAN;
            
        case Platform::WEB:
            return GraphicsAPI::WEBGPU;
            
        default:
            if (is_api_supported(GraphicsAPI::VULKAN)) {
                return GraphicsAPI::VULKAN;
            }
            return GraphicsAPI::OPENGL;
    }
}

GPUVendor GraphicsDeviceFactory::detect_vendor_from_vendor_id(uint32_t vendor_id) {
    switch (vendor_id) {
        case 0x10DE: return GPUVendor::NVIDIA;
        case 0x1002: return GPUVendor::AMD;
        case 0x8086: return GPUVendor::INTEL;
        case 0x5143: return GPUVendor::QUALCOMM;
        case 0x13B5: return GPUVendor::ARM;
        case 0x106B: return GPUVendor::APPLE;
        default: return GPUVendor::UNKNOWN;
    }
}

GPUVendor GraphicsDeviceFactory::detect_vendor_from_name(const std::string& device_name) {
    std::string lower_name = device_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name.find("nvidia") != std::string::npos || lower_name.find("geforce") != std::string::npos) {
        return GPUVendor::NVIDIA;
    }
    if (lower_name.find("amd") != std::string::npos || lower_name.find("radeon") != std::string::npos) {
        return GPUVendor::AMD;
    }
    if (lower_name.find("intel") != std::string::npos) {
        return GPUVendor::INTEL;
    }
    if (lower_name.find("qualcomm") != std::string::npos || lower_name.find("adreno") != std::string::npos) {
        return GPUVendor::QUALCOMM;
    }
    if (lower_name.find("arm") != std::string::npos || lower_name.find("mali") != std::string::npos) {
        return GPUVendor::ARM;
    }
    if (lower_name.find("apple") != std::string::npos) {
        return GPUVendor::APPLE;
    }
    
    return GPUVendor::UNKNOWN;
}

float GraphicsDeviceFactory::estimate_device_performance(const DeviceInfo& info) {
    float score = 0.0f;
    
    // Base score from VRAM (normalized to 0-1 range)
    score += std::min(static_cast<float>(info.total_vram_mb) / 8192.0f, 1.0f) * 0.3f;
    
    // Vendor performance modifiers
    score += get_vendor_performance_modifier(info.vendor) * 0.2f;
    
    // API performance modifiers
    score += get_api_performance_modifier(info.api, info.vendor) * 0.1f;
    
    // Discrete GPU bonus
    if (info.is_discrete_gpu()) {
        score += 0.2f;
    }
    
    // Feature bonuses
    if (info.supports_ray_tracing) score += 0.1f;
    if (info.supports_mesh_shaders) score += 0.05f;
    if (info.supports_variable_rate_shading) score += 0.05f;
    
    return std::clamp(score, 0.0f, 1.0f);
}

float GraphicsDeviceFactory::get_vendor_performance_modifier(GPUVendor vendor) {
    switch (vendor) {
        case GPUVendor::NVIDIA: return 1.0f;
        case GPUVendor::AMD: return 0.9f;
        case GPUVendor::INTEL: return 0.6f;
        case GPUVendor::QUALCOMM: return 0.4f;
        case GPUVendor::ARM: return 0.3f;
        case GPUVendor::APPLE: return 0.8f;
        default: return 0.5f;
    }
}

float GraphicsDeviceFactory::get_api_performance_modifier(GraphicsAPI api, GPUVendor vendor) {
    switch (api) {
        case GraphicsAPI::VULKAN:
            return (vendor == GPUVendor::AMD) ? 1.1f : 1.0f;  // AMD benefits more from Vulkan
        case GraphicsAPI::D3D11:
            return (vendor == GPUVendor::NVIDIA) ? 1.05f : 1.0f;  // NVIDIA has good D3D11 drivers
        case GraphicsAPI::D3D12:
            return 1.1f;  // Generally better performance
        case GraphicsAPI::METAL:
            return (vendor == GPUVendor::APPLE) ? 1.2f : 1.0f;  // Apple's native API
        default:
            return 1.0f;
    }
}

bool GraphicsDeviceFactory::check_api_compatibility(GraphicsAPI api, Platform platform) {
    // Check for Vulkan specifically
    if (api == GraphicsAPI::VULKAN) {
        try {
            // Try to load Vulkan library
            VkApplicationInfo app_info{};
            app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.apiVersion = VK_API_VERSION_1_0;
            
            VkInstanceCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.pApplicationInfo = &app_info;
            
            VkInstance test_instance;
            VkResult result = vkCreateInstance(&create_info, nullptr, &test_instance);
            
            if (result == VK_SUCCESS) {
                vkDestroyInstance(test_instance, nullptr);
                return true;
            }
        } catch (...) {
            return false;
        }
    }
    
    return true;  // Assume other APIs are compatible for now
}

GraphicsDeviceConfig GraphicsDeviceFactory::get_recommended_config(const DeviceInfo& device_info) {
    GraphicsDeviceConfig config;
    
    // Enable validation in debug builds or for development
#ifdef _DEBUG
    config.enable_validation = true;
#else
    config.enable_validation = false;
#endif
    
    // Configure based on device capabilities
    if (device_info.total_vram_mb >= 4096) {
        config.preferred_memory_usage = GraphicsDeviceConfig::MemoryUsage::HIGH_PERFORMANCE;
    } else if (device_info.total_vram_mb >= 2048) {
        config.preferred_memory_usage = GraphicsDeviceConfig::MemoryUsage::BALANCED;
    } else {
        config.preferred_memory_usage = GraphicsDeviceConfig::MemoryUsage::CONSERVATIVE;
    }
    
    // Enable advanced features if supported
    config.enable_compute_shaders = device_info.supports_compute;
    config.enable_geometry_shaders = device_info.supports_geometry_shaders;
    config.enable_tessellation = device_info.supports_tessellation;
    config.enable_ray_tracing = device_info.supports_ray_tracing;
    
    return config;
}

void GraphicsDeviceFactory::print_device_info(const DeviceInfo& info) {
    CORE_INFO("=== Graphics Device Info ===");
    CORE_INFO("Name: {}", info.device_name);
    CORE_INFO("Vendor: {}", static_cast<int>(info.vendor));
    CORE_INFO("API: {}", static_cast<int>(info.api));
    CORE_INFO("Platform: {}", static_cast<int>(info.platform));
    CORE_INFO("Dedicated VRAM: {} MB", info.dedicated_vram_mb);
    CORE_INFO("Shared VRAM: {} MB", info.shared_vram_mb);
    CORE_INFO("Total VRAM: {} MB", info.total_vram_mb);
    CORE_INFO("Performance Score: {:.2f}", info.estimated_performance_score);
    CORE_INFO("Supports Compute: {}", info.supports_compute);
    CORE_INFO("Supports Ray Tracing: {}", info.supports_ray_tracing);
    CORE_INFO("Driver Version: {}", info.driver_version);
    CORE_INFO("API Version: {}", info.api_version);
}

void GraphicsDeviceFactory::print_all_devices() {
    auto devices = enumerate_devices();
    CORE_INFO("=== All Available Graphics Devices ===");
    
    for (size_t i = 0; i < devices.size(); i++) {
        CORE_INFO("Device {}: {}", i, devices[i].device_name);
    }
}

} // namespace gfx
