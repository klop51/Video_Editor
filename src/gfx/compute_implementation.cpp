// Week 10: Complete Compute Shader System Implementation
// Advanced GPU compute capabilities with full parallel processing

#include "compute_shader_system.hpp"
#include "parallel_effects.hpp"
#include "gpu_histogram_temporal.hpp"
#include "core/logging.hpp"
#include "core/profiling.hpp"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace VideoEditor::GFX {

// =============================================================================
// Compute Context Implementation
// =============================================================================

Core::Result<void> ComputeContext::initialize(GraphicsDevice* device) {
    try {
        device_ = device;
        
        auto d3d_device = device->get_d3d_device();
        if (!d3d_device) {
            return Core::Result<void>::error("Invalid D3D device");
        }

        // Get immediate context
        d3d_device->GetImmediateContext(immediate_context_.GetAddressOf());

        // Create deferred context for batch operations
        HRESULT hr = d3d_device->CreateDeferredContext(0, deferred_context_.GetAddressOf());
        if (FAILED(hr)) {
            Core::Logger::warning("ComputeContext", "Failed to create deferred context, using immediate context only");
            deferred_context_ = nullptr;
        }

        // Initialize resource arrays
        active_shaders_.reserve(32);
        active_buffers_.reserve(128);
        active_textures_.reserve(64);

        Core::Logger::info("ComputeContext", "Compute context initialized successfully");
        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in ComputeContext::initialize: " + std::string(e.what()));
    }
}

void ComputeContext::shutdown() {
    // Wait for any pending operations
    if (batch_mode_) {
        end_batch();
    }

    // Clear all resources
    active_textures_.clear();
    active_buffers_.clear();
    active_shaders_.clear();
    batched_operations_.clear();

    deferred_context_.Reset();
    immediate_context_.Reset();
    device_ = nullptr;

    Core::Logger::info("ComputeContext", "Compute context shutdown complete");
}

std::unique_ptr<ComputeShader> ComputeContext::create_shader() {
    auto shader = std::make_unique<ComputeShader>();
    return shader;
}

std::unique_ptr<ComputeBuffer> ComputeContext::create_buffer() {
    auto buffer = std::make_unique<ComputeBuffer>();
    return buffer;
}

std::unique_ptr<ComputeTexture> ComputeContext::create_texture() {
    auto texture = std::make_unique<ComputeTexture>();
    return texture;
}

void ComputeContext::begin_batch() {
    batch_mode_ = true;
    batched_operations_.clear();
    
    if (profiling_enabled_) {
        Core::Logger::debug("ComputeContext", "Batch mode enabled");
    }
}

void ComputeContext::end_batch() {
    if (!batch_mode_) return;
    
    batch_mode_ = false;
    
    if (profiling_enabled_) {
        Core::Logger::debug("ComputeContext", "Batch mode disabled, {} operations batched", 
                           batched_operations_.size());
    }
}

Core::Result<void> ComputeContext::execute_batch() {
    if (batched_operations_.empty()) {
        return Core::Result<void>::success();
    }

    try {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Execute all batched operations
        for (auto& operation : batched_operations_) {
            operation();
        }

        // Clear batch
        batched_operations_.clear();

        auto end_time = std::chrono::high_resolution_clock::now();
        float execution_time = std::chrono::duration<float, std::milli>(end_time - start_time).count();

        if (profiling_enabled_) {
            accumulated_metrics_.total_time_ms += execution_time;
            Core::Logger::debug("ComputeContext", "Batch executed in {:.2f}ms", execution_time);
        }

        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in execute_batch: " + std::string(e.what()));
    }
}

void ComputeContext::flush_gpu_cache() {
    if (immediate_context_) {
        immediate_context_->Flush();
    }
}

size_t ComputeContext::get_gpu_memory_usage() const {
    size_t total_memory = 0;
    
    // Estimate memory usage from active resources
    for (const auto& buffer : active_buffers_) {
        if (buffer) {
            total_memory += buffer->get_total_size();
        }
    }
    
    for (const auto& texture : active_textures_) {
        if (texture) {
            // Estimate texture memory usage
            uint32_t pixel_size = 4; // Assume 4 bytes per pixel for estimation
            if (texture->is_3d()) {
                total_memory += texture->get_width() * texture->get_height() * texture->get_depth() * pixel_size;
            } else {
                total_memory += texture->get_width() * texture->get_height() * pixel_size;
            }
        }
    }
    
    return total_memory;
}

