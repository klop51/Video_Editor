#pragma once
#include "decode/decoder.hpp"
#include "gfx/vk_device.hpp"
#include <memory>
#include <cstdint>

namespace ve::render {

// GPU texture resource for decoded frames
class GpuFrameResource {
public:
    GpuFrameResource() = default;
    ~GpuFrameResource();

    // Initialize with graphics device
    bool initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device);

    // Upload decoded frame to GPU texture
    bool upload_frame(const ve::decode::VideoFrame& frame);

    // Get texture ID for rendering
    uint32_t get_texture_id() const { return texture_id_; }

    // Get frame dimensions
    int get_width() const { return width_; }
    int get_height() const { return height_; }

    // Check if resource is valid
    bool is_valid() const { return texture_id_ != 0; }

private:
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    uint32_t texture_id_ = 0;
    int width_ = 0;
    int height_ = 0;
    ve::decode::PixelFormat format_ = ve::decode::PixelFormat::Unknown;
};

// Manager for GPU frame resources with caching
class GpuFrameManager {
public:
    explicit GpuFrameManager(std::shared_ptr<ve::gfx::GraphicsDevice> device);
    ~GpuFrameManager();

    // Get or create GPU resource for a frame
    std::shared_ptr<GpuFrameResource> get_frame_resource(const ve::decode::VideoFrame& frame);

    // Clear all cached resources
    void clear_cache();

private:
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    std::vector<std::shared_ptr<GpuFrameResource>> resource_cache_;
};

} // namespace ve::render
