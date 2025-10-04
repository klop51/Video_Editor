#include "gfx/opengl_headers.hpp"
#include "gfx/vk_device.hpp"
#include "gfx/vk_instance.hpp"
#include "core/log.hpp"

// Minimal DirectX 11 stub implementation outside any namespace to avoid MSVC conflicts

struct MinimalGraphicsDevice {
    bool created = false;
    uint32_t next_id = 1;

    bool create() {
        if (created) return true;
        ve::log::info("DirectX 11 graphics device created (stub)");
        created = true;
        return true;
    }

    void destroy() {
        if (!created) return;
        ve::log::info("DirectX 11 graphics device destroyed");
        created = false;
    }

    uint32_t create_texture(int width, int height, int format) {
        if (!created) return 0;
        uint32_t id = next_id++;
        ve::log::debug("Created texture {} ({}x{}) [stub]", id, width, height);
        return id;
    }
};

static MinimalGraphicsDevice g_device;

namespace ve::gfx {

// PIMPL implementation
struct GraphicsDevice::Impl {
    bool created_ = false;

    bool create(const GraphicsDeviceInfo& info) {
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

    uint32_t create_texture(int width, int height, int format) {
        return g_device.create_texture(width, height, format);
    }

    void destroy_texture(uint32_t texture_id) {
        ve::log::debug("Destroyed texture {} [stub]", texture_id);
    }

    void upload_texture(uint32_t texture_id, const void* data, int width, int height, int format) {
        ve::log::debug("Uploaded texture {} [stub]", texture_id);
    }

    uint32_t create_shader_program(const char* vertex_src, const char* fragment_src) {
        if (!created_) return 0;
        uint32_t id = g_device.next_id++;
        ve::log::debug("Created shader program {} [stub]", id);
        return id;
    }

    void destroy_shader_program(uint32_t program_id) {
        ve::log::debug("Destroyed shader program {} [stub]", program_id);
    }

    void use_shader_program(uint32_t program_id) {
        ve::log::debug("Using shader program {} [stub]", program_id);
    }

    void set_uniform1f(uint32_t program_id, const char* name, float v) {
        ve::log::debug("set_uniform1f({}, {}, {}) [stub]", program_id, name, v);
    }

    void set_uniform1i(uint32_t program_id, const char* name, int v) {
        ve::log::debug("set_uniform1i({}, {}, {}) [stub]", program_id, name, v);
    }

    void clear(float r, float g, float b, float a) {
        ve::log::debug("clear({}, {}, {}, {}) [stub]", r, g, b, a);
    }

    void draw_texture(uint32_t texture_id, float x, float y, float width, float height) {
        ve::log::debug("draw_texture({}, {}, {}, {}, {}) [stub]", texture_id, x, y, width, height);
    }

    void set_viewport(int width, int height) {
        ve::log::debug("set_viewport({}, {}) [stub]", width, height);
    }
};

// GraphicsDevice implementation
GraphicsDevice::GraphicsDevice() : pImpl(new Impl()) {}

GraphicsDevice::~GraphicsDevice() {
    if (pImpl) {
        delete pImpl;
        pImpl = nullptr;
    }
}

GraphicsDevice::GraphicsDevice(GraphicsDevice&& other) noexcept : pImpl(other.pImpl) {
    other.pImpl = nullptr;
}

GraphicsDevice& GraphicsDevice::operator=(GraphicsDevice&& other) noexcept {
    if (this != &other) {
        if (pImpl) delete pImpl;
        pImpl = other.pImpl;
        other.pImpl = nullptr;
    }
    return *this;
}

bool GraphicsDevice::create(const GraphicsDeviceInfo& info) {
    return pImpl ? pImpl->create(info) : false;
}

void GraphicsDevice::destroy() {
    if (pImpl) pImpl->destroy();
}

bool GraphicsDevice::is_valid() const {
    return pImpl ? pImpl->is_valid() : false;
}

uint32_t GraphicsDevice::create_texture(int width, int height, int format) {
    return pImpl ? pImpl->create_texture(width, height, format) : 0;
}

unsigned int GraphicsDevice::create_dynamic_texture(int width, int height, int format) noexcept {
    return create_texture(width, height, format);
}

void GraphicsDevice::destroy_texture(uint32_t texture_id) {
    if (pImpl) pImpl->destroy_texture(texture_id);
}

void GraphicsDevice::upload_texture(uint32_t texture_id, const void* data, int width, int height, int format) {
    if (pImpl) pImpl->upload_texture(texture_id, data, width, height, format);
}

bool GraphicsDevice::map_texture_discard(unsigned int texture_id, MappedTexture& mapped, unsigned int* error_code) noexcept {
    (void)texture_id;
    mapped.data = nullptr;
    mapped.row_pitch = 0;
    if (error_code) {
        *error_code = 0u;
    }
    return false;
}

void GraphicsDevice::unmap_texture(unsigned int texture_id) noexcept {
    (void)texture_id;
}

void GraphicsDevice::flush() noexcept {}

uint32_t GraphicsDevice::create_shader_program(const char* vertex_src, const char* fragment_src) {
    return pImpl ? pImpl->create_shader_program(vertex_src, fragment_src) : 0;
}

void GraphicsDevice::destroy_shader_program(uint32_t program_id) {
    if (pImpl) pImpl->destroy_shader_program(program_id);
}

void GraphicsDevice::use_shader_program(uint32_t program_id) {
    if (pImpl) pImpl->use_shader_program(program_id);
}

void GraphicsDevice::set_uniform1f(uint32_t program_id, const char* name, float v) {
    if (pImpl) pImpl->set_uniform1f(program_id, name, v);
}

void GraphicsDevice::set_uniform1i(uint32_t program_id, const char* name, int v) {
    if (pImpl) pImpl->set_uniform1i(program_id, name, v);
}

void GraphicsDevice::clear(float r, float g, float b, float a) {
    if (pImpl) pImpl->clear(r, g, b, a);
}

void GraphicsDevice::draw_texture(uint32_t texture_id, float x, float y, float width, float height) {
    if (pImpl) pImpl->draw_texture(texture_id, x, y, width, height);
}

void GraphicsDevice::set_viewport(int width, int height) {
    if (pImpl) pImpl->set_viewport(width, height);
}

} // namespace ve::gfx