void ComputeContext::cleanup_temporary_resources() {
    // Remove null pointers from active resource lists
    active_shaders_.erase(
        std::remove_if(active_shaders_.begin(), active_shaders_.end(),
                      [](const std::unique_ptr<ComputeShader>& shader) { return !shader || !shader->is_valid(); }),
        active_shaders_.end()
    );

    active_buffers_.erase(
        std::remove_if(active_buffers_.begin(), active_buffers_.end(),
                      [](const std::unique_ptr<ComputeBuffer>& buffer) { return !buffer || !buffer->get_buffer(); }),
        active_buffers_.end()
    );

    active_textures_.erase(
        std::remove_if(active_textures_.begin(), active_textures_.end(),
                      [](const std::unique_ptr<ComputeTexture>& texture) { 
                          return !texture || (!texture->get_texture_2d() && !texture->get_texture_3d()); }),
        active_textures_.end()
    );

    if (profiling_enabled_) {
        Core::Logger::debug("ComputeContext", "Cleaned up temporary resources. Active: {} shaders, {} buffers, {} textures",
                           active_shaders_.size(), active_buffers_.size(), active_textures_.size());
    }
}

void ComputeContext::reset_performance_metrics() {
    accumulated_metrics_ = ComputePerformanceMetrics{};
}

// =============================================================================
// Compute Shader System Implementation
// =============================================================================

Core::Result<void> ComputeShaderSystem::initialize(GraphicsDevice* device) {
    try {
        device_ = device;
        
        // Create primary context
        primary_context_ = std::make_unique<ComputeContext>();
        auto result = primary_context_->initialize(device);
        if (!result.is_success()) {
            return result;
        }

        // Query compute capabilities
        query_compute_capabilities();
        
        // Setup performance monitoring
        setup_performance_monitoring();
        
        // Precompile common shaders
        precompile_common_shaders();

        Core::Logger::info("ComputeShaderSystem", "Compute shader system initialized");
        Core::Logger::info("ComputeShaderSystem", "Max thread groups: {}x{}x{}", 
                          capabilities_.max_thread_groups_x, 
                          capabilities_.max_thread_groups_y, 
                          capabilities_.max_thread_groups_z);
        Core::Logger::info("ComputeShaderSystem", "Max threads per group: {}", capabilities_.max_threads_per_group);
        Core::Logger::info("ComputeShaderSystem", "Shared memory size: {} bytes", capabilities_.max_shared_memory_size);

        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in ComputeShaderSystem::initialize: " + std::string(e.what()));
    }
}

void ComputeShaderSystem::shutdown() {
    shader_library_.clear();
    primary_context_.reset();
    device_ = nullptr;
    
    Core::Logger::info("ComputeShaderSystem", "Compute shader system shutdown complete");
}

std::unique_ptr<ComputeContext> ComputeShaderSystem::create_context() {
    auto context = std::make_unique<ComputeContext>();
    auto result = context->initialize(device_);
    
    if (!result.is_success()) {
        Core::Logger::error("ComputeShaderSystem", "Failed to create compute context: {}", result.error());
        return nullptr;
    }
    
    return context;
}

Core::Result<ComputeShader*> ComputeShaderSystem::load_shader(const std::string& name, const std::string& file_path) {
    try {
        auto shader = std::make_unique<ComputeShader>();
        auto result = shader->create_from_file(device_, file_path);
        
        if (!result.is_success()) {
            return Core::Result<ComputeShader*>::error(result.error());
        }

        ComputeShader* shader_ptr = shader.get();
        shader_library_[name] = std::move(shader);
        
        Core::Logger::info("ComputeShaderSystem", "Loaded shader: {} from {}", name, file_path);
        return Core::Result<ComputeShader*>::success(shader_ptr);

    } catch (const std::exception& e) {
        return Core::Result<ComputeShader*>::error("Exception in load_shader: " + std::string(e.what()));
    }
}

ComputeShader* ComputeShaderSystem::get_shader(const std::string& name) {
    auto it = shader_library_.find(name);
    return (it != shader_library_.end()) ? it->second.get() : nullptr;
}

