#include "gpu_error_handler.hpp"
#include <memory>

namespace ve_gfx {

GPUErrorConfig ErrorHandlerFactory::get_production_config() {
    GPUErrorConfig config;
    config.log_level = 2; // Warning level
    config.enable_debug_callbacks = false;
    config.throw_on_error = true;
    config.enable_validation = false;
    return config;
}

std::unique_ptr<GPUErrorHandler> ErrorHandlerFactory::create_with_config(const GPUErrorConfig& config) {
    return std::make_unique<GPUErrorHandler>(config);
}

} // namespace ve_gfx