#pragma once
#include "decode/frame.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace ve::decode {

// Placeholder GPU handle abstraction
struct GpuTextureHandle {
    uint64_t id = 0; // backend-specific
    int width = 0;
    int height = 0;
};

struct UploadResult {
    GpuTextureHandle handle;
    bool reused = false; // true if cached texture reused
};

class IGpuUploader {
public:
    virtual ~IGpuUploader() = default;
    virtual std::optional<UploadResult> upload_rgba(const VideoFrame& rgba_frame) = 0;
};

// Factory returning a stub (no real GPU). Future: choose Vulkan/DX12 backend.
std::unique_ptr<IGpuUploader> create_stub_uploader();

} // namespace ve::decode
