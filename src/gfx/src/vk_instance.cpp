#include "gfx/vk_instance.hpp"
#include "core/log.hpp"

namespace ve::gfx {

VulkanInstance::~VulkanInstance() { destroy(); }

bool VulkanInstance::create(const VulkanInstanceInfo& info) noexcept {
    if(created_) return true;
#ifdef VE_GFX_VULKAN
    // Minimal Vulkan instance creation (no error codes surfaced yet)
    // Defer actual vkCreateInstance integration until Vulkan SDK available.
    (void)info;
    created_ = true; // placeholder
#else
    (void)info;
    ve::log::warn("VulkanInstance created in stub mode (Vulkan SDK not found)");
    created_ = true;
#endif
    return created_;
}

void VulkanInstance::destroy() noexcept {
#ifdef VE_GFX_VULKAN
    if(instance_){
        // vkDestroyInstance(instance_, nullptr); // Uncomment when Vulkan SDK integrated
        instance_ = nullptr;
    }
#endif
    created_ = false;
}

} // namespace ve::gfx
