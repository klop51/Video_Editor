#include "gfx/vk_device.hpp"
#include "gfx/vk_instance.hpp"
#include "core/log.hpp"

namespace ve::gfx {

VulkanDevice::~VulkanDevice() { destroy(); }

bool VulkanDevice::create(const VulkanInstance& instance, const VulkanDeviceInfo& info) noexcept {
    if(created_) return true;
    (void)instance; (void)info;
#ifdef VE_GFX_VULKAN
    // Enumerate physical devices, pick one, create logical device + queues (deferred until Vulkan SDK available)
    created_ = true; // placeholder success
#else
    ve::log::warn("VulkanDevice created in stub mode (Vulkan SDK not found)");
    created_ = true;
#endif
    return created_;
}

void VulkanDevice::destroy() noexcept {
#ifdef VE_GFX_VULKAN
    if(device_){
        // vkDestroyDevice(device_, nullptr); // Uncomment when Vulkan SDK integrated
        device_ = nullptr;
    }
    physical_ = nullptr;
#endif
    created_ = false;
}

} // namespace ve::gfx
