#include "render/gpu_frame_resource.hpp"
#include "core/log.hpp"
#include "decode/color_convert.hpp"
#include <algorithm>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace ve::render {

namespace {
std::string format_hresult(unsigned int hr) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << hr;
    return oss.str();
}
}

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
    // Removed per-frame debug logging to prevent spam (was causing potential crashes)
    
    if (!device_) {
        ve::log::error("GPU frame resource not initialized");
        return false;
    }

    timing_ = frame.timing;
    auto timer = timing_;
    clear_error();

    // Convert frame data to RGBA8 for upload
    const uint8_t* upload_data = nullptr;
    size_t upload_size = 0;
    converted_data_.clear();

    switch (frame.format) {
        case ve::decode::PixelFormat::RGBA32:
            upload_data = frame.data.data();
            upload_size = frame.data.size();
            break;

        case ve::decode::PixelFormat::RGB24: {
            const size_t pixel_count = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
            converted_data_.resize(pixel_count * 4);
            const uint8_t* src = frame.data.data();
            uint8_t* dst = converted_data_.data();
            for (size_t i = 0; i < pixel_count; ++i) {
                dst[i * 4 + 0] = src[i * 3 + 0];
                dst[i * 4 + 1] = src[i * 3 + 1];
                dst[i * 4 + 2] = src[i * 3 + 2];
                dst[i * 4 + 3] = 255;
            }
            upload_data = converted_data_.data();
            upload_size = converted_data_.size();
            break;
        }

        case ve::decode::PixelFormat::YUV420P: {
            const size_t pixel_count = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
            converted_data_.resize(pixel_count * 4);

            const uint8_t* y_plane = frame.data.data();
            const uint8_t* u_plane = y_plane + (frame.width * frame.height);
            const uint8_t* v_plane = u_plane + (frame.width * frame.height / 4);

            for (int y = 0; y < frame.height; ++y) {
                for (int x = 0; x < frame.width; ++x) {
                    int y_val = y_plane[y * frame.width + x];
                    int u_val = u_plane[(y / 2) * (frame.width / 2) + (x / 2)];
                    int v_val = v_plane[(y / 2) * (frame.width / 2) + (x / 2)];

                    double y_d = y_val;
                    double u_d = u_val - 128.0;
                    double v_d = v_val - 128.0;

                    int r = static_cast<int>(y_d + 1.402 * v_d);
                    int g = static_cast<int>(y_d - 0.344 * u_d - 0.714 * v_d);
                    int b = static_cast<int>(y_d + 1.772 * u_d);

                    r = std::max(0, std::min(255, r));
                    g = std::max(0, std::min(255, g));
                    b = std::max(0, std::min(255, b));

                    size_t idx = static_cast<size_t>(y * frame.width + x);
                    converted_data_[idx * 4 + 0] = static_cast<uint8_t>(r);
                    converted_data_[idx * 4 + 1] = static_cast<uint8_t>(g);
                    converted_data_[idx * 4 + 2] = static_cast<uint8_t>(b);
                    converted_data_[idx * 4 + 3] = 255;
                }
            }

            upload_data = converted_data_.data();
            upload_size = converted_data_.size();
            break;
        }

        case ve::decode::PixelFormat::NV12: {
            auto rgba_frame_opt = ve::decode::to_rgba_scaled(frame, frame.width, frame.height);
            if (!rgba_frame_opt.has_value()) {
                ve::log::error("Failed to convert NV12 frame to RGBA");
                return false;
            }

            converted_rgba_frame_ = rgba_frame_opt.value();
            upload_data = converted_rgba_frame_.data.data();
            upload_size = converted_rgba_frame_.data.size();
            break;
        }

        default:
            ve::log::error("Unsupported pixel format for GPU upload: " + std::to_string(static_cast<int>(frame.format)));
            return false;
    }

    if (timer) {
        timer->afterConversion();
    }

    if (!upload_data || upload_size == 0) {
        ve::log::error("No frame data available for GPU upload");
        return false;
    }

    const size_t expected_size = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height) * 4;
    if (upload_size < expected_size) {
        ve::log::error("Frame upload buffer is smaller than expected RGBA size");
        return false;
    }

    // Create or recreate texture if dimensions changed
    if (texture_id_ == 0 || width_ != frame.width || height_ != frame.height) {
        if (texture_id_ != 0) {
            ve::log::info("GPU_DEBUG: Destroying existing texture " + std::to_string(texture_id_));
            device_->destroy_texture(texture_id_);
        }

        // Removed per-frame texture creation logging to prevent spam
        texture_id_ = device_->create_dynamic_texture(frame.width, frame.height, 0);
        if (texture_id_ == 0) {
            ve::log::error("Failed to create GPU texture");
            return false;
        }

        width_ = frame.width;
        height_ = frame.height;
        format_ = ve::decode::PixelFormat::RGBA32;
    }

    ve::gfx::GraphicsDevice::MappedTexture mapped;
    unsigned int error_code = 0;
    if (!device_->map_texture_discard(texture_id_, mapped, &error_code)) {
        device_error_ = true;
        last_error_code_ = error_code;
        ve::log::error("Failed to map GPU texture for frame upload (hr=0x" + format_hresult(error_code) + ")");
        return false;
    }

    const size_t src_row_pitch = static_cast<size_t>(frame.width) * 4;
    for (int y = 0; y < frame.height; ++y) {
        std::memcpy(static_cast<uint8_t*>(mapped.data) + static_cast<size_t>(y) * mapped.row_pitch,
                    upload_data + static_cast<size_t>(y) * src_row_pitch,
                    src_row_pitch);
    }

    device_->unmap_texture(texture_id_);

    if (timer) {
        timer->afterUpload();
    }

    return true;
}

