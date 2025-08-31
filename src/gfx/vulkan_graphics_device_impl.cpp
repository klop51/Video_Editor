#include "vulkan_graphics_device.hpp"
#include "graphics_device_factory.hpp"
#include "../core/logging.hpp"
#include <algorithm>
#include <set>
#include <cstring>
#include <fstream>

// Platform-specific includes
#ifdef _WIN32
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#elif defined(__APPLE__)
#include <vulkan/vulkan_metal.h>
#endif

namespace gfx {

// Vulkan validation layers
const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

// Required device extensions
const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_MULTIVIEW_EXTENSION_NAME
};

// Optional device extensions for enhanced features
const std::vector<const char*> OPTIONAL_DEVICE_EXTENSIONS = {
    VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME,
    VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_NV_MESH_SHADER_EXTENSION_NAME,
    VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME
};

// Vulkan Device Capabilities Implementation
void VulkanDeviceCapabilities::detect_capabilities(VkPhysicalDevice physical_device) {
    // Get basic device properties and features
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    vkGetPhysicalDeviceFeatures(physical_device, &device_features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    
    // Get queue family properties
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    queue_families.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
    
    // Detect feature support
    supports_geometry_shaders = device_features.geometryShader;
    supports_tessellation_shaders = device_features.tessellationShader;
    supports_compute_shaders = true; // Vulkan guarantees compute support
    supports_multiview = true; // Check for extension support
    
    // Check for advanced features via extensions
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());
    
    for (const auto& extension : available_extensions) {
        supported_extensions.push_back(extension.extensionName);
        
        if (strcmp(extension.extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0) {
            supports_variable_rate_shading = true;
        }
        if (strcmp(extension.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
            supports_ray_tracing = true;
        }
        if (strcmp(extension.extensionName, VK_NV_MESH_SHADER_EXTENSION_NAME) == 0) {
            supports_mesh_shaders = true;
        }
    }
    
    // Find memory type indices
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        const auto& memory_type = memory_properties.memoryTypes[i];
        
        if (memory_type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            device_local_memory_type_index = i;
        }
        if (memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            host_visible_memory_type_index = i;
        }
        if ((memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            host_coherent_memory_type_index = i;
        }
    }
    
    // Find queue family indices
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        const auto& queue_family = queue_families[i];
        
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family = i;
        }
        if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            compute_queue_family = i;
        }
        if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            transfer_queue_family = i;
        }
        
        // Check present support (would need surface for accurate check)
        present_queue_family = graphics_queue_family;
    }
}

bool VulkanDeviceCapabilities::is_suitable_for_graphics() const {
    return graphics_queue_family.has_value() && 
           present_queue_family.has_value() &&
           device_local_memory_type_index != UINT32_MAX;
}

uint32_t VulkanDeviceCapabilities::rate_device_suitability() const {
    uint32_t score = 0;
    
    // Discrete GPUs have a significant advantage
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    
    // Maximum possible size of textures affects graphics quality
    score += device_properties.limits.maxImageDimension2D;
    
    // Feature support scoring
    if (supports_geometry_shaders) score += 100;
    if (supports_tessellation_shaders) score += 100;
    if (supports_compute_shaders) score += 200;
    if (supports_ray_tracing) score += 500;
    if (supports_mesh_shaders) score += 300;
    if (supports_variable_rate_shading) score += 200;
    
    // Memory scoring - more VRAM is better
    for (uint32_t i = 0; i < memory_properties.memoryHeapCount; i++) {
        if (memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            score += static_cast<uint32_t>(memory_properties.memoryHeaps[i].size / (1024 * 1024));  // MB
        }
    }
    
    // Queue family scoring
    if (graphics_queue_family.has_value()) score += 100;
    if (compute_queue_family.has_value()) score += 50;
    if (transfer_queue_family.has_value()) score += 25;
    
    return score;
}

// Vulkan Memory Allocator Implementation
VulkanMemoryAllocator::VulkanMemoryAllocator(VkDevice device, VkPhysicalDevice physical_device)
    : device_(device), physical_device_(physical_device) {
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties_);
}

VulkanMemoryAllocator::~VulkanMemoryAllocator() {
    // All allocations should be freed by this point
    if (allocation_count_.load() > 0) {
        CORE_WARN("VulkanMemoryAllocator destroyed with {} active allocations", allocation_count_.load());
    }
}

VulkanMemoryAllocator::Allocation VulkanMemoryAllocator::allocate_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device_, buffer, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);
    
    Allocation allocation;
    allocation.size = mem_requirements.size;
    allocation.memory_type_index = alloc_info.memoryTypeIndex;
    
    VkResult result = vkAllocateMemory(device_, &alloc_info, nullptr, &allocation.memory);
    if (result != VK_SUCCESS) {
        CORE_ERROR("Failed to allocate buffer memory: {}", vulkan_utils::vk_result_to_string(result));
        return {};
    }
    
    // Bind the buffer to memory
    vkBindBufferMemory(device_, buffer, allocation.memory, 0);
    
    // Update statistics
    total_allocated_memory_ += allocation.size;
    total_used_memory_ += allocation.size;
    allocation_count_++;
    
    return allocation;
}

