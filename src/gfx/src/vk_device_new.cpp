// ---- global scope: ALL system/third-party/STD includes here
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdint>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>
#include <string>

// DirectX 11 includes
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

// Automatically link required DirectX libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ---- your project headers
#include "gfx/opengl_headers.hpp"
#include "gfx/vk_device.hpp"
#include "gfx/vk_instance.hpp"
#include "core/log.hpp"

// Real DirectX 11 implementation

struct D3D11Texture {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    
    ~D3D11Texture() {
        if (rtv) rtv->Release();
        if (srv) srv->Release();
        if (texture) texture->Release();
    }
};

struct D3D11ShaderProgram {
    ID3D11VertexShader* vertex_shader = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11InputLayout* input_layout = nullptr;
    ID3D11Buffer* constant_buffer = nullptr;
    
    ~D3D11ShaderProgram() {
        if (constant_buffer) constant_buffer->Release();
        if (input_layout) input_layout->Release();
        if (pixel_shader) pixel_shader->Release();
        if (vertex_shader) vertex_shader->Release();
    }
};

class D3D11GraphicsDevice {
private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain1* swapchain = nullptr;
    ID3D11RenderTargetView* backbuffer_rtv = nullptr;
    
    std::unordered_map<unsigned int, std::unique_ptr<D3D11Texture>> textures;
    std::unordered_map<unsigned int, std::unique_ptr<D3D11ShaderProgram>> shaders;
    unsigned int next_texture_id = 1;
    unsigned int next_shader_id = 1;
    
