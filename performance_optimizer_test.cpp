#ifdef HAVE_GTEST
#include <gtest/gtest.h>
#else
// Simple test framework fallback
#include <iostream>
#include <cassert>
#define TEST_F(test_fixture, test_name) void test_fixture##_##test_name()
#define EXPECT_TRUE(condition) do { if (!(condition)) { std::cerr << "FAIL: " << #condition << " at line " << __LINE__ << std::endl; } else { std::cout << "PASS: " << #condition << std::endl; } } while(0)
#define EXPECT_FALSE(condition) EXPECT_TRUE(!(condition))
#define EXPECT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "FAIL: " << #a << " == " << #b << " at line " << __LINE__ << std::endl; } else { std::cout << "PASS: " << #a << " == " << #b << std::endl; } } while(0)
#define EXPECT_NE(a, b) do { if ((a) == (b)) { std::cerr << "FAIL: " << #a << " != " << #b << " at line " << __LINE__ << std::endl; } else { std::cout << "PASS: " << #a << " != " << #b << std::endl; } } while(0)
#define EXPECT_GT(a, b) do { if ((a) <= (b)) { std::cerr << "FAIL: " << #a << " > " << #b << " at line " << __LINE__ << std::endl; } else { std::cout << "PASS: " << #a << " > " << #b << std::endl; } } while(0)
#define EXPECT_GE(a, b) do { if ((a) < (b)) { std::cerr << "FAIL: " << #a << " >= " << #b << " at line " << __LINE__ << std::endl; } else { std::cout << "PASS: " << #a << " >= " << #b << std::endl; } } while(0)
#define ASSERT_TRUE(condition) do { if (!(condition)) { std::cerr << "FATAL: " << #condition << " at line " << __LINE__ << std::endl; exit(1); } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { std::cerr << "FATAL: " << #a << " != " << #b << " at line " << __LINE__ << std::endl; exit(1); } } while(0)
#define GTEST_SKIP() do { std::cout << "SKIPPED: Test skipped" << std::endl; return; } while(0)

class testing {
public:
    class Test {
    public:
        virtual void SetUp() {}
        virtual void TearDown() {}
    };
};
#endif
#include "media_io/performance_optimizer.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <random>

using namespace media_io;

class PerformanceOptimizerTest : public testing::Test {
protected:
    void SetUp() override {
        optimizer_ = std::make_unique<PerformanceOptimizer>();
    }

    void TearDown() override {
        optimizer_.reset();
    }

    std::unique_ptr<PerformanceOptimizer> optimizer_;
};

#ifndef HAVE_GTEST
// Simple test runner
void run_all_tests() {
    std::cout << "\n🎯 Week 12 Performance Optimization Test Suite" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    PerformanceOptimizerTest test;
    
    std::cout << "\n🔍 Running Hardware Detection Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_HardwareDetection();
    test.TearDown();
    
    test.SetUp();
    test.PerformanceOptimizerTest_OptimalHardwareSelection();
    test.TearDown();
    
    std::cout << "\n📊 Running Codec Performance Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_CodecPerformanceProfiles();
    test.TearDown();
    
    test.SetUp();
    test.PerformanceOptimizerTest_PerformanceTargets();
    test.TearDown();
    
    std::cout << "\n🚀 Running Threading Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_LockFreeQueue();
    test.TearDown();
    
    test.SetUp();
    test.PerformanceOptimizerTest_MultiThreadedDecoding();
    test.TearDown();
    
    std::cout << "\n🧠 Running Memory Management Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_NUMAAllocator();
    test.TearDown();
    
    test.SetUp();
    test.PerformanceOptimizerTest_PredictiveFrameCache();
    test.TearDown();
    
    std::cout << "\n📈 Running Performance Metrics Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_PerformanceMetrics();
    test.TearDown();
    
    std::cout << "\n⚙️ Running Strategy Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_OptimizationStrategies();
    test.TearDown();
    
    std::cout << "\n💻 Running System Capabilities Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_SystemCapabilities();
    test.TearDown();
    
    std::cout << "\n🏆 Running Benchmark Tests..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_PerformanceBenchmarks();
    test.TearDown();
    
    std::cout << "\n🎯 Running Week 12 Integration Test..." << std::endl;
    test.SetUp();
    test.PerformanceOptimizerTest_Week12Integration();
    test.TearDown();
    
    std::cout << "\n✅ All Performance Optimization Tests Completed!" << std::endl;
}

