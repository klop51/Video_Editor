#include "render/render_graph.hpp"
#include "render/gpu_frame_resource.hpp"
#include "render/shader_renderer.hpp"
#include "core/log.hpp"
#include <memory>

namespace ve::render {

// Implementation struct for GpuRenderGraph
struct GpuRenderGraph::Impl {
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    std::unique_ptr<GpuFrameManager> frame_manager_;
    std::unique_ptr<ShaderRenderer> shader_renderer_;
    std::unique_ptr<ve::decode::VideoFrame> current_frame_;
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

        // For now, assume we have a decoded frame available
        // In a real implementation, this would come from the decode pipeline
        if (!current_frame_) {
            ve::log::debug("No frame available for rendering at timestamp " + std::to_string(req.timestamp_us));
            result.success = true; // Not an error, just no frame to render
            return result;
        }

        // Get or create GPU resource for the frame
        auto gpu_resource = frame_manager_->get_frame_resource(*current_frame_);
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

bool GpuRenderGraph::initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device) {
    return impl_->initialize(device);
}

FrameResult GpuRenderGraph::render(const FrameRequest& req) {
    return impl_->render(req);
}

void GpuRenderGraph::set_current_frame(const ve::decode::VideoFrame& frame) {
    impl_->current_frame_ = std::make_unique<ve::decode::VideoFrame>(frame);
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