    DXGI_FORMAT get_dxgi_format(int format) {
        switch (format) {
            case 0: return DXGI_FORMAT_R8G8B8A8_UNORM;  // RGBA8
            case 1: return DXGI_FORMAT_R32G32B32A32_FLOAT; // RGBA32F
            default: return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

public:
    struct DeviceInfo {
        bool debug_enabled = false;
        bool enable_swapchain = false;
    };

    bool create(const DeviceInfo& info = {}) {
        // Create D3D11 device
        UINT create_device_flags = 0;
        if (info.debug_enabled) {
            create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
        }

        D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = D3D11CreateDevice(
            nullptr,                    // Adapter (nullptr = default)
            D3D_DRIVER_TYPE_HARDWARE,   // Driver Type
            nullptr,                    // Software
            create_device_flags,        // Flags
            feature_levels,             // Feature Levels
            ARRAYSIZE(feature_levels),  // Number of feature levels
            D3D11_SDK_VERSION,          // SDK version
            &device,                    // Returns the Direct3D device
            &feature_level,             // Returns feature level
            &context                    // Returns the device immediate context
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create D3D11 device: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return false;
        }
        
        ve::log::info("D3D11 device created successfully with feature level: 0x" + std::to_string(static_cast<unsigned int>(feature_level)));
        
        // Create swapchain if requested
        if (info.enable_swapchain) {
            // For now, create a minimal swapchain - will be enhanced when we need window presentation
            // This is a placeholder for headless rendering
            ve::log::info("Swapchain creation deferred until window handle available");
        }
        
        return true;
    }

    void destroy() {
        // Clean up all resources
        textures.clear();
        shaders.clear();
        
        if (backbuffer_rtv) {
            backbuffer_rtv->Release();
            backbuffer_rtv = nullptr;
        }
        
        if (swapchain) {
            swapchain->Release();
            swapchain = nullptr;
        }
        
        if (context) {
            context->Release();
            context = nullptr;
        }
        
        if (device) {
            device->Release();
            device = nullptr;
        }
        
        ve::log::info("D3D11 device destroyed");
    }

    unsigned int create_texture(int width, int height, int format) {
        if (!device) return 0;
        
        unsigned int id = next_texture_id++;
        auto texture = std::make_unique<D3D11Texture>();
        
        // Create texture description
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = get_dxgi_format(format);
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture->texture);
        if (FAILED(hr)) {
            ve::log::error("Failed to create D3D11 texture: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        // Create shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        
        hr = device->CreateShaderResourceView(texture->texture, &srv_desc, &texture->srv);
        if (FAILED(hr)) {
            ve::log::error("Failed to create shader resource view: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        // Create render target view
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
        rtv_desc.Format = desc.Format;
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        
        hr = device->CreateRenderTargetView(texture->texture, &rtv_desc, &texture->rtv);
        if (FAILED(hr)) {
            ve::log::error("Failed to create render target view: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        texture->width = width;
        texture->height = height;
        texture->format = desc.Format;
        
        ve::log::debug("Created D3D11 texture " + std::to_string(id) + " (" + std::to_string(width) + "x" + std::to_string(height) + ")");
        
        textures[id] = std::move(texture);
        return id;
    }

    void destroy_texture(unsigned int texture_id) {
        if (textures.erase(texture_id)) {
            ve::log::debug("Destroyed D3D11 texture " + std::to_string(texture_id));
        }
    }

    void upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) {
        auto it = textures.find(texture_id);
        if (it == textures.end()) {
            ve::log::error("Texture " + std::to_string(texture_id) + " not found for upload");
            return;
        }
        
        auto& texture = it->second;
        if (texture->width != static_cast<UINT>(width) || texture->height != static_cast<UINT>(height)) {
            ve::log::error("Texture size mismatch for upload: " + std::to_string(texture->width) + "x" + std::to_string(texture->height) + 
                          " vs " + std::to_string(width) + "x" + std::to_string(height));
            return;
        }
        
        // Calculate row pitch based on format
        UINT row_pitch = width * 4; // Assuming 4 bytes per pixel for now
        if (format == 1) row_pitch = width * 16; // RGBA32F = 16 bytes per pixel
        
        // Update texture data
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context->Map(texture->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            // Copy row by row if pitch differs
            const char* src_data = static_cast<const char*>(data);
            char* dst_data = static_cast<char*>(mapped.pData);
            
            for (int y = 0; y < height; ++y) {
                memcpy(dst_data + y * mapped.RowPitch, src_data + y * row_pitch, row_pitch);
            }
            
            context->Unmap(texture->texture, 0);
        }
        
        ve::log::debug("Uploaded data to D3D11 texture " + std::to_string(texture_id));
    }

    unsigned int create_shader_program(const char* vertex_src, const char* fragment_src) {
        if (!device) return 0;
        
        unsigned int id = next_shader_id++;
        auto shader = std::make_unique<D3D11ShaderProgram>();
        
        // Compile vertex shader
        ID3DBlob* vs_blob = nullptr;
        ID3DBlob* error_blob = nullptr;
        
        HRESULT hr = D3DCompile(
            vertex_src, strlen(vertex_src), nullptr, nullptr, nullptr,
            "main", "vs_5_0", 0, 0, &vs_blob, &error_blob
        );
        
        if (FAILED(hr)) {
            if (error_blob) {
                const char* error_msg = static_cast<const char*>(error_blob->GetBufferPointer());
                ve::log::error("Vertex shader compilation failed: " + std::string(error_msg));
                error_blob->Release();
            }
            if (vs_blob) vs_blob->Release();
            return 0;
        }
        
        hr = device->CreateVertexShader(
            vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(),
            nullptr,
            &shader->vertex_shader
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create vertex shader: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            vs_blob->Release();
            return 0;
        }
        
        // Compile pixel shader
        ID3DBlob* ps_blob = nullptr;
        hr = D3DCompile(
            fragment_src, strlen(fragment_src), nullptr, nullptr, nullptr,
            "main", "ps_5_0", 0, 0, &ps_blob, &error_blob
        );
        
        if (FAILED(hr)) {
            if (error_blob) {
                const char* error_msg = static_cast<const char*>(error_blob->GetBufferPointer());
                ve::log::error("Pixel shader compilation failed: " + std::string(error_msg));
                error_blob->Release();
            }
            if (ps_blob) ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        hr = device->CreatePixelShader(
            ps_blob->GetBufferPointer(),
            ps_blob->GetBufferSize(),
            nullptr,
            &shader->pixel_shader
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create pixel shader: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        // Create input layout (basic vertex format)
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        
        hr = device->CreateInputLayout(
            layout,
            ARRAYSIZE(layout),
            vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(),
            &shader->input_layout
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create input layout: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        // Create constant buffer
        D3D11_BUFFER_DESC cb_desc = {};
        cb_desc.ByteWidth = 256; // 256 bytes should be enough for most uniforms
        cb_desc.Usage = D3D11_USAGE_DYNAMIC;
        cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        hr = device->CreateBuffer(&cb_desc, nullptr, &shader->constant_buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create constant buffer: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        ve::log::debug("Created D3D11 shader program " + std::to_string(id));
        
        ps_blob->Release();
        vs_blob->Release();
        
        shaders[id] = std::move(shader);
        return id;
    }

    void destroy_shader_program(unsigned int program_id) {
        if (shaders.erase(program_id)) {
            ve::log::debug("Destroyed D3D11 shader program " + std::to_string(program_id));
        }
    }

    void use_shader_program(unsigned int program_id) {
        auto it = shaders.find(program_id);
        if (it == shaders.end()) return;
        
        auto& shader = it->second;
        context->VSSetShader(shader->vertex_shader, nullptr, 0);
        context->PSSetShader(shader->pixel_shader, nullptr, 0);
        context->IASetInputLayout(shader->input_layout);
        
        ve::log::debug("Using D3D11 shader program " + std::to_string(program_id));
    }

    void set_uniform1f(unsigned int program_id, const char* name, float v) {
        // For now, just log the call - uniform management requires more sophisticated mapping
        ve::log::debug("set_uniform1f(" + std::to_string(program_id) + ", " + std::string(name) + ", " + std::to_string(v) + ")");
    }

    void set_uniform1i(unsigned int program_id, const char* name, int v) {
        // For now, just log the call
        ve::log::debug("set_uniform1i(" + std::to_string(program_id) + ", " + std::string(name) + ", " + std::to_string(v) + ")");
    }

    void set_uniform4f(unsigned int program_id, const char* name, float x, float y, float z, float w) {
        // For now, just log the call
        (void)program_id; (void)name; (void)x; (void)y; (void)z; (void)w;
    }

    void clear(float r, float g, float b, float a) {
        if (!context || !backbuffer_rtv) return;
        
        float clear_color[4] = {r, g, b, a};
        context->ClearRenderTargetView(backbuffer_rtv, clear_color);
        
        ve::log::debug("clear(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ", " + std::to_string(a) + ")");
    }

    void draw_texture(unsigned int texture_id, float x, float y, float width, float height) {
        // Basic texture drawing - this would need proper vertex buffer setup in a real implementation
        ve::log::debug("draw_texture(" + std::to_string(texture_id) + ", " + std::to_string(x) + ", " + std::to_string(y) + ", " + 
                      std::to_string(width) + ", " + std::to_string(height) + ")");
        
        auto it = textures.find(texture_id);
        if (it == textures.end()) return;
        
        // Bind texture as shader resource
        auto& texture = it->second;
        ID3D11ShaderResourceView* srvs[] = { texture->srv };
        context->PSSetShaderResources(0, 1, srvs);
    }

    void set_viewport(int width, int height) {
        if (!context) return;
        
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        
        context->RSSetViewports(1, &viewport);
        
        ve::log::debug("set_viewport(" + std::to_string(width) + ", " + std::to_string(height) + ")");
    }

    bool get_last_present_rgba(const void** data, int* width, int* height, int* stride) {
        // For headless rendering, we would read back from a render target
        // This is a placeholder - implementation would require creating staging texture
        // and mapping it for CPU access
        (void)data; (void)width; (void)height; (void)stride;
        return false;
    }
};

// Global device instance
static D3D11GraphicsDevice g_device;

namespace ve::gfx {

// Implementation class for the PIMPL pattern
class GraphicsDevice::Impl {
public:
    bool create() {
        if (created_) return true;
        created_ = g_device.create();
        return created_;
    }

    void destroy() {
        if (!created_) return;
        g_device.destroy();
        created_ = false;
    }

    bool is_valid() const { return created_; }

    unsigned int create_texture(int width, int height, int format) {
        return g_device.create_texture(width, height, format);
    }

    void destroy_texture(unsigned int texture_id) {
        g_device.destroy_texture(texture_id);
    }

    void upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) {
        g_device.upload_texture(texture_id, data, width, height, format);
    }

    unsigned int create_shader_program(const char* vertex_src, const char* fragment_src) {
        return g_device.create_shader_program(vertex_src, fragment_src);
    }

    void destroy_shader_program(unsigned int program_id) {
        g_device.destroy_shader_program(program_id);
    }

    void use_shader_program(unsigned int program_id) {
        g_device.use_shader_program(program_id);
    }

    void set_uniform1f(unsigned int program_id, const char* name, float v) {
        g_device.set_uniform1f(program_id, name, v);
    }

    void set_uniform1i(unsigned int program_id, const char* name, int v) {
        g_device.set_uniform1i(program_id, name, v);
    }

    void set_uniform4f(unsigned int program_id, const char* name, float x, float y, float z, float w) {
        g_device.set_uniform4f(program_id, name, x, y, z, w);
    }

    void clear(float r, float g, float b, float a) {
        g_device.clear(r, g, b, a);
    }

    void draw_texture(unsigned int texture_id, float x, float y, float width, float height) {
        g_device.draw_texture(texture_id, x, y, width, height);
    }

    void set_viewport(int width, int height) {
        g_device.set_viewport(width, height);
    }

    bool get_last_present_rgba(const void** data, int* width, int* height, int* stride) {
        return g_device.get_last_present_rgba(data, width, height, stride);
    }

private:
    bool created_ = false;
};

// GraphicsDevice implementation using PIMPL
GraphicsDevice::GraphicsDevice() : impl_(std::make_unique<Impl>()) {}
GraphicsDevice::~GraphicsDevice() = default;

bool GraphicsDevice::create() noexcept {
    return impl_->create();
}

void GraphicsDevice::destroy() noexcept {
    impl_->destroy();
}

bool GraphicsDevice::is_valid() const noexcept {
    return impl_->is_valid();
}

unsigned int GraphicsDevice::create_texture(int width, int height, int format) noexcept {
    return impl_->create_texture(width, height, format);
}

void GraphicsDevice::destroy_texture(unsigned int texture_id) noexcept {
    impl_->destroy_texture(texture_id);
}

void GraphicsDevice::upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) noexcept {
    impl_->upload_texture(texture_id, data, width, height, format);
}

unsigned int GraphicsDevice::create_shader_program(const char* vertex_src, const char* fragment_src) noexcept {
    return impl_->create_shader_program(vertex_src, fragment_src);
}

void GraphicsDevice::destroy_shader_program(unsigned int program_id) noexcept {
    impl_->destroy_shader_program(program_id);
}

void GraphicsDevice::use_shader_program(unsigned int program_id) noexcept {
    impl_->use_shader_program(program_id);
}

void GraphicsDevice::set_uniform1f(unsigned int program_id, const char* name, float v) noexcept {
    impl_->set_uniform1f(program_id, name, v);
}

void GraphicsDevice::set_uniform1i(unsigned int program_id, const char* name, int v) noexcept {
    impl_->set_uniform1i(program_id, name, v);
}

void GraphicsDevice::set_uniform4f(unsigned int program_id, const char* name, float x, float y, float z, float w) noexcept {
    impl_->set_uniform4f(program_id, name, x, y, z, w);
}

void GraphicsDevice::clear(float r, float g, float b, float a) noexcept {
    impl_->clear(r, g, b, a);
}

void GraphicsDevice::draw_texture(unsigned int texture_id, float x, float y, float width, float height) noexcept {
    impl_->draw_texture(texture_id, x, y, width, height);
}

void GraphicsDevice::set_viewport(int width, int height) noexcept {
    impl_->set_viewport(width, height);
}

bool GraphicsDevice::get_last_present_rgba(const void** data, int* width, int* height, int* stride) noexcept {
    return impl_->get_last_present_rgba(data, width, height, stride);
}

} // namespace ve::gfx
