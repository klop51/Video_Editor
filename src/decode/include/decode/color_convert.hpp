#pragma once
#include "decode/frame.hpp"
#include <optional>

namespace ve::decode {

// Convert limited set of pixel formats to RGBA32 (8-bit) packed.
// Returns std::nullopt if format unsupported.
std::optional<VideoFrame> to_rgba(const VideoFrame& src) noexcept;

}
