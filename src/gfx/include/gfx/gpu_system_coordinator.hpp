// Week 8: GPU System Coordinator
// Master controller that orchestrates all GPU systems for optimal performance

#pragma once

#include "gfx/streaming_uploader.hpp"
#include "gfx/gpu_memory_manager.hpp"
#include "gfx/async_renderer.hpp"
#include "gfx/performance_monitor.hpp"
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

namespace ve::gfx {

// Forward declarations
class GraphicsDevice;
using TextureHandle = uint32_t;

/**
 * @brief Quality levels for adaptive rendering
 */
enum class QualityLevel {
    LOW,        // Minimal quality for performance
    MEDIUM,     // Balanced quality/performance
    HIGH,       // High quality with good performance
    ULTRA       // Maximum quality regardless of performance
};

/**
 * @brief Smart upload parameters with automatic optimization
 */
struct SmartUploadParams {
    const uint8_t* data;
    size_t data_size;
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_pixel;
    UploadJob::Priority priority = UploadJob::Priority::NORMAL;
    bool enable_compression = true;
    bool check_memory_pressure = true;
    std::chrono::milliseconds deadline = std::chrono::milliseconds(0);
    std::function<void(bool success, TextureHandle result)> completion_callback;
};

/**
 * @brief Adaptive render parameters with performance optimization
 */
struct AdaptiveRenderParams {
    RenderJob::Type job_type;
    TextureHandle input_texture;
    TextureHandle output_texture = 0;
    QualityLevel max_quality = QualityLevel::HIGH;
    bool enable_adaptive_quality = true;
    bool respect_deadlines = true;
    RenderJob::Priority priority = RenderJob::Priority::NORMAL;
    std::function<void(bool success, TextureHandle result)> completion_callback;
    
    // Job-specific parameters
    union {
        struct {
            int effect_type;
            void* parameters;
            size_t param_size;
        } single_effect;
        
        struct {
            std::vector<int> effect_types;
            std::vector<void*> parameters;
            std::vector<size_t> param_sizes;
        } effect_chain;
        
        struct {
            RenderGraph* graph;
            RenderContext* context;
        } render_graph;
    } job_data;
};

/**
 * @brief System coordination statistics
 */
struct CoordinationStats {
    // Upload coordination
    size_t smart_uploads_submitted = 0;
    size_t uploads_throttled_for_memory = 0;
    size_t uploads_compressed_automatically = 0;
    
    // Render coordination
    size_t adaptive_renders_submitted = 0;
    size_t quality_downgrades_applied = 0;
    size_t renders_delayed_for_memory = 0;
    
    // Memory coordination
    size_t memory_pressure_events = 0;
    size_t automatic_evictions_triggered = 0;
    size_t memory_optimization_cycles = 0;
    
    // Performance coordination
    size_t performance_adjustments_made = 0;
    size_t fps_based_optimizations = 0;
    size_t bandwidth_optimizations = 0;
    
    // Overall efficiency
    float average_gpu_utilization = 0.0f;
    float average_memory_efficiency = 0.0f;
    float coordination_overhead_ms = 0.0f;
    
    void reset() {
        *this = CoordinationStats{};
    }
};

/**
 * @brief Master GPU system coordinator
 * 
 * Orchestrates StreamingTextureUploader, GPUMemoryManager, AsyncRenderer,
 * and PerformanceMonitor to work together optimally. Provides intelligent
 * resource management, adaptive quality scaling, and performance optimization.
 */
class GPUSystemCoordinator {
public:
    /**
     * @brief Configuration for system coordination
     */
    struct Config {
        // Memory management
        bool enable_memory_coordination = true;
        float memory_pressure_upload_throttle = 0.8f;  // Throttle uploads at 80% memory
        float memory_pressure_quality_reduce = 0.85f;  // Reduce quality at 85% memory
        
        // Performance management
        bool enable_performance_adaptation = true;
        float fps_threshold_for_quality_reduction = 30.0f;  // Reduce quality below 30 FPS
        float fps_threshold_for_quality_increase = 50.0f;   // Increase quality above 50 FPS
        
        // Upload coordination
        bool enable_smart_uploads = true;
        size_t max_concurrent_uploads_per_gb = 2;        // Scale uploads with VRAM
        bool enable_automatic_compression = true;
        
        // Render coordination
        bool enable_adaptive_rendering = true;
        size_t max_concurrent_renders_per_gb = 4;        // Scale renders with VRAM
        bool enable_deadline_management = true;
        
        // Monitoring
        uint32_t coordination_update_interval_ms = 100;  // Update every 100ms
        bool enable_detailed_statistics = true;
    };
    
