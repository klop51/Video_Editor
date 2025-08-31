// GPU Memory Optimizer Tests
// Comprehensive testing for intelligent caching, streaming, and VRAM management

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "gpu_memory_optimizer.hpp"
#include "graphics_device.hpp"
#include <thread>
#include <chrono>

namespace video_editor::gfx::test {

// Mock Graphics Device for testing
class MockGraphicsDevice : public GraphicsDevice {
public:
    MOCK_METHOD(MemoryInfo, get_memory_info, (), (const, override));
    MOCK_METHOD(TextureHandle, create_texture, (const TextureDesc&), (override));
    MOCK_METHOD(void, destroy_texture, (TextureHandle), (override));
    MOCK_METHOD(bool, is_texture_valid, (TextureHandle), (const, override));
};

// Mock Texture Handle for testing
class MockTextureHandle : public TextureHandle {
private:
    uint64_t id_;
    size_t memory_size_;
    TextureFormat format_;
    bool valid_;

public:
    MockTextureHandle(uint64_t id = 0, size_t memory_size = 1024*1024, 
                     TextureFormat format = TextureFormat::RGBA8)
        : id_(id), memory_size_(memory_size), format_(format), valid_(true) {}
    
    bool is_valid() const override { return valid_; }
    uint64_t get_id() const override { return id_; }
    size_t get_memory_size() const override { return memory_size_; }
    TextureFormat get_format() const override { return format_; }
    
    void invalidate() { valid_ = false; }
};

// Test fixture for GPU Memory Optimizer tests
class GPUMemoryOptimizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_device_ = std::make_unique<MockGraphicsDevice>();
        
        // Setup default memory info
        MemoryInfo default_memory{};
        default_memory.total_memory = 4ULL * 1024 * 1024 * 1024; // 4GB
        default_memory.used_memory = 1ULL * 1024 * 1024 * 1024;  // 1GB used
        default_memory.available_memory = 3ULL * 1024 * 1024 * 1024; // 3GB available
        
        ON_CALL(*mock_device_, get_memory_info())
            .WillByDefault(::testing::Return(default_memory));
        
        // Create optimizer with test configuration
        GPUMemoryOptimizer::OptimizerConfig config;
        config.cache_config.max_cache_size = 512ULL * 1024 * 1024; // 512MB cache
        config.cache_config.enable_compression = true;
        config.cache_config.enable_prediction = true;
        config.enable_background_optimization = false; // Disable for deterministic tests
        
        optimizer_ = std::make_unique<GPUMemoryOptimizer>(mock_device_.get(), config);
    }
    
    void TearDown() override {
        optimizer_.reset();
        mock_device_.reset();
    }
    
    MockTextureHandle create_test_texture(uint64_t id, size_t size_mb = 1) {
        return MockTextureHandle(id, size_mb * 1024 * 1024);
    }
    
    std::unique_ptr<MockGraphicsDevice> mock_device_;
    std::unique_ptr<GPUMemoryOptimizer> optimizer_;
};

// ============================================================================
// Intelligent Cache Tests
// ============================================================================

TEST_F(GPUMemoryOptimizerTest, BasicCacheOperations) {
    // Test basic put/get operations
    uint64_t hash1 = 12345;
    uint64_t hash2 = 67890;
    
    auto texture1 = create_test_texture(1, 10); // 10MB texture
    auto texture2 = create_test_texture(2, 5);  // 5MB texture
    
    // Cache textures
    EXPECT_TRUE(optimizer_->cache_texture(hash1, texture1, 1.0f));
    EXPECT_TRUE(optimizer_->cache_texture(hash2, texture2, 0.8f));
    
    // Retrieve textures
    auto retrieved1 = optimizer_->get_texture(hash1);
    auto retrieved2 = optimizer_->get_texture(hash2);
    
    EXPECT_TRUE(retrieved1.is_valid());
    EXPECT_TRUE(retrieved2.is_valid());
    EXPECT_EQ(retrieved1.get_id(), 1);
    EXPECT_EQ(retrieved2.get_id(), 2);
    
    // Test cache miss
    auto missing = optimizer_->get_texture(99999);
    EXPECT_FALSE(missing.is_valid());
}

TEST_F(GPUMemoryOptimizerTest, CacheEvictionBySize) {
    // Fill cache beyond capacity to trigger eviction
    std::vector<MockTextureHandle> textures;
    std::vector<uint64_t> hashes;
    
    // Create textures that exceed cache size (512MB total)
    for (int i = 0; i < 10; ++i) {
        uint64_t hash = 1000 + i;
        auto texture = create_test_texture(i, 100); // 100MB each
        
        textures.push_back(texture);
        hashes.push_back(hash);
        
        bool cached = optimizer_->cache_texture(hash, texture, float(i) / 10.0f);
        EXPECT_TRUE(cached);
    }
    
    // Some textures should have been evicted
    int cached_count = 0;
    for (size_t i = 0; i < hashes.size(); ++i) {
        auto retrieved = optimizer_->get_texture(hashes[i]);
        if (retrieved.is_valid()) {
            cached_count++;
        }
    }
    
    // Should have fewer than 10 textures cached (due to size limit)
    EXPECT_LT(cached_count, 10);
    EXPECT_GT(cached_count, 0); // But some should still be cached
}

