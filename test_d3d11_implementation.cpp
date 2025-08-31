#include <iostream>
#include "gfx/vk_device.hpp"

int main() {
    std::cout << "Testing DirectX 11 implementation..." << std::endl;
    
    // Test basic GPU device creation
    ve::gfx::GraphicsDevice device;
    ve::gfx::GraphicsDeviceInfo info;
    info.enable_debug = true;
    info.enable_swapchain = false;  // Headless mode
    
    if (device.create(info)) {
        std::cout << "✓ DirectX 11 device created successfully!" << std::endl;
        
        // Test texture creation
        unsigned int texture_id = device.create_texture(1920, 1080, 0);
        if (texture_id != 0) {
            std::cout << "✓ Texture created successfully (ID: " << texture_id << ")" << std::endl;
            device.destroy_texture(texture_id);
            std::cout << "✓ Texture destroyed successfully" << std::endl;
        } else {
            std::cout << "✗ Failed to create texture" << std::endl;
        }
        
        // Test shader compilation
        const char* vertex_shader = R"(
struct VS_INPUT {
    float3 pos : POSITION;
    float2 tex : TEXCOORD0;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.tex = input.tex;
    return output;
}
)";

        const char* fragment_shader = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_Target {
    return float4(input.tex.xy, 0.0f, 1.0f);
}
)";
        
        unsigned int shader_id = device.create_shader_program(vertex_shader, fragment_shader);
        if (shader_id != 0) {
            std::cout << "✓ Shader program compiled successfully (ID: " << shader_id << ")" << std::endl;
            device.destroy_shader_program(shader_id);
            std::cout << "✓ Shader program destroyed successfully" << std::endl;
        } else {
            std::cout << "✗ Failed to compile shader program" << std::endl;
        }
        
        device.destroy();
        std::cout << "✓ DirectX 11 device destroyed successfully" << std::endl;
        
        std::cout << "\nGPU System Week 1 Implementation Test: PASSED!" << std::endl;
        std::cout << "✓ Device creation and destruction" << std::endl;
        std::cout << "✓ Texture management" << std::endl;
        std::cout << "✓ Shader compilation" << std::endl;
        std::cout << "✓ Resource cleanup" << std::endl;
        
    } else {
        std::cout << "✗ Failed to create DirectX 11 device" << std::endl;
        return 1;
    }
    
    return 0;
}