int main() {
    run_all_tests();
    return 0;
}
#endif

//=============================================================================
// Hardware Detection Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, HardwareDetection) {
    ASSERT_TRUE(optimizer_->initialize());
    
    auto hardware = optimizer_->detect_available_hardware();
    EXPECT_FALSE(hardware.empty()) << "Should detect at least CPU decoding";
    
    // CPU should always be available
    EXPECT_TRUE(optimizer_->is_hardware_available(HardwareAcceleration::NONE));
    
    std::cout << "🔍 Detected Hardware Acceleration:" << std::endl;
    for (auto hw : hardware) {
        std::string hw_name;
        switch (hw) {
            case HardwareAcceleration::NONE: hw_name = "CPU Only"; break;
            case HardwareAcceleration::NVIDIA_NVDEC: hw_name = "NVIDIA NVDEC"; break;
            case HardwareAcceleration::INTEL_QUICKSYNC: hw_name = "Intel Quick Sync"; break;
            case HardwareAcceleration::AMD_VCE: hw_name = "AMD VCE"; break;
            case HardwareAcceleration::DXVA2: hw_name = "DXVA2"; break;
            case HardwareAcceleration::D3D11VA: hw_name = "D3D11VA"; break;
            default: hw_name = "Unknown"; break;
        }
        std::cout << "  - " << hw_name << " ✅" << std::endl;
    }
}

TEST_F(PerformanceOptimizerTest, OptimalHardwareSelection) {
    ASSERT_TRUE(optimizer_->initialize(OptimizationStrategy::SPEED_FIRST));
    
    auto h264_hw = optimizer_->select_optimal_hardware("h264");
    auto h265_hw = optimizer_->select_optimal_hardware("h265");
    auto prores_hw = optimizer_->select_optimal_hardware("prores");
    
    std::cout << "🎯 Optimal Hardware Selection:" << std::endl;
    std::cout << "  - H.264: " << static_cast<int>(h264_hw) << std::endl;
    std::cout << "  - H.265: " << static_cast<int>(h265_hw) << std::endl;
    std::cout << "  - ProRes: " << static_cast<int>(prores_hw) << std::endl;
    
    // ProRes often works better on CPU
    EXPECT_EQ(prores_hw, HardwareAcceleration::NONE);
}

//=============================================================================
// Codec Performance Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, CodecPerformanceProfiles) {
    ASSERT_TRUE(optimizer_->initialize());
    
    auto h264_perf = optimizer_->get_codec_performance("h264");
    auto h265_perf = optimizer_->get_codec_performance("h265");
    auto prores_perf = optimizer_->get_codec_performance("prores");
    auto av1_perf = optimizer_->get_codec_performance("av1");
    
    EXPECT_EQ(h264_perf.codec_name, "h264");
    EXPECT_EQ(h265_perf.codec_name, "h265");
    EXPECT_EQ(prores_perf.codec_name, "prores");
    EXPECT_EQ(av1_perf.codec_name, "av1");
    
    // H.264 should be baseline performance
    EXPECT_EQ(h264_perf.cpu_decode_factor, 1.0);
    
    // H.265 should be more CPU intensive
    EXPECT_GT(h265_perf.cpu_decode_factor, h264_perf.cpu_decode_factor);
    
    // AV1 should be most CPU intensive
    EXPECT_GT(av1_perf.cpu_decode_factor, h265_perf.cpu_decode_factor);
    
    std::cout << "📊 Codec Performance Profiles:" << std::endl;
    std::cout << "  - H.264 CPU Factor: " << h264_perf.cpu_decode_factor << "x" << std::endl;
    std::cout << "  - H.265 CPU Factor: " << h265_perf.cpu_decode_factor << "x" << std::endl;
    std::cout << "  - ProRes CPU Factor: " << prores_perf.cpu_decode_factor << "x" << std::endl;
    std::cout << "  - AV1 CPU Factor: " << av1_perf.cpu_decode_factor << "x" << std::endl;
}

