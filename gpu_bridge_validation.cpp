/**
 * GPU Bridge Validation Test
 * 
 * Phase 2 of GPU Debug Testing Guide
 * Tests our graphics device bridge interface for correctness and stability
 * 
 * This validates:
 * 1. Bridge initialization and cleanup
 * 2. GPU memory allocation/deallocation  
 * 3. Basic texture operations
 * 4. Resource handle management
 * 5. Effect processor functionality
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>

// Include our bridge headers 
#include "gfx/graphics_device_bridge.hpp"

// Basic test utilities
class Timer {
    std::chrono::high_resolution_clock::time_point start_time;
public:
    void start() { start_time = std::chrono::high_resolution_clock::now(); }
    double elapsed_ms() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_time);
        return duration.count() / 1000.0;
    }
};

class GPUBridgeValidator {
private:
    std::unique_ptr<ve::gfx::GraphicsDeviceBridge> bridge;
    bool test_passed = true;
    int tests_run = 0;
    int tests_failed = 0;

    void assert_test(bool condition, const std::string& test_name) {
        tests_run++;
        if (!condition) {
            std::cout << "❌ FAILED: " << test_name << std::endl;
            tests_failed++;
            test_passed = false;
        } else {
            std::cout << "✅ PASSED: " << test_name << std::endl;
        }
    }

public:
    bool run_all_tests() {
        std::cout << "=== GPU Bridge Validation - Phase 2 ===" << std::endl;
        std::cout << "Starting bridge interface validation..." << std::endl;

        // Test 1: Bridge Initialization
        test_bridge_initialization();
        
        // Test 2: Memory Management
        test_memory_management();
        
        // Test 3: Texture Operations  
        test_texture_operations();
        
        // Test 4: Resource Handles
        test_resource_handles();
        
        // Test 5: Effect Processors
        test_effect_processors();
        
        // Test 6: Error Handling
        test_error_handling();
        
        // Test 7: Performance Validation
        test_performance_characteristics();

        // Print summary
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Tests run: " << tests_run << std::endl;
        std::cout << "Tests failed: " << tests_failed << std::endl;
        std::cout << "Success rate: " << (100.0 * (tests_run - tests_failed) / tests_run) << "%" << std::endl;

        return test_passed;
    }

private:
    void test_bridge_initialization() {
        std::cout << "\n--- Test 1: Bridge Initialization ---" << std::endl;
        
        Timer timer;
        timer.start();
        
        try {
            // Create bridge instance
            bridge = ve::gfx::GraphicsDeviceBridge::create();
            double init_time = timer.elapsed_ms();
            
            assert_test(bridge != nullptr, "Bridge creation");
            assert_test(init_time < 1000.0, "Initialization time < 1s");
            
            // Test bridge status
            bool is_ready = bridge->is_device_ready();
            assert_test(is_ready, "Device ready status");
            
            std::cout << "   Bridge initialized in " << init_time << "ms" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Bridge initialization failed: " << e.what() << std::endl;
            test_passed = false;
        }
    }

    void test_memory_management() {
        std::cout << "\n--- Test 2: Memory Management ---" << std::endl;
        
        if (!bridge) {
            std::cout << "❌ Skipping - Bridge not initialized" << std::endl;
            return;
        }

        try {
            // Test memory allocation
            Timer timer;
            timer.start();
            
            auto texture = bridge->create_texture(1920, 1080, ve::gfx::TextureFormat::RGBA8);
            double alloc_time = timer.elapsed_ms();
            
            assert_test(texture.is_valid(), "Texture allocation");
            assert_test(alloc_time < 100.0, "Allocation time < 100ms");
            
            // Test memory usage tracking
            auto memory_info = bridge->get_memory_usage();
            assert_test(memory_info.allocated_bytes > 0, "Memory tracking");
            
            std::cout << "   Allocated " << (memory_info.allocated_bytes / 1024 / 1024) 
                     << "MB in " << alloc_time << "ms" << std::endl;
            
            // Cleanup
            bridge->release_texture(texture);
            
        } catch (const std::exception& e) {
            std::cout << "❌ Memory management test failed: " << e.what() << std::endl;
            test_passed = false;
        }
    }

    void test_texture_operations() {
        std::cout << "\n--- Test 3: Texture Operations ---" << std::endl;
        
        if (!bridge) {
            std::cout << "❌ Skipping - Bridge not initialized" << std::endl;
            return;
        }

        try {
            // Create test textures
            auto src_texture = bridge->create_texture(512, 512, ve::gfx::TextureFormat::RGBA8);
            auto dst_texture = bridge->create_texture(512, 512, ve::gfx::TextureFormat::RGBA8);
            
            assert_test(src_texture.is_valid(), "Source texture creation");
            assert_test(dst_texture.is_valid(), "Destination texture creation");
            
            // Test texture copy
            Timer timer;
            timer.start();
            
            bool copy_success = bridge->copy_texture(src_texture, dst_texture);
            double copy_time = timer.elapsed_ms();
            
            assert_test(copy_success, "Texture copy operation");
            assert_test(copy_time < 50.0, "Copy time < 50ms");
            
            std::cout << "   Texture copy completed in " << copy_time << "ms" << std::endl;
            
            // Cleanup
            bridge->release_texture(src_texture);
            bridge->release_texture(dst_texture);
            
        } catch (const std::exception& e) {
            std::cout << "❌ Texture operations test failed: " << e.what() << std::endl;
            test_passed = false;
        }
    }

    void test_resource_handles() {
        std::cout << "\n--- Test 4: Resource Handles ---" << std::endl;
        
        if (!bridge) {
            std::cout << "❌ Skipping - Bridge not initialized" << std::endl;
            return;
        }

        try {
            // Test handle uniqueness
            std::vector<ve::gfx::TextureHandle> handles;
            for (int i = 0; i < 10; ++i) {
                auto texture = bridge->create_texture(64, 64, ve::gfx::TextureFormat::RGBA8);
                handles.push_back(texture);
            }
            
            // Check all handles are unique
            bool unique = true;
            for (size_t i = 0; i < handles.size(); ++i) {
                for (size_t j = i + 1; j < handles.size(); ++j) {
                    if (handles[i].get_id() == handles[j].get_id()) {
                        unique = false;
                        break;
                    }
                }
            }
            
            assert_test(unique, "Handle uniqueness");
            assert_test(handles.size() == 10, "Multiple handle creation");
            
            // Test handle validation
            assert_test(handles[0].is_valid(), "Handle validity");
            
            // Cleanup
            for (auto& handle : handles) {
                bridge->release_texture(handle);
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ Resource handles test failed: " << e.what() << std::endl;
            test_passed = false;
        }
    }

    void test_effect_processors() {
        std::cout << "\n--- Test 5: Effect Processors ---" << std::endl;
        
        if (!bridge) {
            std::cout << "❌ Skipping - Bridge not initialized" << std::endl;
            return;
        }

        try {
            // Test YUV to RGB conversion
            auto yuv_processor = bridge->get_yuv_to_rgb_processor();
            assert_test(yuv_processor != nullptr, "YUV processor creation");
            
            // Test color correction  
            auto color_processor = bridge->get_color_correction_processor();
            assert_test(color_processor != nullptr, "Color correction processor creation");
            
            // Test scaling
            auto scale_processor = bridge->get_scaling_processor();
            assert_test(scale_processor != nullptr, "Scaling processor creation");
            
            std::cout << "   All effect processors available" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Effect processors test failed: " << e.what() << std::endl;
            test_passed = false;
        }
    }

    void test_error_handling() {
        std::cout << "\n--- Test 6: Error Handling ---" << std::endl;
        
        if (!bridge) {
            std::cout << "❌ Skipping - Bridge not initialized" << std::endl;
            return;
        }

        try {
            // Test invalid texture creation
            auto invalid_texture = bridge->create_texture(0, 0, ve::gfx::TextureFormat::RGBA8);
            assert_test(!invalid_texture.is_valid(), "Invalid texture rejection");
            
            // Test invalid handle operations
            ve::gfx::TextureHandle invalid_handle;
            bool release_result = bridge->release_texture(invalid_handle);
            assert_test(!release_result, "Invalid handle rejection");
            
            std::cout << "   Error handling working correctly" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Error handling test failed: " << e.what() << std::endl;
            test_passed = false;
        }
    }

    void test_performance_characteristics() {
        std::cout << "\n--- Test 7: Performance Validation ---" << std::endl;
        
        if (!bridge) {
            std::cout << "❌ Skipping - Bridge not initialized" << std::endl;
            return;
        }

        try {
            // Measure allocation performance
            Timer timer;
            timer.start();
            
            std::vector<ve::gfx::TextureHandle> textures;
            for (int i = 0; i < 100; ++i) {
                auto texture = bridge->create_texture(256, 256, ve::gfx::TextureFormat::RGBA8);
                textures.push_back(texture);
            }
            
            double alloc_time = timer.elapsed_ms();
            double avg_alloc = alloc_time / 100.0;
            
            assert_test(avg_alloc < 10.0, "Average allocation time < 10ms");
            
            // Measure deallocation performance
            timer.start();
            for (auto& texture : textures) {
                bridge->release_texture(texture);
            }
            double dealloc_time = timer.elapsed_ms();
            
            assert_test(dealloc_time < 1000.0, "Batch deallocation time < 1s");
            
            std::cout << "   Avg allocation: " << avg_alloc << "ms" << std::endl;
            std::cout << "   Batch deallocation: " << dealloc_time << "ms" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Performance validation failed: " << e.what() << std::endl;
            test_passed = false;
        }
    }
};

int main() {
    std::cout << "GPU Bridge Validation Tool - Phase 2" << std::endl;
    std::cout << "=====================================\n" << std::endl;
    
    GPUBridgeValidator validator;
    bool success = validator.run_all_tests();
    
    if (success) {
        std::cout << "\n🎉 All tests passed! Bridge is ready for production use." << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some tests failed. Review the results above." << std::endl;
        return 1;
    }
}
