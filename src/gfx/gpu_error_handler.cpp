#include "gpu_error_handler.hpp"
#include <memory>

namespace video_editor::gfx {

ErrorHandlerConfig ErrorHandlerFactory::get_production_config() {
    ErrorHandlerConfig config;
    // Production settings optimized for stability
    config.max_retry_attempts = 3;
    config.retry_delay = std::chrono::milliseconds(100);
    config.auto_device_recovery = true;
    config.enable_graceful_degradation = true;
    config.enable_error_logging = true;
    config.enable_crash_dumps = false; // EXCEPTION_POLICY_OK - Core modules must remain exception-free per DR-0003
    config.enable_thermal_throttling = true;
    config.memory_warning_threshold = 0.85f; // Conservative for production
    config.memory_critical_threshold = 0.92f;
    return config;
}

ErrorHandlerConfig ErrorHandlerFactory::get_development_config() {
    ErrorHandlerConfig config;
    // Development settings with more verbose logging
    config.max_retry_attempts = 5;
    config.retry_delay = std::chrono::milliseconds(50);
    config.auto_device_recovery = true;
    config.enable_graceful_degradation = true;
    config.enable_error_logging = true;
    config.enable_crash_dumps = true;
    config.enable_thermal_throttling = true;
    config.memory_warning_threshold = 0.8f;
    config.memory_critical_threshold = 0.9f;
    return config;
}

ErrorHandlerConfig ErrorHandlerFactory::get_performance_config() {
    ErrorHandlerConfig config;
    // Performance-optimized settings with minimal overhead
    config.max_retry_attempts = 1;
    config.retry_delay = std::chrono::milliseconds(10);
    config.auto_device_recovery = false;
    config.enable_graceful_degradation = false;
    config.enable_error_logging = false;
    config.enable_crash_dumps = false;
    config.enable_thermal_throttling = false;
    config.memory_warning_threshold = 0.95f;
    config.memory_critical_threshold = 0.98f;
    return config;
}

ErrorHandlerConfig ErrorHandlerFactory::get_stability_config() {
    ErrorHandlerConfig config;
    // Maximum stability settings
    config.max_retry_attempts = 10;
    config.retry_delay = std::chrono::milliseconds(200);
    config.auto_device_recovery = true;
    config.enable_graceful_degradation = true;
    config.enable_error_logging = true;
    config.enable_crash_dumps = false; // Still respect exception policy
    config.enable_thermal_throttling = true;
    config.memory_warning_threshold = 0.7f; // Very conservative
    config.memory_critical_threshold = 0.85f;
    return config;
}

std::unique_ptr<GPUErrorHandler> ErrorHandlerFactory::create_for_device(GraphicsDevice* device) {
    return create_with_config(device, get_production_config());
}

std::unique_ptr<GPUErrorHandler> ErrorHandlerFactory::create_with_config(GraphicsDevice* device, const ErrorHandlerConfig& config) {
    return std::make_unique<GPUErrorHandler>(device, config);
}

} // namespace video_editor::gfx