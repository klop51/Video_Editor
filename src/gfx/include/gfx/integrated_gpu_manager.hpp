// Week 8: Integrated GPU Manager
// High-level unified interface for all GPU systems

#pragma once

#include "gfx/gpu_system_coordinator.hpp"
#include "gfx/memory_aware_uploader.hpp"
#include "gfx/performance_adaptive_renderer.hpp"
#include "gfx/streaming_texture_uploader.hpp"
#include "gfx/gpu_memory_manager.hpp"
#include "gfx/async_renderer.hpp"
#include "gfx/performance_monitor.hpp"
#include <memory>
#include <future>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>

namespace ve::gfx {

/**
 * @brief High-level GPU operation type
 */
enum class GPUOperationType {
    TEXTURE_UPLOAD,         // Upload texture to GPU
    EFFECT_RENDERING,       // Render effect to texture
    MEMORY_MANAGEMENT,      // Manage GPU memory
    PERFORMANCE_OPTIMIZATION, // Optimize performance
    CACHE_OPERATION,        // Cache management operation
    RESOURCE_CLEANUP        // Clean up GPU resources
};

/**
 * @brief GPU operation priority
 */
enum class GPUOperationPriority {
    CRITICAL,               // Must complete immediately
    HIGH,                   // Important, complete soon
    NORMAL,                 // Standard priority
    LOW,                    // Can be delayed
    BACKGROUND              // Run when system is idle
};

/**
 * @brief GPU operation status
 */
enum class GPUOperationStatus {
    PENDING,                // Waiting to start
    IN_PROGRESS,            // Currently executing
    COMPLETED,              // Successfully completed
    FAILED,                 // Failed with error
    CANCELLED,              // Cancelled by user
    QUEUED                  // Queued for later execution
};

/**
 * @brief Comprehensive GPU system status
 */
struct IntegratedGPUStatus {
    // Overall system health
    bool is_healthy = true;
    float overall_efficiency = 0.0f;
    std::string status_summary;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    
    // Memory status
    size_t total_gpu_memory_mb = 0;
    size_t used_gpu_memory_mb = 0;
    size_t available_gpu_memory_mb = 0;
    float memory_utilization = 0.0f;
    size_t textures_in_memory = 0;
    
    // Performance status
    float current_fps = 0.0f;
    float target_fps = 30.0f;
    float average_frame_time_ms = 0.0f;
    DetailedQualityLevel current_quality = DetailedQualityLevel::MEDIUM;
    bool performance_adaptation_active = false;
    
    // Operation status
    size_t pending_uploads = 0;
    size_t active_render_jobs = 0;
    size_t queued_operations = 0;
    float operation_queue_pressure = 0.0f;
    
    // Coordination status
    bool systems_coordinated = false;
    size_t coordination_optimizations_active = 0;
    float coordination_efficiency = 0.0f;
    
    // Resource utilization
    float gpu_utilization = 0.0f;
    float upload_bandwidth_mbps = 0.0f;
    float render_throughput_mpps = 0.0f;   // Megapixels per second
    
    // Timestamps
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point system_start_time;
};

/**
 * @brief GPU operation configuration
 */
struct GPUOperationConfig {
    GPUOperationType type = GPUOperationType::EFFECT_RENDERING;
    GPUOperationPriority priority = GPUOperationPriority::NORMAL;
    
    // Quality settings
    DetailedQualityLevel quality_preference = DetailedQualityLevel::HIGH;
    bool allow_quality_adaptation = true;
    bool respect_memory_constraints = true;
    bool enable_performance_optimization = true;
    
    // Timing constraints
    uint32_t max_execution_time_ms = 0;     // 0 = no limit
    uint32_t deadline_ms = 0;               // 0 = no deadline
    bool allow_background_execution = true;
    
    // Resource preferences
    size_t max_memory_usage_mb = 0;         // 0 = no limit
    float max_gpu_utilization = 1.0f;       // 100% by default
    bool prefer_speed_over_quality = false;
    
    // Callbacks
    std::function<void(GPUOperationStatus status)> status_callback;
    std::function<void(float progress)> progress_callback;
    std::function<void(const std::string& error)> error_callback;
};

/**
 * @brief GPU operation result
 */
struct GPUOperationResult {
    GPUOperationStatus status = GPUOperationStatus::PENDING;
    std::string operation_id;
    
    // Results
    TextureHandle result_texture;           // For operations that produce textures
    std::vector<uint8_t> result_data;       // For operations that produce data
    
    // Performance metrics
    float execution_time_ms = 0.0f;
    size_t memory_used_mb = 0;
    float gpu_utilization_average = 0.0f;
    DetailedQualityLevel quality_used = DetailedQualityLevel::MEDIUM;
    
