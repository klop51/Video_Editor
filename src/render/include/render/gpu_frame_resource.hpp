#pragma once
#include "../../decode/include/decode/decoder.hpp"
#include "../../gfx/include/gfx/vk_device.hpp"
#include <memory>
#include <cstdint>
#include <utility>

#include "../../core/include/core/stage_timer.hpp"

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

    bool has_device_error() const noexcept { return device_error_; }
    unsigned int last_error_code() const noexcept { return last_error_code_; }
    void clear_error() noexcept {
        device_error_ = false;
        last_error_code_ = 0;
    }

    std::shared_ptr<ve::core::StageTimer> take_timing() {
        return std::exchange(timing_, nullptr);
    }

    // Get texture ID for rendering
    uint32_t get_texture_id() const { return texture_id_; }

    // Get frame dimensions
    int get_width() const { return width_; }
    int get_height() const { return height_; }

    // Check if resource is valid
    bool is_valid() const { return texture_id_ != 0; }
    void trim_cpu_buffers();

private:
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    uint32_t texture_id_ = 0;
    int width_ = 0;
    int height_ = 0;
    ve::decode::PixelFormat format_ = ve::decode::PixelFormat::Unknown;
    std::vector<uint8_t> converted_data_; // For storing converted RGB data
    ve::decode::VideoFrame converted_rgba_frame_; // For storing converted RGBA frame
    std::shared_ptr<ve::core::StageTimer> timing_;
    bool device_error_ = false;
    unsigned int last_error_code_ = 0;
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

    bool consume_device_error(unsigned int& error_code);
    void trim();

private:
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    std::vector<std::shared_ptr<GpuFrameResource>> resource_ring_;
    size_t next_index_ = 0;
    static constexpr size_t kRingSize = 3;
    bool device_error_pending_ = false;
    unsigned int pending_error_code_ = 0;
};

} // namespace ve::render
