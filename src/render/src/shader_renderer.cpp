#include "render/shader_renderer.hpp"
#include "core/log.hpp"

namespace ve::render {

ShaderRenderer::~ShaderRenderer() {
    if (device_ && shader_program_ != 0) {
        device_->destroy_shader_program(shader_program_);
        shader_program_ = 0;
    }
}

bool ShaderRenderer::initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device) {
    device_ = device;
    if (!device_) {
        ve::log::error("ShaderRenderer: No graphics device provided");
        return false;
    }

    // Create shader program
    shader_program_ = device_->create_shader_program(VERTEX_SHADER, FRAGMENT_SHADER);
    if (shader_program_ == 0) {
        ve::log::error("ShaderRenderer: Failed to create shader program");
        return false;
    }

    // Bind program and set static uniforms
    device_->use_shader_program(shader_program_);
    device_->set_uniform1i(shader_program_, "textureSampler", 0);

    initialized_ = true;
    ve::log::info("ShaderRenderer initialized successfully");
    return true;
}

bool ShaderRenderer::render_frame(std::shared_ptr<GpuFrameResource> frame_resource,
                                 int viewport_width, int viewport_height,
                                 float brightness) {
    if (!initialized_ || !frame_resource || !frame_resource->is_valid()) {
        return false;
    }

    // Use brightness parameter (suppress unused warning)
    device_->use_shader_program(shader_program_);
    device_->set_uniform1f(shader_program_, "brightness", brightness);

    // Set viewport
    set_viewport(viewport_width, viewport_height);

    // Clear screen
    device_->clear(0.0f, 0.0f, 0.0f, 1.0f);

    // Program already bound; drawing uses VAO/VBO in device

    // Calculate aspect ratio and positioning
    float frame_width = static_cast<float>(frame_resource->get_width());
    float frame_height = static_cast<float>(frame_resource->get_height());
    float frame_aspect = frame_width / frame_height;
    float viewport_aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    float draw_width, draw_height, draw_x, draw_y;

    if (frame_aspect > viewport_aspect) {
        // Frame is wider than viewport
        draw_width = static_cast<float>(viewport_width);
        draw_height = draw_width / frame_aspect;
        draw_x = 0.0f;
        draw_y = (static_cast<float>(viewport_height) - draw_height) / 2.0f;
    } else {
        // Frame is taller than viewport
        draw_height = static_cast<float>(viewport_height);
        draw_width = draw_height * frame_aspect;
        draw_x = (static_cast<float>(viewport_width) - draw_width) / 2.0f;
        draw_y = 0.0f;
    }

    // Render the texture
    device_->draw_texture(frame_resource->get_texture_id(),
                         draw_x, draw_y,
                         draw_width, draw_height);

    if (auto timer = frame_resource->take_timing()) {
        timer->endAndMaybeLog("TIMING_GPU");
    }

    return true;
}

void ShaderRenderer::set_viewport(int width, int height) {
    if (device_) {
        device_->set_viewport(width, height);
    } else {
        (void)width; (void)height; // keep interface stable if uninitialized
    }
}

} // namespace ve::render