    // Error information
    std::string error_message;
    int error_code = 0;
    
    // Optimization information
    bool was_optimized = false;
    std::vector<std::string> optimizations_applied;
    float optimization_benefit = 0.0f;
};

/**
 * @brief Integrated GPU Manager
 * 
 * High-level unified interface that coordinates all GPU systems:
 * - StreamingTextureUploader for efficient uploads
 * - GPUMemoryManager for intelligent memory management
 * - AsyncRenderer for non-blocking rendering
 * - PerformanceMonitor for real-time performance tracking
 * - GPUSystemCoordinator for system coordination
 * - MemoryAwareUploader for memory-conscious uploads
 * - PerformanceAdaptiveRenderer for adaptive quality
 * 
 * Provides simple, high-level API for complex GPU operations.
 */
class IntegratedGPUManager {
public:
    /**
     * @brief Configuration for integrated GPU manager
     */
    struct Config {
        // System configuration
        bool enable_coordination = true;
        bool enable_adaptive_performance = true;
        bool enable_memory_optimization = true;
        bool enable_streaming_uploads = true;
        
        // Performance targets
        float target_fps = 30.0f;
        float min_acceptable_fps = 20.0f;
        DetailedQualityLevel default_quality = DetailedQualityLevel::HIGH;
        
        // Memory configuration
        size_t max_gpu_memory_usage_mb = 0;    // 0 = auto-detect
        float memory_pressure_threshold = 0.8f; // Trigger optimizations at 80%
        bool enable_aggressive_memory_management = false;
        
        // Operation queue configuration
        size_t max_pending_operations = 100;
        uint32_t operation_timeout_ms = 30000;  // 30 second timeout
        bool enable_operation_prioritization = true;
        
        // Advanced features
        bool enable_predictive_optimization = true;
        bool enable_thermal_awareness = false;
        bool enable_power_efficiency_mode = false;
        
        // Monitoring and logging
        bool enable_detailed_logging = true;
        bool enable_performance_history = true;
        uint32_t status_update_interval_ms = 1000; // Update status every second
    };
    
