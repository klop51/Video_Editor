// 8K Video VRAM Management Demo
// Demonstrates GPU Memory Optimizer handling large video files with limited VRAM

#include "gpu_memory_optimizer.hpp"
#include "graphics_device.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>

namespace video_editor::gfx::demo {

// 8K Video specifications
struct VideoSpecs {
    uint32_t width = 7680;        // 8K width
    uint32_t height = 4320;       // 8K height
    uint32_t fps = 30;            // 30 fps
    uint32_t duration_seconds = 120; // 2 minutes
    TextureFormat format = TextureFormat::RGBA8;
    
    uint32_t total_frames() const { return duration_seconds * fps; }
    size_t frame_size_bytes() const { return width * height * 4; } // RGBA8 = 4 bytes per pixel
    size_t total_video_size() const { return frame_size_bytes() * total_frames(); }
};

class EightKVideoDemo {
private:
    std::unique_ptr<GraphicsDevice> device_;
    std::unique_ptr<GPUMemoryOptimizer> optimizer_;
    VideoSpecs video_specs_;
    std::mt19937 random_generator_;
    
    // Demo statistics
    struct DemoStats {
        uint32_t frames_processed = 0;
        uint32_t cache_hits = 0;
        uint32_t cache_misses = 0;
        size_t peak_vram_usage = 0;
        size_t current_vram_usage = 0;
        double average_frame_time_ms = 0.0;
        bool had_vram_exhaustion = false;
        uint32_t cleanup_operations = 0;
    } stats_;

public:
    EightKVideoDemo() : random_generator_(std::random_device{}()) {
        initialize_graphics_device();
        setup_memory_optimizer();
        print_demo_info();
    }
    
    void run_playback_demo() {
        std::cout << "\n=== 8K Video Playback Demo ===" << std::endl;
        
        // Test 1: Sequential playback (normal viewing)
        std::cout << "Testing sequential playback..." << std::endl;
        test_sequential_playback();
        
        // Test 2: Random seeking (scrubbing)
        std::cout << "Testing random seeking/scrubbing..." << std::endl;
        test_random_seeking();
        
        // Test 3: High-speed scrubbing
        std::cout << "Testing high-speed scrubbing..." << std::endl;
        test_high_speed_scrubbing();
        
        // Test 4: Memory pressure scenarios
        std::cout << "Testing memory pressure scenarios..." << std::endl;
        test_memory_pressure();
        
        print_final_statistics();
    }

private:
    void initialize_graphics_device() {
        // Create graphics device with limited VRAM simulation
        GraphicsDevice::Config device_config;
        device_config.enable_debug = true;
        device_config.preferred_api = GraphicsAPI::D3D11;
        
        device_ = GraphicsDevice::create(device_config);
        
        // Simulate 4GB GPU with limited available VRAM
        // In real scenario, this would be actual hardware limitations
    }
    
    void setup_memory_optimizer() {
        GPUMemoryOptimizer::OptimizerConfig config;
        
        // Configure for 8K video processing with limited VRAM
        config.cache_config.max_cache_size = 2ULL * 1024 * 1024 * 1024; // 2GB cache (conservative)
        config.cache_config.min_free_vram = 512ULL * 1024 * 1024;       // Keep 512MB free
        config.cache_config.eviction_threshold = 0.8f;                   // Start cleanup at 80%
        config.cache_config.enable_compression = true;                   // Essential for 8K
        config.cache_config.enable_prediction = true;                    // Predictive loading
        config.cache_config.prediction_lookahead = 90;                   // 3 seconds at 30fps
        
        // Streaming optimization for large files
        config.streaming_config.streaming_buffer_size = 1ULL * 1024 * 1024 * 1024; // 1GB streaming buffer
        config.streaming_config.read_ahead_frames = 60;                  // 2 seconds ahead
        config.streaming_config.max_concurrent_loads = 6;                // Parallel loading
        config.streaming_config.enable_adaptive_quality = true;          // Quality scaling
        config.streaming_config.enable_predictive_loading = true;        // AI prediction
        
        // Memory monitoring
        config.memory_thresholds.warning_threshold = 0.75f;              // 75% usage warning
        config.memory_thresholds.critical_threshold = 0.90f;             // 90% critical
        config.memory_thresholds.cleanup_threshold = 0.85f;              // 85% cleanup trigger
        
        config.enable_background_optimization = true;
        config.optimization_interval_ms = 500; // Optimize every 500ms
        
        optimizer_ = std::make_unique<GPUMemoryOptimizer>(device_.get(), config);
        
        // Setup memory pressure callback
        optimizer_->set_memory_pressure_callback([this](float pressure) {
            handle_memory_pressure(pressure);
        });
    }
    