VulkanMemoryAllocator::Allocation VulkanMemoryAllocator::allocate_image_memory(VkImage image, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device_, image, &mem_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);
    
    Allocation allocation;
    allocation.size = mem_requirements.size;
    allocation.memory_type_index = alloc_info.memoryTypeIndex;
    
    VkResult result = vkAllocateMemory(device_, &alloc_info, nullptr, &allocation.memory);
    if (result != VK_SUCCESS) {
        CORE_ERROR("Failed to allocate image memory: {}", vulkan_utils::vk_result_to_string(result));
        return {};
    }
    
    // Bind the image to memory
    vkBindImageMemory(device_, image, allocation.memory, 0);
    
    // Update statistics
    total_allocated_memory_ += allocation.size;
    total_used_memory_ += allocation.size;
    allocation_count_++;
    
    return allocation;
}

void VulkanMemoryAllocator::deallocate(const Allocation& allocation) {
    if (allocation.memory != VK_NULL_HANDLE) {
        if (allocation.mapped_data) {
            vkUnmapMemory(device_, allocation.memory);
        }
        
        vkFreeMemory(device_, allocation.memory, nullptr);
        
        // Update statistics
        total_used_memory_ -= allocation.size;
        allocation_count_--;
    }
}

void* VulkanMemoryAllocator::map_memory(const Allocation& allocation) {
    if (allocation.mapped_data) {
        return allocation.mapped_data;
    }
    
    void* data;
    VkResult result = vkMapMemory(device_, allocation.memory, allocation.offset, allocation.size, 0, &data);
    if (result != VK_SUCCESS) {
        CORE_ERROR("Failed to map memory: {}", vulkan_utils::vk_result_to_string(result));
        return nullptr;
    }
    
    return data;
}

void VulkanMemoryAllocator::unmap_memory(const Allocation& allocation) {
    if (allocation.mapped_data && !allocation.persistent_mapped) {
        vkUnmapMemory(device_, allocation.memory);
    }
}

void VulkanMemoryAllocator::flush_memory(const Allocation& allocation, VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = allocation.memory;
    range.offset = allocation.offset + offset;
    range.size = (size == VK_WHOLE_SIZE) ? allocation.size - offset : size;
    
    vkFlushMappedMemoryRanges(device_, 1, &range);
}

uint32_t VulkanMemoryAllocator::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    for (uint32_t i = 0; i < memory_properties_.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && is_memory_type_suitable(i, properties)) {
            return i;
        }
    }
    
    CORE_ERROR("Failed to find suitable memory type!");
    return UINT32_MAX;
}

bool VulkanMemoryAllocator::is_memory_type_suitable(uint32_t memory_type, VkMemoryPropertyFlags required_properties) const {
    const VkMemoryPropertyFlags available_properties = memory_properties_.memoryTypes[memory_type].propertyFlags;
    return (available_properties & required_properties) == required_properties;
}

// Vulkan Command Manager Implementation
VulkanCommandManager::VulkanCommandManager(VkDevice device, uint32_t queue_family_index)
    : device_(device), queue_family_index_(queue_family_index) {
    
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family_index_;
    
    VkResult result = vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_);
    if (result != VK_SUCCESS) {
        CORE_ERROR("Failed to create command pool: {}", vulkan_utils::vk_result_to_string(result));
    }
}

VulkanCommandManager::~VulkanCommandManager() {
    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
    }
}

VkCommandBuffer VulkanCommandManager::allocate_command_buffer(VkCommandBufferLevel level) {
    std::lock_guard<std::mutex> lock(command_buffer_mutex_);
    
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = level;
    alloc_info.commandBufferCount = 1;
    
    VkCommandBuffer command_buffer;
    VkResult result = vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer);
    if (result != VK_SUCCESS) {
        CORE_ERROR("Failed to allocate command buffer: {}", vulkan_utils::vk_result_to_string(result));
        return VK_NULL_HANDLE;
    }
    
    return command_buffer;
}

void VulkanCommandManager::free_command_buffer(VkCommandBuffer command_buffer) {
    std::lock_guard<std::mutex> lock(command_buffer_mutex_);
    
    if (command_buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
    }
}

VkCommandBuffer VulkanCommandManager::begin_single_time_commands() {
    VkCommandBuffer command_buffer = allocate_command_buffer();
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(command_buffer, &begin_info);
    
    return command_buffer;
}

void VulkanCommandManager::end_single_time_commands(VkCommandBuffer command_buffer, VkQueue queue) {
    vkEndCommandBuffer(command_buffer);
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    
    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    
    free_command_buffer(command_buffer);
}

