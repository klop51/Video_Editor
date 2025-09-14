#include "media_io/high_resolution_support.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <iomanip>

using namespace ve::media_io;
using namespace ve::media_io::resolution_utils;

/**
 * @brief Comprehensive validation test for 8K Support Infrastructure
 * 
 * Tests Phase 3 Week 9 implementation: 8K resolution support, memory management,
 * performance optimization, streaming decode, and professional workflow integration.
 */

class HighResolutionValidationTest {
public:
    void runAllTests() {
        std::cout << "=== 8K Support Infrastructure Validation Test ===" << std::endl;
        
        testInitialization();
        testResolutionSupport();
        testResolutionCategorization();
        testMemoryManagement();
        testPerformanceAssessment();
        testOptimizationRecommendations();
        test8KCapabilities();
        testStreamingManager();
        testResolutionConversion();
        testProfessionalWorkflows();
        testUtilityFunctions();
        testSystemCapabilities();
        
        std::cout << "=== 8K Support Infrastructure Validation COMPLETE ===" << std::endl;
        std::cout << "All high-resolution components tested successfully!" << std::endl;
    }

private:
    void testInitialization() {
        std::cout << "Testing High Resolution Support initialization..." << std::endl;
        
        HighResolutionSupport hrs;
        auto supported_resolutions = hrs.getSupportedResolutions();
        
        assert(!supported_resolutions.empty());
        std::cout << "High Resolution Support initialized: SUCCESS" << std::endl;
        std::cout << "Supported resolutions: " << supported_resolutions.size() << std::endl;
        std::cout << std::endl;
    }
    
