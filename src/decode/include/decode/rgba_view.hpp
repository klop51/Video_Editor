#pragma once
#include <optional>
#include "decode/frame.hpp"

namespace ve::decode {

// Lightweight non-owning RGBA frame view.
// Lifetime rules:
//  * Memory is backed by a small thread-local ring (currently size 2) of reusable buffers.
//  * On a given thread the data pointer stays valid until that thread performs two further
//    successful conversions (i.e. at most 2 in-flight views per thread).
//  * Never use across threads without copying. Safe to pass pointer immediately to GPU upload
//    or to construct a QImage that is then copied into a QPixmap.
struct RgbaView {
    uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0; // bytes per row (always width*4 for now)
};

// Convert to RGBA (and optionally scale) writing into a thread-local reusable buffer ring.
// Returns std::nullopt on failure. Intended to reduce per-frame heap churn for hot paths.
// Scaling uses swscale when FFmpeg is available; otherwise if scaling is requested and
// manual path is used, a simple nearest-neighbor fallback is applied.
std::optional<RgbaView> to_rgba_scaled_view(const VideoFrame& src, int target_w, int target_h) noexcept;

}
