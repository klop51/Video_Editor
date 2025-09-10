#pragma once

#include <cstddef>  // For size_t

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

    // Buffer management (Week 2: Memory Management)
    unsigned int create_buffer(int size, int usage_flags, const void* initial_data = nullptr) noexcept;
    void destroy_buffer(unsigned int buffer_id) noexcept;
    void upload_buffer(unsigned int buffer_id, const void* data, int size, int offset = 0) noexcept;

    // Memory usage tracking
    void get_memory_usage(size_t* total, size_t* used, size_t* available) noexcept;

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

    // Week 6: Advanced Effects Shaders
    bool create_effect_pipeline() noexcept;
    bool apply_color_correction(unsigned int input_texture, unsigned int output_texture,
                               float brightness, float contrast, float saturation, float gamma) noexcept;
    bool apply_gaussian_blur(unsigned int input_texture, unsigned int intermediate_texture,
                            unsigned int output_texture, float radius) noexcept;
    bool apply_sharpen(unsigned int input_texture, unsigned int output_texture,
                      float strength, float edge_threshold = 0.1f) noexcept;
    bool apply_lut(unsigned int input_texture, unsigned int lut_texture,
                  unsigned int output_texture, float strength = 1.0f) noexcept;

private:
    struct Impl;
    Impl* impl_;
    bool created_ = false;
};

} // namespace ve::gfx
