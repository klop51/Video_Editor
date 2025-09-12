#include "render/gpu_frame_resource.hpp"
#include "core/log.hpp"
#include <algorithm>

namespace ve::render {

GpuFrameResource::~GpuFrameResource() {
    if (device_ && texture_id_ != 0) {
        device_->destroy_texture(texture_id_);
        texture_id_ = 0;
    }
}

bool GpuFrameResource::initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device) {
    device_ = device;
    return device_ != nullptr;
}

bool GpuFrameResource::upload_frame(const ve::decode::VideoFrame& frame) {
    if (!device_) {
        ve::log::error("GPU frame resource not initialized");
        return false;
    }

    // Convert frame format to OpenGL format
    int gl_format = 0; // 0 = RGBA, 1 = RGB
    void* upload_data = nullptr;
    size_t upload_size = 0;

    switch (frame.format) {
        case ve::decode::PixelFormat::RGBA32:
            gl_format = 0; // RGBA
            upload_data = const_cast<uint8_t*>(frame.data.data());
            upload_size = frame.data.size();
            break;

        case ve::decode::PixelFormat::RGB24:
            gl_format = 1; // RGB
            upload_data = const_cast<uint8_t*>(frame.data.data());
            upload_size = frame.data.size();
            break;

        case ve::decode::PixelFormat::YUV420P: {
            // Convert YUV420P to RGB for now (simple CPU conversion)
            // In a real implementation, this would be done with shaders
            gl_format = 1; // RGB
            upload_size = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height) * 3;
            std::vector<uint8_t> rgb_data(upload_size);

            // Simple YUV to RGB conversion (not color-accurate)
            const uint8_t* y_plane = frame.data.data();
            const uint8_t* u_plane = y_plane + (frame.width * frame.height);
            const uint8_t* v_plane = u_plane + (frame.width * frame.height / 4);

            for (int y = 0; y < frame.height; ++y) {
                for (int x = 0; x < frame.width; ++x) {
                    int y_val = y_plane[y * frame.width + x];
                    int u_val = u_plane[(y/2) * (frame.width/2) + (x/2)];
                    int v_val = v_plane[(y/2) * (frame.width/2) + (x/2)];

                    // Simple YUV to RGB conversion
                    double y_d = y_val;
                    double u_d = u_val - 128.0;
                    double v_d = v_val - 128.0;
                    
                    int r = static_cast<int>(y_d + 1.402 * v_d);
                    int g = static_cast<int>(y_d - 0.344 * u_d - 0.714 * v_d);
                    int b = static_cast<int>(y_d + 1.772 * u_d);

                    // Clamp to valid range
                    r = std::max(0, std::min(255, r));
                    g = std::max(0, std::min(255, g));
                    b = std::max(0, std::min(255, b));

                    size_t idx = static_cast<size_t>((y * frame.width + x) * 3);
                    rgb_data[idx] = static_cast<uint8_t>(r);
                    rgb_data[idx + 1] = static_cast<uint8_t>(g);
                    rgb_data[idx + 2] = static_cast<uint8_t>(b);
                }
            }

            upload_data = rgb_data.data();
            break;
        }

        default:
            ve::log::error("Unsupported pixel format for GPU upload: " + std::to_string(static_cast<int>(frame.format)));
            return false;
    }

    // Create or recreate texture if dimensions changed
    if (texture_id_ == 0 || width_ != frame.width || height_ != frame.height || format_ != frame.format) {
        if (texture_id_ != 0) {
            device_->destroy_texture(texture_id_);
        }

        texture_id_ = device_->create_texture(frame.width, frame.height, gl_format);
        if (texture_id_ == 0) {
            ve::log::error("Failed to create GPU texture");
            return false;
        }

        width_ = frame.width;
        height_ = frame.height;
        format_ = frame.format;
    }

    // Upload data to texture
    device_->upload_texture(texture_id_, upload_data, frame.width, frame.height, gl_format);

    return true;
}

GpuFrameManager::GpuFrameManager(std::shared_ptr<ve::gfx::GraphicsDevice> device)
    : device_(device) {
}

GpuFrameManager::~GpuFrameManager() {
    clear_cache();
}

std::shared_ptr<GpuFrameResource> GpuFrameManager::get_frame_resource(const ve::decode::VideoFrame& frame) {
    // For now, create a new resource each time
    // In a real implementation, this would cache based on frame characteristics
    auto resource = std::make_shared<GpuFrameResource>();
    if (resource->initialize(device_)) {
        if (resource->upload_frame(frame)) {
            resource_cache_.push_back(resource);
            return resource;
        }
    }
    return nullptr;
}

void GpuFrameManager::clear_cache() {
    resource_cache_.clear();
}

} // namespace ve::render