void GpuFrameResource::trim_cpu_buffers() {
    converted_data_.clear();
    converted_data_.shrink_to_fit();
    converted_rgba_frame_.data.clear();
    converted_rgba_frame_.data.shrink_to_fit();
}

GpuFrameManager::GpuFrameManager(std::shared_ptr<ve::gfx::GraphicsDevice> device)
    : device_(std::move(device)),
      resource_ring_(kRingSize) {
}

GpuFrameManager::~GpuFrameManager() {
    clear_cache();
}

std::shared_ptr<GpuFrameResource> GpuFrameManager::get_frame_resource(const ve::decode::VideoFrame& frame) {
    if (!device_) {
        return nullptr;
    }

    auto& slot = resource_ring_[next_index_];
    const size_t current_index = next_index_;
    next_index_ = (next_index_ + 1) % kRingSize;

    if (!slot) {
        slot = std::make_shared<GpuFrameResource>();
        if (!slot->initialize(device_)) {
            ve::log::error("Failed to initialize GPU frame resource for slot " + std::to_string(current_index));
            slot.reset();
            return nullptr;
        }
    }

    if (!slot->upload_frame(frame)) {
        ve::log::error("Failed to upload frame into GPU slot " + std::to_string(current_index));
        if (slot->has_device_error()) {
            device_error_pending_ = true;
            pending_error_code_ = slot->last_error_code();
        }
        return nullptr;
    }

    return slot;
}

void GpuFrameManager::clear_cache() {
    resource_ring_.clear();
    resource_ring_.resize(kRingSize);
    next_index_ = 0;
    device_error_pending_ = false;
    pending_error_code_ = 0;
}

void GpuFrameManager::trim() {
    for (auto& slot : resource_ring_) {
        if (slot) {
            slot->trim_cpu_buffers();
        }
    }
}

bool GpuFrameManager::consume_device_error(unsigned int& error_code) {
    if (!device_error_pending_) {
        return false;
    }

    error_code = pending_error_code_;
    device_error_pending_ = false;
    pending_error_code_ = 0;
    return true;
}

} // namespace ve::render