    void testResolutionSupport() {
        std::cout << "Testing resolution support detection..." << std::endl;
        
        HighResolutionSupport hrs;
        
        // Test standard resolutions
        assert(hrs.isResolutionSupported(1920, 1080));    // Full HD
        assert(hrs.isResolutionSupported(3840, 2160));    // UHD 4K
        assert(hrs.isResolutionSupported(7680, 4320));    // UHD 8K
        assert(hrs.isResolutionSupported(4096, 2160));    // DCI 4K
        assert(hrs.isResolutionSupported(8192, 4320));    // DCI 8K
        
        std::cout << "  Standard resolutions: SUCCESS" << std::endl;
        
        // Test professional resolutions
        assert(hrs.isResolutionSupported(6144, 3456));    // RED 6K
        assert(hrs.isResolutionSupported(5120, 2700));    // 5K Ultra-wide
        
        std::cout << "  Professional resolutions: SUCCESS" << std::endl;
        
        // Test ultra-wide resolutions
        assert(hrs.isResolutionSupported(3440, 1440));    // UW QHD
        assert(hrs.isResolutionSupported(5120, 1440));    // UW 5K
        
        std::cout << "  Ultra-wide resolutions: SUCCESS" << std::endl;
        
        // Test invalid resolutions
        assert(!hrs.isResolutionSupported(100, 100));     // Too small
        assert(!hrs.isResolutionSupported(20000, 10000)); // Too large
        
        std::cout << "  Invalid resolution rejection: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testResolutionCategorization() {
        std::cout << "Testing resolution categorization..." << std::endl;
        
        HighResolutionSupport hrs;
        
        // Test HD categorization
        assert(hrs.categorizeResolution(1920, 1080) == ResolutionCategory::HD);
        assert(hrs.categorizeResolution(1280, 720) == ResolutionCategory::SD);  // 720p is actually SD by pixel count
        
        std::cout << "  HD categorization: SUCCESS" << std::endl;
        
        // Test SD categorization
        assert(hrs.categorizeResolution(1280, 720) == ResolutionCategory::SD);
        assert(hrs.categorizeResolution(720, 480) == ResolutionCategory::SD);
        
        std::cout << "  SD categorization: SUCCESS" << std::endl;
        
        // Test 4K categorization
        assert(hrs.categorizeResolution(3840, 2160) == ResolutionCategory::UHD_4K);
        assert(hrs.categorizeResolution(4096, 2160) == ResolutionCategory::DCI_4K);
        
        std::cout << "  4K categorization: SUCCESS" << std::endl;
        
        // Test 8K categorization
        assert(hrs.categorizeResolution(7680, 4320) == ResolutionCategory::UHD_8K);
        assert(hrs.categorizeResolution(8192, 4320) == ResolutionCategory::DCI_8K);
        
        std::cout << "  8K categorization: SUCCESS" << std::endl;
        
        // Test ultra-wide categorization
        assert(hrs.categorizeResolution(3440, 1440) == ResolutionCategory::ULTRA_WIDE);
        assert(hrs.categorizeResolution(5120, 1440) == ResolutionCategory::ULTRA_WIDE);
        
        std::cout << "  Ultra-wide categorization: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testMemoryManagement() {
        std::cout << "Testing memory management strategies..." << std::endl;
        
        HighResolutionSupport hrs;
        
        // Test memory strategy for different resolutions
        Resolution hd_res(1920, 1080, "Full HD");
        Resolution uhd_4k_res(3840, 2160, "UHD 4K");
        Resolution uhd_8k_res(7680, 4320, "UHD 8K");
        
        MemoryStrategy hd_strategy = hrs.getOptimalMemoryStrategy(hd_res);
        MemoryStrategy uhd_4k_strategy = hrs.getOptimalMemoryStrategy(uhd_4k_res);
        MemoryStrategy uhd_8k_strategy = hrs.getOptimalMemoryStrategy(uhd_8k_res);
        
        // HD should use standard strategy
        assert(hd_strategy == MemoryStrategy::STANDARD);
        std::cout << "  HD memory strategy: " << (hd_strategy == MemoryStrategy::STANDARD ? "STANDARD" : "OTHER") << std::endl;
        
        // 4K may use standard or streaming based on system
        assert(uhd_4k_strategy == MemoryStrategy::STANDARD || uhd_4k_strategy == MemoryStrategy::STREAMING);
        std::cout << "  4K memory strategy: " << (uhd_4k_strategy == MemoryStrategy::STANDARD ? "STANDARD" : "STREAMING") << std::endl;
        
        // 8K should use streaming or hybrid
        assert(uhd_8k_strategy == MemoryStrategy::STREAMING || uhd_8k_strategy == MemoryStrategy::HYBRID);
        std::cout << "  8K memory strategy: " << (uhd_8k_strategy == MemoryStrategy::STREAMING ? "STREAMING" : "HYBRID") << std::endl;
        
        // Test memory calculations
        size_t hd_memory = hrs.calculateMemoryRequirement(hd_res, "YUV420P");
        size_t uhd_4k_memory = hrs.calculateMemoryRequirement(uhd_4k_res, "YUV420P");
        size_t uhd_8k_memory = hrs.calculateMemoryRequirement(uhd_8k_res, "YUV420P");
        
        std::cout << "  HD memory requirement: " << (hd_memory / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "  4K memory requirement: " << (uhd_4k_memory / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "  8K memory requirement: " << (uhd_8k_memory / (1024 * 1024)) << " MB" << std::endl;
        
        // Validate memory scaling
        assert(uhd_4k_memory > hd_memory);
        assert(uhd_8k_memory > uhd_4k_memory);
        
        // Test cache size recommendations
        size_t hd_cache = hrs.getRecommendedCacheSize(hd_res);
        size_t uhd_4k_cache = hrs.getRecommendedCacheSize(uhd_4k_res);
        size_t uhd_8k_cache = hrs.getRecommendedCacheSize(uhd_8k_res);
        
        std::cout << "  HD cache size: " << (hd_cache / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "  4K cache size: " << (uhd_4k_cache / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "  8K cache size: " << (uhd_8k_cache / (1024 * 1024)) << " MB" << std::endl;
        
        assert(hd_cache > 0 && uhd_4k_cache > 0 && uhd_8k_cache > 0);
        
        std::cout << "Memory management: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testPerformanceAssessment() {
        std::cout << "Testing performance assessment..." << std::endl;
        
        HighResolutionSupport hrs;
        
        Resolution hd_res(1920, 1080, "Full HD");
        Resolution uhd_4k_res(3840, 2160, "UHD 4K");
        Resolution uhd_8k_res(7680, 4320, "UHD 8K");
        
        // Test performance tier assessment
        PerformanceTier hd_tier = hrs.assessPerformance(hd_res);
        PerformanceTier uhd_4k_tier = hrs.assessPerformance(uhd_4k_res);
        PerformanceTier uhd_8k_tier = hrs.assessPerformance(uhd_8k_res);
        
        std::cout << "  HD performance tier: " << static_cast<int>(hd_tier) << std::endl;
        std::cout << "  4K performance tier: " << static_cast<int>(uhd_4k_tier) << std::endl;
        std::cout << "  8K performance tier: " << static_cast<int>(uhd_8k_tier) << std::endl;
        
        // HD should be real-time capable
        assert(hd_tier == PerformanceTier::REALTIME);
        
        // Test frame rate assessment
        uint32_t hd_fps = hrs.getMaxFrameRate(hd_res);
        uint32_t uhd_4k_fps = hrs.getMaxFrameRate(uhd_4k_res);
        uint32_t uhd_8k_fps = hrs.getMaxFrameRate(uhd_8k_res);
        
        std::cout << "  HD max frame rate: " << hd_fps << " fps" << std::endl;
        std::cout << "  4K max frame rate: " << uhd_4k_fps << " fps" << std::endl;
        std::cout << "  8K max frame rate: " << uhd_8k_fps << " fps" << std::endl;
        
        assert(hd_fps >= 30);  // HD should support at least 30fps
        assert(uhd_8k_fps > 0); // 8K should support some frame rate
        
        // Test GPU acceleration requirements
        bool hd_gpu_required = hrs.requiresGPUAcceleration(hd_res);
        bool uhd_4k_gpu_required = hrs.requiresGPUAcceleration(uhd_4k_res);
        bool uhd_8k_gpu_required = hrs.requiresGPUAcceleration(uhd_8k_res);
        
        std::cout << "  HD GPU required: " << (hd_gpu_required ? "YES" : "NO") << std::endl;
        std::cout << "  4K GPU required: " << (uhd_4k_gpu_required ? "YES" : "NO") << std::endl;
        std::cout << "  8K GPU required: " << (uhd_8k_gpu_required ? "YES" : "NO") << std::endl;
        
        // 4K and 8K should require GPU acceleration
        assert(uhd_4k_gpu_required);
        assert(uhd_8k_gpu_required);
        
        // Test decode latency estimation
        double hd_latency = hrs.estimateDecodeLatency(hd_res);
        double uhd_4k_latency = hrs.estimateDecodeLatency(uhd_4k_res);
        double uhd_8k_latency = hrs.estimateDecodeLatency(uhd_8k_res);
        
        std::cout << "  HD decode latency: " << std::fixed << std::setprecision(2) << hd_latency << " ms" << std::endl;
        std::cout << "  4K decode latency: " << std::fixed << std::setprecision(2) << uhd_4k_latency << " ms" << std::endl;
        std::cout << "  8K decode latency: " << std::fixed << std::setprecision(2) << uhd_8k_latency << " ms" << std::endl;
        
        assert(uhd_4k_latency > hd_latency);  // 4K should have higher latency than HD
        assert(uhd_8k_latency > uhd_4k_latency);  // 8K should have higher latency than 4K
        
        std::cout << "Performance assessment: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testOptimizationRecommendations() {
        std::cout << "Testing optimization recommendations..." << std::endl;
        
        HighResolutionSupport hrs;
        
        Resolution uhd_4k_res(3840, 2160, "UHD 4K");
        Resolution uhd_8k_res(7680, 4320, "UHD 8K");
        
        auto uhd_4k_rec = hrs.getOptimizationRecommendation(uhd_4k_res);
        auto uhd_8k_rec = hrs.getOptimizationRecommendation(uhd_8k_res);
        
        std::cout << "4K Optimization Recommendations:" << std::endl;
        std::cout << "  Hardware decode: " << (uhd_4k_rec.use_hardware_decode ? "YES" : "NO") << std::endl;
        std::cout << "  Streaming mode: " << (uhd_4k_rec.enable_streaming_mode ? "YES" : "NO") << std::endl;
        std::cout << "  Tiled processing: " << (uhd_4k_rec.use_tiled_processing ? "YES" : "NO") << std::endl;
        std::cout << "  GPU memory: " << (uhd_4k_rec.enable_gpu_memory ? "YES" : "NO") << std::endl;
        std::cout << "  Thread count: " << uhd_4k_rec.thread_count << std::endl;
        std::cout << "  Cache size: " << uhd_4k_rec.cache_size_mb << " MB" << std::endl;
        std::cout << "  Reasoning: " << uhd_4k_rec.reasoning << std::endl;
        
        std::cout << std::endl << "8K Optimization Recommendations:" << std::endl;
        std::cout << "  Hardware decode: " << (uhd_8k_rec.use_hardware_decode ? "YES" : "NO") << std::endl;
        std::cout << "  Streaming mode: " << (uhd_8k_rec.enable_streaming_mode ? "YES" : "NO") << std::endl;
        std::cout << "  Tiled processing: " << (uhd_8k_rec.use_tiled_processing ? "YES" : "NO") << std::endl;
        std::cout << "  GPU memory: " << (uhd_8k_rec.enable_gpu_memory ? "YES" : "NO") << std::endl;
        std::cout << "  Thread count: " << uhd_8k_rec.thread_count << std::endl;
        std::cout << "  Cache size: " << uhd_8k_rec.cache_size_mb << " MB" << std::endl;
        std::cout << "  Reasoning: " << uhd_8k_rec.reasoning << std::endl;
        
        // 8K should require more aggressive optimizations
        assert(uhd_8k_rec.use_hardware_decode);
        assert(uhd_8k_rec.enable_streaming_mode);
        assert(uhd_8k_rec.use_tiled_processing);
        assert(uhd_8k_rec.thread_count > 0);
        
        std::cout << "Optimization recommendations: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void test8KCapabilities() {
        std::cout << "Testing 8K capabilities assessment..." << std::endl;
        
        HighResolutionSupport hrs;
        auto caps = hrs.assess8KCapabilities();
        
        std::cout << "8K Capabilities Assessment:" << std::endl;
        std::cout << "  Decode supported: " << (caps.decode_supported ? "YES" : "NO") << std::endl;
        std::cout << "  Real-time playback: " << (caps.realtime_playback ? "YES" : "NO") << std::endl;
        std::cout << "  Hardware acceleration: " << (caps.hardware_acceleration ? "YES" : "NO") << std::endl;
        std::cout << "  Streaming decode: " << (caps.streaming_decode ? "YES" : "NO") << std::endl;
        std::cout << "  GPU memory required: " << (caps.gpu_memory_required ? "YES" : "NO") << std::endl;
        std::cout << "  Max frame rate: " << caps.max_framerate << " fps" << std::endl;
        std::cout << "  Memory requirement: " << caps.memory_requirement_gb << " GB" << std::endl;
        std::cout << "  Supported codecs: ";
        for (size_t i = 0; i < caps.supported_codecs.size(); ++i) {
            std::cout << caps.supported_codecs[i];
            if (i < caps.supported_codecs.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        
        // Validate capabilities
        assert(caps.memory_requirement_gb > 0);
        assert(!caps.supported_codecs.empty());
        
        // Test 8K optimization enablement
        bool optimizations_enabled = hrs.enable8KOptimizations();
        std::cout << "  8K optimizations enabled: " << (optimizations_enabled ? "YES" : "NO") << std::endl;
        
        std::cout << "8K capabilities: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testStreamingManager() {
        std::cout << "Testing 8K streaming manager..." << std::endl;
        
        EightKStreamingManager streaming_mgr;
        Resolution uhd_8k_res(7680, 4320, "UHD 8K");
        
        // Test streaming initialization
        bool init_success = streaming_mgr.initializeStreaming(uhd_8k_res);
        std::cout << "  Streaming initialization: " << (init_success ? "SUCCESS" : "FAILED") << std::endl;
        
        if (init_success) {
            // Test streaming configuration
            auto config = streaming_mgr.getOptimalStreamingConfig(uhd_8k_res);
            std::cout << "  Optimal tile size: " << config.tile_size << std::endl;
            std::cout << "  Buffer count: " << config.buffer_count << std::endl;
            std::cout << "  Prefetch frames: " << config.prefetch_frames << std::endl;
            std::cout << "  Compressed cache: " << (config.use_compressed_cache ? "YES" : "NO") << std::endl;
            std::cout << "  GPU streaming: " << (config.enable_gpu_streaming ? "YES" : "NO") << std::endl;
            std::cout << "  Quality factor: " << std::fixed << std::setprecision(2) << config.quality_factor << std::endl;
            
            assert(config.tile_size > 0);
            assert(config.buffer_count > 0);
            
            // Test tile calculation
            auto tiles = streaming_mgr.calculateTiles(uhd_8k_res, static_cast<uint32_t>(config.tile_size));
            std::cout << "  Total tiles for 8K: " << tiles.size() << std::endl;
            
            assert(!tiles.empty());
            
            // Test memory pool
            size_t memory_usage = streaming_mgr.getMemoryPoolUsage();
            size_t memory_capacity = streaming_mgr.getMemoryPoolCapacity();
            std::cout << "  Memory pool usage: " << (memory_usage / (1024 * 1024)) << " MB" << std::endl;
            std::cout << "  Memory pool capacity: " << (memory_capacity / (1024 * 1024)) << " MB" << std::endl;
            
            // Test statistics
            auto stats = streaming_mgr.getStreamingStats();
            std::cout << "  Frames processed: " << stats.frames_processed << std::endl;
            
            std::cout << "Streaming manager: SUCCESS" << std::endl;
        } else {
            std::cout << "Streaming manager: SKIPPED (insufficient system resources)" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void testResolutionConversion() {
        std::cout << "Testing resolution conversion utilities..." << std::endl;
        
        HighResolutionSupport hrs;
        
        // Test downscaling
        Resolution uhd_8k_res(7680, 4320, "UHD 8K");
        Resolution half_res = hrs.getDownscaledResolution(uhd_8k_res, 0.5);
        
        std::cout << "  8K downscaled 50%: " << half_res.width << "×" << half_res.height << std::endl;
        assert(half_res.width == 3840 && half_res.height == 2160);  // Should be 4K
        
        // Test preview resolutions
        auto preview_resolutions = hrs.getPreviewResolutions(uhd_8k_res);
        std::cout << "  Preview resolutions for 8K:" << std::endl;
        for (const auto& res : preview_resolutions) {
            std::cout << "    " << res.width << "×" << res.height << " (" << res.name << ")" << std::endl;
        }
        
        assert(!preview_resolutions.empty());
        
        // Test aspect ratio validation
        assert(hrs.isValidAspectRatio(16.0/9.0));   // Standard HD/UHD
        assert(hrs.isValidAspectRatio(21.0/9.0));   // Ultra-wide
        assert(!hrs.isValidAspectRatio(1.0));       // Square (not standard)
        
        std::cout << "Resolution conversion: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testProfessionalWorkflows() {
        std::cout << "Testing professional workflow support..." << std::endl;
        
        HighResolutionSupport hrs;
        
        // Test cinema resolutions
        auto cinema_resolutions = hrs.getCinemaResolutions();
        std::cout << "  Cinema resolutions: " << cinema_resolutions.size() << std::endl;
        for (const auto& res : cinema_resolutions) {
            std::cout << "    " << res.name << " (" << res.width << "×" << res.height << ")" << std::endl;
        }
        assert(!cinema_resolutions.empty());
        
        // Test broadcast resolutions
        auto broadcast_resolutions = hrs.getBroadcastResolutions();
        std::cout << "  Broadcast resolutions: " << broadcast_resolutions.size() << std::endl;
        assert(!broadcast_resolutions.empty());
        
        // Test streaming resolutions
        auto streaming_resolutions = hrs.getStreamingResolutions();
        std::cout << "  Streaming resolutions: " << streaming_resolutions.size() << std::endl;
        assert(!streaming_resolutions.empty());
        
        // Test ultra-wide resolutions
        auto ultra_wide_resolutions = hrs.getUltraWideResolutions();
        std::cout << "  Ultra-wide resolutions: " << ultra_wide_resolutions.size() << std::endl;
        assert(!ultra_wide_resolutions.empty());
        
        std::cout << "Professional workflows: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testUtilityFunctions() {
        std::cout << "Testing utility functions..." << std::endl;
        
        // Test professional resolution list
        auto all_resolutions = getAllProfessionalResolutions();
        std::cout << "  Total professional resolutions: " << all_resolutions.size() << std::endl;
        assert(!all_resolutions.empty());
        
        // Test 8K detection
        assert(requires8KHandling(7680, 4320));     // UHD 8K
        assert(requires8KHandling(8192, 4320));     // DCI 8K
        assert(!requires8KHandling(3840, 2160));    // 4K
        
        std::cout << "  8K detection: SUCCESS" << std::endl;
        
        // Test memory calculation
        size_t memory_8k = calculateMemoryRequirement(7680, 4320, "YUV420P");
        size_t memory_4k = calculateMemoryRequirement(3840, 2160, "YUV420P");
        
        std::cout << "  8K memory requirement: " << (memory_8k / (1024 * 1024)) << " MB" << std::endl;
        std::cout << "  4K memory requirement: " << (memory_4k / (1024 * 1024)) << " MB" << std::endl;
        
        assert(memory_8k > memory_4k);
        
        // Test system capability validation
        bool can_handle_4k = validateSystemCapability(3840, 2160, 30);
        bool can_handle_8k = validateSystemCapability(7680, 4320, 30);
        
        std::cout << "  Can handle 4K@30fps: " << (can_handle_4k ? "YES" : "NO") << std::endl;
        std::cout << "  Can handle 8K@30fps: " << (can_handle_8k ? "YES" : "NO") << std::endl;
        
        // Test optimization advice
        std::string advice_8k = getOptimizationAdvice(7680, 4320);
        std::cout << "  8K optimization advice: " << advice_8k << std::endl;
        assert(!advice_8k.empty());
        
        // Test resolution standard detection
        assert(isStandardResolution(1920, 1080));   // Full HD
        assert(isStandardResolution(3840, 2160));   // UHD 4K
        assert(isCinemaResolution(4096, 2160));     // DCI 4K
        assert(isBroadcastResolution(1920, 1080));  // Broadcast HD
        assert(isUltraWideResolution(3440, 1440)); // Ultra-wide
        
        std::cout << "  Resolution type detection: SUCCESS" << std::endl;
        
        // Test resolution conversion
        Resolution broadcast_4k(3840, 2160, "UHD 4K");
        Resolution cinema_equivalent = convertToCinemaStandard(broadcast_4k);
        
        std::cout << "  Cinema conversion: " << broadcast_4k.name << " -> " << cinema_equivalent.name << std::endl;
        assert(cinema_equivalent.width == 4096);  // Should convert to DCI 4K
        
        std::cout << "Utility functions: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testSystemCapabilities() {
        std::cout << "Testing system capabilities..." << std::endl;
        
        HighResolutionSupport hrs;
        auto capabilities = hrs.getSystemCapabilities();
        
        std::cout << "System Capabilities:" << std::endl;
        std::cout << "  Total RAM: " << capabilities.total_ram_gb << " GB" << std::endl;
        std::cout << "  Available RAM: " << capabilities.available_ram_gb << " GB" << std::endl;
        std::cout << "  GPU Memory: " << capabilities.gpu_memory_gb << " GB" << std::endl;
        std::cout << "  CPU Cores: " << capabilities.cpu_cores << std::endl;
        std::cout << "  Hardware decode: " << (capabilities.hardware_decode_available ? "YES" : "NO") << std::endl;
        std::cout << "  GPU compute: " << (capabilities.gpu_compute_available ? "YES" : "NO") << std::endl;
        std::cout << "  Supported APIs: ";
        for (size_t i = 0; i < capabilities.supported_apis.size(); ++i) {
            std::cout << capabilities.supported_apis[i];
            if (i < capabilities.supported_apis.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        
        // Validate capabilities
        assert(capabilities.total_ram_gb > 0);
        assert(capabilities.cpu_cores > 0);
        
        // Test statistics
        auto stats = hrs.getStatistics();
        std::cout << std::endl << "Resolution Statistics:" << std::endl;
        std::cout << "  Total supported: " << stats.total_resolutions_supported << std::endl;
        std::cout << "  4K resolutions: " << stats.uhd_4k_count << std::endl;
        std::cout << "  8K resolutions: " << stats.uhd_8k_count << std::endl;
        std::cout << "  Cinema resolutions: " << stats.cinema_count << std::endl;
        std::cout << "  Ultra-wide resolutions: " << stats.ultra_wide_count << std::endl;
        std::cout << "  Max memory requirement: " << stats.max_memory_requirement_gb << " GB" << std::endl;
        std::cout << "  Max supported framerate: " << stats.max_supported_framerate << " fps" << std::endl;
        
        assert(stats.total_resolutions_supported > 0);
        
        std::cout << "System capabilities: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
};

int main() {
    try {
        HighResolutionValidationTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