TEST_F(PerformanceOptimizerTest, PerformanceTargets) {
    ASSERT_TRUE(optimizer_->initialize());
    
    // Test performance targets for Week 12 goals
    EXPECT_TRUE(optimizer_->can_achieve_target_fps("prores", 3840, 2160, 60.0)) 
        << "Should achieve 4K ProRes 60fps target";
    
    EXPECT_TRUE(optimizer_->can_achieve_target_fps("h265", 7680, 4320, 30.0))
        << "Should achieve 8K HEVC 30fps target";
    
    EXPECT_TRUE(optimizer_->can_achieve_target_fps("h264", 1920, 1080, 60.0))
        << "Should achieve 1080p H.264 60fps for multiple streams";
    
    std::cout << "🎯 Week 12 Performance Targets:" << std::endl;
    std::cout << "  - 4K ProRes 60fps: " 
              << (optimizer_->can_achieve_target_fps("prores", 3840, 2160, 60.0) ? "✅" : "❌") 
              << std::endl;
    std::cout << "  - 8K HEVC 30fps: "
              << (optimizer_->can_achieve_target_fps("h265", 7680, 4320, 30.0) ? "✅" : "❌")
              << std::endl;
    std::cout << "  - 1080p H.264 60fps (4x streams): "
              << (optimizer_->can_achieve_target_fps("h264", 1920, 1080, 60.0) ? "✅" : "❌")
              << std::endl;
}

//=============================================================================
// Threading and Queue Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, LockFreeQueue) {
    LockFreeDecodeQueue queue(16);
    
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
    
    // Test enqueue/dequeue
    DecodeWorkItem work;
    work.frame_number = 42;
    work.priority = 1;
    work.compressed_data = {1, 2, 3, 4};
    
    EXPECT_TRUE(queue.enqueue(std::move(work)));
    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1);
    
    DecodeWorkItem retrieved;
    EXPECT_TRUE(queue.dequeue(retrieved));
    EXPECT_EQ(retrieved.frame_number, 42);
    EXPECT_EQ(retrieved.priority, 1);
    EXPECT_EQ(retrieved.compressed_data.size(), 4);
    
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST_F(PerformanceOptimizerTest, MultiThreadedDecoding) {
    ASSERT_TRUE(optimizer_->initialize());
    
    const int num_frames = 50;
    std::vector<std::future<std::shared_ptr<MediaFrame>>> futures;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Submit decode work
    for (int i = 0; i < num_frames; ++i) {
        DecodeWorkItem work;
        work.frame_number = i;
        work.priority = (i % 10); // Varying priorities
        work.compressed_data.resize(1000); // Mock compressed data
        work.submit_time = std::chrono::steady_clock::now();
        work.preferred_hw_accel = HardwareAcceleration::NONE;
        
        futures.push_back(optimizer_->submit_decode_work(std::move(work)));
    }
    
    // Wait for all frames to decode
    int successful_decodes = 0;
    for (auto& future : futures) {
        try {
            auto frame = future.get();
            if (frame) {
                successful_decodes++;
            }
        } catch (const std::exception& e) {
            // Handle decode errors
        }
    }
    
    auto total_time = std::chrono::steady_clock::now() - start_time;
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_time).count();
    
    EXPECT_GT(successful_decodes, num_frames * 0.8) << "At least 80% of frames should decode successfully";
    
    std::cout << "🚀 Multi-threaded Decode Performance:" << std::endl;
    std::cout << "  - Frames decoded: " << successful_decodes << "/" << num_frames << std::endl;
    std::cout << "  - Total time: " << total_ms << "ms" << std::endl;
    std::cout << "  - Average per frame: " << (total_ms / num_frames) << "ms" << std::endl;
    std::cout << "  - Effective FPS: " << (1000.0 * num_frames / total_ms) << " fps" << std::endl;
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, NUMAAllocator) {
    if (!performance_utils::is_numa_available()) {
        GTEST_SKIP() << "NUMA not available on this system";
    }
    
    NUMAAllocator allocator;
    
    const size_t alloc_size = 1024 * 1024; // 1MB
    void* ptr = allocator.allocate(alloc_size);
    
    ASSERT_NE(ptr, nullptr);
    
    // Test that we can write to the memory
    memset(ptr, 0xAA, alloc_size);
    
    allocator.deallocate(ptr);
    
    std::cout << "🧠 NUMA Allocator: ✅" << std::endl;
}

