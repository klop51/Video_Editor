#pragma once

#include "../core/types.hpp"
#include "../core/error_handling.hpp"
#include "graphics_device.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include <functional>

namespace gfx {

// Forward declarations
class VulkanBuffer;
class VulkanTexture;
class VulkanShader;
class VulkanRenderPass;
class VulkanPipeline;

// Vulkan-specific configuration and capabilities
struct VulkanDeviceCapabilities {
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    VkPhysicalDeviceMemoryProperties memory_properties;
    std::vector<VkQueueFamilyProperties> queue_families;
    std::vector<const char*> supported_extensions;
    
    // Feature support detection
    bool supports_geometry_shaders = false;
    bool supports_tessellation_shaders = false;
    bool supports_compute_shaders = false;
    bool supports_multiview = false;
    bool supports_variable_rate_shading = false;
    bool supports_ray_tracing = false;
    bool supports_mesh_shaders = false;
    
    // Memory type detection
    uint32_t device_local_memory_type_index = UINT32_MAX;
    uint32_t host_visible_memory_type_index = UINT32_MAX;
    uint32_t host_coherent_memory_type_index = UINT32_MAX;
    
    // Queue family indices
    std::optional<uint32_t> graphics_queue_family;
    std::optional<uint32_t> compute_queue_family;
    std::optional<uint32_t> transfer_queue_family;
    std::optional<uint32_t> present_queue_family;
    
    void detect_capabilities(VkPhysicalDevice physical_device);
    bool is_suitable_for_graphics() const;
    uint32_t rate_device_suitability() const;
};

// Vulkan memory allocator for efficient GPU memory management
class VulkanMemoryAllocator {
public:
    VulkanMemoryAllocator(VkDevice device, VkPhysicalDevice physical_device);
    ~VulkanMemoryAllocator();

    struct Allocation {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        VkDeviceSize size = 0;
        void* mapped_data = nullptr;
        uint32_t memory_type_index = 0;
        bool persistent_mapped = false;
    };

    // Memory allocation interface
    Allocation allocate_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags properties);
    Allocation allocate_image_memory(VkImage image, VkMemoryPropertyFlags properties);
    void deallocate(const Allocation& allocation);
    
    // Memory mapping
    void* map_memory(const Allocation& allocation);
    void unmap_memory(const Allocation& allocation);
    void flush_memory(const Allocation& allocation, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    
    // Statistics
    VkDeviceSize get_total_allocated_memory() const { return total_allocated_memory_; }
    VkDeviceSize get_total_used_memory() const { return total_used_memory_; }
    size_t get_allocation_count() const { return allocation_count_; }

private:
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VkPhysicalDeviceMemoryProperties memory_properties_;
    
    std::atomic<VkDeviceSize> total_allocated_memory_{0};
    std::atomic<VkDeviceSize> total_used_memory_{0};
    std::atomic<size_t> allocation_count_{0};
    
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
    bool is_memory_type_suitable(uint32_t memory_type, VkMemoryPropertyFlags required_properties) const;
};

// Vulkan command buffer pool and management
class VulkanCommandManager {
public:
    VulkanCommandManager(VkDevice device, uint32_t queue_family_index);
    ~VulkanCommandManager();

    // Command buffer allocation
    VkCommandBuffer allocate_command_buffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    void free_command_buffer(VkCommandBuffer command_buffer);
    
    // One-time command execution
    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer command_buffer, VkQueue queue);
    
    // Command buffer recording helpers
    void begin_render_pass(VkCommandBuffer cmd, VkRenderPass render_pass, VkFramebuffer framebuffer, 
                          VkRect2D render_area, const std::vector<VkClearValue>& clear_values);
    void end_render_pass(VkCommandBuffer cmd);
    
    // Resource barriers and synchronization
    void pipeline_barrier(VkCommandBuffer cmd, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
                         const std::vector<VkImageMemoryBarrier>& image_barriers = {},
                         const std::vector<VkBufferMemoryBarrier>& buffer_barriers = {},
                         const std::vector<VkMemoryBarrier>& memory_barriers = {});
    
    // Resource transitions
    void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, 
                                VkImageLayout new_layout, VkImageSubresourceRange subresource_range);

private:
    VkDevice device_;
    VkCommandPool command_pool_;
    uint32_t queue_family_index_;
    
    std::vector<VkCommandBuffer> available_command_buffers_;
    std::mutex command_buffer_mutex_;
};

// Vulkan synchronization objects manager
class VulkanSyncManager {
public:
    VulkanSyncManager(VkDevice device);
    ~VulkanSyncManager();

    // Synchronization object creation
    VkSemaphore create_semaphore();
    VkFence create_fence(bool signaled = false);
    VkEvent create_event();
    
    // Cleanup
    void destroy_semaphore(VkSemaphore semaphore);
    void destroy_fence(VkFence fence);
    void destroy_event(VkEvent event);
    
    // Fence operations
    bool wait_for_fence(VkFence fence, uint64_t timeout = UINT64_MAX);
    bool wait_for_fences(const std::vector<VkFence>& fences, bool wait_all = true, uint64_t timeout = UINT64_MAX);
    void reset_fence(VkFence fence);
    void reset_fences(const std::vector<VkFence>& fences);

private:
    VkDevice device_;
    std::vector<VkSemaphore> semaphores_;
    std::vector<VkFence> fences_;
    std::vector<VkEvent> events_;
    std::mutex sync_mutex_;
};