void VulkanCommandManager::begin_render_pass(VkCommandBuffer cmd, VkRenderPass render_pass, VkFramebuffer framebuffer,
                                            VkRect2D render_area, const std::vector<VkClearValue>& clear_values) {
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea = render_area;
    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();
    
    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandManager::end_render_pass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void VulkanCommandManager::pipeline_barrier(VkCommandBuffer cmd, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
                                           const std::vector<VkImageMemoryBarrier>& image_barriers,
                                           const std::vector<VkBufferMemoryBarrier>& buffer_barriers,
                                           const std::vector<VkMemoryBarrier>& memory_barriers) {
    vkCmdPipelineBarrier(
        cmd,
        src_stage, dst_stage,
        0,
        static_cast<uint32_t>(memory_barriers.size()), memory_barriers.data(),
        static_cast<uint32_t>(buffer_barriers.size()), buffer_barriers.data(),
        static_cast<uint32_t>(image_barriers.size()), image_barriers.data()
    );
}

void VulkanCommandManager::transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout,
                                                   VkImageLayout new_layout, VkImageSubresourceRange subresource_range) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = subresource_range;
    
    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;
    
    // Determine appropriate pipeline stages and access masks
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        CORE_ERROR("Unsupported layout transition!");
        return;
    }
    
    vkCmdPipelineBarrier(cmd, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// Vulkan Graphics Device Implementation
VulkanGraphicsDevice::VulkanGraphicsDevice() = default;

VulkanGraphicsDevice::~VulkanGraphicsDevice() {
    shutdown();
}

bool VulkanGraphicsDevice::initialize(const GraphicsDeviceConfig& config) {
    config_ = config;
    
    try {
        if (!create_instance()) {
            CORE_ERROR("Failed to create Vulkan instance");
            return false;
        }
        
        if (config_.enable_validation && !setup_debug_messenger()) {
            CORE_WARN("Failed to setup debug messenger");
        }
        
        if (!create_surface()) {
            CORE_ERROR("Failed to create window surface");
            return false;
        }
        
        if (!pick_physical_device()) {
            CORE_ERROR("Failed to find suitable physical device");
            return false;
        }
        
        if (!create_logical_device()) {
            CORE_ERROR("Failed to create logical device");
            return false;
        }
        
        if (!create_swapchain()) {
            CORE_ERROR("Failed to create swapchain");
            return false;
        }
        
        if (!create_image_views()) {
            CORE_ERROR("Failed to create image views");
            return false;
        }
        
        if (!create_sync_objects()) {
            CORE_ERROR("Failed to create synchronization objects");
            return false;
        }
        
        // Initialize subsystem managers
        memory_allocator_ = std::make_unique<VulkanMemoryAllocator>(device_, physical_device_);
        command_manager_ = std::make_unique<VulkanCommandManager>(device_, capabilities_.graphics_queue_family.value());
        sync_manager_ = std::make_unique<VulkanSyncManager>(device_);
        
        CORE_INFO("Vulkan graphics device initialized successfully");
        CORE_INFO("Device: {}", capabilities_.device_properties.deviceName);
        CORE_INFO("Driver Version: {}", capabilities_.device_properties.driverVersion);
        
        return true;
        
    } catch (const std::exception& e) {
        CORE_ERROR("Exception during Vulkan device initialization: {}", e.what());
        shutdown();
        return false;
    }
}

void VulkanGraphicsDevice::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    
    // Cleanup in reverse order of creation
    sync_manager_.reset();
    command_manager_.reset();
    memory_allocator_.reset();
    
    cleanup_swapchain();
    
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    
    if (debug_messenger_ != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance_, debug_messenger_, nullptr);
        }
        debug_messenger_ = VK_NULL_HANDLE;
    }
    
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    
    CORE_INFO("Vulkan graphics device shutdown complete");
}

// Utility implementations
namespace vulkan_utils {
    std::string vk_result_to_string(VkResult result) {
        switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        default: return "Unknown VkResult";
        }
    }
    
    std::string vk_format_to_string(VkFormat format) {
        switch (format) {
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
        default: return "Unknown VkFormat";
        }
    }
    
    bool is_depth_format(VkFormat format) {
        return format == VK_FORMAT_D16_UNORM ||
               format == VK_FORMAT_D32_SFLOAT ||
               format == VK_FORMAT_D16_UNORM_S8_UINT ||
               format == VK_FORMAT_D24_UNORM_S8_UINT ||
               format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
    
    bool is_stencil_format(VkFormat format) {
        return format == VK_FORMAT_S8_UINT ||
               format == VK_FORMAT_D16_UNORM_S8_UINT ||
               format == VK_FORMAT_D24_UNORM_S8_UINT ||
               format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
    
    bool is_depth_stencil_format(VkFormat format) {
        return is_depth_format(format) || is_stencil_format(format);
    }
    
    VkImageAspectFlags get_image_aspect_flags(VkFormat format) {
        if (is_depth_stencil_format(format)) {
            VkImageAspectFlags aspect_flags = 0;
            if (is_depth_format(format)) {
                aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
            }
            if (is_stencil_format(format)) {
                aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            return aspect_flags;
        }
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

// Factory function implementation
std::unique_ptr<VulkanGraphicsDevice> create_vulkan_device(const GraphicsDeviceConfig& config) {
    auto device = std::make_unique<VulkanGraphicsDevice>();
    if (!device->initialize(config)) {
        return nullptr;
    }
    return device;
}

} // namespace gfx
