#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <map>
#include <array>
#include <unordered_map>

/**
 * @file high_resolution_support.hpp
 * @brief Phase 3 Week 9: 8K Support Infrastructure
 * 
 * Implements comprehensive high-resolution video support infrastructure for
 * professional video production workflows, including 8K, 6K, and ultra-wide
 * format handling with optimized memory management and performance.
 */

namespace ve {
namespace media_io {

/**
 * @brief Professional resolution definitions and capabilities
 */
struct Resolution {
    uint32_t width;
    uint32_t height;
    std::string name;
    std::string description;
    
    // Technical specifications
    uint64_t total_pixels;
    double aspect_ratio;
    bool is_cinema_format;
    bool is_ultra_wide;
    bool requires_special_handling;
    
    Resolution(uint32_t w, uint32_t h, const std::string& n, const std::string& desc = "")
        : width(w), height(h), name(n), description(desc)
        , total_pixels(static_cast<uint64_t>(w) * h)
        , aspect_ratio(static_cast<double>(w) / h)
        , is_cinema_format(false)
        , is_ultra_wide(aspect_ratio > 2.1)
        , requires_special_handling(total_pixels > 33177600) // > 4K UHD
    {
        // Classify cinema formats
        if ((width == 4096 && height == 2160) ||  // DCI 4K
            (width == 8192 && height == 4320) ||  // DCI 8K
            (width == 2048 && height == 1080)) {  // DCI 2K
            is_cinema_format = true;
        }
    }
    
    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }
};

/**
 * @brief Professional resolution categories and standards
 */
enum class ResolutionCategory {
    SD,              // Standard Definition (480p, 576p)
    HD,              // High Definition (720p, 1080p)
    UHD_4K,          // Ultra High Definition 4K (2160p)
    DCI_4K,          // Digital Cinema 4K
    UHD_8K,          // Ultra High Definition 8K (4320p)
    DCI_8K,          // Digital Cinema 8K
    CUSTOM_6K,       // RED 6K and similar
    ULTRA_WIDE,      // Ultra-wide formats
    HIGH_FRAMERATE,  // High frame rate variants
    CUSTOM           // Custom/non-standard resolutions
};

/**
 * @brief Memory optimization strategies for different resolution tiers
 */
enum class MemoryStrategy {
    STANDARD,        // Normal memory allocation for HD/4K
    STREAMING,       // Streaming decode for 8K+
    TILED,          // Tiled processing for very large frames
    COMPRESSED,      // Compressed memory formats
    HYBRID          // Adaptive strategy based on available resources
};

/**
 * @brief Performance tier classification for resolution handling
 */
enum class PerformanceTier {
    REALTIME,        // Real-time playback capability
    NEAR_REALTIME,   // Near real-time with minimal buffering
    PREVIEW_QUALITY, // Reduced quality for smooth preview
    OFFLINE_ONLY,    // Offline processing only
    UNSUPPORTED      // Resolution not supported on current hardware
};

/**
 * @brief High-resolution video support infrastructure
 */
class HighResolutionSupport {
public:
    HighResolutionSupport();
    ~HighResolutionSupport() = default;
    
    // Resolution management
    std::vector<Resolution> getSupportedResolutions() const;
    bool isResolutionSupported(uint32_t width, uint32_t height) const;
    ResolutionCategory categorizeResolution(uint32_t width, uint32_t height) const;
    Resolution getResolutionInfo(uint32_t width, uint32_t height) const;
    
    // Memory management strategies
    MemoryStrategy getOptimalMemoryStrategy(const Resolution& resolution) const;
    size_t calculateMemoryRequirement(const Resolution& resolution, const std::string& pixel_format) const;
    size_t getRecommendedCacheSize(const Resolution& resolution) const;
    bool validateMemoryAvailability(const Resolution& resolution) const;
    
    // Performance assessment
    PerformanceTier assessPerformance(const Resolution& resolution) const;
    uint32_t getMaxFrameRate(const Resolution& resolution) const;
    bool requiresGPUAcceleration(const Resolution& resolution) const;
    double estimateDecodeLatency(const Resolution& resolution) const;
    
