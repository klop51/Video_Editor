#pragma once
#include <memory>
#include <string>
#include <cstdint>

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
    FrameResult render(const FrameRequest& req);
};

} // namespace ve::render
