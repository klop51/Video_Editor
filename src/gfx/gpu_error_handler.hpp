// GPU Error Handler & Recovery System
// Week 16: Production-ready error handling for all GPU system components

#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>

namespace video_editor::gfx {

// Forward declarations
class GraphicsDevice;
class TextureHandle;
class BufferHandle;

// ============================================================================
// GPU Error Types & Recovery Strategies
// ============================================================================

enum class GPUErrorType {
    DeviceLost,                    // Device reset/lost (driver crash, TDR)
    OutOfMemory,                   // VRAM/system memory exhaustion
    ShaderCompilationError,        // Shader compilation failure
    ResourceCreationFailure,       // Texture/buffer creation failed
    CommandExecutionFailure,       // Command buffer execution failed
    DriverTimeout,                 // GPU operation timeout
    UnsupportedOperation,          // Feature not supported on device
    InvalidResourceState,          // Resource in invalid state
    SynchronizationError,          // GPU/CPU sync failure
    ThermalThrottling,            // GPU overheating protection
    Unknown                        // Unclassified error
};

enum class RecoveryStrategy {
    Retry,                        // Retry the operation
    FallbackCPU,                  // Fall back to CPU implementation
    ReduceQuality,                // Reduce quality settings
    DeviceReset,                  // Reset graphics device
    GracefulDegradation,          // Disable advanced features
    AbortOperation,               // Cancel current operation
    UserNotification,             // Notify user and wait for action
    RestartApplication           // Restart application (last resort)
};

enum class ErrorSeverity {
    Info,        // Informational (performance warning)
    Warning,     // Warning (degraded performance)
    Error,       // Error (operation failed but recoverable)
    Critical,    // Critical (system stability at risk)
    Fatal        // Fatal (application must restart)
};

// ============================================================================
// GPU Error Information
// ============================================================================

struct GPUErrorInfo {
    GPUErrorType type;
    ErrorSeverity severity;
    std::string description;
    std::string component;           // Which component reported error
    std::chrono::steady_clock::time_point timestamp;
    uint32_t error_code;            // Platform-specific error code
    std::string callstack;          // Call stack when error occurred
    
    // Context information
    size_t available_vram_mb;
    size_t used_vram_mb;
    float gpu_utilization;
    float gpu_temperature;
    
    // Recovery information
    std::vector<RecoveryStrategy> suggested_strategies;
    uint32_t retry_count;
    bool auto_recovery_attempted;
    bool recovery_successful;
};

// ============================================================================
// Error Recovery Callback Interface
// ============================================================================

class IErrorRecoveryCallback {
public:
    virtual ~IErrorRecoveryCallback() = default;
    
    // Called when error is detected
    virtual void on_error_detected(const GPUErrorInfo& error) = 0;
    
    // Called before recovery attempt
    virtual void on_recovery_start(const GPUErrorInfo& error, RecoveryStrategy strategy) = 0;
    
    // Called after recovery attempt
    virtual void on_recovery_complete(const GPUErrorInfo& error, RecoveryStrategy strategy, bool success) = 0;
    
    // Called when all recovery strategies have failed
    virtual void on_recovery_failed(const GPUErrorInfo& error) = 0;
};

// ============================================================================
// GPU Error Handler Configuration
// ============================================================================

struct ErrorHandlerConfig {
    // Retry configuration
    uint32_t max_retry_attempts = 3;
    std::chrono::milliseconds retry_delay = std::chrono::milliseconds(100);
    float retry_backoff_multiplier = 2.0f;
    
    // Device lost recovery
    bool auto_device_recovery = true;
    std::chrono::seconds device_reset_timeout = std::chrono::seconds(10);
    
    // Memory management
    float memory_warning_threshold = 0.9f;    // 90% VRAM usage
    float memory_critical_threshold = 0.95f;  // 95% VRAM usage
    bool auto_memory_cleanup = true;
    
    // Performance degradation
    bool enable_graceful_degradation = true;
    uint32_t performance_failure_threshold = 5; // Failures before degradation
    
    // Thermal protection
    float thermal_warning_temp = 85.0f;       // °C
    float thermal_critical_temp = 95.0f;      // °C
    bool enable_thermal_throttling = true;
    
    // Logging and reporting
    bool enable_error_logging = true;
    bool enable_crash_dumps = true;
    std::string log_file_path = "gpu_errors.log";
    
    // User interaction
    bool show_error_dialogs = true;
    std::chrono::seconds user_response_timeout = std::chrono::seconds(30);
};

// ============================================================================
// Device Lost Recovery Manager
// ============================================================================

class DeviceLostRecoveryManager {
public:
    DeviceLostRecoveryManager(GraphicsDevice* device);
    ~DeviceLostRecoveryManager();
    
