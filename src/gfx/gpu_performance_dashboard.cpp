#include "gpu_performance_dashboard.hpp"
#include "gpu_error_handler.hpp"
#include "gpu_memory_optimizer.hpp"

namespace video_editor::gfx {

PerformanceProfiler::~PerformanceProfiler() {
    // Cleanup profiler resources if needed
}

// ============================================================================
// PerformanceDashboard Implementation
// ============================================================================

PerformanceDashboard::PerformanceDashboard(GraphicsDevice* device, const PerformanceTargets& targets)
    : device_(device), targets_(targets), monitoring_active_(false), monitoring_enabled_(false),
      error_handler_(nullptr), memory_optimizer_(nullptr) {
    // Initialize dashboard
}

PerformanceDashboard::~PerformanceDashboard() {
    monitoring_active_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void PerformanceDashboard::integrate_with_error_handler(class GPUErrorHandler* error_handler) {
    error_handler_ = error_handler;
    // Setup integration logic here
}

void PerformanceDashboard::start_monitoring() {
    monitoring_enabled_ = true;
    // Start monitoring threads/timers here
}

void PerformanceDashboard::integrate_with_memory_optimizer(class GPUMemoryOptimizer* memory_optimizer) {
    memory_optimizer_ = memory_optimizer;
    // Setup integration logic here  
}

} // namespace video_editor::gfx