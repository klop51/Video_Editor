#include "render/render_graph.hpp"
#include "render/gpu_frame_resource.hpp"
#include "render/shader_renderer.hpp"
#include "core/log.hpp"
#include <memory>

namespace ve::render {

// Optimized frame storage using persistent upload buffer instead of deep copying
struct CurrentFrame {
    int width = 0;
    int height = 0;
    int64_t pts = 0;
    ve::decode::PixelFormat format = ve::decode::PixelFormat::RGB24;
    std::vector<uint8_t> uploadBuffer; // persistent buffer, reused across frames
};

// Implementation struct for GpuRenderGraph
struct GpuRenderGraph::Impl {
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    std::unique_ptr<GpuFrameManager> frame_manager_;
    std::unique_ptr<ShaderRenderer> shader_renderer_;
    CurrentFrame current_frame_; // Use optimized frame storage instead of deep copy
    int viewport_width_ = 1920;
    int viewport_height_ = 1080;
    float brightness_ = 0.0f;

    Impl() = default;

    bool initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device) {
        device_ = device;
        frame_manager_ = std::make_unique<GpuFrameManager>(device);
        shader_renderer_ = std::make_unique<ShaderRenderer>();

        if (!shader_renderer_->initialize(device)) {
            ve::log::error("Failed to initialize shader renderer");
            return false;
        }

        return true;
    }

    FrameResult render(const FrameRequest& req) {
        FrameResult result;
        result.success = false;

        if (!frame_manager_ || !shader_renderer_) {
            ve::log::error("Render graph not properly initialized");
            return result;
        }

        // Check if we have frame data available
        if (current_frame_.uploadBuffer.empty() || current_frame_.width == 0 || current_frame_.height == 0) {
            ve::log::debug("No frame available for rendering at timestamp " + std::to_string(req.timestamp_us));
            result.success = true; // Not an error, just no frame to render
            return result;
        }

        // Create temporary VideoFrame for compatibility with existing GPU resource manager
        // TODO: Eventually update GpuFrameManager to work directly with CurrentFrame
        ve::decode::VideoFrame temp_frame;
        temp_frame.width = current_frame_.width;
        temp_frame.height = current_frame_.height;
        temp_frame.pts = current_frame_.pts;
        temp_frame.format = current_frame_.format;
        temp_frame.data = current_frame_.uploadBuffer; // Reference the persistent buffer

        // Get or create GPU resource for the frame
        auto gpu_resource = frame_manager_->get_frame_resource(temp_frame);
        if (!gpu_resource) {
            ve::log::error("Failed to create GPU resource for frame");
            return result;
        }

        // Render the frame with effects
        if (shader_renderer_->render_frame(gpu_resource, viewport_width_, viewport_height_, brightness_)) {
            result.success = true;
            ve::log::debug("Frame rendered successfully at timestamp " + std::to_string(req.timestamp_us));
        } else {
            ve::log::error("Failed to render frame");
        }

        return result;
    }
};

GpuRenderGraph::GpuRenderGraph() : impl_(std::make_unique<Impl>()) {}

GpuRenderGraph::~GpuRenderGraph() = default;

bool GpuRenderGraph::initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device) {
    return impl_->initialize(device);
}

FrameResult GpuRenderGraph::render(const FrameRequest& req) {
    return impl_->render(req);
}

void GpuRenderGraph::set_current_frame(const ve::decode::VideoFrame& frame) {
    // Optimized approach: reuse upload buffer instead of deep copying entire VideoFrame
    
    // Ensure upload buffer is large enough (grows only when needed)
    if (impl_->current_frame_.uploadBuffer.capacity() < frame.data.size()) {
        impl_->current_frame_.uploadBuffer.reserve(frame.data.size());
        ve::log::debug("GPU upload buffer expanded to " + std::to_string(frame.data.size()) + " bytes");
    }
    
    // Copy frame data into persistent buffer (single memcpy instead of full object allocation)
    impl_->current_frame_.uploadBuffer.assign(frame.data.begin(), frame.data.end());
    
    // Copy frame metadata (lightweight)
    impl_->current_frame_.width = frame.width;
    impl_->current_frame_.height = frame.height;
    impl_->current_frame_.pts = frame.pts;
    impl_->current_frame_.format = frame.format;
    
    ve::log::debug("Frame " + std::to_string(frame.pts) + " optimized upload: " + 
                  std::to_string(frame.width) + "x" + std::to_string(frame.height) + 
                  ", buffer reused: " + std::to_string(frame.data.size()) + " bytes");
}

void GpuRenderGraph::set_viewport(int width, int height) {
    impl_->viewport_width_ = width;
    impl_->viewport_height_ = height;
    if (impl_->shader_renderer_) {
        impl_->shader_renderer_->set_viewport(width, height);
    }
}

void GpuRenderGraph::set_brightness(float brightness) {
    impl_->brightness_ = brightness;
}

} // namespace ve::render