    // Optimization recommendations
    struct OptimizationRecommendation {
        bool use_hardware_decode;
        bool enable_streaming_mode;
        bool use_tiled_processing;
        bool enable_gpu_memory;
        uint32_t thread_count;
        size_t cache_size_mb;
        std::string reasoning;
    };
    
    OptimizationRecommendation getOptimizationRecommendation(const Resolution& resolution) const;
    
    // 8K specific support
    struct EightKCapabilities {
        bool decode_supported;
        bool realtime_playback;
        bool hardware_acceleration;
        bool streaming_decode;
        bool gpu_memory_required;
        uint32_t max_framerate;
        size_t memory_requirement_gb;
        std::vector<std::string> supported_codecs;
    };
    
    EightKCapabilities assess8KCapabilities() const;
    bool enable8KOptimizations();
    void configure8KPipeline();
    
    // Resolution conversion utilities
    Resolution getDownscaledResolution(const Resolution& source, double scale_factor) const;
    std::vector<Resolution> getPreviewResolutions(const Resolution& source) const;
    bool isValidAspectRatio(double aspect_ratio) const;
    
    // Professional workflow support
    std::vector<Resolution> getCinemaResolutions() const;
    std::vector<Resolution> getBroadcastResolutions() const;
    std::vector<Resolution> getStreamingResolutions() const;
    std::vector<Resolution> getUltraWideResolutions() const;
    
    // System capabilities
    struct SystemCapabilities {
        uint64_t total_ram_gb;
        uint64_t available_ram_gb;
        uint64_t gpu_memory_gb;
        uint32_t cpu_cores;
        bool hardware_decode_available;
        bool gpu_compute_available;
        std::vector<std::string> supported_apis;
    };
    
    SystemCapabilities getSystemCapabilities() const;
    void updateSystemCapabilities();
    
    // Statistics and monitoring
    struct ResolutionStats {
        uint32_t total_resolutions_supported;
        uint32_t uhd_4k_count;
        uint32_t uhd_8k_count;
        uint32_t cinema_count;
        uint32_t ultra_wide_count;
        size_t max_memory_requirement_gb;
        uint32_t max_supported_framerate;
    };
    
    ResolutionStats getStatistics() const;
    
    // Public constants for resolution thresholds
    static constexpr uint64_t UHD_4K_PIXELS = 8294400;      // 3840×2160
    static constexpr uint64_t DCI_4K_PIXELS = 8847360;      // 4096×2160
    static constexpr uint64_t UHD_8K_PIXELS = 33177600;     // 7680×4320
    static constexpr uint64_t DCI_8K_PIXELS = 35389440;     // 8192×4320

private:
    // Resolution database
    std::vector<Resolution> professional_resolutions_;
    std::vector<Resolution> cinema_resolutions_;
    std::vector<Resolution> broadcast_resolutions_;
    std::vector<Resolution> streaming_resolutions_;
    std::vector<Resolution> ultra_wide_resolutions_;
    
    // System state
    SystemCapabilities system_capabilities_;
    bool eight_k_optimizations_enabled_;
    
    static constexpr size_t HIGH_MEMORY_THRESHOLD_GB = 16;
    static constexpr size_t EXTREME_MEMORY_THRESHOLD_GB = 64;
    
    // Helper methods
    void initializeProfessionalResolutions();
    void initializeCinemaResolutions();
    void initializeBroadcastResolutions();
    void initializeStreamingResolutions();
    void initializeUltraWideResolutions();
    
    size_t calculateYUV420MemorySize(uint32_t width, uint32_t height) const;
    size_t calculateYUV422MemorySize(uint32_t width, uint32_t height) const;
    size_t calculateYUV444MemorySize(uint32_t width, uint32_t height) const;
    size_t calculateRGBMemorySize(uint32_t width, uint32_t height, uint32_t bit_depth) const;
    
