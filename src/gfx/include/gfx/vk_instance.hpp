#pragma once
#include <cstdint>
#include <memory>

// Forward declare Windows types
using HWND = struct HWND__*;

namespace ve::gfx {

// DirectX 11-based graphics context
struct D3D11ContextInfo {
    bool enable_debug = false;
    bool enable_vsync = true;
    HWND window_handle = nullptr; // Optional window for presentation
};

class D3D11Context {
public:
    D3D11Context() = default;
    ~D3D11Context();
    D3D11Context(const D3D11Context&) = delete;
    D3D11Context& operator=(const D3D11Context&) = delete;
    D3D11Context(D3D11Context&& other) noexcept { *this = std::move(other); }
    D3D11Context& operator=(D3D11Context&& other) noexcept;

    bool create(const D3D11ContextInfo& info) noexcept;
    void destroy() noexcept;
    bool is_valid() const noexcept { return created_; }
    
    // DirectX 11 device access - return void* to avoid including D3D11 headers
    void* get_device() const noexcept { return device_; }
    void* get_device_context() const noexcept { return device_context_; }
    void* get_swap_chain() const noexcept { return swap_chain_; }

private:
    bool created_ = false;
    void* device_ = nullptr;          // ID3D11Device*
    void* device_context_ = nullptr;  // ID3D11DeviceContext*
    void* swap_chain_ = nullptr;      // IDXGISwapChain*
    void* render_target_view_ = nullptr; // ID3D11RenderTargetView*
};

} // namespace ve::gfx