TEST_F(PerformanceOptimizerTest, PredictiveFrameCache) {
    const size_t cache_size = 100 * 1024 * 1024; // 100MB
    PredictiveFrameCache cache(cache_size);
    
    // Test cache miss
    auto frame = cache.get_frame(42);
    EXPECT_EQ(frame, nullptr);
    
    // Create and cache a frame
    auto test_frame = std::make_shared<MediaFrame>();
    test_frame->width = 1920;
    test_frame->height = 1080;
    test_frame->data.resize(test_frame->width * test_frame->height * 3);
    
    cache.cache_frame(42, test_frame);
    
    // Test cache hit
    auto cached_frame = cache.get_frame(42);
    EXPECT_NE(cached_frame, nullptr);
    EXPECT_EQ(cached_frame->width, 1920);
    EXPECT_EQ(cached_frame->height, 1080);
    
    // Test prediction
    std::vector<int64_t> access_pattern = {40, 41, 42, 43, 44};
    cache.predict_access_pattern(access_pattern);
    
    double hit_rate = cache.get_hit_rate();
    EXPECT_GT(hit_rate, 0.0);
    
    std::cout << "🎯 Predictive Frame Cache:" << std::endl;
    std::cout << "  - Memory usage: " << (cache.get_memory_usage() / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  - Hit rate: " << (hit_rate * 100.0) << "%" << std::endl;
}

//=============================================================================
// Performance Metrics Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, PerformanceMetrics) {
    ASSERT_TRUE(optimizer_->initialize());
    
    // Generate some decode activity
    const int num_operations = 20;
    for (int i = 0; i < num_operations; ++i) {
        DecodeWorkItem work;
        work.frame_number = i;
        work.priority = 1;
        work.compressed_data.resize(1000);
        work.submit_time = std::chrono::steady_clock::now();
        work.preferred_hw_accel = HardwareAcceleration::NONE;
        
        auto future = optimizer_->submit_decode_work(std::move(work));
        
        // Wait for completion to generate metrics
        try {
            future.get();
        } catch (...) {
            // Ignore errors for metrics test
        }
    }
    
    // Allow some time for metrics to accumulate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto metrics = optimizer_->get_performance_metrics();
    
    EXPECT_GT(metrics.frames_per_second, 0.0);
    EXPECT_GE(metrics.avg_decode_time.count(), 0);
    
    std::cout << "📈 Performance Metrics:" << std::endl;
    std::cout << "  - Average decode time: " << metrics.avg_decode_time.count() << " μs" << std::endl;
    std::cout << "  - Frames per second: " << metrics.frames_per_second << " fps" << std::endl;
    std::cout << "  - Queue depth: " << metrics.decode_queue_depth << std::endl;
    std::cout << "  - Memory usage: " << (metrics.current_memory_usage / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  - Cache hit rate: " << metrics.cache_hit_rate_percent << "%" << std::endl;
}

//=============================================================================
// Optimization Strategy Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, OptimizationStrategies) {
    // Test different strategies
    std::vector<OptimizationStrategy> strategies = {
        OptimizationStrategy::QUALITY_FIRST,
        OptimizationStrategy::SPEED_FIRST,
        OptimizationStrategy::BALANCED,
        OptimizationStrategy::MEMORY_EFFICIENT,
        OptimizationStrategy::POWER_EFFICIENT,
        OptimizationStrategy::REAL_TIME
    };
    
    std::cout << "⚙️ Optimization Strategies:" << std::endl;
    
    for (auto strategy : strategies) {
        optimizer_ = std::make_unique<PerformanceOptimizer>();
        EXPECT_TRUE(optimizer_->initialize(strategy));
        
        auto h264_hw = optimizer_->select_optimal_hardware("h264");
        auto prores_hw = optimizer_->select_optimal_hardware("prores");
        
        std::string strategy_name;
        switch (strategy) {
            case OptimizationStrategy::QUALITY_FIRST: strategy_name = "Quality First"; break;
            case OptimizationStrategy::SPEED_FIRST: strategy_name = "Speed First"; break;
            case OptimizationStrategy::BALANCED: strategy_name = "Balanced"; break;
            case OptimizationStrategy::MEMORY_EFFICIENT: strategy_name = "Memory Efficient"; break;
            case OptimizationStrategy::POWER_EFFICIENT: strategy_name = "Power Efficient"; break;
            case OptimizationStrategy::REAL_TIME: strategy_name = "Real Time"; break;
        }
        
        std::cout << "  - " << strategy_name << ": H.264=" << static_cast<int>(h264_hw) 
                  << ", ProRes=" << static_cast<int>(prores_hw) << std::endl;
    }
}