    /**
     * @brief Create integrated GPU manager
     * @param config Configuration options
     */
    explicit IntegratedGPUManager(const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~IntegratedGPUManager();
    
    // Non-copyable, non-movable
    IntegratedGPUManager(const IntegratedGPUManager&) = delete;
    IntegratedGPUManager& operator=(const IntegratedGPUManager&) = delete;
    
    /**
     * @brief Initialize all GPU systems
     * @return True if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown all GPU systems gracefully
     */
    void shutdown();
    
    /**
     * @brief Check if GPU manager is initialized and ready
     */
    bool is_ready() const { return initialized_.load(); }
    
    // ========================================
    // High-Level Operations
    // ========================================
    
    /**
     * @brief Upload texture with automatic optimization
     * @param image_data Image data to upload
     * @param width Image width
     * @param height Image height
     * @param format Texture format
     * @param config Operation configuration
     * @return Future that resolves to operation result
     */
    std::future<GPUOperationResult> upload_texture_optimized(const void* image_data, int width, int height,
                                                             TextureFormat format,
                                                             const GPUOperationConfig& config = GPUOperationConfig{});
    
    /**
     * @brief Apply effect with adaptive quality and memory management
     * @param effect_type Effect type identifier
     * @param parameters Effect parameters
     * @param param_size Parameter size in bytes
     * @param input_texture Source texture
     * @param config Operation configuration
     * @return Future that resolves to operation result
     */
    std::future<GPUOperationResult> apply_effect_intelligent(int effect_type, const void* parameters, size_t param_size,
                                                             TextureHandle input_texture,
                                                             const GPUOperationConfig& config = GPUOperationConfig{});
    
    /**
     * @brief Process entire effect chain with full optimization
     * @param effects Vector of effects to apply in sequence
     * @param input_texture Source texture
     * @param config Operation configuration
     * @return Future that resolves to final operation result
     */
    std::future<GPUOperationResult> process_effect_chain_optimized(const std::vector<RenderJob>& effects,
                                                                   TextureHandle input_texture,
                                                                   const GPUOperationConfig& config = GPUOperationConfig{});
    
    /**
     * @brief Optimize GPU performance automatically
     * @param aggressive Whether to use aggressive optimization
     * @return True if optimizations were applied
     */
    bool optimize_performance(bool aggressive = false);
    
    /**
     * @brief Clean up GPU memory intelligently
     * @param target_free_mb Target amount of memory to free (0 = optimal)
     * @return Amount of memory freed in MB
     */
    size_t cleanup_memory_intelligent(size_t target_free_mb = 0);
    
    // ========================================
    // System Status and Control
    // ========================================
    
    /**
     * @brief Get comprehensive system status
     */
    IntegratedGPUStatus get_system_status() const;
    
    /**
     * @brief Get system health score (0.0 - 1.0)
     */
    float get_system_health_score() const;
    
    /**
     * @brief Get operation status by ID
     * @param operation_id Operation identifier
     */
    GPUOperationStatus get_operation_status(const std::string& operation_id) const;
    
    /**
     * @brief Cancel operation by ID
     * @param operation_id Operation identifier
     * @return True if operation was cancelled
     */
    bool cancel_operation(const std::string& operation_id);
    
    /**
     * @brief Cancel all pending operations
     * @return Number of operations cancelled
     */
    size_t cancel_all_operations();
    
    /**
     * @brief Set performance mode
     * @param prefer_quality True to prefer quality, false to prefer performance
     */
    void set_performance_mode(bool prefer_quality);
    
    /**
     * @brief Enable or disable automatic optimization
     * @param enabled Whether to enable automatic optimization
     */
    void set_automatic_optimization(bool enabled);
    
    // ========================================
    // Configuration and Tuning
    // ========================================
    
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
     * @brief Set memory usage limit
     * @param max_memory_mb Maximum GPU memory usage in MB
     */
    void set_memory_limit(size_t max_memory_mb);
    
    /**
     * @brief Set performance targets
     * @param target_fps Target FPS to maintain
     * @param quality Target quality level
     */
    void set_performance_targets(float target_fps, DetailedQualityLevel quality);
    
    // ========================================
    // Advanced Features
    // ========================================
    
    /**
     * @brief Get performance recommendations
     * @return Vector of actionable performance recommendations
     */
    std::vector<std::string> get_performance_recommendations() const;
    
    /**
     * @brief Get memory usage breakdown
     * @return Map of subsystem names to memory usage in MB
     */
    std::map<std::string, size_t> get_memory_usage_breakdown() const;
    
    /**
     * @brief Get performance history (if enabled)
     * @return Vector of recent performance measurements
     */
    std::vector<GPUPerformanceStats> get_performance_history() const;
    
    /**
     * @brief Force garbage collection and optimization
     * @return True if optimization completed successfully
     */
    bool force_optimization_cycle();
    
    /**
     * @brief Get detailed system diagnostics
     * @return Map of diagnostic information
     */
    std::map<std::string, std::string> get_system_diagnostics() const;

private:
    // Initialization helpers
    bool initialize_subsystems();
    bool setup_coordination();
    void start_monitoring_thread();
    
    // Operation management
    std::string generate_operation_id();
    void track_operation(const std::string& operation_id, const GPUOperationConfig& config);
    void complete_operation(const std::string& operation_id, const GPUOperationResult& result);
    void cleanup_completed_operations();
    
    // System monitoring
    void monitoring_thread_main();
    void update_system_status();
    void check_system_health();
    void apply_automatic_optimizations();
    
    // Resource management
    bool check_resource_availability(const GPUOperationConfig& config) const;
    void allocate_resources_for_operation(const std::string& operation_id, const GPUOperationConfig& config);
    void release_resources_for_operation(const std::string& operation_id);
    
    // Configuration
    Config config_;
    
    // Subsystem components
    std::unique_ptr<StreamingTextureUploader> texture_uploader_;
    std::unique_ptr<GPUMemoryManager> memory_manager_;
    std::unique_ptr<AsyncRenderer> async_renderer_;
    std::unique_ptr<PerformanceMonitor> performance_monitor_;
    std::unique_ptr<GPUSystemCoordinator> system_coordinator_;
    std::unique_ptr<MemoryAwareUploader> memory_aware_uploader_;
    std::unique_ptr<PerformanceAdaptiveRenderer> adaptive_renderer_;
    
    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> automatic_optimization_enabled_{true};
    
    // Operation tracking
    mutable std::mutex operations_mutex_;
    std::map<std::string, GPUOperationConfig> pending_operations_;
    std::map<std::string, GPUOperationResult> completed_operations_;
    std::atomic<uint64_t> operation_counter_{0};
    
    // System status
    mutable std::mutex status_mutex_;
    IntegratedGPUStatus current_status_;
    
    // Monitoring thread
    std::thread monitoring_thread_;
    
    // Resource allocation tracking
    mutable std::mutex resource_mutex_;
    size_t allocated_memory_mb_ = 0;
    float allocated_gpu_utilization_ = 0.0f;
    
    // Performance history (if enabled)
    mutable std::mutex history_mutex_;
    std::vector<GPUPerformanceStats> performance_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
};

} // namespace ve::gfx