void ComputeShaderSystem::precompile_common_shaders() {
    // Common compute shader sources for immediate use
    std::vector<std::pair<std::string, std::string>> common_shaders = {
        {"copy_texture", R"(
            Texture2D<float4> InputTexture : register(t0);
            RWTexture2D<float4> OutputTexture : register(u0);
            
            [numthreads(8, 8, 1)]
            void cs_main(uint3 id : SV_DispatchThreadID) {
                OutputTexture[id.xy] = InputTexture[id.xy];
            }
        )"},
        
        {"clear_buffer", R"(
            RWBuffer<uint> OutputBuffer : register(u0);
            
            cbuffer ClearConstants : register(b0) {
                uint ClearValue;
                uint BufferSize;
            };
            
            [numthreads(64, 1, 1)]
            void cs_main(uint3 id : SV_DispatchThreadID) {
                if (id.x < BufferSize) {
                    OutputBuffer[id.x] = ClearValue;
                }
            }
        )"},
        
        {"luminance_histogram", R"(
            Texture2D<float4> InputTexture : register(t0);
            RWBuffer<uint> HistogramBuffer : register(u0);
            
            cbuffer HistogramConstants : register(b0) {
                uint ImageWidth;
                uint ImageHeight;
                uint HistogramBins;
                float pad;
            };
            
            groupshared uint LocalHistogram[256];
            
            [numthreads(16, 16, 1)]
            void cs_main(uint3 id : SV_DispatchThreadID, uint3 gid : SV_GroupThreadID, uint gindex : SV_GroupIndex) {
                // Clear local histogram
                if (gindex < 256) {
                    LocalHistogram[gindex] = 0;
                }
                GroupMemoryBarrierWithGroupSync();
                
                // Process pixel
                if (id.x < ImageWidth && id.y < ImageHeight) {
                    float4 color = InputTexture[id.xy];
                    float luminance = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
                    uint bin = min(uint(luminance * (HistogramBins - 1)), HistogramBins - 1);
                    InterlockedAdd(LocalHistogram[bin], 1);
                }
                GroupMemoryBarrierWithGroupSync();
                
                // Write to global histogram
                if (gindex < 256) {
                    InterlockedAdd(HistogramBuffer[gindex], LocalHistogram[gindex]);
                }
            }
        )"}
    };

    for (const auto& [name, source] : common_shaders) {
        try {
            auto shader = std::make_unique<ComputeShader>();
            ComputeShaderDesc desc;
            desc.shader_source = source;
            desc.entry_point = "cs_main";
            
            auto result = shader->create_from_source(device_, desc);
            if (result.is_success()) {
                shader_library_[name] = std::move(shader);
                Core::Logger::debug("ComputeShaderSystem", "Precompiled shader: {}", name);
            } else {
                Core::Logger::warning("ComputeShaderSystem", "Failed to precompile shader {}: {}", name, result.error());
            }
        } catch (const std::exception& e) {
            Core::Logger::warning("ComputeShaderSystem", "Exception precompiling shader {}: {}", name, e.what());
        }
    }
}

void ComputeShaderSystem::query_compute_capabilities() {
    auto d3d_device = device_->get_d3d_device();
    if (!d3d_device) {
        Core::Logger::warning("ComputeShaderSystem", "Cannot query compute capabilities - invalid device");
        return;
    }

    // Get feature level
    D3D_FEATURE_LEVEL feature_level = d3d_device->GetFeatureLevel();
    
    // Set capabilities based on feature level
    switch (feature_level) {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            capabilities_.max_thread_groups_x = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
            capabilities_.max_thread_groups_y = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
            capabilities_.max_thread_groups_z = D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
            capabilities_.max_threads_per_group = D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
            capabilities_.max_shared_memory_size = D3D11_CS_THREAD_GROUP_SHARED_MEMORY_SIZE;
            capabilities_.supports_double_precision = true;
            capabilities_.supports_atomic_operations = true;
            capabilities_.supports_wave_intrinsics = false;
            capabilities_.wave_size = 32; // Typical for most GPUs
            break;
            
        default:
            // Conservative defaults for older hardware
            capabilities_.max_thread_groups_x = 65535;
            capabilities_.max_thread_groups_y = 65535;
            capabilities_.max_thread_groups_z = 1;
            capabilities_.max_threads_per_group = 512;
            capabilities_.max_shared_memory_size = 16384;
            capabilities_.supports_double_precision = false;
            capabilities_.supports_atomic_operations = true;
            capabilities_.supports_wave_intrinsics = false;
            capabilities_.wave_size = 32;
            break;
    }

    // Try to query additional capabilities
    try {
        D3D11_FEATURE_DATA_D3D11_OPTIONS options = {};
        HRESULT hr = d3d_device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options));
        if (SUCCEEDED(hr)) {
            // Update capabilities based on actual hardware support
        }
    } catch (...) {
        // Ignore query failures
    }
}

void ComputeShaderSystem::setup_performance_monitoring() {
    system_metrics_ = ComputePerformanceMetrics{};
    last_metrics_update_ = std::chrono::steady_clock::now();
    system_profiling_enabled_ = false;
}

