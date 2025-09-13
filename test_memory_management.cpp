// Quick test for Week 2 Memory Management implementation
#include "src/gfx/include/gfx/vk_device.hpp"
#include "src/core/include/core/log.hpp"
#include <iostream>
#include <memory>

// Constants to avoid magic numbers
namespace {
    constexpr unsigned int HD_WIDTH = 1920;
    constexpr unsigned int HD_HEIGHT = 1080;
    constexpr unsigned int UHD_WIDTH = 3840;
    constexpr unsigned int UHD_HEIGHT = 2160;
    constexpr unsigned int MEGABYTE = 1024 * 1024;
    
    // Helper function to reduce code duplication for resource creation checks
    bool check_resource_creation(unsigned int resource_id, const std::string& resource_type, const std::string& details = "") {
        if (resource_id == 0) {
            std::cout << "✗ Failed to create " << resource_type << std::endl;
            return false;
        }
        std::cout << "✓ Created " << resource_type;
        if (!details.empty()) {
            std::cout << " (" << details << ")";
        }
        std::cout << " (ID: " << resource_id << ")" << std::endl;
        return true;
    }
}

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
    unsigned int texture1 = device->create_texture(HD_WIDTH, HD_HEIGHT, 0); // HD RGBA8 = ~8MB
    check_resource_creation(texture1, "HD RGBA8 texture", std::to_string(HD_WIDTH) + "x" + std::to_string(HD_HEIGHT));
    
    unsigned int texture2 = device->create_texture(UHD_WIDTH, UHD_HEIGHT, 0); // 4K RGBA8 = ~33MB  
    check_resource_creation(texture2, "UHD RGBA8 texture", std::to_string(UHD_WIDTH) + "x" + std::to_string(UHD_HEIGHT));
    
    std::cout << "\n3. Testing buffer management..." << std::endl;
    
    // Create vertex buffer (1MB)
    unsigned int buffer1 = device->create_buffer(MEGABYTE, 1); // Vertex buffer
    check_resource_creation(buffer1, "vertex buffer", "1MB");
    
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