TEST_F(GPUMemoryOptimizerTest, CacheEvictionByQuality) {
    // Test that low-quality textures are evicted first
    auto high_quality_texture = create_test_texture(1, 100);
    auto low_quality_texture = create_test_texture(2, 100);
    
    uint64_t high_quality_hash = 1001;
    uint64_t low_quality_hash = 1002;
    
    // Cache textures with different quality scores
    EXPECT_TRUE(optimizer_->cache_texture(high_quality_hash, high_quality_texture, 1.0f));
    EXPECT_TRUE(optimizer_->cache_texture(low_quality_hash, low_quality_texture, 0.1f));
    
    // Add more textures to trigger eviction
    for (int i = 0; i < 5; ++i) {
        auto texture = create_test_texture(100 + i, 80);
        optimizer_->cache_texture(2000 + i, texture, 0.8f);
    }
    
    // High quality texture should still be cached
    auto high_quality_retrieved = optimizer_->get_texture(high_quality_hash);
    EXPECT_TRUE(high_quality_retrieved.is_valid());
    
    // Low quality texture may have been evicted
    auto low_quality_retrieved = optimizer_->get_texture(low_quality_hash);
    // Note: This might still be cached depending on implementation details
}

TEST_F(GPUMemoryOptimizerTest, FrameAccessPatternDetection) {
    // Test sequential access pattern detection
    for (uint32_t frame = 100; frame < 120; ++frame) {
        optimizer_->notify_frame_change(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Access pattern should be detected as sequential
    // Note: Would need access to internal state to verify this
    // In a real implementation, could expose get_access_pattern() method
    
    // Test random access pattern
    std::vector<uint32_t> random_frames = {500, 200, 800, 150, 600, 300};
    for (uint32_t frame : random_frames) {
        optimizer_->notify_frame_change(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ============================================================================
// VRAM Monitoring Tests
// ============================================================================

TEST_F(GPUMemoryOptimizerTest, VRAMMonitoringBasic) {
    // Test basic VRAM monitoring
    auto stats = optimizer_->get_memory_statistics();
    
    // Should have initial statistics
    EXPECT_GT(stats.total_vram, 0);
    EXPECT_GE(stats.available_vram, 0);
    EXPECT_LE(stats.used_vram, stats.total_vram);
}

TEST_F(GPUMemoryOptimizerTest, MemoryPressureHandling) {
    // Simulate high memory usage
    MemoryInfo high_usage_memory{};
    high_usage_memory.total_memory = 4ULL * 1024 * 1024 * 1024; // 4GB
    high_usage_memory.used_memory = 3.8ULL * 1024 * 1024 * 1024; // 3.8GB used (95%)
    high_usage_memory.available_memory = 0.2ULL * 1024 * 1024 * 1024; // 0.2GB available
    
    EXPECT_CALL(*mock_device_, get_memory_info())
        .WillRepeatedly(::testing::Return(high_usage_memory));
    
    // Cache some textures
    for (int i = 0; i < 5; ++i) {
        auto texture = create_test_texture(i, 10);
        optimizer_->cache_texture(1000 + i, texture, 0.5f);
    }
    
    // Request memory that should trigger cleanup
    bool memory_available = optimizer_->ensure_memory_available(100ULL * 1024 * 1024);
    
    // Should handle memory pressure (exact behavior depends on implementation)
    // At minimum, should not crash
}

// ============================================================================
// Streaming Optimizer Tests
// ============================================================================

class StreamingOptimizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_device_ = std::make_unique<MockGraphicsDevice>();
        
        MemoryInfo memory_info{};
        memory_info.total_memory = 8ULL * 1024 * 1024 * 1024; // 8GB
        memory_info.used_memory = 2ULL * 1024 * 1024 * 1024;  // 2GB
        memory_info.available_memory = 6ULL * 1024 * 1024 * 1024; // 6GB
        
        ON_CALL(*mock_device_, get_memory_info())
            .WillByDefault(::testing::Return(memory_info));
        
        IntelligentCache::Config cache_config;
        cache_config.max_cache_size = 1ULL * 1024 * 1024 * 1024; // 1GB
        cache_ = std::make_unique<IntelligentCache>(cache_config);
        
        StreamingOptimizer::StreamingConfig streaming_config;
        streaming_config.read_ahead_frames = 10;
        streaming_config.max_concurrent_loads = 2;
        streaming_ = std::make_unique<StreamingOptimizer>(
            cache_.get(), mock_device_.get(), streaming_config);
    }
    
    void TearDown() override {
        streaming_.reset();
        cache_.reset();
        mock_device_.reset();
    }
    
    std::unique_ptr<MockGraphicsDevice> mock_device_;
    std::unique_ptr<IntelligentCache> cache_;
    std::unique_ptr<StreamingOptimizer> streaming_;
};

TEST_F(StreamingOptimizerTest, BasicStreamingOperations) {
    // Start streaming from frame 100
    streaming_->start_streaming(100);
    
    // Let streaming run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check streaming statistics
    auto stats = streaming_->get_statistics();
    EXPECT_GT(stats.frames_streamed, 0);
    
    // Test seeking
    streaming_->seek_to_frame(200);
    
    // Test playback speed adjustment
    streaming_->set_playback_speed(2.0f);
    
    // Stop streaming
    streaming_->stop_streaming();
}

TEST_F(StreamingOptimizerTest, BufferManagement) {
    streaming_->start_streaming(0);
    
    // Let streaming build buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Analyze access patterns
    streaming_->analyze_access_patterns();
    
    // Adjust cache dynamically
    streaming_->adjust_cache_size_dynamically();
    
    // Check buffer health
    bool is_healthy = streaming_->is_buffer_healthy();
    // Should not crash, exact result depends on implementation
    
    streaming_->stop_streaming();
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(GPUMemoryOptimizerTest, FullWorkflowSimulation) {
    // Simulate a complete video editing workflow
    
    // 1. Sequential playback
    for (uint32_t frame = 0; frame < 100; ++frame) {
        optimizer_->notify_frame_change(frame);
        
        // Simulate texture requests for this frame
        uint64_t hash = std::hash<std::string>{}("frame_" + std::to_string(frame));
        auto texture = optimizer_->get_texture(hash);
        
        if (!texture.is_valid()) {
            // Cache miss - create and cache texture
            auto new_texture = create_test_texture(frame, 8); // 8MB per frame
            optimizer_->cache_texture(hash, new_texture, 1.0f);
        }
        
        // Small delay to simulate real-time playback
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 2. Random seeking (scrubbing)
    std::vector<uint32_t> seek_frames = {150, 75, 200, 50, 300, 25};
    for (uint32_t frame : seek_frames) {
        optimizer_->notify_frame_change(frame);
        
        uint64_t hash = std::hash<std::string>{}("frame_" + std::to_string(frame));
        auto texture = optimizer_->get_texture(hash);
        
        if (!texture.is_valid()) {
            auto new_texture = create_test_texture(frame, 8);
            optimizer_->cache_texture(hash, new_texture, 1.0f);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // 3. Check final statistics
    auto stats = optimizer_->get_memory_statistics();
    EXPECT_GT(stats.cache_hits + stats.cache_misses, 0);
    EXPECT_LE(stats.used_vram, stats.total_vram);
    
    // Hit ratio should be reasonable for this access pattern
    EXPECT_GT(stats.hit_ratio, 0.0f);
    EXPECT_LE(stats.hit_ratio, 1.0f);
}

TEST_F(GPUMemoryOptimizerTest, MemoryLeakPrevention) {
    // Test that memory is properly freed when cache is destroyed
    size_t initial_memory = 0;
    
    {
        // Create a scope-limited optimizer
        GPUMemoryOptimizer::OptimizerConfig config;
        config.cache_config.max_cache_size = 100ULL * 1024 * 1024; // 100MB
        config.enable_background_optimization = false;
        
        auto scoped_optimizer = std::make_unique<GPUMemoryOptimizer>(mock_device_.get(), config);
        
        // Fill with textures
        for (int i = 0; i < 10; ++i) {
            auto texture = create_test_texture(i, 5); // 5MB each
            scoped_optimizer->cache_texture(2000 + i, texture, 1.0f);
        }
        
        auto stats = scoped_optimizer->get_memory_statistics();
        initial_memory = stats.used_vram;
        EXPECT_GT(initial_memory, 0);
        
        // Optimizer goes out of scope here
    }
    
    // Memory should be cleaned up (in a real implementation with actual GPU resources)
    // For this test, we just verify the optimizer can be destroyed without issues
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(GPUMemoryOptimizerTest, CachePerformance) {
    const int NUM_OPERATIONS = 10000;
    const int NUM_UNIQUE_HASHES = 1000;
    
    // Pre-populate cache
    std::vector<uint64_t> hashes;
    for (int i = 0; i < NUM_UNIQUE_HASHES; ++i) {
        uint64_t hash = 5000 + i;
        auto texture = create_test_texture(i, 1); // 1MB each
        hashes.push_back(hash);
        optimizer_->cache_texture(hash, texture, 1.0f);
    }
    
    // Measure cache lookup performance
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, NUM_UNIQUE_HASHES - 1);
    
    int hits = 0;
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        uint64_t hash = hashes[dis(gen)];
        auto texture = optimizer_->get_texture(hash);
        if (texture.is_valid()) {
            hits++;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    
    // Performance expectations
    double operations_per_second = (NUM_OPERATIONS * 1000000.0) / duration;
    
    // Should handle at least 100K lookups per second
    EXPECT_GT(operations_per_second, 100000.0);
    
    // Should have a good hit ratio
    double hit_ratio = double(hits) / NUM_OPERATIONS;
    EXPECT_GT(hit_ratio, 0.8); // At least 80% hit ratio
    
    std::cout << "Cache Performance: " << operations_per_second 
              << " lookups/sec, Hit ratio: " << hit_ratio << std::endl;
}

} // namespace video_editor::gfx::test

// Test main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