    // Check if device is lost
    bool is_device_lost() const;
    
    // Attempt device recovery
    bool attempt_recovery();
    
    // Register resource for automatic restoration
    void register_resource(const std::string& id, std::function<bool()> restore_func);
    void unregister_resource(const std::string& id);
    
    // Backup and restore critical state
    void backup_device_state();
    bool restore_device_state();
    
private:
    struct ResourceRestoreInfo {
        std::string id;
        std::function<bool()> restore_function;
        uint32_t priority;
        bool critical;
    };
    
    GraphicsDevice* device_;
    std::vector<ResourceRestoreInfo> registered_resources_;
    std::atomic<bool> device_lost_;
    std::atomic<bool> recovery_in_progress_;
    mutable std::mutex resources_mutex_;
    
    // State backup
    struct DeviceState {
        std::vector<std::pair<std::string, std::vector<uint8_t>>> shader_cache;
        std::vector<std::pair<std::string, std::vector<uint8_t>>> texture_data;
        // Additional state as needed
    } backed_up_state_;
    
    bool attempt_device_reset();
    bool restore_resources();
    void cleanup_lost_resources();
};

// ============================================================================
// Memory Pressure Handler
// ============================================================================

class MemoryPressureHandler {
public:
    MemoryPressureHandler(GraphicsDevice* device, const ErrorHandlerConfig& config);
    ~MemoryPressureHandler();
    
    // Monitor memory usage
    void start_monitoring();
    void stop_monitoring();
    
    // Check current memory pressure
    float get_memory_pressure() const;
    bool is_under_memory_pressure() const;
    
    // Emergency memory cleanup
    size_t emergency_cleanup();
    
    // Register memory cleanup callbacks
    void register_cleanup_callback(const std::string& component, 
                                 std::function<size_t()> cleanup_func, 
                                 uint32_t priority);
    
private:
    struct CleanupCallback {
        std::string component;
        std::function<size_t()> cleanup_function;
        uint32_t priority;
        std::chrono::steady_clock::time_point last_called;
    };
    
    GraphicsDevice* device_;
    ErrorHandlerConfig config_;
    std::vector<CleanupCallback> cleanup_callbacks_;
    
    std::atomic<bool> monitoring_active_;
    std::thread monitoring_thread_;
    mutable std::mutex callbacks_mutex_;
    
    void memory_monitoring_loop();
    size_t get_available_memory() const;
    size_t get_used_memory() const;
    void trigger_memory_warning();
    void trigger_memory_critical();
};

// ============================================================================
// Thermal Protection Manager
// ============================================================================

class ThermalProtectionManager {
public:
    ThermalProtectionManager(GraphicsDevice* device, const ErrorHandlerConfig& config);
    ~ThermalProtectionManager();
    
    void start_monitoring();
    void stop_monitoring();
    
    float get_gpu_temperature() const;
    bool is_thermal_throttling_active() const;
    
    // Register throttling callbacks
    void register_throttling_callback(std::function<void(float)> callback);
    
private:
    GraphicsDevice* device_;
    ErrorHandlerConfig config_;
    
    std::atomic<bool> monitoring_active_;
    std::atomic<bool> throttling_active_;
    std::thread monitoring_thread_;
    
    std::vector<std::function<void(float)>> throttling_callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    void thermal_monitoring_loop();
    void activate_thermal_throttling(float temperature);
    void deactivate_thermal_throttling();
};

// ============================================================================
// Main GPU Error Handler
// ============================================================================

class GPUErrorHandler {
public:
    GPUErrorHandler(GraphicsDevice* device, const ErrorHandlerConfig& config = {});
    ~GPUErrorHandler();
    
    // Error reporting
    void report_error(GPUErrorType type, const std::string& description, 
                     const std::string& component = "", uint32_t error_code = 0);
    
    // Recovery operations
    bool attempt_recovery(const GPUErrorInfo& error);
    bool attempt_strategy(const GPUErrorInfo& error, RecoveryStrategy strategy);
    
    // Callback management
    void register_callback(std::shared_ptr<IErrorRecoveryCallback> callback);
    void unregister_callback(std::shared_ptr<IErrorRecoveryCallback> callback);
    
    // Error statistics
    struct ErrorStatistics {
        uint32_t total_errors;
        uint32_t successful_recoveries;
        uint32_t failed_recoveries;
        std::unordered_map<GPUErrorType, uint32_t> error_counts;
        std::unordered_map<RecoveryStrategy, uint32_t> strategy_success_counts;
        std::chrono::steady_clock::time_point first_error;
        std::chrono::steady_clock::time_point last_error;
        float mean_time_between_failures_hours;
    };
    
