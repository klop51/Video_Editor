#pragma once
#include "frame.hpp"
#include <optional>

namespace ve::decode {

// Convert limited set of pixel formats to RGBA32 (8-bit) packed.
// Returns std::nullopt if format unsupported.
std::optional<VideoFrame> to_rgba(const VideoFrame& src) noexcept;

// Convert to RGBA32 and optionally resize to target size using the best available backend.
// If target_w/target_h are <= 0, falls back to source size.
// When FFmpeg is enabled, uses swscale for fast SIMD conversion + scaling.
std::optional<VideoFrame> to_rgba_scaled(const VideoFrame& src, int target_w, int target_h) noexcept;

}
