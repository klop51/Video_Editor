#pragma once
#include <string>
#include <memory>
#include <cstdint>
#include "render/render_graph.hpp"
#include "cache/frame_cache.hpp"

namespace ve::render {

// SourceNode pulls decoded frames (or cached) for a given timestamp
class SourceNode : public INode {
public:
    explicit SourceNode(ve::cache::FrameCache* cache) : cache_(cache) {}
    std::string name() const override { return "SourceNode"; }
    // For now returns bool if frame found in cache
    bool get_frame(int64_t pts, ve::cache::CachedFrame& out);
private:
    ve::cache::FrameCache* cache_{}; // non-owning
};

// OutputNode represents final stage prior to UI (placeholder)
class OutputNode : public INode {
public:
    std::string name() const override { return "OutputNode"; }
};

} // namespace ve::render