    ErrorStatistics get_error_statistics() const;
    void reset_error_statistics();
    
    // System health monitoring
    bool is_system_healthy() const;
    float get_system_stability_score() const; // 0.0 = unstable, 1.0 = very stable
    
    // Configuration
    void update_config(const ErrorHandlerConfig& config);
    const ErrorHandlerConfig& get_config() const { return config_; }
    
    // Emergency operations
    void emergency_shutdown();
    bool emergency_device_reset();
    
private:
    GraphicsDevice* device_;
    ErrorHandlerConfig config_;
    
    // Sub-managers
    std::unique_ptr<DeviceLostRecoveryManager> device_recovery_;
    std::unique_ptr<MemoryPressureHandler> memory_handler_;
    std::unique_ptr<ThermalProtectionManager> thermal_manager_;
    
    // Error tracking
    std::vector<GPUErrorInfo> error_history_;
    ErrorStatistics statistics_;
    mutable std::mutex error_mutex_;
    
    // Callbacks
    std::vector<std::weak_ptr<IErrorRecoveryCallback>> callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    // Recovery strategies implementation
    bool attempt_retry_strategy(const GPUErrorInfo& error);
    bool attempt_fallback_cpu_strategy(const GPUErrorInfo& error);
    bool attempt_reduce_quality_strategy(const GPUErrorInfo& error);
    bool attempt_device_reset_strategy(const GPUErrorInfo& error);
    bool attempt_graceful_degradation_strategy(const GPUErrorInfo& error);
    bool attempt_user_notification_strategy(const GPUErrorInfo& error);
    
    // Utility functions
    void log_error(const GPUErrorInfo& error);
    void notify_callbacks_error_detected(const GPUErrorInfo& error);
    void notify_callbacks_recovery_start(const GPUErrorInfo& error, RecoveryStrategy strategy);
    void notify_callbacks_recovery_complete(const GPUErrorInfo& error, RecoveryStrategy strategy, bool success);
    void notify_callbacks_recovery_failed(const GPUErrorInfo& error);
    
    void update_error_statistics(const GPUErrorInfo& error, bool recovery_successful);
    GPUErrorInfo create_error_info(GPUErrorType type, const std::string& description, 
                                  const std::string& component, uint32_t error_code);
    
    std::vector<RecoveryStrategy> determine_recovery_strategies(const GPUErrorInfo& error);
    ErrorSeverity determine_error_severity(GPUErrorType type, const std::string& component);
};

// ============================================================================
// RAII Error Context Helper
// ============================================================================

class ErrorContext {
public:
    ErrorContext(GPUErrorHandler* handler, const std::string& operation_name);
    ~ErrorContext();
    
    void set_component(const std::string& component) { component_ = component; }
    void add_context_info(const std::string& key, const std::string& value);
    
    // Check for errors and handle them
    bool check_and_handle_errors();
    
private:
    GPUErrorHandler* handler_;
    std::string operation_name_;
    std::string component_;
    std::unordered_map<std::string, std::string> context_info_;
    std::chrono::steady_clock::time_point start_time_;
};

// ============================================================================
// Error Handler Factory
// ============================================================================

class ErrorHandlerFactory {
public:
    static std::unique_ptr<GPUErrorHandler> create_for_device(GraphicsDevice* device);
    static std::unique_ptr<GPUErrorHandler> create_with_config(GraphicsDevice* device, 
                                                              const ErrorHandlerConfig& config);
    
    // Predefined configurations
    static ErrorHandlerConfig get_development_config();
    static ErrorHandlerConfig get_production_config();
    static ErrorHandlerConfig get_performance_config();
    static ErrorHandlerConfig get_stability_config();
};

// ============================================================================
// Utility Macros for Error Handling
// ============================================================================

#define GPU_ERROR_CONTEXT(handler, operation) \
    video_editor::gfx::ErrorContext _error_ctx(handler, operation)

#define GPU_CHECK_ERRORS(handler) \
    if (handler && !handler->is_system_healthy()) { \
        /* Handle system health issues */ \
    }

#define GPU_TRY_OPERATION(handler, operation, ...) \
    do { \
        GPU_ERROR_CONTEXT(handler, #operation); \
        auto result = operation(__VA_ARGS__); \
        if (!result) { \
            handler->report_error(GPUErrorType::Unknown, \
                                "Operation failed: " #operation, \
                                __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

} // namespace video_editor::gfx
