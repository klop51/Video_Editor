#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace ve::gfx {

struct VulkanInstanceInfo {
    std::vector<const char*> enabled_extensions;
    bool validation_enabled = false;
};

class VulkanInstance {
public:
    VulkanInstance() = default;
    ~VulkanInstance();
    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;
    VulkanInstance(VulkanInstance&& other) noexcept { *this = std::move(other); }
    VulkanInstance& operator=(VulkanInstance&& other) noexcept {
        if(this!=&other){
            destroy();
            created_ = other.created_;
#ifdef VE_GFX_VULKAN
            instance_ = other.instance_;
            other.instance_ = nullptr;
#endif
            other.created_ = false;
        }
        return *this;
    }

    bool create(const VulkanInstanceInfo& info) noexcept; // returns false on failure
    void destroy() noexcept;
    bool is_valid() const noexcept { return created_; }

private:
    bool created_ = false;
#ifdef VE_GFX_VULKAN
    struct VkInstance_T* instance_ = nullptr;
#endif
};

} // namespace ve::gfx