ComputePerformanceMetrics ComputeShaderSystem::get_system_metrics() const {
    auto now = std::chrono::steady_clock::now();
    float time_delta = std::chrono::duration<float>(now - last_metrics_update_).count();
    
    // Update derived metrics
    ComputePerformanceMetrics metrics = system_metrics_;
    if (time_delta > 0.0f) {
        metrics.gpu_utilization_percent = (metrics.gpu_execution_time_ms / (time_delta * 1000.0f)) * 100.0f;
        metrics.memory_bandwidth_gb_s = ComputeUtils::estimate_memory_bandwidth_gb_s(
            metrics.memory_transferred_bytes, metrics.gpu_execution_time_ms);
    }
    
    return metrics;
}

void ComputeShaderSystem::enable_system_profiling(bool enabled) {
    system_profiling_enabled_ = enabled;
    if (primary_context_) {
        primary_context_->enable_profiling(enabled);
    }
    
    Core::Logger::info("ComputeShaderSystem", "System profiling {}", enabled ? "enabled" : "disabled");
}

void ComputeShaderSystem::log_performance_summary() const {
    auto metrics = get_system_metrics();
    
    Core::Logger::info("ComputeShaderSystem", "=== Compute Performance Summary ===");
    Core::Logger::info("ComputeShaderSystem", "Total GPU time: {:.2f}ms", metrics.gpu_execution_time_ms);
    Core::Logger::info("ComputeShaderSystem", "Memory usage: {:.2f} MB", metrics.memory_used_bytes / (1024.0f * 1024.0f));
    Core::Logger::info("ComputeShaderSystem", "Memory bandwidth: {:.2f} GB/s", metrics.memory_bandwidth_gb_s);
    Core::Logger::info("ComputeShaderSystem", "GPU utilization: {:.1f}%", metrics.gpu_utilization_percent);
    Core::Logger::info("ComputeShaderSystem", "Operations/second: {}", metrics.operations_per_second);
    Core::Logger::info("ComputeShaderSystem", "Active thread groups: {}", metrics.active_thread_groups);
    
    if (!metrics.effect_timings.empty()) {
        Core::Logger::info("ComputeShaderSystem", "Per-effect timings:");
        for (const auto& [effect_name, timing] : metrics.effect_timings) {
            Core::Logger::info("ComputeShaderSystem", "  {}: {:.2f}ms", effect_name, timing);
        }
    }
}

// =============================================================================
// Effect Chain Implementation Excerpt
// =============================================================================

Core::Result<void> EffectChain::initialize(ComputeShaderSystem* compute_system) {
    compute_system_ = compute_system;
    effects_.clear();
    intermediate_textures_.clear();
    
    last_metrics_ = EffectPerformanceMetrics{};
    profiling_enabled_ = false;
    last_width_ = 0;
    last_height_ = 0;
    last_color_space_ = EffectColorSpace::RGB;
    
    Core::Logger::info("EffectChain", "Effect chain initialized");
    return Core::Result<void>::success();
}

Core::Result<void> EffectChain::add_effect(std::unique_ptr<ParallelEffect> effect) {
    if (!effect) {
        return Core::Result<void>::error("Cannot add null effect");
    }
    
    auto result = effect->initialize(compute_system_);
    if (!result.is_success()) {
        return Core::Result<void>::error("Failed to initialize effect: " + result.error());
    }
    
    EffectInstance instance;
    instance.effect = std::move(effect);
    instance.parameters = instance.effect->get_default_parameters();
    instance.enabled = true;
    
    effects_.push_back(std::move(instance));
    
    Core::Logger::info("EffectChain", "Added effect: {} (total: {})", 
                      effects_.back().effect->get_name(), effects_.size());
    
    return Core::Result<void>::success();
}

