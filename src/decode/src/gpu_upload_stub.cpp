#include "decode/gpu_upload.hpp"
#include "core/log.hpp"
#include <atomic>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace ve::decode {

#ifdef _WIN32
namespace {

class D3D11HardwareUploader final : public IGpuUploader {
public:
    D3D11HardwareUploader() {
        initialize_d3d11();
    }
    
    std::optional<UploadResult> upload_rgba(const VideoFrame& rgba_frame) override {
        // Traditional CPU->GPU upload
        UploadResult r;
        r.handle.id = next_id_++;
        r.handle.width = rgba_frame.width;
        r.handle.height = rgba_frame.height;
        r.handle.is_hardware_frame = false;
        r.reused = false;
        r.zero_copy = false;
        return r;
    }
    
    std::optional<UploadResult> upload_hardware_frame(const HardwareFrameInfo& hw_info, 
                                                     int width, int height) override {
        if (!device_ || !hw_info.can_zero_copy) {
            return std::nullopt;
        }
        
        UploadResult r;
        r.handle.id = next_id_++;
        r.handle.width = width;
        r.handle.height = height;
        r.handle.is_hardware_frame = true;
        r.handle.native_handle = hw_info.d3d11_texture;
        r.reused = false;
        r.zero_copy = true;
        
        ve::log::info("Zero-copy hardware frame upload: " + std::to_string(width) + "x" + std::to_string(height));
        return r;
    }
    
    bool supports_hardware_frames() const override {
        return device_ != nullptr;
    }
    
    int get_preferred_hardware_format() const override {
        return DXGI_FORMAT_NV12; // Optimal for hardware decode
    }

private:
    void initialize_d3d11() {
        HRESULT hr = D3D11CreateDevice(
            nullptr,                    // Adapter
            D3D_DRIVER_TYPE_HARDWARE,   // Driver type
            nullptr,                    // Software
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT, // Flags for video support
            nullptr,                    // Feature levels
            0,                          // Number of feature levels
            D3D11_SDK_VERSION,          // SDK version
            &device_,                   // Device
            nullptr,                    // Feature level
            &context_                   // Context
        );
        
        if (SUCCEEDED(hr)) {
            ve::log::info("D3D11 hardware uploader initialized successfully");
        } else {
            ve::log::error("Failed to initialize D3D11 device for hardware upload");
        }
    }
    
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    static std::atomic<uint64_t> next_id_;
};

std::atomic<uint64_t> D3D11HardwareUploader::next_id_{1};

} // anonymous namespace
#endif

namespace { 

class StubUploader final : public IGpuUploader {
public:
    std::optional<UploadResult> upload_rgba(const VideoFrame& rgba_frame) override {
        UploadResult r;
        r.handle.id = next_id_++;
        r.handle.width = rgba_frame.width;
        r.handle.height = rgba_frame.height;
        r.handle.is_hardware_frame = false;
        r.reused = false;
        r.zero_copy = false;
        return r;
    }
    
    std::optional<UploadResult> upload_hardware_frame(const HardwareFrameInfo& hw_info, 
                                                     int width, int height) override {
        // Stub implementation - no real hardware acceleration
        return std::nullopt;
    }
    
    bool supports_hardware_frames() const override {
        return false;
    }
    
    int get_preferred_hardware_format() const override {
        return 0; // No preference
    }

private:
    static std::atomic<uint64_t> next_id_;
};

std::atomic<uint64_t> StubUploader::next_id_{1};

}

std::unique_ptr<IGpuUploader> create_hardware_uploader() {
#ifdef _WIN32
    return std::make_unique<D3D11HardwareUploader>();
#else
    return create_stub_uploader();
#endif
}

std::unique_ptr<IGpuUploader> create_stub_uploader() { 
    return std::make_unique<StubUploader>(); 
}

} // namespace ve::decode