    /**
     * @brief Create GPU system coordinator
     * @param config Configuration options
     */
    explicit GPUSystemCoordinator(const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~GPUSystemCoordinator();
    
    // Non-copyable, non-movable
    GPUSystemCoordinator(const GPUSystemCoordinator&) = delete;
    GPUSystemCoordinator& operator=(const GPUSystemCoordinator&) = delete;
    
    /**
     * @brief Initialize with graphics device and create subsystems
     * @param device Graphics device for GPU operations
     * @return True if initialization successful
     */
    bool initialize(GraphicsDevice* device);
    
    /**
     * @brief Shutdown coordinator and cleanup subsystems
     */
    void shutdown();
    
    /**
     * @brief Check if coordinator is initialized
     */
    bool is_initialized() const { return initialized_.load(); }
    
    /**
     * @brief Smart texture upload with automatic optimization
     * @param target Target texture handle (0 to allocate new)
     * @param params Upload parameters with optimization options
     * @return Future that resolves to texture handle
     */
    std::future<TextureHandle> upload_texture_smart(TextureHandle target, const SmartUploadParams& params);
    
    /**
     * @brief Adaptive effect rendering with performance optimization
     * @param params Render parameters with adaptive options
     * @return Future that resolves to output texture handle
     */
    std::future<TextureHandle> render_effect_adaptive(const AdaptiveRenderParams& params);
    
    /**
     * @brief Process complete video frame with optimal resource management
     * @param frame_data Frame pixel data
     * @param width Frame width
     * @param height Frame height
     * @param effects List of effects to apply
     * @return Future that resolves to processed frame texture
     */
    std::future<TextureHandle> process_video_frame(const uint8_t* frame_data, uint32_t width, uint32_t height,
                                                   const std::vector<int>& effects);
    
    /**
     * @brief Apply effects intelligently with resource coordination
     * @param input_texture Source texture
     * @param effects List of effects to apply
     * @param target_quality Desired quality level
     * @return Future that resolves to output texture
     */
    std::future<TextureHandle> apply_effects_intelligently(TextureHandle input_texture, 
                                                           const std::vector<int>& effects,
                                                           QualityLevel target_quality = QualityLevel::HIGH);
    
    /**
     * @brief Optimize entire pipeline automatically
     * Analyzes current performance and adjusts all systems for optimal operation
     */
    void optimize_pipeline_automatically();
    
    /**
     * @brief Force coordinate memory pressure response
     * Manually trigger memory pressure coordination (useful for testing)
     */
    void coordinate_memory_pressure();
    
    /**
     * @brief Adjust quality based on current performance
     * @param force_update Force adjustment even if performance is stable
     */
    void adjust_quality_based_on_performance(bool force_update = false);
    
    /**
     * @brief Get current coordination statistics
     */
    CoordinationStats get_coordination_stats() const;
    
    /**
     * @brief Reset coordination statistics
     */
    void reset_coordination_stats();
    
    /**
     * @brief Update configuration
     * @param new_config New configuration options
     */
    void update_config(const Config& new_config);
    
    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }
    
    /**
     * @brief Get access to individual subsystems (for advanced usage)
     */
    StreamingTextureUploader* get_uploader() const { return uploader_.get(); }
    GPUMemoryManager* get_memory_manager() const { return memory_manager_.get(); }
    AsyncRenderer* get_async_renderer() const { return async_renderer_.get(); }
    PerformanceMonitor* get_performance_monitor() const { return performance_monitor_.get(); }
    
    /**
     * @brief Enable or disable coordination
     * @param enabled Whether to enable automatic coordination
     */
    void set_coordination_enabled(bool enabled);
    
    /**
     * @brief Check if coordination is enabled
     */
    bool is_coordination_enabled() const { return coordination_enabled_.load(); }

private:
    // Core coordination logic
    void coordination_thread_main();
    void update_system_coordination();
    void handle_memory_pressure_event(MemoryPressure level, const GPUMemoryStats& stats);
    void handle_performance_change(const GPUPerformanceStats& stats);
    
    // Upload coordination
    bool should_throttle_uploads() const;
    float calculate_upload_throttle_factor() const;
    UploadJob::Priority adjust_upload_priority(UploadJob::Priority original) const;
    
    // Render coordination
    QualityLevel calculate_adaptive_quality(QualityLevel requested, const GPUPerformanceStats& stats) const;
    bool should_delay_render_for_memory() const;
    RenderJob::Priority adjust_render_priority(RenderJob::Priority original) const;
    
    // Resource management
    void balance_resource_allocation();
    void optimize_concurrent_operations();
    void cleanup_unused_resources();
    
    // Statistics and monitoring
    void update_coordination_statistics();
    void log_coordination_decisions(const std::string& decision, const std::string& reason) const;
    
    // Configuration and state
    Config config_;
    GraphicsDevice* device_ = nullptr;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> coordination_enabled_{true};
    
    // Subsystems
    std::unique_ptr<StreamingTextureUploader> uploader_;
    std::unique_ptr<GPUMemoryManager> memory_manager_;
    std::unique_ptr<AsyncRenderer> async_renderer_;
    std::unique_ptr<PerformanceMonitor> performance_monitor_;
    
    // Coordination thread
    std::thread coordination_thread_;
    
    // Current state
    mutable std::mutex state_mutex_;
    QualityLevel current_quality_level_ = QualityLevel::HIGH;
    MemoryPressure current_memory_pressure_ = MemoryPressure::LOW;
    float current_fps_ = 60.0f;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    CoordinationStats coordination_stats_;
    
    // Timing
    std::chrono::steady_clock::time_point last_optimization_time_;
    std::chrono::steady_clock::time_point last_performance_check_;
};

} // namespace ve::gfx
