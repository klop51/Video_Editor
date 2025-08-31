// Week 8: GPU Memory Manager Implementation
// Intelligent GPU memory allocation and optimization

#include "gfx/gpu_memory_manager.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <thread>

namespace ve::gfx {

// LRUEvictionPolicy Implementation
void LRUEvictionPolicy::record_access(uint32_t handle) {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    AccessInfo& info = access_times_[handle];
    info.last_access = std::chrono::steady_clock::now();
    info.access_count++;
}

std::vector<uint32_t> LRUEvictionPolicy::get_eviction_candidates(size_t target_bytes, 
                                                                const std::unordered_map<uint32_t, GPUAllocation>& allocations) {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    // Create list of candidates with their last access times
    std::vector<std::pair<uint32_t, std::chrono::steady_clock::time_point>> candidates;
    
    for (const auto& [handle, allocation] : allocations) {
        if (allocation.is_persistent) {
            continue;  // Skip persistent allocations
        }
        
        auto it = access_times_.find(handle);
        if (it != access_times_.end()) {
            candidates.emplace_back(handle, it->second.last_access);
        } else {
            // No access recorded, consider it very old
            candidates.emplace_back(handle, std::chrono::steady_clock::time_point::min());
        }
    }
    
    // Sort by last access time (oldest first)
    std::sort(candidates.begin(), candidates.end(),
             [](const auto& a, const auto& b) {
                 return a.second < b.second;
             });
    
    // Select candidates until we have enough bytes
    std::vector<uint32_t> eviction_list;
    size_t bytes_to_free = 0;
    
    for (const auto& [handle, last_access] : candidates) {
        auto it = allocations.find(handle);
        if (it != allocations.end()) {
            eviction_list.push_back(handle);
            bytes_to_free += it->second.size_bytes;
            
            if (bytes_to_free >= target_bytes) {
                break;
            }
        }
    }
    
    return eviction_list;
}

void LRUEvictionPolicy::remove_handle(uint32_t handle) {
    std::lock_guard<std::mutex> lock(access_mutex_);
    access_times_.erase(handle);
}

void LRUEvictionPolicy::clear() {
    std::lock_guard<std::mutex> lock(access_mutex_);
    access_times_.clear();
}

// GPUMemoryManager Implementation
GPUMemoryManager::GPUMemoryManager(const Config& config) 
    : config_(config) {
    
    ve::log::info("Creating GPUMemoryManager");
    
    // Initialize timing
    last_defrag_time_ = std::chrono::steady_clock::now();
    last_pressure_check_ = last_defrag_time_;
    
    // Start background management thread
    background_thread_ = std::thread(&GPUMemoryManager::background_management_thread, this);
    
    ve::log::debug("GPUMemoryManager initialized");
}

GPUMemoryManager::~GPUMemoryManager() {
    ve::log::debug("Shutting down GPUMemoryManager");
    
    shutdown_requested_.store(true);
    
    if (background_thread_.joinable()) {
        background_thread_.join();
    }
    
    ve::log::info("GPUMemoryManager shutdown complete");
}

void GPUMemoryManager::initialize(size_t total_memory, size_t available_memory) {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    stats_.total_gpu_memory = total_memory;
    stats_.available_gpu_memory = available_memory;
    stats_.used_gpu_memory = total_memory - available_memory;
    
    ve::log::info("GPU Memory initialized: Total:", total_memory / (1024*1024), "MB, Available:", 
                  available_memory / (1024*1024), "MB, Used:", stats_.used_gpu_memory / (1024*1024), "MB");
}

void GPUMemoryManager::register_allocation(const GPUAllocation& allocation) {
    std::lock_guard<std::mutex> alloc_lock(allocations_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    allocations_[allocation.handle] = allocation;
    lru_policy_.record_access(allocation.handle);
    
    // Update statistics
    stats_.used_gpu_memory += allocation.size_bytes;
    stats_.available_gpu_memory = stats_.total_gpu_memory - stats_.used_gpu_memory;
    stats_.allocation_count++;
    
    // Update type-specific counters
    switch (allocation.type) {
        case GPUAllocation::Type::TEXTURE_2D:
        case GPUAllocation::Type::TEXTURE_3D:
            stats_.texture_memory += allocation.size_bytes;
            if (allocation.texture_info.is_render_target) {
                stats_.render_target_memory += allocation.size_bytes;
            }
            break;
        case GPUAllocation::Type::VERTEX_BUFFER:
        case GPUAllocation::Type::INDEX_BUFFER:
        case GPUAllocation::Type::CONSTANT_BUFFER:
            stats_.buffer_memory += allocation.size_bytes;
            break;
        case GPUAllocation::Type::STAGING_BUFFER:
            stats_.staging_memory += allocation.size_bytes;
            break;
    }
    
    ve::log::debug("Registered allocation:", allocation.handle, "Size:", allocation.size_bytes, 
                   "Type:", static_cast<int>(allocation.type));
    
    // Check memory pressure
    check_memory_pressure();
}

void GPUMemoryManager::unregister_allocation(uint32_t handle) {
    std::lock_guard<std::mutex> alloc_lock(allocations_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    auto it = allocations_.find(handle);
    if (it == allocations_.end()) {
        ve::log::warning("Attempted to unregister unknown allocation:", handle);
        return;
    }
    
    const GPUAllocation& allocation = it->second;
    
    // Update statistics
    stats_.used_gpu_memory -= allocation.size_bytes;
    stats_.available_gpu_memory = stats_.total_gpu_memory - stats_.used_gpu_memory;
    stats_.deallocation_count++;
    
    // Update type-specific counters
    switch (allocation.type) {
        case GPUAllocation::Type::TEXTURE_2D:
        case GPUAllocation::Type::TEXTURE_3D:
            stats_.texture_memory -= allocation.size_bytes;
            if (allocation.texture_info.is_render_target) {
                stats_.render_target_memory -= allocation.size_bytes;
            }
            break;
        case GPUAllocation::Type::VERTEX_BUFFER:
        case GPUAllocation::Type::INDEX_BUFFER:
        case GPUAllocation::Type::CONSTANT_BUFFER:
            stats_.buffer_memory -= allocation.size_bytes;
            break;
        case GPUAllocation::Type::STAGING_BUFFER:
            stats_.staging_memory -= allocation.size_bytes;
            break;
    }
    
    allocations_.erase(it);
    lru_policy_.remove_handle(handle);
    
    ve::log::debug("Unregistered allocation:", handle, "Size:", allocation.size_bytes);
}

void GPUMemoryManager::record_access(uint32_t handle) {
    lru_policy_.record_access(handle);
    
    // Update allocation access info
    std::lock_guard<std::mutex> lock(allocations_mutex_);
    auto it = allocations_.find(handle);
    if (it != allocations_.end()) {
        it->second.last_used_time = std::chrono::steady_clock::now();
        it->second.access_count++;
    }
}

bool GPUMemoryManager::can_allocate(size_t requested_bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Simple check: is there enough available memory?
    if (stats_.available_gpu_memory >= requested_bytes) {
        return true;
    }
    
    // Check if we could free enough memory through eviction
    if (config_.enable_automatic_eviction) {
        size_t potentially_freeable = 0;
        
        std::lock_guard<std::mutex> alloc_lock(allocations_mutex_);
        for (const auto& [handle, allocation] : allocations_) {
            if (!allocation.is_persistent) {
                potentially_freeable += allocation.size_bytes;
                if (potentially_freeable + stats_.available_gpu_memory >= requested_bytes) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool GPUMemoryManager::ensure_available_memory(size_t required_bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (stats_.available_gpu_memory >= required_bytes) {
        return true;  // Already enough memory
    }
    
    if (!config_.enable_automatic_eviction) {
        return false;  // Can't evict automatically
    }
    
    size_t bytes_to_free = required_bytes - stats_.available_gpu_memory;
    
    // Add some buffer to avoid immediate pressure again
    bytes_to_free = std::max(bytes_to_free, config_.min_eviction_size);
    bytes_to_free = std::min(bytes_to_free, config_.max_eviction_size);
    
    size_t freed = evict_least_recently_used(bytes_to_free);
    
    ve::log::info("Freed", freed, "bytes for new allocation requiring", required_bytes, "bytes");
    
    return stats_.available_gpu_memory >= required_bytes;
}

size_t GPUMemoryManager::evict_least_recently_used(size_t target_bytes) {
    std::lock_guard<std::mutex> alloc_lock(allocations_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    std::vector<uint32_t> candidates = lru_policy_.get_eviction_candidates(target_bytes, allocations_);
    
    size_t bytes_freed = 0;
    for (uint32_t handle : candidates) {
        auto it = allocations_.find(handle);
        if (it != allocations_.end() && !it->second.is_persistent) {
            size_t allocation_size = it->second.size_bytes;
            
            // Note: In a real implementation, we would call the graphics device
            // to actually free the GPU resource here
            ve::log::debug("Evicting allocation:", handle, "Size:", allocation_size);
            
            bytes_freed += allocation_size;
            stats_.eviction_count++;
            
            // Remove from tracking
            allocations_.erase(it);
            lru_policy_.remove_handle(handle);
            
            if (bytes_freed >= target_bytes) {
                break;
            }
        }
    }
    
    // Update available memory
    stats_.available_gpu_memory += bytes_freed;
    stats_.used_gpu_memory -= bytes_freed;
    
    ve::log::info("Evicted", candidates.size(), "allocations, freed", bytes_freed, "bytes");
    return bytes_freed;
}

GPUMemoryStats GPUMemoryManager::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Update fragmentation calculation
    auto& mutable_stats = const_cast<GPUMemoryStats&>(stats_);
    mutable_stats.fragmentation_bytes = calculate_fragmentation();
    
    return stats_;
}

MemoryPressure GPUMemoryManager::get_memory_pressure() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_pressure_;
}

void GPUMemoryManager::set_pressure_callback(MemoryPressureCallback callback) {
    pressure_callback_ = callback;
}

void GPUMemoryManager::update_available_memory(size_t new_available) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    size_t old_used = stats_.used_gpu_memory;
    stats_.available_gpu_memory = new_available;
    stats_.used_gpu_memory = stats_.total_gpu_memory - new_available;
    
    if (stats_.used_gpu_memory != old_used) {
        ve::log::debug("Memory usage updated: Used:", stats_.used_gpu_memory / (1024*1024), 
                       "MB, Available:", stats_.available_gpu_memory / (1024*1024), "MB");
    }
    
    check_memory_pressure();
}

void GPUMemoryManager::set_persistent(uint32_t handle, bool persistent) {
    std::lock_guard<std::mutex> lock(allocations_mutex_);
    
    auto it = allocations_.find(handle);
    if (it != allocations_.end()) {
        it->second.is_persistent = persistent;
        ve::log::debug("Set allocation", handle, "persistent:", persistent);
    }
}

void GPUMemoryManager::check_memory_pressure() {
    if (stats_.total_gpu_memory == 0) {
        return;  // Not initialized yet
    }
    
    float utilization = stats_.memory_utilization_percent() / 100.0f;
    MemoryPressure new_pressure;
    
    if (utilization >= 0.9f) {
        new_pressure = MemoryPressure::CRITICAL;
    } else if (utilization >= 0.8f) {
        new_pressure = MemoryPressure::HIGH;
    } else if (utilization >= 0.6f) {
        new_pressure = MemoryPressure::MEDIUM;
    } else {
        new_pressure = MemoryPressure::LOW;
    }
    
    if (new_pressure != current_pressure_) {
        MemoryPressure old_pressure = current_pressure_;
        current_pressure_ = new_pressure;
        
        ve::log::info("Memory pressure changed from", static_cast<int>(old_pressure), 
                      "to", static_cast<int>(new_pressure), "- Utilization:", utilization * 100.0f, "%");
        
        trigger_pressure_callback(new_pressure);
        
        // Automatic cleanup if pressure is high
        if (config_.enable_automatic_eviction && new_pressure >= MemoryPressure::HIGH) {
            perform_automatic_cleanup();
        }
    }
}

void GPUMemoryManager::trigger_pressure_callback(MemoryPressure level) {
    if (pressure_callback_) {
        try {
            pressure_callback_(level, stats_);
        } catch (const std::exception& e) {
            ve::log::error("Exception in memory pressure callback:", e.what());
        }
    }
}

bool GPUMemoryManager::perform_automatic_cleanup() {
    if (!config_.enable_automatic_eviction) {
        return false;
    }
    
    float utilization = stats_.memory_utilization_percent() / 100.0f;
    
    if (utilization < config_.high_watermark) {
        return false;  // No cleanup needed
    }
    
    // Calculate target free amount
    size_t target_used = static_cast<size_t>(stats_.total_gpu_memory * config_.low_watermark);
    size_t bytes_to_free = stats_.used_gpu_memory - target_used;
    
    bytes_to_free = std::max(bytes_to_free, config_.min_eviction_size);
    bytes_to_free = std::min(bytes_to_free, config_.max_eviction_size);
    
    ve::log::info("Performing automatic cleanup: freeing", bytes_to_free, "bytes");
    
    size_t freed = evict_least_recently_used(bytes_to_free);
    return freed > 0;
}

size_t GPUMemoryManager::calculate_fragmentation() const {
    // Simplified fragmentation calculation
    // In a real implementation, this would analyze actual memory layout
    
    size_t total_allocations = allocations_.size();
    if (total_allocations == 0) {
        return 0;
    }
    
    // Estimate fragmentation based on allocation count and available memory
    // More allocations = more potential fragmentation
    float fragmentation_ratio = std::min(1.0f, total_allocations / 1000.0f);
    return static_cast<size_t>(stats_.available_gpu_memory * fragmentation_ratio * 0.1f);
}

void GPUMemoryManager::background_management_thread() {
    ve::log::debug("GPU memory background management thread started");
    
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto now = std::chrono::steady_clock::now();
        
        // Periodic pressure checking
        auto pressure_check_interval = std::chrono::seconds(5);
        if (now - last_pressure_check_ >= pressure_check_interval) {
            check_memory_pressure();
            last_pressure_check_ = now;
        }
        
        // Periodic defragmentation
        if (config_.enable_defragmentation) {
            auto defrag_interval = std::chrono::seconds(config_.defrag_interval_seconds);
            if (now - last_defrag_time_ >= defrag_interval) {
                defragment_memory();
                last_defrag_time_ = now;
            }
        }
        
        // Preemptive cleanup if enabled
        if (config_.enable_preemptive_cleanup && current_pressure_ >= MemoryPressure::MEDIUM) {
            perform_automatic_cleanup();
        }
    }
    
    ve::log::debug("GPU memory background management thread finished");
}

bool GPUMemoryManager::defragment_memory() {
    // Simplified defragmentation placeholder
    // In a real implementation, this would reorganize GPU memory layout
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    size_t old_fragmentation = stats_.fragmentation_bytes;
    stats_.fragmentation_bytes = calculate_fragmentation();
    
    if (old_fragmentation > stats_.fragmentation_bytes) {
        ve::log::info("Memory defragmentation reduced fragmentation by", 
                      (old_fragmentation - stats_.fragmentation_bytes) / (1024*1024), "MB");
        return true;
    }
    
    return false;
}

const GPUAllocation* GPUMemoryManager::get_allocation_info(uint32_t handle) const {
    std::lock_guard<std::mutex> lock(allocations_mutex_);
    
    auto it = allocations_.find(handle);
    return (it != allocations_.end()) ? &it->second : nullptr;
}

std::vector<GPUAllocation> GPUMemoryManager::get_all_allocations() const {
    std::lock_guard<std::mutex> lock(allocations_mutex_);
    
    std::vector<GPUAllocation> result;
    result.reserve(allocations_.size());
    
    for (const auto& [handle, allocation] : allocations_) {
        result.push_back(allocation);
    }
    
    return result;
}

void GPUMemoryManager::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Preserve memory sizes but reset counters
    size_t total = stats_.total_gpu_memory;
    size_t available = stats_.available_gpu_memory;
    size_t used = stats_.used_gpu_memory;
    
    stats_.reset();
    stats_.total_gpu_memory = total;
    stats_.available_gpu_memory = available;
    stats_.used_gpu_memory = used;
    
    ve::log::info("GPU memory statistics reset");
}

void GPUMemoryManager::update_config(const Config& new_config) {
    config_ = new_config;
    ve::log::info("GPU memory manager configuration updated");
}

void GPUMemoryManager::set_automatic_management(bool enabled) {
    config_.enable_automatic_eviction = enabled;
    ve::log::info("Automatic GPU memory management", enabled ? "enabled" : "disabled");
}

} // namespace ve::gfx
