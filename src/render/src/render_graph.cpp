#include "render/render_graph.hpp"
#include "render/nodes.hpp"
namespace ve::render {

FrameResult RenderGraph::render(const FrameRequest& req) {
    (void)req; // placeholder for now just returns success
    FrameResult r; r.success = true; return r;
}

// Factory function to create GPU-enabled render graph
std::unique_ptr<RenderGraph> create_gpu_render_graph(std::shared_ptr<ve::gfx::GraphicsDevice> device) {
    auto graph = std::make_unique<GpuRenderGraph>();
    if (graph->initialize(device)) {
        return graph;
    }
    return nullptr;
}

} // namespace ve::render
