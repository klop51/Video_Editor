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

// ---- your project headers
#include "gfx/opengl_headers.hpp"
#include "gfx/vk_device.hpp"
#include "gfx/vk_instance.hpp"

// Minimal DirectX 11 stub implementation outside any namespace to avoid MSVC conflicts

struct MinimalGraphicsDevice {
    bool created = false;
    unsigned int next_id = 1;

    bool create() {
        if (created) return true;
        // ve::log::info("DirectX 11 graphics device created (stub)");
        created = true;
        return true;
    }

    void destroy() {
        if (!created) return;
        // ve::log::info("DirectX 11 graphics device destroyed");
        created = false;
    }

    unsigned int create_texture(int width, int height, int format) {
        (void)width; (void)height; (void)format; // unused parameters
        if (!created) return 0;
        unsigned int id = next_id++;
        // ve::log::debug("Created texture {} ({}x{}) [stub]", id, width, height);
        return id;
    }
};

static MinimalGraphicsDevice g_device;

namespace ve::gfx {

// ---- ONLY your declarations/definitions here. No system/STD includes.

// PIMPL implementation
struct GraphicsDevice::Impl {
    bool created_ = false;

    bool create(const GraphicsDeviceInfo& info) {
        (void)info; // unused parameter
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
        (void)texture_id; // unused parameter
        // ve::log::debug("Destroyed texture {} [stub]", texture_id);
    }

    void upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) {
        (void)texture_id; (void)data; (void)width; (void)height; (void)format; // unused parameters
        // ve::log::debug("Uploaded texture {} [stub]", texture_id);
    }

    unsigned int create_shader_program(const char* vertex_src, const char* fragment_src) {
        (void)vertex_src; (void)fragment_src; // unused parameters
        if (!created_) return 0;
        unsigned int id = g_device.next_id++;
        // ve::log::debug("Created shader program {} [stub]", id);
        return id;
    }

    void destroy_shader_program(unsigned int program_id) {
        (void)program_id; // unused parameter
        // ve::log::debug("Destroyed shader program {} [stub]", program_id);
    }

    void use_shader_program(unsigned int program_id) {
        (void)program_id; // unused parameter
        // ve::log::debug("Using shader program {} [stub]", program_id);
    }

    void set_uniform1f(unsigned int program_id, const char* name, float v) {
        (void)program_id; (void)name; (void)v; // unused parameters
        // ve::log::debug("set_uniform1f({}, {}, {}) [stub]", program_id, name, v);
    }

    void set_uniform1i(unsigned int program_id, const char* name, int v) {
        (void)program_id; (void)name; (void)v; // unused parameters
        // ve::log::debug("set_uniform1i({}, {}, {}) [stub]", program_id, name, v);
    }

    void clear(float r, float g, float b, float a) {
        (void)r; (void)g; (void)b; (void)a; // unused parameters
        // ve::log::debug("clear({}, {}, {}, {}) [stub]", r, g, b, a);
    }

    void draw_texture(unsigned int texture_id, float x, float y, float width, float height) {
        (void)texture_id; (void)x; (void)y; (void)width; (void)height; // unused parameters
        // ve::log::debug("draw_texture({}, {}, {}, {}, {}) [stub]", texture_id, x, y, width, height);
    }

    void set_viewport(int width, int height) {
        (void)width; (void)height; // unused parameters
        // ve::log::debug("set_viewport({}, {}) [stub]", width, height);
    }
};

// GraphicsDevice implementation
GraphicsDevice::GraphicsDevice() : impl_(new Impl()) {}

GraphicsDevice::~GraphicsDevice() {
    if (impl_) {
        delete impl_;
        impl_ = nullptr;
    }
}

GraphicsDevice::GraphicsDevice(GraphicsDevice&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

GraphicsDevice& GraphicsDevice::operator=(GraphicsDevice&& other) noexcept {
    if (this != &other) {
        if (impl_) delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

bool GraphicsDevice::create(const GraphicsDeviceInfo& info) noexcept {
    return impl_ ? impl_->create(info) : false;
}

void GraphicsDevice::destroy() noexcept {
    if (impl_) impl_->destroy();
}

bool GraphicsDevice::is_valid() const noexcept {
    return impl_ ? impl_->is_valid() : false;
}

unsigned int GraphicsDevice::create_texture(int width, int height, int format) noexcept {
    return impl_ ? impl_->create_texture(width, height, format) : 0;
}

void GraphicsDevice::destroy_texture(unsigned int texture_id) noexcept {
    if (impl_) impl_->destroy_texture(texture_id);
}

void GraphicsDevice::upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) noexcept {
    if (impl_) impl_->upload_texture(texture_id, data, width, height, format);
}

unsigned int GraphicsDevice::create_shader_program(const char* vertex_src, const char* fragment_src) noexcept {
    return impl_ ? impl_->create_shader_program(vertex_src, fragment_src) : 0;
}

void GraphicsDevice::destroy_shader_program(unsigned int program_id) noexcept {
    if (impl_) impl_->destroy_shader_program(program_id);
}

void GraphicsDevice::use_shader_program(unsigned int program_id) noexcept {
    if (impl_) impl_->use_shader_program(program_id);
}

void GraphicsDevice::set_uniform1f(unsigned int program_id, const char* name, float v) noexcept {
    if (impl_) impl_->set_uniform1f(program_id, name, v);
}

void GraphicsDevice::set_uniform1i(unsigned int program_id, const char* name, int v) noexcept {
    if (impl_) impl_->set_uniform1i(program_id, name, v);
}

void GraphicsDevice::clear(float r, float g, float b, float a) noexcept {
    if (impl_) impl_->clear(r, g, b, a);
}

void GraphicsDevice::draw_texture(unsigned int texture_id, float x, float y, float width, float height) noexcept {
    if (impl_) impl_->draw_texture(texture_id, x, y, width, height);
}

void GraphicsDevice::set_viewport(int width, int height) noexcept {
    if (impl_) impl_->set_viewport(width, height);
}

} // namespace ve::gfx