//=============================================================================
// System Information Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, SystemCapabilities) {
    auto cpu_features = performance_utils::detect_cpu_features();
    auto memory_info = performance_utils::get_system_memory_info();
    auto gpu_caps = performance_utils::detect_gpu_capabilities();
    
    std::cout << "💻 System Capabilities:" << std::endl;
    std::cout << "  CPU Features:" << std::endl;
    std::cout << "    - AVX2: " << (cpu_features.has_avx2 ? "✅" : "❌") << std::endl;
    std::cout << "    - AVX512: " << (cpu_features.has_avx512 ? "✅" : "❌") << std::endl;
    std::cout << "    - SSE4.1: " << (cpu_features.has_sse4_1 ? "✅" : "❌") << std::endl;
    std::cout << "    - FMA: " << (cpu_features.has_fma ? "✅" : "❌") << std::endl;
    std::cout << "    - L3 Cache: " << (cpu_features.l3_cache_size / 1024 / 1024) << " MB" << std::endl;
    
    std::cout << "  Memory:" << std::endl;
    std::cout << "    - Total Physical: " << (memory_info.total_physical_memory / 1024.0 / 1024.0 / 1024.0) << " GB" << std::endl;
    std::cout << "    - Available: " << (memory_info.available_physical_memory / 1024.0 / 1024.0 / 1024.0) << " GB" << std::endl;
    std::cout << "    - Page Size: " << memory_info.page_size << " bytes" << std::endl;
    std::cout << "    - Cache Line: " << memory_info.cache_line_size << " bytes" << std::endl;
    
    std::cout << "  GPUs:" << std::endl;
    for (const auto& gpu : gpu_caps) {
        std::cout << "    - " << gpu.vendor << " " << gpu.model << std::endl;
        std::cout << "      Memory: " << (gpu.total_memory / 1024.0 / 1024.0 / 1024.0) << " GB" << std::endl;
        std::cout << "      H.264: " << (gpu.supports_h264_decode ? "✅" : "❌") << std::endl;
        std::cout << "      H.265: " << (gpu.supports_h265_decode ? "✅" : "❌") << std::endl;
        std::cout << "      AV1: " << (gpu.supports_av1_decode ? "✅" : "❌") << std::endl;
    }
    
    EXPECT_GT(memory_info.total_physical_memory, 0);
    EXPECT_GT(memory_info.page_size, 0);
    EXPECT_GT(cpu_features.l3_cache_size, 0);
}

//=============================================================================
// Performance Benchmarking Tests
//=============================================================================

TEST_F(PerformanceOptimizerTest, PerformanceBenchmarks) {
    auto h264_1080p = performance_utils::benchmark_decode_performance("h264", 1920, 1080);
    auto h265_4k = performance_utils::benchmark_decode_performance("h265", 3840, 2160);
    auto av1_8k = performance_utils::benchmark_decode_performance("av1", 7680, 4320);
    
    auto memory_bandwidth = performance_utils::benchmark_memory_bandwidth();
    auto cpu_score = performance_utils::benchmark_cpu_performance();
    
    std::cout << "🏆 Performance Benchmarks:" << std::endl;
    std::cout << "  Decode Performance:" << std::endl;
    std::cout << "    - H.264 1080p: " << h264_1080p << " fps" << std::endl;
    std::cout << "    - H.265 4K: " << h265_4k << " fps" << std::endl;
    std::cout << "    - AV1 8K: " << av1_8k << " fps" << std::endl;
    std::cout << "  System Performance:" << std::endl;
    std::cout << "    - Memory Bandwidth: " << memory_bandwidth << " GB/s" << std::endl;
    std::cout << "    - CPU Score: " << cpu_score << " points" << std::endl;
    
    EXPECT_GT(h264_1080p, 0.0);
    EXPECT_GT(h265_4k, 0.0);
    EXPECT_GT(av1_8k, 0.0);
    EXPECT_GT(memory_bandwidth, 0.0);
    EXPECT_GT(cpu_score, 0.0);
    
    // Week 12 targets verification
    EXPECT_GE(h264_1080p, 60.0) << "Should achieve 1080p H.264 at 60fps";
    EXPECT_GE(h265_4k, 15.0) << "Should achieve 4K H.265 at reasonable fps";
}

