#include "render/render_graph.hpp"
#include "render/nodes.hpp"
#include "core/log.hpp"

namespace ve::render {

FrameResult RenderGraph::render(const FrameRequest& req) {
    (void)req; // placeholder for now just returns success
    FrameResult r; r.success = true; return r;
}

} // namespace ve::render