    PerformanceTier classifyPerformanceTier(uint64_t total_pixels, size_t memory_requirement) const;
    uint32_t calculateOptimalThreadCount(const Resolution& resolution) const;
    
    bool detectHardwareDecodeSupport() const;
    bool detectGPUComputeSupport() const;
    void probeSystemMemory();
};

/**
 * @brief 8K-specific memory management and streaming support
 */
class EightKStreamingManager {
public:
    EightKStreamingManager();
    ~EightKStreamingManager();
    
    // Streaming configuration
    struct StreamingConfig {
        size_t tile_size;           // Tile dimensions for processing
        size_t buffer_count;        // Number of buffers in pipeline
        size_t prefetch_frames;     // Frames to prefetch ahead
        bool use_compressed_cache;  // Use compressed frame cache
        bool enable_gpu_streaming;  // Stream directly to GPU memory
        double quality_factor;      // Quality vs performance trade-off
    };
    
    bool initializeStreaming(const Resolution& resolution);
    StreamingConfig getOptimalStreamingConfig(const Resolution& resolution) const;
    void configureStreamingPipeline(const StreamingConfig& config);
    
    // Memory pool management
    bool allocateStreamingBuffers(size_t buffer_size, size_t buffer_count);
    void releaseStreamingBuffers();
    size_t getMemoryPoolUsage() const;
    size_t getMemoryPoolCapacity() const;
    
    // Tile processing for 8K frames
    struct TileInfo {
        uint32_t tile_x, tile_y;
        uint32_t tile_width, tile_height;
        size_t tile_index;
        bool is_boundary_tile;
    };
    
    std::vector<TileInfo> calculateTiles(const Resolution& resolution, uint32_t tile_size) const;
    bool processTile(const TileInfo& tile, const void* input_data, void* output_data);
    
    // Performance monitoring
    struct StreamingStats {
        double average_decode_time_ms;
        double peak_memory_usage_gb;
        uint32_t frames_processed;
        uint32_t cache_hits;
        uint32_t cache_misses;
        double throughput_mbps;
    };
    
    StreamingStats getStreamingStats() const;
    void resetStreamingStats();
    
private:
    StreamingConfig current_config_;
    std::vector<std::unique_ptr<uint8_t[]>> memory_pool_;
    StreamingStats stats_;
    bool is_initialized_;
    
    void optimizeForCPUProcessing();
    void optimizeForGPUProcessing();
    size_t calculateOptimalTileSize(const Resolution& resolution) const;
};

/**
 * @brief Utility functions for high-resolution support
 */
namespace resolution_utils {

/**
 * @brief Get all professional resolutions supported by the system
 */
std::vector<Resolution> getAllProfessionalResolutions();

/**
 * @brief Check if a resolution requires special 8K handling
 */
bool requires8KHandling(uint32_t width, uint32_t height);

/**
 * @brief Calculate memory requirements for different pixel formats
 */
size_t calculateMemoryRequirement(uint32_t width, uint32_t height, const std::string& pixel_format);

/**
 * @brief Get recommended preview resolution for smooth playback
 */
Resolution getPreviewResolution(uint32_t source_width, uint32_t source_height, PerformanceTier target_tier);

/**
 * @brief Validate if system can handle the specified resolution
 */
bool validateSystemCapability(uint32_t width, uint32_t height, uint32_t framerate);

/**
 * @brief Get optimization recommendations for specific resolution
 */
std::string getOptimizationAdvice(uint32_t width, uint32_t height);

/**
 * @brief Convert between different resolution standards
 */
Resolution convertToCinemaStandard(const Resolution& broadcast_resolution);
Resolution convertToBroadcastStandard(const Resolution& cinema_resolution);

/**
 * @brief Professional resolution validation
 */
bool isStandardResolution(uint32_t width, uint32_t height);
bool isCinemaResolution(uint32_t width, uint32_t height);
bool isBroadcastResolution(uint32_t width, uint32_t height);
bool isUltraWideResolution(uint32_t width, uint32_t height);

} // namespace resolution_utils

} // namespace media_io
} // namespace ve
