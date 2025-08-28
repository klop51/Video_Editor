#pragma once
#include "render/gpu_frame_resource.hpp"
#include "gfx/vk_device.hpp"
#include <memory>
#include <string>

namespace ve::render {

// Shader-based renderer for video processing
class ShaderRenderer {
public:
    ShaderRenderer() = default;
    ~ShaderRenderer();

    // Initialize with graphics device
    bool initialize(std::shared_ptr<ve::gfx::GraphicsDevice> device);

    // Render a frame with optional effects
    bool render_frame(std::shared_ptr<GpuFrameResource> frame_resource,
                     int viewport_width, int viewport_height,
                     float brightness = 0.0f);

    // Set rendering viewport
    void set_viewport(int width, int height);

private:
    std::shared_ptr<ve::gfx::GraphicsDevice> device_;
    uint32_t shader_program_ = 0;
    bool initialized_ = false;

    // Shader sources
    static constexpr const char* VERTEX_SHADER = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;

        out vec2 TexCoord;

        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    static constexpr const char* FRAGMENT_SHADER = R"(
        #version 330 core
        out vec4 FragColor;

        in vec2 TexCoord;

        uniform sampler2D textureSampler;
        uniform float brightness;

        void main() {
            vec4 texColor = texture(textureSampler, TexCoord);
            // Apply brightness adjustment
            texColor.rgb += brightness;
            // Clamp to valid range
            texColor.rgb = clamp(texColor.rgb, 0.0, 1.0);
            FragColor = texColor;
        }
    )";
};

// Effect parameters
struct EffectParams {
    float brightness = 0.0f;  // -1.0 to 1.0
    float contrast = 1.0f;    // 0.0 to 2.0
    float saturation = 1.0f;  // 0.0 to 2.0
};

} // namespace ve::render
