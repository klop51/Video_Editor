#pragma once
#include "render/gpu_frame_resource.hpp"
#include "../../gfx/include/gfx/vk_device.hpp"
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

    // HLSL Shader sources (Week 3: Shader System Implementation)
    static constexpr const char* VERTEX_SHADER = R"(
        struct VS_INPUT {
            float2 position : POSITION;
            float2 texcoord : TEXCOORD0;
        };

        struct PS_INPUT {
            float4 position : SV_POSITION;
            float2 texcoord : TEXCOORD0;
        };

        PS_INPUT main(VS_INPUT input) {
            PS_INPUT output;
            output.position = float4(input.position, 0.0, 1.0);
            output.texcoord = input.texcoord;
            return output;
        }
    )";

    static constexpr const char* FRAGMENT_SHADER = R"(
        struct PS_INPUT {
            float4 position : SV_POSITION;
            float2 texcoord : TEXCOORD0;
        };

        Texture2D textureSampler : register(t0);
        SamplerState samplerState : register(s0);

        cbuffer EffectConstants : register(b0) {
            float brightness;
            float3 padding;
        };

        float4 main(PS_INPUT input) : SV_TARGET {
            float4 texColor = textureSampler.Sample(samplerState, input.texcoord);
            // Apply brightness adjustment
            texColor.rgb += brightness;
            // Clamp to valid range
            texColor.rgb = saturate(texColor.rgb);
            return texColor;
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
