#include "render/render_graph.hpp"
#include "render/gpu_frame_resource.hpp"
#include "render/shader_renderer.hpp"
#include "core/log.hpp"
#include <memory>
#include <cstring>
#include <string>
// ChatGPT Optimization: Direct OpenGL support (disabled due to function pointer issues)
// #ifdef _WIN32
// #include <windows.h>
// #endif
// #include <GL/gl.h>
// #include <GL/glext.h>

namespace ve::render {

// ChatGPT Optimization: Direct OpenGL texture management with persistent buffers
struct CurrentFrame {
    int width = 0;
    int height = 0;
    int64_t pts = 0;
    ve::decode::PixelFormat format = ve::decode::PixelFormat::RGB24;
    std::vector<uint8_t> uploadBuffer; // persistent buffer, grows only when needed
    std::shared_ptr<ve::core::StageTimer> timing;
};

// Implementation struct for GpuRenderGraph with memory optimization
struct GpuRenderGraph::Impl {
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    std::unique_ptr<GpuFrameManager> frame_manager_;
    std::unique_ptr<ShaderRenderer> shader_renderer_;
    
    // ChatGPT Optimization: Persistent frame buffer (memory management only)
    CurrentFrame current_frame_;
    
    // ChatGPT Crash Prevention: Frame acceptance control
    std::atomic<bool> acceptingFrames_{true};
    bool gpu_failure_logged_ = false;
    unsigned int last_gpu_error_ = 0;
    
    int viewport_width_ = 1920;
    int viewport_height_ = 1080;
    float brightness_ = 0.0f;

    Impl() = default;
    ~Impl() = default;

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

    void trim() {
        current_frame_.uploadBuffer.clear();
        current_frame_.uploadBuffer.shrink_to_fit();
        if (frame_manager_) {
            frame_manager_->trim();
        }
    }

    void handle_gpu_failure(unsigned int error_code) {
        last_gpu_error_ = error_code;
        acceptingFrames_.store(false, std::memory_order_release);
        if (!gpu_failure_logged_) {
            ve::log::error("GPU upload failed (hr=0x" + std::to_string(error_code) + "). Falling back to CPU path.");
            gpu_failure_logged_ = true;
        }

        current_frame_.uploadBuffer.clear();
        current_frame_.width = 0;
        current_frame_.height = 0;
        current_frame_.pts = 0;
        current_frame_.timing.reset();

        if (frame_manager_) {
            frame_manager_->clear_cache();
        }

        if (device_) {
            device_->flush();
        }
    }
    
    FrameResult render(const FrameRequest& req) {
        // ChatGPT Stop Token System: Check if we're still accepting frames
        if (!acceptingFrames_.load(std::memory_order_acquire)) {
            FrameResult result;
            result.success = true;
            if (gpu_failure_logged_) {
                result.request_cpu_fallback = true;
            }
            return result;
        }
        
        FrameResult result;
        result.success = false;

        // ChatGPT Optimization: Check if we have frame data to render
        if (current_frame_.uploadBuffer.empty() || current_frame_.width == 0 || current_frame_.height == 0) {
            ve::log::debug("No frame available for rendering at timestamp " + std::to_string(req.timestamp_us));
            result.success = true; // Not an error, just no frame to render
            return result;
        }

        // Use existing rendering path with optimized memory management
        try {
            if (!frame_manager_ || !shader_renderer_) {
                ve::log::error("Render graph not properly initialized");
                return result;
            }

            // Create temporary VideoFrame for compatibility with existing GPU resource manager
            ve::decode::VideoFrame temp_frame;
            temp_frame.width = current_frame_.width;
            temp_frame.height = current_frame_.height;
            temp_frame.pts = current_frame_.pts;
            temp_frame.format = current_frame_.format;
            temp_frame.data = current_frame_.uploadBuffer; // Reference the persistent buffer
            temp_frame.timing = current_frame_.timing;

            // Get or create GPU resource for the frame
            auto gpu_resource = frame_manager_->get_frame_resource(temp_frame);
            unsigned int error_code = 0;
            if (!gpu_resource && frame_manager_ && frame_manager_->consume_device_error(error_code)) {
                handle_gpu_failure(error_code);
                result.success = true;
                result.request_cpu_fallback = true;
                return result;
            }

            if (!gpu_resource) {
                ve::log::error("Failed to create GPU resource for frame");
                result.request_cpu_fallback = false;
                return result;
            }

            // Render the frame with effects
            if (shader_renderer_->render_frame(gpu_resource, viewport_width_, viewport_height_, brightness_)) {
                result.success = true;
                ve::log::debug("Frame rendered successfully at timestamp " + std::to_string(req.timestamp_us));
            } else {
                if (auto timer = gpu_resource->take_timing()) {
                    timer->endAndMaybeLog("TIMING_GPU_FAIL");
                }
                ve::log::error("Failed to render frame");
            }
        } catch (const std::exception& e) {
            ve::log::error("Exception during rendering: " + std::string(e.what()));
        } catch (...) {
            ve::log::error("Unknown exception during rendering");
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
    // ChatGPT Crash Prevention: Check if we're still accepting frames
    if (!impl_->acceptingFrames_.load(std::memory_order_acquire)) return;
    
    // ChatGPT Optimization: Reuse upload buffer for memory efficiency
    
    // Ensure upload buffer is large enough (grows only when needed, never shrinks)
    const size_t bytes = frame.data.size();
    if (impl_->current_frame_.uploadBuffer.capacity() < bytes) {
        impl_->current_frame_.uploadBuffer.reserve(bytes);
        ve::log::debug("GPU upload buffer expanded to " + std::to_string(bytes) + " bytes");
    }

    // Single memcpy into persistent buffer - no intermediate allocations
    impl_->current_frame_.uploadBuffer.resize(bytes);
    if (bytes > 0) {
        std::memcpy(impl_->current_frame_.uploadBuffer.data(), frame.data.data(), bytes);
    }
    
    // Copy frame metadata (lightweight)
    impl_->current_frame_.width = frame.width;
    impl_->current_frame_.height = frame.height;
    impl_->current_frame_.pts = frame.pts;
    impl_->current_frame_.format = frame.format;
    impl_->current_frame_.timing = frame.timing;
    
    ve::log::debug("Frame " + std::to_string(frame.pts) + " optimized: " + 
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

void GpuRenderGraph::requestStop() {
    // ChatGPT Crash Prevention: Stop accepting frames (no QTimer in this implementation)
    impl_->acceptingFrames_.store(false, std::memory_order_release);
    if (impl_->frame_manager_) {
        impl_->frame_manager_->clear_cache();
    }
    impl_->current_frame_.uploadBuffer.clear();
    impl_->current_frame_.width = 0;
    impl_->current_frame_.height = 0;
    impl_->current_frame_.pts = 0;
    impl_->current_frame_.timing.reset();
    if (impl_->device_) {
        impl_->device_->flush();
    }
    ve::log::info("GpuRenderGraph: requestStop - flushed GPU resources");
}

void GpuRenderGraph::setAcceptingFrames(bool on) {
    impl_->acceptingFrames_.store(on, std::memory_order_release);
    if (on) {
        impl_->gpu_failure_logged_ = false;
        impl_->last_gpu_error_ = 0;
    }
}

unsigned int GpuRenderGraph::last_gpu_error() const {
    return impl_->last_gpu_error_;
}

void GpuRenderGraph::trim() {
    if (impl_) {
        impl_->trim();
    }
}

} // namespace ve::render
