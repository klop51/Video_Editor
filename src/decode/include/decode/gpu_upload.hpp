#pragma once
#include "decode/frame.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <memory>

namespace ve::decode {

// GPU handle abstraction supporting hardware decode
struct GpuTextureHandle {
    uint64_t id = 0; // backend-specific
    int width = 0;
    int height = 0;
    bool is_hardware_frame = false; // True if sourced from hardware decode
    void* native_handle = nullptr; // D3D11 texture, CUDA pointer, etc.
};

struct UploadResult {
    GpuTextureHandle handle;
    bool reused = false; // true if cached texture reused
    bool zero_copy = false; // true if no CPU->GPU transfer occurred
};

// Hardware frame information for zero-copy paths
struct HardwareFrameInfo {
    void* d3d11_texture = nullptr;
    void* cuda_ptr = nullptr;
    void* dxva_surface = nullptr;
    int format = 0; // Hardware pixel format
    bool can_zero_copy = false;
};

class IGpuUploader {
public:
    virtual ~IGpuUploader() = default;
    
    // Traditional CPU frame upload
    virtual std::optional<UploadResult> upload_rgba(const VideoFrame& rgba_frame) = 0;
    
    // Zero-copy hardware frame upload
    virtual std::optional<UploadResult> upload_hardware_frame(const HardwareFrameInfo& hw_info, 
                                                             int width, int height) = 0;
    
    // Check if hardware acceleration is available
    virtual bool supports_hardware_frames() const = 0;
    
    // Get optimal hardware frame format
    virtual int get_preferred_hardware_format() const = 0;
};

// Hardware-accelerated GPU uploader factory
std::unique_ptr<IGpuUploader> create_hardware_uploader();

// Factory returning a stub (no real GPU). Future: choose Vulkan/DX12 backend.
std::unique_ptr<IGpuUploader> create_stub_uploader();

} // namespace ve::decode
