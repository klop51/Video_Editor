#pragma once
#include <cstdint>

namespace ve::gfx {

class D3D11Context; // fwd

struct GraphicsDeviceInfo {
    bool enable_swapchain = true;
    bool enable_debug = false;
};

class GraphicsDevice {
public:
    GraphicsDevice();
    ~GraphicsDevice();
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;
    GraphicsDevice(GraphicsDevice&& other) noexcept;
    GraphicsDevice& operator=(GraphicsDevice&& other) noexcept;

    bool create(const GraphicsDeviceInfo& info) noexcept;
    void destroy() noexcept;
    bool is_valid() const noexcept { return created_; }

    // Texture management
    uint32_t create_texture(int width, int height, int format) noexcept;
    void destroy_texture(uint32_t texture_id) noexcept;
    void upload_texture(uint32_t texture_id, const void* data, int width, int height, int format) noexcept;

    // Shader management
    uint32_t create_shader_program(const char* vertex_src, const char* fragment_src) noexcept;
    void destroy_shader_program(uint32_t program_id) noexcept;
    void use_shader_program(uint32_t program_id) noexcept;
    void set_uniform1f(uint32_t program_id, const char* name, float v) noexcept;
    void set_uniform1i(uint32_t program_id, const char* name, int v) noexcept;

    // Rendering
    void clear(float r, float g, float b, float a) noexcept;
    void draw_texture(uint32_t texture_id, float x, float y, float width, float height) noexcept;
    // Viewport / projection
    void set_viewport(int width, int height) noexcept;

private:
    struct Impl;
    Impl* impl_;
    bool created_ = false;
};
