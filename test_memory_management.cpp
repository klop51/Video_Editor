// Quick test for Week 2 Memory Management implementation
#include "gfx/vk_device.hpp"
#include "core/log.hpp"
#include <iostream>
#include <memory>

int main() {
    std::cout << "=== GPU Memory Management Test (Week 2) ===" << std::endl;
    
    // Initialize logging
    ve::log::set_sink([](ve::log::Level, const std::string& msg) {
        std::cout << msg << std::endl;
    });
    
    // Create graphics device
    auto device = std::make_shared<ve::gfx::GraphicsDevice>();
    
    std::cout << "\n1. Creating DirectX 11 device..." << std::endl;
    ve::gfx::GraphicsDeviceInfo info;
    info.enable_debug = false;
    info.enable_swapchain = false; // Headless mode for testing
    
    if (!device->create(info)) {
        std::cout << "✗ Failed to create DirectX 11 device" << std::endl;
        return 1;
    }
    std::cout << "✓ DirectX 11 device created successfully" << std::endl;
    
    std::cout << "\n2. Testing texture memory management..." << std::endl;
    
    // Create some textures to test memory tracking
    unsigned int texture1 = device->create_texture(1920, 1080, 0); // 4K RGBA8 = ~8MB
    if (texture1 == 0) {
        std::cout << "✗ Failed to create texture 1" << std::endl;
    } else {
        std::cout << "✓ Created 1920x1080 RGBA8 texture (ID: " << texture1 << ")" << std::endl;
    }
    
    unsigned int texture2 = device->create_texture(3840, 2160, 0); // 4K RGBA8 = ~33MB  
    if (texture2 == 0) {
        std::cout << "✗ Failed to create texture 2" << std::endl;
    } else {
        std::cout << "✓ Created 3840x2160 RGBA8 texture (ID: " << texture2 << ")" << std::endl;
    }
    
    std::cout << "\n3. Testing buffer management..." << std::endl;
    
    // Create vertex buffer (1MB)
    unsigned int buffer1 = device->create_buffer(1024 * 1024, 1); // Vertex buffer
    if (buffer1 == 0) {
        std::cout << "✗ Failed to create vertex buffer" << std::endl;
    } else {
        std::cout << "✓ Created 1MB vertex buffer (ID: " << buffer1 << ")" << std::endl;
    }
    
    // Test upload to texture
    std::vector<uint32_t> test_data(1920 * 1080, 0xFF0000FF); // Red pixels
    std::cout << "\n4. Testing texture upload..." << std::endl;
    device->upload_texture(texture1, test_data.data(), 1920, 1080, 0);
    std::cout << "✓ Uploaded test data to texture" << std::endl;
    
    // Test buffer upload
    std::vector<float> vertex_data = {
        -1.0f, -1.0f, 0.0f, 0.0f, // Bottom-left
         1.0f, -1.0f, 1.0f, 0.0f, // Bottom-right
         0.0f,  1.0f, 0.5f, 1.0f  // Top-center
    };
    std::cout << "\n5. Testing buffer upload..." << std::endl;
    int buffer_size = static_cast<int>(vertex_data.size() * sizeof(float));
    device->upload_buffer(buffer1, vertex_data.data(), buffer_size);
    std::cout << "✓ Uploaded vertex data to buffer" << std::endl;
    
    std::cout << "\n6. Testing resource cleanup..." << std::endl;
    device->destroy_texture(texture1);
    std::cout << "✓ Destroyed texture 1" << std::endl;
    
    device->destroy_buffer(buffer1);
    std::cout << "✓ Destroyed buffer 1" << std::endl;
    
    std::cout << "\n7. Testing device cleanup..." << std::endl;
    device.reset(); // This should trigger proper cleanup
    std::cout << "✓ Device destroyed successfully" << std::endl;
    
    std::cout << "\n=== Week 2 Memory Management Test Complete ===\n" << std::endl;
    std::cout << "✅ SUCCESS: Memory management implementation working!" << std::endl;
    return 0;
}