Core::Result<EffectPerformanceMetrics> EffectChain::process_chain(
    ComputeTexture* input,
    ComputeTexture* output,
    const EffectRenderInfo& render_info) {
    
    if (!input || !output) {
        return Core::Result<EffectPerformanceMetrics>::error("Invalid input or output texture");
    }
    
    if (effects_.empty()) {
        // No effects, just copy input to output
        auto copy_shader = compute_system_->get_shader("copy_texture");
        if (copy_shader) {
            copy_shader->bind_texture_srv(0, input);
            copy_shader->bind_texture_uav(0, output);
            auto result = copy_shader->dispatch_2d(render_info.input_width, render_info.input_height);
            if (result.is_success()) {
                return Core::Result<EffectPerformanceMetrics>::success(result.value());
            }
        }
        return Core::Result<EffectPerformanceMetrics>::success(EffectPerformanceMetrics{});
    }
    
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        EffectPerformanceMetrics combined_metrics = {};
        
        // Allocate intermediate textures if needed
        if (render_info.input_width != last_width_ || 
            render_info.input_height != last_height_ || 
            render_info.color_space != last_color_space_) {
            
            auto result = allocate_intermediate_textures(render_info.input_width, 
                                                        render_info.input_height, 
                                                        render_info.color_space);
            if (!result.is_success()) {
                return Core::Result<EffectPerformanceMetrics>::error(result.error());
            }
            
            last_width_ = render_info.input_width;
            last_height_ = render_info.input_height;
            last_color_space_ = render_info.color_space;
        }
        
        // Process effects in chain
        ComputeTexture* current_input = input;
        ComputeTexture* current_output = nullptr;
        
        for (size_t i = 0; i < effects_.size(); ++i) {
            auto& effect_instance = effects_[i];
            
            if (!effect_instance.enabled || !effect_instance.effect->is_enabled()) {
                continue;
            }
            
            // Determine output texture
            if (i == effects_.size() - 1) {
                current_output = output;  // Last effect outputs to final output
            } else {
                current_output = intermediate_textures_[i % intermediate_textures_.size()].get();
            }
            
            // Process effect
            auto effect_start = std::chrono::high_resolution_clock::now();
            auto result = effect_instance.effect->process(
                current_input, current_output, 
                effect_instance.parameters, render_info);
            auto effect_end = std::chrono::high_resolution_clock::now();
            
            if (!result.is_success()) {
                return Core::Result<EffectPerformanceMetrics>::error(
                    "Effect " + effect_instance.effect->get_name() + " failed: " + result.error());
            }
            
            // Accumulate metrics
            auto effect_metrics = result.value();
            combined_metrics.total_time_ms += effect_metrics.total_time_ms;
            combined_metrics.gpu_time_ms += effect_metrics.gpu_time_ms;
            combined_metrics.cpu_time_ms += effect_metrics.cpu_time_ms;
            combined_metrics.memory_used_bytes += effect_metrics.memory_used_bytes;
            combined_metrics.dispatches_count += effect_metrics.dispatches_count;
            
            if (profiling_enabled_) {
                float effect_time = std::chrono::duration<float, std::milli>(effect_end - effect_start).count();
                combined_metrics.effect_timings.emplace_back(effect_instance.effect->get_name(), effect_time);
            }
            
            // Set up for next iteration
            current_input = current_output;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        combined_metrics.total_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
        
        last_metrics_ = combined_metrics;
        
        if (profiling_enabled_) {
            Core::Logger::debug("EffectChain", "Processed chain with {} effects in {:.2f}ms", 
                               effects_.size(), combined_metrics.total_time_ms);
        }
        
        return Core::Result<EffectPerformanceMetrics>::success(combined_metrics);
        
    } catch (const std::exception& e) {
        return Core::Result<EffectPerformanceMetrics>::error("Exception in process_chain: " + std::string(e.what()));
    }
}

Core::Result<void> EffectChain::allocate_intermediate_textures(uint32_t width, uint32_t height, 
                                                              EffectColorSpace color_space) {
    // Clear existing textures
    intermediate_textures_.clear();
    
    if (effects_.size() <= 1) {
        return Core::Result<void>::success(); // No intermediate textures needed
    }
    
    try {
        DXGI_FORMAT format = get_texture_format(color_space);
        
        // Create intermediate textures (we need at most effects.size() - 1)
        size_t num_intermediates = std::min(effects_.size() - 1, size_t(4)); // Limit to 4 for memory efficiency
        
        for (size_t i = 0; i < num_intermediates; ++i) {
            auto texture = std::make_unique<ComputeTexture>();
            auto result = texture->create_2d(compute_system_->get_primary_context()->device_, 
                                           width, height, format, true);
            if (!result.is_success()) {
                return Core::Result<void>::error("Failed to create intermediate texture: " + result.error());
            }
            
            intermediate_textures_.push_back(std::move(texture));
        }
        
        Core::Logger::debug("EffectChain", "Allocated {} intermediate textures ({}x{})", 
                           num_intermediates, width, height);
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in allocate_intermediate_textures: " + std::string(e.what()));
    }
}

DXGI_FORMAT EffectChain::get_texture_format(EffectColorSpace color_space) const {
    switch (color_space) {
        case EffectColorSpace::RGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case EffectColorSpace::YUV420:
        case EffectColorSpace::YUV422:
        case EffectColorSpace::YUV444:
            return DXGI_FORMAT_R8G8B8A8_UNORM; // Convert to RGB for processing
        case EffectColorSpace::HDR10:
        case EffectColorSpace::REC2020:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

} // namespace VideoEditor::GFX
