#pragma once

// NO standard library includes inside headers to avoid MSVC namespace conflicts!

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
    bool is_valid() const noexcept;

    // Texture management
    unsigned int create_texture(int width, int height, int format) noexcept;
    void destroy_texture(unsigned int texture_id) noexcept;
    void upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) noexcept;

    // Shader management
    unsigned int create_shader_program(const char* vertex_src, const char* fragment_src) noexcept;
    void destroy_shader_program(unsigned int program_id) noexcept;
    void use_shader_program(unsigned int program_id) noexcept;
    void set_uniform1f(unsigned int program_id, const char* name, float v) noexcept;
    void set_uniform1i(unsigned int program_id, const char* name, int v) noexcept;

    // Rendering
    void clear(float r, float g, float b, float a) noexcept;
    void draw_texture(unsigned int texture_id, float x, float y, float width, float height) noexcept;
    // Viewport / projection
    void set_viewport(int width, int height) noexcept;

    // Experimental CPU readback of last presented RGBA frame (stub path only).
    // Returns true if a frame is available; data points to internal buffer valid until next draw.
    bool get_last_present_rgba(const void** data, int* width, int* height, int* stride) noexcept;

private:
    struct Impl;
    Impl* impl_;
    bool created_ = false;
};

} // namespace ve::gfx
