#pragma once
#include <cstdint>
#include <vector>

namespace ve::gfx {

class VulkanInstance; // fwd

struct VulkanDeviceInfo {
    bool enable_swapchain = true;
};

class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice();
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&& other) noexcept { *this = std::move(other); }
    VulkanDevice& operator=(VulkanDevice&& other) noexcept {
        if(this!=&other){
            destroy();
            created_ = other.created_;
#ifdef VE_GFX_VULKAN
            physical_ = other.physical_;
            device_ = other.device_;
            other.physical_ = nullptr;
            other.device_ = nullptr;
#endif
            other.created_ = false;
        }
        return *this;
    }

    bool create(const VulkanInstance& instance, const VulkanDeviceInfo& info) noexcept;
    void destroy() noexcept;
    bool is_valid() const noexcept { return created_; }

private:
    bool created_ = false;
#ifdef VE_GFX_VULKAN
    struct VkPhysicalDevice_T* physical_ = nullptr;
    struct VkDevice_T* device_ = nullptr;
#endif
};

} // namespace ve::gfx
