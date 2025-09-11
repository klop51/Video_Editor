// Graphics Device Foundation - Bridge Implementation
// Connects our GPU system design with existing GraphicsDevice class

#pragma once

#include "gfx/vk_device.hpp"
#include "advanced_shader_effects.hpp"
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <chrono>
#include <array>

namespace video_editor::gfx {

// Forward declarations
class TextureHandle;
class BufferHandle;
class ComputeShader;
class CommandBuffer;

// ============================================================================
// Graphics API Enumeration
// ============================================================================

enum class GraphicsAPI {
    D3D11,
    Vulkan,
    Auto  // Let system choose best available
};

// ============================================================================
// Texture and Buffer Descriptions
// ============================================================================

enum class TextureFormat {
    RGBA8,
    RGBA32F,
    R8,
    R32F,
    BGRA8
};

enum class TextureUsage {
    ShaderResource = 1,
    RenderTarget = 2,
    UnorderedAccess = 4
};

struct TextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    TextureUsage usage = TextureUsage::ShaderResource;
};

enum class BufferUsage {
    Vertex = 1,
    Index = 2,
    Constant = 4,
    UnorderedAccess = 8
};

struct BufferDesc {
    size_t size = 0;
    BufferUsage usage = BufferUsage::Vertex;
};

// ============================================================================
// Handle Classes (RAII wrappers for GPU resources)
// ============================================================================

class TextureHandle {
public:
    TextureHandle() : id_(0), valid_(false) {}
    explicit TextureHandle(uint32_t id) : id_(id), valid_(id != 0) {}
    
    bool is_valid() const { return valid_; }
    uint32_t get_id() const { return id_; }
    
    void invalidate() { valid_ = false; id_ = 0; }

private:
    uint32_t id_;
    bool valid_;
};

class BufferHandle {
public:
    BufferHandle() : id_(0), valid_(false) {}
    explicit BufferHandle(uint32_t id) : id_(id), valid_(id != 0) {}
    
    bool is_valid() const { return valid_; }
    uint32_t get_id() const { return id_; }
    
    void invalidate() { valid_ = false; id_ = 0; }

private:
    uint32_t id_;
    bool valid_;
};

// ============================================================================
// Compute Shader Support
// ============================================================================

struct ComputeShaderDesc {
    std::string source_code;
    std::string entry_point = "CSMain";
    std::string target_profile = "cs_5_0";
};

class ComputeShader {
public:
    ComputeShader(uint32_t id) : id_(id), valid_(id != 0) {}
    
    bool is_valid() const { return valid_; }
    uint32_t get_id() const { return id_; }

private:
    uint32_t id_;
    bool valid_;
};

// ============================================================================
// Command Buffer Support  
// ============================================================================

class CommandBuffer {
public:
    CommandBuffer() : recording_(false) {}
    
    void begin() { recording_ = true; }
    void end() { recording_ = false; }
    
    void set_render_target(const TextureHandle& target);
    void clear_render_target(const std::array<float, 4>& color);
    void set_compute_shader(ComputeShader* shader);
    void set_compute_texture(uint32_t slot, const TextureHandle& texture);
    void set_compute_buffer(uint32_t slot, const BufferHandle& buffer);
    void dispatch(uint32_t x, uint32_t y, uint32_t z);
    
    bool is_recording() const { return recording_; }

private:
    bool recording_;
};

// ============================================================================
// Bridge Graphics Device Class
// ============================================================================

class GraphicsDevice {
public:
    struct Config {
        GraphicsAPI preferred_api = GraphicsAPI::D3D11;
        bool enable_debug = false;
        bool enable_performance_monitoring = false;
    };
    
    static std::unique_ptr<GraphicsDevice> create(const Config& config);
    
    GraphicsDevice();
    ~GraphicsDevice();
    
    // Basic device operations
    bool is_valid() const;
    void wait_for_completion();
    
    // Resource creation
    TextureHandle create_texture(const TextureDesc& desc);
    BufferHandle create_buffer(const BufferDesc& desc);
    std::unique_ptr<ComputeShader> create_compute_shader(const ComputeShaderDesc& desc);
    std::unique_ptr<CommandBuffer> create_command_buffer();
    
    // Command execution
    void execute_command_buffer(CommandBuffer* cmd_buffer);
    
    // Memory information
    size_t get_total_memory() const;
    size_t get_available_memory() const;
    size_t get_used_memory() const;

private:
    // Bridge to existing implementation
    ve::gfx::GraphicsDevice impl_device_;
    bool initialized_;
    
    // Resource tracking
    std::vector<uint32_t> texture_ids_;
    std::vector<uint32_t> buffer_ids_;
    uint32_t next_texture_id_;
    uint32_t next_buffer_id_;  // Reserved for future buffer ID generation
    
    // Internal helpers
    bool initialize_impl(const Config& config);
    void cleanup_resources();
    uint32_t create_texture_impl(const TextureDesc& desc);
    uint32_t create_buffer_impl(const BufferDesc& desc);
    
    // Suppress unused field warnings
    void suppress_unused_warnings() const { (void)next_buffer_id_; }
};

// ============================================================================
// Effect Processing Classes (Stubs for GPU Test Suite)
// ============================================================================

class FilmGrainProcessor {
public:
    FilmGrainProcessor(GraphicsDevice* device) : device_(device) {}
    TextureHandle apply(const TextureHandle& input, const ::gfx::FilmGrainParams& params);

private:
    GraphicsDevice* device_;
};

class VignetteProcessor {
public:
    VignetteProcessor(GraphicsDevice* device) : device_(device) {}
    TextureHandle apply(const TextureHandle& input, const ::gfx::VignetteParams& params);

private:
    GraphicsDevice* device_;
};

class ChromaticAberrationProcessor {
public:
    struct ChromaticAberrationParams {
        float strength = 0.4f;
        float edge_falloff = 2.0f;
    };
    
    ChromaticAberrationProcessor(GraphicsDevice* device) : device_(device) {}
    TextureHandle apply(const TextureHandle& input, const ChromaticAberrationParams& params);

private:
    GraphicsDevice* device_;
};

class ColorGradingProcessor {
public:
    struct ColorWheelParams {
        std::array<float, 3> lift{0.0f, 0.0f, 0.0f};
        std::array<float, 3> gamma{1.0f, 1.0f, 1.0f};
        std::array<float, 3> gain{1.0f, 1.0f, 1.0f};
    };
    
    struct BezierCurveParams {
        std::vector<std::pair<float, float>> red_curve;
        std::vector<std::pair<float, float>> green_curve;
        std::vector<std::pair<float, float>> blue_curve;
    };
    
    struct HSLQualifierParams {
        float hue_center = 0.5f;
        float hue_width = 0.1f;
        float saturation_boost = 1.0f;
    };
    
    ColorGradingProcessor(GraphicsDevice* device) : device_(device) {}
    TextureHandle apply_color_wheels(const TextureHandle& input, const ColorWheelParams& params);
    TextureHandle apply_curves(const TextureHandle& input, const BezierCurveParams& params);
    TextureHandle apply_hsl_qualifier(const TextureHandle& input, const HSLQualifierParams& params);

private:
    GraphicsDevice* device_;
};

// ============================================================================
// Forward Declarations for GPU Memory Optimizer
// ============================================================================

// GPUMemoryOptimizer is defined in gpu_memory_optimizer.hpp
// This forward declaration avoids circular dependencies
class GPUMemoryOptimizer;

} // namespace video_editor::gfx