// Main Vulkan graphics device implementation
class VulkanGraphicsDevice : public GraphicsDevice {
public:
    VulkanGraphicsDevice();
    ~VulkanGraphicsDevice() override;

    // GraphicsDevice interface implementation
    bool initialize(const GraphicsDeviceConfig& config) override;
    void shutdown() override;
    
    // Resource creation
    std::unique_ptr<Buffer> create_buffer(const BufferDesc& desc) override;
    std::unique_ptr<Texture> create_texture(const TextureDesc& desc) override;
    std::unique_ptr<Shader> create_shader(const ShaderDesc& desc) override;
    std::unique_ptr<RenderPass> create_render_pass(const RenderPassDesc& desc) override;
    std::unique_ptr<Pipeline> create_graphics_pipeline(const GraphicsPipelineDesc& desc) override;
    std::unique_ptr<Pipeline> create_compute_pipeline(const ComputePipelineDesc& desc) override;
    
    // Command recording and execution
    void begin_frame() override;
    void end_frame() override;
    void present() override;
    
    // Render commands
    void begin_render_pass(RenderPass* render_pass, const RenderPassBeginInfo& info) override;
    void end_render_pass() override;
    void bind_pipeline(Pipeline* pipeline) override;
    void bind_descriptor_sets(Pipeline* pipeline, uint32_t first_set, 
                             const std::vector<DescriptorSet*>& sets) override;
    
    // Draw commands
    void draw(uint32_t vertex_count, uint32_t instance_count = 1, 
             uint32_t first_vertex = 0, uint32_t first_instance = 0) override;
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1,
                     uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0) override;
    void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override;
    
    // Resource binding
    void bind_vertex_buffers(uint32_t first_binding, const std::vector<Buffer*>& buffers,
                            const std::vector<VkDeviceSize>& offsets) override;
    void bind_index_buffer(Buffer* buffer, VkDeviceSize offset, IndexType index_type) override;
    
    // Synchronization
    void wait_idle() override;
    void flush_commands() override;
    
    // Vulkan-specific interface
    VkDevice get_device() const { return device_; }
    VkPhysicalDevice get_physical_device() const { return physical_device_; }
    VkQueue get_graphics_queue() const { return graphics_queue_; }
    VkQueue get_compute_queue() const { return compute_queue_; }
    VkQueue get_transfer_queue() const { return transfer_queue_; }
    
    VkCommandBuffer get_current_command_buffer() const { return current_command_buffer_; }
    const VulkanDeviceCapabilities& get_capabilities() const { return capabilities_; }
    
    VulkanMemoryAllocator& get_memory_allocator() { return *memory_allocator_; }
    VulkanCommandManager& get_command_manager() { return *command_manager_; }
    VulkanSyncManager& get_sync_manager() { return *sync_manager_; }

private:
    // Vulkan objects
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    
    // Queues
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    VkQueue transfer_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    
    // Swapchain resources
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    std::vector<VkFramebuffer> swapchain_framebuffers_;
    VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent_ = {0, 0};
    
    // Command recording
    VkCommandBuffer current_command_buffer_ = VK_NULL_HANDLE;
    uint32_t current_frame_index_ = 0;
    uint32_t current_image_index_ = 0;
    
    // Synchronization
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    
    // Managers
    std::unique_ptr<VulkanMemoryAllocator> memory_allocator_;
    std::unique_ptr<VulkanCommandManager> command_manager_;
    std::unique_ptr<VulkanSyncManager> sync_manager_;
    
    // Device capabilities and configuration
    VulkanDeviceCapabilities capabilities_;
    GraphicsDeviceConfig config_;
    
    // Debug and validation
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    bool validation_layers_enabled_ = false;
    
    // Initialization helpers
    bool create_instance();
    bool setup_debug_messenger();
    bool create_surface();
    bool pick_physical_device();
    bool create_logical_device();
    bool create_swapchain();
    bool create_image_views();
    bool create_sync_objects();
    
    // Swapchain management
    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
    bool recreate_swapchain();
    void cleanup_swapchain();
    
    // Device selection
    bool is_device_suitable(VkPhysicalDevice device);
    uint32_t rate_device_suitability(VkPhysicalDevice device);
    
    // Extension and layer support
    bool check_validation_layer_support();
    std::vector<const char*> get_required_extensions();
    bool check_device_extension_support(VkPhysicalDevice device);
    
    // Debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);
    
    // Resource management
    void transition_swapchain_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
    
    // Error handling
    void check_vk_result(VkResult result, const std::string& operation);
};

// Factory function for creating Vulkan device
std::unique_ptr<VulkanGraphicsDevice> create_vulkan_device(const GraphicsDeviceConfig& config = {});

// Vulkan utility functions
namespace vulkan_utils {
    std::string vk_result_to_string(VkResult result);
    std::string vk_format_to_string(VkFormat format);
    std::string vk_present_mode_to_string(VkPresentModeKHR present_mode);
    
    bool is_depth_format(VkFormat format);
    bool is_stencil_format(VkFormat format);
    bool is_depth_stencil_format(VkFormat format);
    
    VkImageAspectFlags get_image_aspect_flags(VkFormat format);
    uint32_t get_format_size(VkFormat format);
    
    // SPIR-V reflection utilities
    struct SpirVReflection {
        std::vector<VkDescriptorSetLayoutBinding> descriptor_bindings;
        std::vector<VkPushConstantRange> push_constant_ranges;
        VkShaderStageFlags stage_flags;
    };
    
    SpirVReflection reflect_spirv_shader(const std::vector<uint32_t>& spirv_code);
}

} // namespace gfx