//=============================================================================
// Integration Test
//=============================================================================

TEST_F(PerformanceOptimizerTest, Week12Integration) {
    std::cout << "\n🎯 Week 12 Performance Optimization Integration Test" << std::endl;
    std::cout << "Testing production-ready performance targets..." << std::endl;
    
    ASSERT_TRUE(optimizer_->initialize(OptimizationStrategy::BALANCED));
    
    // Test 1: Hardware acceleration detection
    auto hardware = optimizer_->detect_available_hardware();
    bool has_hw_accel = hardware.size() > 1; // More than just CPU
    std::cout << "\n1. Hardware Acceleration: " << (has_hw_accel ? "✅" : "⚠️ CPU Only") << std::endl;
    
    // Test 2: Performance targets
    bool prores_4k_60 = optimizer_->can_achieve_target_fps("prores", 3840, 2160, 60.0);
    bool hevc_8k_30 = optimizer_->can_achieve_target_fps("h265", 7680, 4320, 30.0);
    bool multi_stream_1080p = optimizer_->can_achieve_target_fps("h264", 1920, 1080, 60.0);
    
    std::cout << "2. Performance Targets:" << std::endl;
    std::cout << "   - 4K ProRes 60fps: " << (prores_4k_60 ? "✅" : "❌") << std::endl;
    std::cout << "   - 8K HEVC 30fps: " << (hevc_8k_30 ? "✅" : "❌") << std::endl;
    std::cout << "   - 4x 1080p streams: " << (multi_stream_1080p ? "✅" : "❌") << std::endl;
    
    // Test 3: Memory efficiency
    auto memory_info = performance_utils::get_system_memory_info();
    bool sufficient_memory = memory_info.total_physical_memory >= (16LL * 1024 * 1024 * 1024); // 16GB
    std::cout << "3. Memory: " << (memory_info.total_physical_memory / 1024.0 / 1024.0 / 1024.0) 
              << " GB " << (sufficient_memory ? "✅" : "⚠️ Limited") << std::endl;
    
    // Test 4: Threading optimization
    size_t optimal_threads = performance_utils::get_optimal_thread_count();
    bool good_threading = optimal_threads >= 4;
    std::cout << "4. Threading: " << optimal_threads << " threads " 
              << (good_threading ? "✅" : "⚠️ Limited") << std::endl;
    
    // Test 5: Lock-free performance
    auto start = std::chrono::steady_clock::now();
    LockFreeDecodeQueue queue(1024);
    
    // Stress test the queue
    const int operations = 10000;
    for (int i = 0; i < operations; ++i) {
        DecodeWorkItem work;
        work.frame_number = i;
        queue.enqueue(std::move(work));
        
        DecodeWorkItem retrieved;
        queue.dequeue(retrieved);
    }
    
    auto queue_time = std::chrono::steady_clock::now() - start;
    auto queue_us = std::chrono::duration_cast<std::chrono::microseconds>(queue_time).count();
    bool fast_queue = queue_us < 10000; // Less than 10ms for 10k ops
    
    std::cout << "5. Lock-free Queue: " << queue_us << " μs " 
              << (fast_queue ? "✅" : "⚠️ Slow") << std::endl;
    
    // Overall assessment
    int passed_tests = (has_hw_accel ? 1 : 0) + (prores_4k_60 ? 1 : 0) + 
                      (hevc_8k_30 ? 1 : 0) + (multi_stream_1080p ? 1 : 0) +
                      (sufficient_memory ? 1 : 0) + (good_threading ? 1 : 0) +
                      (fast_queue ? 1 : 0);
    
    std::cout << "\n📊 Week 12 Performance Assessment: " << passed_tests << "/7 targets achieved" << std::endl;
    
    if (passed_tests >= 5) {
        std::cout << "🎉 PRODUCTION READY: High-end production workflow capability achieved!" << std::endl;
    } else if (passed_tests >= 3) {
        std::cout << "⚠️ GOOD: Professional workflow capability with some limitations" << std::endl;
    } else {
        std::cout << "❌ NEEDS WORK: Performance optimization requires more development" << std::endl;
    }
    
    // Core requirements should pass
    EXPECT_TRUE(multi_stream_1080p) << "Multi-stream 1080p is essential for professional workflows";
    EXPECT_GE(passed_tests, 3) << "Should achieve at least 3/7 performance targets";
}
