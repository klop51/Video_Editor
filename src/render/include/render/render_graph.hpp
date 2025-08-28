#pragma once
#include <memory>
#include <string>
#include <cstdint>

namespace ve::decode {
    struct VideoFrame;
}

namespace ve::gfx {
    class GraphicsDevice;
}

namespace ve::render {

// Minimal placeholder node interface; will evolve.
class INode {
public:
    virtual ~INode() = default;
    virtual std::string name() const = 0;
};

struct FrameRequest {
    int64_t timestamp_us = 0;
};

struct FrameResult {
    bool success = false;
    // Future: GPU handle / CPU buffer etc.
};

class RenderGraph {
public:
    RenderGraph() = default;
    virtual ~RenderGraph() = default;
    virtual FrameResult render(const FrameRequest& req);
    virtual void set_viewport(int width, int height) { (void)width; (void)height; }
    virtual void set_brightness(float brightness) { (void)brightness; }
};

// GPU-enabled render graph
class GpuRenderGraph : public RenderGraph {
public:
    GpuRenderGraph();
    ~GpuRenderGraph();
    bool initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device);
    FrameResult render(const FrameRequest& req) override;

    // Set the current frame to render
    void set_current_frame(const ve::decode::VideoFrame& frame);

    // Set viewport dimensions
    void set_viewport(int width, int height);

    // Set effect parameters
    void set_brightness(float brightness);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Factory function to create GPU-enabled render graph
std::unique_ptr<RenderGraph> create_gpu_render_graph(std::shared_ptr<ve::gfx::GraphicsDevice> device);

} // namespace ve::render
