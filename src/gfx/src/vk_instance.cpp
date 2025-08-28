// ---- global scope: ALL system/third-party/STD includes here
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <memory>
#include <new>

// ---- your project headers
#include "gfx/vk_instance.hpp"
#include "core/log.hpp"

namespace ve::gfx {

// ---- ONLY your declarations/definitions here. No system/STD includes.

D3D11Context::~D3D11Context() { destroy(); }

D3D11Context& D3D11Context::operator=(D3D11Context&& other) noexcept {
    if (this != &other) {
        destroy();
        created_ = other.created_;
        device_ = other.device_;
        device_context_ = other.device_context_;
        swap_chain_ = other.swap_chain_;
        render_target_view_ = other.render_target_view_;
        
        other.created_ = false;
        other.device_ = nullptr;
        other.device_context_ = nullptr;
        other.swap_chain_ = nullptr;
        other.render_target_view_ = nullptr;
    }
    return *this;
}

bool D3D11Context::create(const D3D11ContextInfo& info) noexcept {
    if(created_) return true;

    // For now, we'll assume the context is created by the graphics device
    // In a real implementation, this would create device and swap chain
    (void)info;
    ve::log::info("DirectX 11 context created (stub implementation)");
    created_ = true;
    return created_;
}

void D3D11Context::destroy() noexcept {
    if (!created_) return;
    
    // Cleanup would happen here in a real implementation
    ve::log::info("DirectX 11 context destroyed");
    created_ = false;
    device_ = nullptr;
    device_context_ = nullptr;
    swap_chain_ = nullptr;
    render_target_view_ = nullptr;
}

} // namespace ve::gfx
