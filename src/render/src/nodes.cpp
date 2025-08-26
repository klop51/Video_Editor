#include "render/nodes.hpp"

namespace ve::render {

bool SourceNode::get_frame(int64_t pts, ve::cache::CachedFrame& out) {
    if(!cache_) return false;
    ve::cache::FrameKey key; key.pts_us = pts;
    return cache_->get(key, out);
}

} // namespace ve::render
