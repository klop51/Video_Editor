// Basic test to validate graphics device bridge approach
#include "src/gfx/graphics_device_bridge.hpp"
#include <iostream>

int main() {
    std::cout << "Testing Graphics Device Bridge..." << std::endl;
    
    // Test basic bridge functionality
    video_editor::gfx::GraphicsDevice::Config config;
    config.enable_debug = true;
    
    auto device = video_editor::gfx::GraphicsDevice::create(config);
    if (!device) {
        std::cout << "❌ Failed to create graphics device" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Graphics device created successfully" << std::endl;
    
    if (!device->is_valid()) {
        std::cout << "❌ Graphics device is not valid" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Graphics device is valid" << std::endl;
    
    // Test texture creation
    video_editor::gfx::TextureDesc tex_desc;
    tex_desc.width = 1920;
    tex_desc.height = 1080;
    tex_desc.format = video_editor::gfx::TextureFormat::RGBA8;
    
    auto texture = device->create_texture(tex_desc);
    if (!texture.is_valid()) {
        std::cout << "❌ Failed to create texture" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Texture created successfully (ID: " << texture.get_id() << ")" << std::endl;
    
    // Test buffer creation
    video_editor::gfx::BufferDesc buf_desc;
    buf_desc.size = 1024;
    buf_desc.usage = video_editor::gfx::BufferUsage::Vertex;
    
    auto buffer = device->create_buffer(buf_desc);
    if (!buffer.is_valid()) {
        std::cout << "❌ Failed to create buffer" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Buffer created successfully (ID: " << buffer.get_id() << ")" << std::endl;
    
    // Test memory info
    size_t total = device->get_total_memory();
    size_t available = device->get_available_memory();
    size_t used = device->get_used_memory();
    
    std::cout << "Memory Info:" << std::endl;
    std::cout << "  Total: " << total / (1024 * 1024) << " MB" << std::endl;
    std::cout << "  Available: " << available / (1024 * 1024) << " MB" << std::endl;
    std::cout << "  Used: " << used / (1024 * 1024) << " MB" << std::endl;
    
    std::cout << "✅ All bridge tests passed!" << std::endl;
    return 0;
}
