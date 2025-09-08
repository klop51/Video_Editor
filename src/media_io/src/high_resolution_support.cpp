#include "media_io/high_resolution_support.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <iostream>

namespace ve {
namespace media_io {

// Professional resolutions database
const std::vector<Resolution> PROFESSIONAL_RESOLUTIONS = {
    // Standard Definition
    {720, 576, "PAL SD", "PAL standard definition"},
    {720, 480, "NTSC SD", "NTSC standard definition"},
    
    // High Definition
    {1280, 720, "HD 720p", "High definition 720p"},
    {1920, 1080, "Full HD", "Full high definition 1080p"},
    
    // Ultra High Definition 4K
    {3840, 2160, "UHD 4K", "Ultra high definition 4K"},
    {4096, 2160, "DCI 4K", "Digital Cinema Initiative 4K"},
    
    // 6K and Intermediate Resolutions
    {6144, 3456, "RED 6K", "RED camera 6K resolution"},
    {5120, 2700, "5K UW", "5K ultra-wide resolution"},
    {4480, 2520, "4.5K", "Intermediate 4.5K resolution"},
    
    // Ultra High Definition 8K
    {7680, 4320, "UHD 8K", "Ultra high definition 8K"},
    {8192, 4320, "DCI 8K", "Digital Cinema Initiative 8K"},
    
    // Ultra-wide and Special Formats
    {3440, 1440, "UW QHD", "Ultra-wide quad HD"},
    {5120, 1440, "UW 5K", "Ultra-wide 5K"},
    {6880, 2880, "UW 6K", "Ultra-wide 6K"},
    
    // High Frame Rate Variants
    {2048, 1080, "DCI 2K", "Digital Cinema 2K"},
    {1998, 1080, "2K Scope", "2K anamorphic scope"},
    
    // Streaming and Web Formats
    {1366, 768, "WXGA", "Wide XGA resolution"},
    {2560, 1440, "QHD", "Quad HD resolution"},
    {3200, 1800, "QHD+", "Quad HD Plus"}
};

HighResolutionSupport::HighResolutionSupport() 
    : eight_k_optimizations_enabled_(false) {
    
    initializeProfessionalResolutions();
    initializeCinemaResolutions();
    initializeBroadcastResolutions();
    initializeStreamingResolutions();
    initializeUltraWideResolutions();
    updateSystemCapabilities();
}

std::vector<Resolution> HighResolutionSupport::getSupportedResolutions() const {
    return professional_resolutions_;
}

bool HighResolutionSupport::isResolutionSupported(uint32_t width, uint32_t height) const {
    // Check against known professional resolutions
    for (const auto& res : professional_resolutions_) {
        if (res.width == width && res.height == height) {
            return true;
        }
    }
    
    // Check if resolution is within reasonable bounds
    if (width < 128 || height < 96) return false;  // Too small
    if (width > 16384 || height > 8640) return false;  // Beyond 16K
    
    // Check memory requirements
    uint64_t total_pixels = static_cast<uint64_t>(width) * height;
    if (total_pixels > DCI_8K_PIXELS * 2) return false;  // Beyond reasonable limits
    
    return validateMemoryAvailability(Resolution(width, height, "Custom"));
}

ResolutionCategory HighResolutionSupport::categorizeResolution(uint32_t width, uint32_t height) const {
    uint64_t total_pixels = static_cast<uint64_t>(width) * height;
    double aspect_ratio = static_cast<double>(width) / height;
    
    // Check for ultra-wide first
    if (aspect_ratio > 2.1) {
        return ResolutionCategory::ULTRA_WIDE;
    }
    
    // Check for cinema formats
    if ((width == 4096 && height == 2160) || (width == 8192 && height == 4320) || 
        (width == 2048 && height == 1080)) {
        return width >= 8192 ? ResolutionCategory::DCI_8K : 
               width >= 4096 ? ResolutionCategory::DCI_4K : ResolutionCategory::HD;
    }
    
    // Classify by pixel count
    if (total_pixels >= UHD_8K_PIXELS) {
        return ResolutionCategory::UHD_8K;
    } else if (total_pixels >= UHD_4K_PIXELS) {
        return ResolutionCategory::UHD_4K;
    } else if (total_pixels >= 1920 * 1080) {
        return ResolutionCategory::HD;
    } else {
        return ResolutionCategory::SD;
    }
}

Resolution HighResolutionSupport::getResolutionInfo(uint32_t width, uint32_t height) const {
    // Check for exact match in professional resolutions
    for (const auto& res : professional_resolutions_) {
        if (res.width == width && res.height == height) {
            return res;
        }
    }
    
    // Create custom resolution info
    std::string name = std::to_string(width) + "×" + std::to_string(height);
    std::string description = "Custom " + name + " resolution";
    
    return Resolution(width, height, name, description);
}

MemoryStrategy HighResolutionSupport::getOptimalMemoryStrategy(const Resolution& resolution) const {
    uint64_t pixels = resolution.total_pixels;
    [[maybe_unused]] size_t memory_req_gb = calculateMemoryRequirement(resolution, "YUV420P") / (1024 * 1024 * 1024);
    
    if (pixels >= UHD_8K_PIXELS) {
        // 8K requires streaming for most systems
        return system_capabilities_.total_ram_gb >= 64 ? MemoryStrategy::HYBRID : MemoryStrategy::STREAMING;
    } else if (pixels >= UHD_4K_PIXELS) {
        // 4K can use standard or streaming based on available memory
        return system_capabilities_.available_ram_gb >= 16 ? MemoryStrategy::STANDARD : MemoryStrategy::STREAMING;
    } else {
        return MemoryStrategy::STANDARD;
    }
}

size_t HighResolutionSupport::calculateMemoryRequirement(const Resolution& resolution, const std::string& pixel_format) const {
    uint32_t width = resolution.width;
    uint32_t height = resolution.height;
    
    // Calculate memory for different pixel formats
    if (pixel_format == "YUV420P" || pixel_format == "YUV420P10LE") {
        return calculateYUV420MemorySize(width, height);
    } else if (pixel_format == "YUV422P" || pixel_format == "YUV422P10LE") {
        return calculateYUV422MemorySize(width, height);
    } else if (pixel_format == "YUV444P" || pixel_format == "YUV444P10LE") {
        return calculateYUV444MemorySize(width, height);
    } else if (pixel_format == "RGB24") {
        return calculateRGBMemorySize(width, height, 8);
    } else if (pixel_format == "RGB48LE") {
        return calculateRGBMemorySize(width, height, 16);
    } else {
        // Default to YUV420P
        return calculateYUV420MemorySize(width, height);
    }
}

size_t HighResolutionSupport::getRecommendedCacheSize(const Resolution& resolution) const {
    size_t frame_size = calculateMemoryRequirement(resolution, "YUV420P");
    uint32_t cache_frames = 10;  // Default cache size
    
    if (resolution.total_pixels >= UHD_8K_PIXELS) {
        cache_frames = 3;  // Smaller cache for 8K
    } else if (resolution.total_pixels >= UHD_4K_PIXELS) {
        cache_frames = 5;  // Medium cache for 4K
    }
    
    return frame_size * cache_frames;
}

bool HighResolutionSupport::validateMemoryAvailability(const Resolution& resolution) const {
    size_t required_memory = calculateMemoryRequirement(resolution, "YUV420P");
    size_t cache_memory = getRecommendedCacheSize(resolution);
    size_t total_required = required_memory + cache_memory;
    
    // Convert to GB for comparison
    size_t required_gb = total_required / (1024 * 1024 * 1024);
    
    return required_gb <= system_capabilities_.available_ram_gb;
}

PerformanceTier HighResolutionSupport::assessPerformance(const Resolution& resolution) const {
    uint64_t pixels = resolution.total_pixels;
    size_t memory_req_gb = calculateMemoryRequirement(resolution, "YUV420P") / (1024 * 1024 * 1024);
    
    return classifyPerformanceTier(pixels, memory_req_gb);
}

uint32_t HighResolutionSupport::getMaxFrameRate(const Resolution& resolution) const {
    PerformanceTier tier = assessPerformance(resolution);
    
    switch (tier) {
        case PerformanceTier::REALTIME:
            return 60;
        case PerformanceTier::NEAR_REALTIME:
            return 30;
        case PerformanceTier::PREVIEW_QUALITY:
            return 24;
        case PerformanceTier::OFFLINE_ONLY:
            return 1;
        default:
            return 0;
    }
}

bool HighResolutionSupport::requiresGPUAcceleration(const Resolution& resolution) const {
    return resolution.total_pixels >= UHD_4K_PIXELS;
}

double HighResolutionSupport::estimateDecodeLatency(const Resolution& resolution) const {
    // Estimate decode latency in milliseconds
    double base_latency = 16.67;  // 60fps = 16.67ms per frame
    double pixel_factor = static_cast<double>(resolution.total_pixels) / UHD_4K_PIXELS;
    
    return base_latency * pixel_factor;
}

HighResolutionSupport::OptimizationRecommendation HighResolutionSupport::getOptimizationRecommendation(const Resolution& resolution) const {
    OptimizationRecommendation rec;
    
    bool is_8k = resolution.total_pixels >= UHD_8K_PIXELS;
    bool is_4k = resolution.total_pixels >= UHD_4K_PIXELS;
    
    rec.use_hardware_decode = is_4k || is_8k;
    rec.enable_streaming_mode = is_8k || (is_4k && system_capabilities_.available_ram_gb < 16);
    rec.use_tiled_processing = is_8k;
    rec.enable_gpu_memory = is_8k && system_capabilities_.gpu_memory_gb >= 8;
    rec.thread_count = calculateOptimalThreadCount(resolution);
    rec.cache_size_mb = static_cast<uint32_t>(getRecommendedCacheSize(resolution) / (1024 * 1024));
    
    // Generate reasoning
    rec.reasoning = "For " + resolution.name + " resolution: ";
    if (is_8k) {
        rec.reasoning += "8K requires streaming decode, tiled processing, and GPU acceleration for optimal performance.";
    } else if (is_4k) {
        rec.reasoning += "4K benefits from hardware decode and GPU acceleration for smooth playback.";
    } else {
        rec.reasoning += "Standard CPU decode sufficient for this resolution.";
    }
    
    return rec;
}

HighResolutionSupport::EightKCapabilities HighResolutionSupport::assess8KCapabilities() const {
    EightKCapabilities caps;
    
    caps.decode_supported = system_capabilities_.total_ram_gb >= 32;
    caps.realtime_playback = system_capabilities_.total_ram_gb >= 64 && 
                            system_capabilities_.gpu_memory_gb >= 16 &&
                            system_capabilities_.hardware_decode_available;
    caps.hardware_acceleration = system_capabilities_.hardware_decode_available;
    caps.streaming_decode = true;  // Always recommended for 8K
    caps.gpu_memory_required = system_capabilities_.gpu_memory_gb >= 8;
    caps.max_framerate = caps.realtime_playback ? 30 : (caps.decode_supported ? 24 : 0);
    caps.memory_requirement_gb = 24;  // Minimum for 8K workflow
    
    caps.supported_codecs = {"HEVC", "H.264", "ProRes", "DNxHR"};
    if (caps.hardware_acceleration) {
        caps.supported_codecs.push_back("AV1");
    }
    
    return caps;
}

bool HighResolutionSupport::enable8KOptimizations() {
    if (system_capabilities_.total_ram_gb < 32) {
        return false;  // Insufficient memory for 8K
    }
    
    eight_k_optimizations_enabled_ = true;
    configure8KPipeline();
    return true;
}

void HighResolutionSupport::configure8KPipeline() {
    if (!eight_k_optimizations_enabled_) return;
    
    // Configure system for 8K processing
    std::cout << "Configuring 8K pipeline optimizations..." << std::endl;
    std::cout << "- Enabling streaming decode" << std::endl;
    std::cout << "- Configuring tiled processing" << std::endl;
    std::cout << "- Optimizing memory pools" << std::endl;
    
    if (system_capabilities_.gpu_memory_gb >= 8) {
        std::cout << "- Enabling GPU memory streaming" << std::endl;
    }
}

Resolution HighResolutionSupport::getDownscaledResolution(const Resolution& source, double scale_factor) const {
    uint32_t new_width = static_cast<uint32_t>(source.width * scale_factor);
    uint32_t new_height = static_cast<uint32_t>(source.height * scale_factor);
    
    // Ensure even dimensions for video compatibility
    new_width = (new_width / 2) * 2;
    new_height = (new_height / 2) * 2;
    
    std::string name = "Scaled " + source.name + " (" + std::to_string(static_cast<int>(scale_factor * 100)) + "%)";
    
    return Resolution(new_width, new_height, name, "Downscaled from " + source.name);
}

std::vector<Resolution> HighResolutionSupport::getPreviewResolutions(const Resolution& source) const {
    std::vector<Resolution> previews;
    
    // Generate common preview scales
    std::vector<double> scales = {1.0, 0.5, 0.25, 0.125};
    
    for (double scale : scales) {
        if (scale < 1.0 || source.total_pixels <= UHD_4K_PIXELS) {
            previews.push_back(getDownscaledResolution(source, scale));
        }
    }
    
    return previews;
}

bool HighResolutionSupport::isValidAspectRatio(double aspect_ratio) const {
    // Common professional aspect ratios
    std::vector<double> valid_ratios = {
        16.0/9.0,   // 1.778 - Standard HD/UHD
        17.0/9.0,   // 1.889 - Ultra-wide
        21.0/9.0,   // 2.333 - Cinema ultra-wide
        4.0/3.0,    // 1.333 - Classic 4:3
        3.0/2.0,    // 1.5   - Photography
        2.35,       // Anamorphic
        2.39,       // Cinema scope
        1.85        // Cinema flat
    };
    
    const double tolerance = 0.01;
    for (double ratio : valid_ratios) {
        if (std::abs(aspect_ratio - ratio) < tolerance) {
            return true;
        }
    }
    
    return false;
}

std::vector<Resolution> HighResolutionSupport::getCinemaResolutions() const {
    return cinema_resolutions_;
}

std::vector<Resolution> HighResolutionSupport::getBroadcastResolutions() const {
    return broadcast_resolutions_;
}

std::vector<Resolution> HighResolutionSupport::getStreamingResolutions() const {
    return streaming_resolutions_;
}

std::vector<Resolution> HighResolutionSupport::getUltraWideResolutions() const {
    return ultra_wide_resolutions_;
}

HighResolutionSupport::SystemCapabilities HighResolutionSupport::getSystemCapabilities() const {
    return system_capabilities_;
}

void HighResolutionSupport::updateSystemCapabilities() {
    probeSystemMemory();
    system_capabilities_.hardware_decode_available = detectHardwareDecodeSupport();
    system_capabilities_.gpu_compute_available = detectGPUComputeSupport();
    system_capabilities_.cpu_cores = std::thread::hardware_concurrency();
    
    // Mock GPU memory detection (would be platform-specific in real implementation)
    system_capabilities_.gpu_memory_gb = 8;  // Assume modern GPU
    
    system_capabilities_.supported_apis = {"DirectX 11", "OpenGL", "Vulkan"};
}

HighResolutionSupport::ResolutionStats HighResolutionSupport::getStatistics() const {
    ResolutionStats stats;
    
    stats.total_resolutions_supported = static_cast<uint32_t>(professional_resolutions_.size());
    stats.uhd_4k_count = 0;
    stats.uhd_8k_count = 0;
    stats.cinema_count = 0;
    stats.ultra_wide_count = 0;
    stats.max_memory_requirement_gb = 0;
    stats.max_supported_framerate = 0;
    
    for (const auto& res : professional_resolutions_) {
        if (res.total_pixels >= UHD_8K_PIXELS) {
            stats.uhd_8k_count++;
        } else if (res.total_pixels >= UHD_4K_PIXELS) {
            stats.uhd_4k_count++;
        }
        
        if (res.is_cinema_format) {
            stats.cinema_count++;
        }
        
        if (res.is_ultra_wide) {
            stats.ultra_wide_count++;
        }
        
        size_t memory_gb = calculateMemoryRequirement(res, "YUV420P") / (1024 * 1024 * 1024);
        stats.max_memory_requirement_gb = std::max(stats.max_memory_requirement_gb, memory_gb);
        
        uint32_t max_fps = getMaxFrameRate(res);
        stats.max_supported_framerate = std::max(stats.max_supported_framerate, max_fps);
    }
    
    return stats;
}

// Private implementation methods

void HighResolutionSupport::initializeProfessionalResolutions() {
    professional_resolutions_ = PROFESSIONAL_RESOLUTIONS;
}

void HighResolutionSupport::initializeCinemaResolutions() {
    cinema_resolutions_ = {
        {2048, 1080, "DCI 2K", "Digital Cinema 2K"},
        {4096, 2160, "DCI 4K", "Digital Cinema 4K"},
        {8192, 4320, "DCI 8K", "Digital Cinema 8K"},
        {1998, 1080, "2K Scope", "2K Anamorphic Scope"}
    };
}

void HighResolutionSupport::initializeBroadcastResolutions() {
    broadcast_resolutions_ = {
        {720, 576, "PAL SD", "PAL Standard Definition"},
        {720, 480, "NTSC SD", "NTSC Standard Definition"},
        {1280, 720, "HD 720p", "Broadcast HD 720p"},
        {1920, 1080, "Full HD", "Broadcast Full HD"},
        {3840, 2160, "UHD 4K", "Broadcast UHD 4K"}
    };
}

void HighResolutionSupport::initializeStreamingResolutions() {
    streaming_resolutions_ = {
        {854, 480, "480p", "Streaming 480p"},
        {1280, 720, "720p", "Streaming 720p"},
        {1920, 1080, "1080p", "Streaming 1080p"},
        {2560, 1440, "1440p", "Streaming 1440p"},
        {3840, 2160, "4K", "Streaming 4K"}
    };
}

void HighResolutionSupport::initializeUltraWideResolutions() {
    ultra_wide_resolutions_ = {
        {2560, 1080, "UW FHD", "Ultra-wide Full HD"},
        {3440, 1440, "UW QHD", "Ultra-wide Quad HD"},
        {3840, 1600, "UW 4K", "Ultra-wide 4K"},
        {5120, 1440, "UW 5K", "Ultra-wide 5K"},
        {5120, 2160, "UW Cinema", "Ultra-wide Cinema"}
    };
}

size_t HighResolutionSupport::calculateYUV420MemorySize(uint32_t width, uint32_t height) const {
    // YUV420: Y plane + U plane (1/4) + V plane (1/4) = 1.5 * width * height
    return static_cast<size_t>(width) * height * 3 / 2;
}

size_t HighResolutionSupport::calculateYUV422MemorySize(uint32_t width, uint32_t height) const {
    // YUV422: Y plane + U plane (1/2) + V plane (1/2) = 2 * width * height
    return static_cast<size_t>(width) * height * 2;
}

size_t HighResolutionSupport::calculateYUV444MemorySize(uint32_t width, uint32_t height) const {
    // YUV444: Y plane + U plane + V plane = 3 * width * height
    return static_cast<size_t>(width) * height * 3;
}

size_t HighResolutionSupport::calculateRGBMemorySize(uint32_t width, uint32_t height, uint32_t bit_depth) const {
    size_t bytes_per_component = (bit_depth + 7) / 8;  // Round up to nearest byte
    return static_cast<size_t>(width) * height * 3 * bytes_per_component;
}

PerformanceTier HighResolutionSupport::classifyPerformanceTier(uint64_t total_pixels, size_t memory_requirement_gb [[maybe_unused]]) const {
    if (total_pixels >= UHD_8K_PIXELS) {
        // 8K classification
        if (system_capabilities_.total_ram_gb >= 64 && system_capabilities_.hardware_decode_available) {
            return PerformanceTier::NEAR_REALTIME;
        } else if (system_capabilities_.total_ram_gb >= 32) {
            return PerformanceTier::PREVIEW_QUALITY;
        } else {
            return PerformanceTier::OFFLINE_ONLY;
        }
    } else if (total_pixels >= UHD_4K_PIXELS) {
        // 4K classification
        if (system_capabilities_.total_ram_gb >= 16 && system_capabilities_.hardware_decode_available) {
            return PerformanceTier::REALTIME;
        } else if (system_capabilities_.total_ram_gb >= 8) {
            return PerformanceTier::NEAR_REALTIME;
        } else {
            return PerformanceTier::PREVIEW_QUALITY;
        }
    } else {
        // HD and below
        return PerformanceTier::REALTIME;
    }
}

uint32_t HighResolutionSupport::calculateOptimalThreadCount(const Resolution& resolution) const {
    uint32_t cpu_cores = system_capabilities_.cpu_cores;
    uint64_t pixels = resolution.total_pixels;
    
    if (pixels >= UHD_8K_PIXELS) {
        return std::min(cpu_cores, 16u);  // Use most cores for 8K
    } else if (pixels >= UHD_4K_PIXELS) {
        return std::min(cpu_cores, 8u);   // Use up to 8 cores for 4K
    } else {
        return std::min(cpu_cores, 4u);   // Use up to 4 cores for HD
    }
}

bool HighResolutionSupport::detectHardwareDecodeSupport() const {
    // Mock hardware detection (platform-specific in real implementation)
    return true;  // Assume modern hardware has hardware decode
}

bool HighResolutionSupport::detectGPUComputeSupport() const {
    // Mock GPU compute detection
    return true;  // Assume modern GPU has compute support
}

void HighResolutionSupport::probeSystemMemory() {
    // Mock memory detection (platform-specific in real implementation)
    system_capabilities_.total_ram_gb = 32;     // Assume 32GB total
    system_capabilities_.available_ram_gb = 24; // Assume 24GB available
}

// EightKStreamingManager implementation

EightKStreamingManager::EightKStreamingManager() 
    : is_initialized_(false) {
    // Initialize streaming stats
    stats_ = {};
}

EightKStreamingManager::~EightKStreamingManager() {
    releaseStreamingBuffers();
}

bool EightKStreamingManager::initializeStreaming(const Resolution& resolution) {
    if (resolution.total_pixels < HighResolutionSupport::UHD_8K_PIXELS) {
        return false;  // Not an 8K resolution
    }
    
    current_config_ = getOptimalStreamingConfig(resolution);
    configureStreamingPipeline(current_config_);
    
    // Allocate streaming buffers
    size_t buffer_size = resolution.width * resolution.height * 3 / 2;  // YUV420
    return allocateStreamingBuffers(buffer_size, current_config_.buffer_count);
}

EightKStreamingManager::StreamingConfig EightKStreamingManager::getOptimalStreamingConfig(const Resolution& resolution) const {
    StreamingConfig config;
    
    config.tile_size = calculateOptimalTileSize(resolution);
    config.buffer_count = 4;           // Quad buffering for smooth streaming
    config.prefetch_frames = 2;        // Prefetch 2 frames ahead
    config.use_compressed_cache = true; // Use compression to save memory
    config.enable_gpu_streaming = true; // Stream to GPU if available
    config.quality_factor = 0.9;       // High quality by default
    
    return config;
}

void EightKStreamingManager::configureStreamingPipeline(const StreamingConfig& config) {
    current_config_ = config;
    
    // Configure pipeline based on available resources
    if (config.enable_gpu_streaming) {
        optimizeForGPUProcessing();
    } else {
        optimizeForCPUProcessing();
    }
    
    is_initialized_ = true;
}

bool EightKStreamingManager::allocateStreamingBuffers(size_t buffer_size, size_t buffer_count) {
    releaseStreamingBuffers();  // Clean up any existing buffers
    
    try {
        memory_pool_.reserve(buffer_count);
        for (size_t i = 0; i < buffer_count; ++i) {
            memory_pool_.emplace_back(std::make_unique<uint8_t[]>(buffer_size));
        }
        return true;
    } catch (const std::bad_alloc&) {
        releaseStreamingBuffers();
        return false;
    }
}

void EightKStreamingManager::releaseStreamingBuffers() {
    memory_pool_.clear();
}

size_t EightKStreamingManager::getMemoryPoolUsage() const {
    return memory_pool_.size() * (current_config_.tile_size * current_config_.tile_size * 3 / 2);
}

size_t EightKStreamingManager::getMemoryPoolCapacity() const {
    return memory_pool_.capacity() * (current_config_.tile_size * current_config_.tile_size * 3 / 2);
}

std::vector<EightKStreamingManager::TileInfo> EightKStreamingManager::calculateTiles(const Resolution& resolution, uint32_t tile_size) const {
    std::vector<TileInfo> tiles;
    
    uint32_t tiles_x = (resolution.width + tile_size - 1) / tile_size;
    uint32_t tiles_y = (resolution.height + tile_size - 1) / tile_size;
    
    for (uint32_t y = 0; y < tiles_y; ++y) {
        for (uint32_t x = 0; x < tiles_x; ++x) {
            TileInfo tile;
            tile.tile_x = x * tile_size;
            tile.tile_y = y * tile_size;
            tile.tile_width = std::min(tile_size, resolution.width - tile.tile_x);
            tile.tile_height = std::min(tile_size, resolution.height - tile.tile_y);
            tile.tile_index = y * tiles_x + x;
            tile.is_boundary_tile = (x == tiles_x - 1) || (y == tiles_y - 1);
            
            tiles.push_back(tile);
        }
    }
    
    return tiles;
}

bool EightKStreamingManager::processTile(const TileInfo& tile [[maybe_unused]], const void* input_data [[maybe_unused]], void* output_data [[maybe_unused]]) {
    if (!is_initialized_) {
        return false;
    }
    
    // Mock tile processing (real implementation would process the tile data)
    stats_.frames_processed++;
    return true;
}

EightKStreamingManager::StreamingStats EightKStreamingManager::getStreamingStats() const {
    return stats_;
}

void EightKStreamingManager::resetStreamingStats() {
    stats_ = {};
}

void EightKStreamingManager::optimizeForCPUProcessing() {
    // Configure for CPU-based processing
    current_config_.tile_size = 512;  // Smaller tiles for CPU
    current_config_.buffer_count = 3; // Fewer buffers to save memory
}

void EightKStreamingManager::optimizeForGPUProcessing() {
    // Configure for GPU-based processing
    current_config_.tile_size = 1024; // Larger tiles for GPU efficiency
    current_config_.buffer_count = 4;  // More buffers for GPU pipeline
}

size_t EightKStreamingManager::calculateOptimalTileSize(const Resolution& resolution) const {
    // Calculate optimal tile size based on resolution and available memory
    if (resolution.total_pixels >= HighResolutionSupport::DCI_8K_PIXELS) {
        return 1024;  // 1K tiles for 8K+
    } else {
        return 512;   // 512px tiles for smaller resolutions
    }
}

// Utility functions implementation

namespace resolution_utils {

std::vector<Resolution> getAllProfessionalResolutions() {
    return PROFESSIONAL_RESOLUTIONS;
}

bool requires8KHandling(uint32_t width, uint32_t height) {
    uint64_t total_pixels = static_cast<uint64_t>(width) * height;
    return total_pixels >= HighResolutionSupport::UHD_8K_PIXELS;
}

size_t calculateMemoryRequirement(uint32_t width, uint32_t height, const std::string& pixel_format) {
    HighResolutionSupport support;
    Resolution res(width, height, "Custom");
    return support.calculateMemoryRequirement(res, pixel_format);
}

Resolution getPreviewResolution(uint32_t source_width, uint32_t source_height, PerformanceTier target_tier) {
    double scale_factor = 1.0;
    
    switch (target_tier) {
        case PerformanceTier::REALTIME:
            scale_factor = 1.0;
            break;
        case PerformanceTier::NEAR_REALTIME:
            scale_factor = 0.75;
            break;
        case PerformanceTier::PREVIEW_QUALITY:
            scale_factor = 0.5;
            break;
        case PerformanceTier::OFFLINE_ONLY:
            scale_factor = 0.25;
            break;
        default:
            scale_factor = 0.5;
    }
    
    uint32_t new_width = static_cast<uint32_t>(source_width * scale_factor);
    uint32_t new_height = static_cast<uint32_t>(source_height * scale_factor);
    
    // Ensure even dimensions
    new_width = (new_width / 2) * 2;
    new_height = (new_height / 2) * 2;
    
    return Resolution(new_width, new_height, "Preview");
}

bool validateSystemCapability(uint32_t width, uint32_t height, uint32_t framerate) {
    HighResolutionSupport support;
    Resolution res(width, height, "Test");
    
    uint32_t max_fps = support.getMaxFrameRate(res);
    return framerate <= max_fps && support.validateMemoryAvailability(res);
}

std::string getOptimizationAdvice(uint32_t width, uint32_t height) {
    HighResolutionSupport support;
    Resolution res(width, height, "Advisory");
    
    auto recommendation = support.getOptimizationRecommendation(res);
    return recommendation.reasoning;
}

Resolution convertToCinemaStandard(const Resolution& broadcast_resolution) {
    // Convert broadcast to cinema equivalent
    if (broadcast_resolution.width == 3840 && broadcast_resolution.height == 2160) {
        return Resolution(4096, 2160, "DCI 4K", "Cinema 4K equivalent");
    } else if (broadcast_resolution.width == 1920 && broadcast_resolution.height == 1080) {
        return Resolution(2048, 1080, "DCI 2K", "Cinema 2K equivalent");
    }
    
    return broadcast_resolution;  // No cinema equivalent
}

Resolution convertToBroadcastStandard(const Resolution& cinema_resolution) {
    // Convert cinema to broadcast equivalent
    if (cinema_resolution.width == 4096 && cinema_resolution.height == 2160) {
        return Resolution(3840, 2160, "UHD 4K", "Broadcast 4K equivalent");
    } else if (cinema_resolution.width == 2048 && cinema_resolution.height == 1080) {
        return Resolution(1920, 1080, "Full HD", "Broadcast HD equivalent");
    }
    
    return cinema_resolution;  // No broadcast equivalent
}

bool isStandardResolution(uint32_t width, uint32_t height) {
    std::vector<std::pair<uint32_t, uint32_t>> standard_resolutions = {
        {1920, 1080}, {3840, 2160}, {7680, 4320}, {1280, 720}
    };
    
    for (const auto& res : standard_resolutions) {
        if (res.first == width && res.second == height) {
            return true;
        }
    }
    
    return false;
}

bool isCinemaResolution(uint32_t width, uint32_t height) {
    return (width == 2048 && height == 1080) ||
           (width == 4096 && height == 2160) ||
           (width == 8192 && height == 4320);
}

bool isBroadcastResolution(uint32_t width, uint32_t height) {
    return (width == 720 && (height == 480 || height == 576)) ||
           (width == 1280 && height == 720) ||
           (width == 1920 && height == 1080) ||
           (width == 3840 && height == 2160);
}

bool isUltraWideResolution(uint32_t width, uint32_t height) {
    double aspect_ratio = static_cast<double>(width) / height;
    return aspect_ratio > 2.1;
}

} // namespace resolution_utils

} // namespace media_io
} // namespace ve