    void print_demo_info() const {
        std::cout << "8K Video VRAM Management Demo" << std::endl;
        std::cout << "=============================" << std::endl;
        std::cout << "Video Resolution: " << video_specs_.width << "x" << video_specs_.height << std::endl;
        std::cout << "Frame Rate: " << video_specs_.fps << " fps" << std::endl;
        std::cout << "Duration: " << video_specs_.duration_seconds << " seconds" << std::endl;
        std::cout << "Total Frames: " << video_specs_.total_frames() << std::endl;
        std::cout << "Frame Size: " << (video_specs_.frame_size_bytes() / (1024*1024)) << " MB" << std::endl;
        std::cout << "Total Video Size: " << (video_specs_.total_video_size() / (1024*1024*1024)) << " GB" << std::endl;
        std::cout << "Available VRAM: Limited to ~4GB (typical GPU)" << std::endl;
        std::cout << std::endl;
    }
    
    void test_sequential_playback() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Simulate normal playback from start to end
        for (uint32_t frame = 0; frame < video_specs_.total_frames(); frame += 5) { // Sample every 5th frame for speed
            auto frame_start = std::chrono::high_resolution_clock::now();
            
            process_frame(frame);
            
            auto frame_end = std::chrono::high_resolution_clock::now();
            auto frame_time = std::chrono::duration_cast<std::chrono::microseconds>(
                frame_end - frame_start).count() / 1000.0; // Convert to milliseconds
            
            update_frame_time_average(frame_time);
            
            // Simulate real-time playback timing
            if (frame % 30 == 0) { // Every second
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                print_progress("Sequential Playback", frame, video_specs_.total_frames());
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        std::cout << "Sequential playback completed in " << total_time << "ms" << std::endl;
        print_current_stats();
    }
    
    void test_random_seeking() {
        std::uniform_int_distribution<uint32_t> frame_dist(0, video_specs_.total_frames() - 1);
        
        // Simulate 1000 random seeks
        for (int i = 0; i < 1000; ++i) {
            uint32_t random_frame = frame_dist(random_generator_);
            
            auto seek_start = std::chrono::high_resolution_clock::now();
            process_frame(random_frame);
            auto seek_end = std::chrono::high_resolution_clock::now();
            
            auto seek_time = std::chrono::duration_cast<std::chrono::microseconds>(
                seek_end - seek_start).count() / 1000.0;
            update_frame_time_average(seek_time);
            
            if (i % 100 == 0) {
                print_progress("Random Seeking", i, 1000);
            }
            
            // Brief pause between seeks
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        std::cout << "Random seeking test completed" << std::endl;
        print_current_stats();
    }
    
    void test_high_speed_scrubbing() {
        // Simulate high-speed scrubbing (10x normal speed)
        uint32_t start_frame = video_specs_.total_frames() / 4;
        uint32_t end_frame = (video_specs_.total_frames() * 3) / 4;
        
        // Forward scrubbing
        for (uint32_t frame = start_frame; frame < end_frame; frame += 10) {
            process_frame(frame);
            
            if (frame % 300 == 0) {
                print_progress("High-Speed Forward", frame - start_frame, end_frame - start_frame);
            }
        }
        
        // Backward scrubbing
        for (uint32_t frame = end_frame; frame > start_frame; frame -= 15) {
            process_frame(frame);
            
            if (frame % 300 == 0) {
                print_progress("High-Speed Backward", end_frame - frame, end_frame - start_frame);
            }
        }
        
        std::cout << "High-speed scrubbing test completed" << std::endl;
        print_current_stats();
    }
    
    void test_memory_pressure() {
        // Intentionally stress memory by requesting many large frames simultaneously
        std::vector<uint64_t> stress_hashes;
        
        // Load a large number of frames to stress memory
        for (uint32_t i = 0; i < 200; ++i) {
            uint32_t frame = i * 10; // Every 10th frame
            uint64_t hash = generate_frame_hash(frame);
            stress_hashes.push_back(hash);
            
            // Create oversized texture to stress memory
            auto texture = create_8k_texture(frame, true); // Force high quality
            optimizer_->cache_texture(hash, texture, 1.0f);
            
            if (i % 50 == 0) {
                print_progress("Memory Stress Test", i, 200);
                
                // Check memory status
                auto vram_status = optimizer_->get_vram_status();
                std::cout << " [VRAM: " << (vram_status.get_usage_ratio() * 100) << "%]";
            }
        }
        
        std::cout << "\nMemory stress test completed" << std::endl;
        print_current_stats();
        
        // Force cleanup
        optimizer_->force_memory_cleanup();
        std::cout << "Forced cleanup completed" << std::endl;
        print_current_stats();
    }
    
    void process_frame(uint32_t frame_number) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        // Notify frame change for pattern analysis
        optimizer_->notify_frame_change(frame_number);
        
        uint64_t hash = generate_frame_hash(frame_number);
        
        // Try to get frame from cache
        auto texture = optimizer_->get_texture(hash);
        
        if (texture.is_valid()) {
            // Cache hit
            stats_.cache_hits++;
        } else {
            // Cache miss - load and cache frame
            stats_.cache_misses++;
            
            texture = create_8k_texture(frame_number);
            if (texture.is_valid()) {
                float quality = calculate_frame_quality(frame_number);
                optimizer_->cache_texture(hash, texture, quality);
            }
        }
        
        stats_.frames_processed++;
        
        // Update memory usage tracking
        auto memory_stats = optimizer_->get_memory_statistics();
        stats_.current_vram_usage = memory_stats.used_vram;
        if (stats_.current_vram_usage > stats_.peak_vram_usage) {
            stats_.peak_vram_usage = stats_.current_vram_usage;
        }
        
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto frame_time = std::chrono::duration_cast<std::chrono::microseconds>(
            frame_end - frame_start).count() / 1000.0;
        
        update_frame_time_average(frame_time);
    }
    
    TextureHandle create_8k_texture(uint32_t frame_number, bool high_quality = false) {
        // In real implementation, would load actual 8K frame from disk
        // For demo, create a mock texture with appropriate size
        
        size_t texture_size = video_specs_.frame_size_bytes();
        if (high_quality) {
            texture_size *= 2; // Simulate uncompressed/higher bit depth
        }
        
        // Create mock texture (in real implementation, would be actual GPU texture)
        TextureDesc desc{};
        desc.width = video_specs_.width;
        desc.height = video_specs_.height;
        desc.format = video_specs_.format;
        desc.usage = TextureUsage::ShaderResource;
        
        return device_->create_texture(desc);
    }
    
    uint64_t generate_frame_hash(uint32_t frame_number) const {
        std::hash<std::string> hasher;
        return hasher("8k_frame_" + std::to_string(frame_number));
    }
    
    float calculate_frame_quality(uint32_t frame_number) const {
        // Vary quality based on frame characteristics
        // Key frames get higher quality, intermediate frames can be lower quality
        if (frame_number % 30 == 0) return 1.0f;      // I-frames (high quality)
        if (frame_number % 10 == 0) return 0.8f;      // P-frames (medium quality)
        return 0.6f;                                   // B-frames (lower quality)
    }
    
    void handle_memory_pressure(float pressure) {
        if (pressure > 0.9f) {
            stats_.had_vram_exhaustion = true;
            stats_.cleanup_operations++;
            std::cout << "\n[WARNING] Critical VRAM pressure: " << (pressure * 100) << "%" << std::endl;
        } else if (pressure > 0.8f) {
            stats_.cleanup_operations++;
            std::cout << "\n[INFO] High VRAM pressure: " << (pressure * 100) << "%" << std::endl;
        }
    }
    
    void update_frame_time_average(double frame_time_ms) {
        stats_.average_frame_time_ms = (stats_.average_frame_time_ms * 0.95) + (frame_time_ms * 0.05);
    }
    
    void print_progress(const std::string& operation, uint32_t current, uint32_t total) {
        float progress = float(current) / float(total) * 100.0f;
        std::cout << "\r" << operation << ": " << std::fixed << std::setprecision(1) 
                  << progress << "% (" << current << "/" << total << ")" << std::flush;
    }
    
    void print_current_stats() {
        auto memory_stats = optimizer_->get_memory_statistics();
        auto streaming_stats = optimizer_->get_streaming_statistics();
        auto vram_status = optimizer_->get_vram_status();
        
        std::cout << "\nCurrent Statistics:" << std::endl;
        std::cout << "  Frames Processed: " << stats_.frames_processed << std::endl;
        std::cout << "  Cache Hit Ratio: " << std::fixed << std::setprecision(2) 
                  << (float(stats_.cache_hits) / float(stats_.cache_hits + stats_.cache_misses) * 100) << "%" << std::endl;
        std::cout << "  Average Frame Time: " << std::fixed << std::setprecision(2) 
                  << stats_.average_frame_time_ms << "ms" << std::endl;
        std::cout << "  Current VRAM Usage: " << (stats_.current_vram_usage / (1024*1024)) << "MB" << std::endl;
        std::cout << "  Peak VRAM Usage: " << (stats_.peak_vram_usage / (1024*1024)) << "MB" << std::endl;
        std::cout << "  VRAM Utilization: " << std::fixed << std::setprecision(1) 
                  << (vram_status.get_usage_ratio() * 100) << "%" << std::endl;
        std::cout << "  Cleanup Operations: " << stats_.cleanup_operations << std::endl;
        std::cout << std::endl;
    }
    
    void print_final_statistics() {
        std::cout << "\n=== Final Demo Statistics ===" << std::endl;
        
        auto memory_stats = optimizer_->get_memory_statistics();
        auto streaming_stats = optimizer_->get_streaming_statistics();
        
        std::cout << "Performance Metrics:" << std::endl;
        std::cout << "  Total Frames Processed: " << stats_.frames_processed << std::endl;
        std::cout << "  Cache Hits: " << stats_.cache_hits << std::endl;
        std::cout << "  Cache Misses: " << stats_.cache_misses << std::endl;
        std::cout << "  Overall Hit Ratio: " << std::fixed << std::setprecision(2) 
                  << (float(stats_.cache_hits) / float(stats_.cache_hits + stats_.cache_misses) * 100) << "%" << std::endl;
        std::cout << "  Average Frame Processing: " << std::fixed << std::setprecision(2) 
                  << stats_.average_frame_time_ms << "ms" << std::endl;
        
        std::cout << "\nMemory Management:" << std::endl;
        std::cout << "  Peak VRAM Usage: " << (stats_.peak_vram_usage / (1024*1024)) << "MB" << std::endl;
        std::cout << "  Had VRAM Exhaustion: " << (stats_.had_vram_exhaustion ? "Yes" : "No") << std::endl;
        std::cout << "  Cleanup Operations: " << stats_.cleanup_operations << std::endl;
        std::cout << "  Memory Efficiency: " << memory_stats.hit_ratio << std::endl;
        
        std::cout << "\nStreaming Performance:" << std::endl;
        std::cout << "  Frames Streamed: " << streaming_stats.frames_streamed << std::endl;
        std::cout << "  Streaming Hit Ratio: " << std::fixed << std::setprecision(2) 
                  << (float(streaming_stats.cache_hits) / 
                     float(streaming_stats.cache_hits + streaming_stats.cache_misses) * 100) << "%" << std::endl;
        std::cout << "  Average Load Time: " << std::fixed << std::setprecision(2) 
                  << streaming_stats.average_load_time_ms << "ms" << std::endl;
        std::cout << "  Buffer Health: " << (streaming_stats.is_underrun ? "Underrun Detected" : "Healthy") << std::endl;
        
        // Success criteria evaluation
        std::cout << "\n=== Success Criteria Evaluation ===" << std::endl;
        
        bool smooth_playback = stats_.average_frame_time_ms < 33.0; // Under 33ms for 30fps
        bool no_vram_exhaustion = !stats_.had_vram_exhaustion;
        bool good_hit_ratio = (float(stats_.cache_hits) / float(stats_.cache_hits + stats_.cache_misses)) > 0.7f;
        
        std::cout << "✓ Smooth Playback (< 33ms/frame): " << (smooth_playback ? "PASS" : "FAIL") << std::endl;
        std::cout << "✓ No VRAM Exhaustion: " << (no_vram_exhaustion ? "PASS" : "FAIL") << std::endl;
        std::cout << "✓ Good Cache Performance (> 70%): " << (good_hit_ratio ? "PASS" : "FAIL") << std::endl;
        
        if (smooth_playback && no_vram_exhaustion && good_hit_ratio) {
            std::cout << "\n🎉 SUCCESS: 8K video processed smoothly with limited VRAM!" << std::endl;
        } else {
            std::cout << "\n⚠️  Some performance criteria not met - optimization needed" << std::endl;
        }
    }
};

} // namespace video_editor::gfx::demo

// Demo main function
int main() {
    try {
        video_editor::gfx::demo::EightKVideoDemo demo;
        demo.run_playback_demo();
        
        std::cout << "\nDemo completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Demo failed with error: " << e.what() << std::endl;
        return 1;
    }
}
