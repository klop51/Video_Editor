// Week 7: Render Graph Implementation
// Scalable effects pipeline with node-based composition

#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

namespace ve::gfx {

// Forward declarations
class GraphicsDevice;
struct RenderContext;

// Handle types for GPU resources
using TextureHandle = unsigned int;
using ShaderHandle = unsigned int;
using BufferHandle = unsigned int;

// Base class for all effect nodes
class EffectNode {
public:
    virtual ~EffectNode() = default;
    
    // Core interface
    virtual void render(RenderContext& ctx) = 0;
    virtual void set_input(int slot, TextureHandle texture) = 0;
    virtual TextureHandle get_output() const = 0;
    virtual uint64_t get_hash() const = 0;  // For caching
    
    // Node management
    virtual std::string get_name() const = 0;
    virtual int get_input_count() const = 0;
    virtual int get_output_count() const = 0;
    
    // Dependencies for optimization
    virtual std::vector<EffectNode*> get_dependencies() const { return dependencies_; }
    virtual void add_dependency(EffectNode* node) { dependencies_.push_back(node); }
    
    // Enable/disable for conditional rendering
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
protected:
    std::vector<EffectNode*> dependencies_;
    bool enabled_ = true;
    mutable uint64_t cached_hash_ = 0;
};

// Render context for passing state between nodes
struct RenderContext {
    GraphicsDevice* device = nullptr;
    TextureHandle current_render_target = 0;
    int viewport_width = 1920;
    int viewport_height = 1080;
    
    // Performance tracking
    float frame_time_ms = 0.0f;
    int effects_rendered = 0;
    
    // Resource management
    std::vector<TextureHandle> intermediate_textures;
    int current_intermediate_index = 0;
    
    TextureHandle get_intermediate_texture() {
        if (current_intermediate_index < intermediate_textures.size()) {
            return intermediate_textures[current_intermediate_index++];
        }
        return 0;  // Failed to allocate
    }
    
    void reset_intermediate_textures() {
        current_intermediate_index = 0;
    }
};

// Color Correction Node Implementation
class ColorCorrectionNode : public EffectNode {
public:
    struct Params {
        float brightness = 0.0f;     // -1.0 to 1.0
        float contrast = 1.0f;       // 0.0 to 2.0
        float saturation = 1.0f;     // 0.0 to 2.0
        float gamma = 1.0f;          // 0.1 to 3.0
        float shadows[3] = {1.0f, 1.0f, 1.0f};
        float midtones[3] = {1.0f, 1.0f, 1.0f};
        float highlights[3] = {1.0f, 1.0f, 1.0f};
        float shadow_range = 0.3f;
        float highlight_range = 0.3f;
    };
    
    ColorCorrectionNode();
    ~ColorCorrectionNode() override = default;
    
    void render(RenderContext& ctx) override;
    void set_input(int slot, TextureHandle texture) override;
    TextureHandle get_output() const override { return output_texture_; }
    uint64_t get_hash() const override;
    
    std::string get_name() const override { return "ColorCorrection"; }
    int get_input_count() const override { return 1; }
    int get_output_count() const override { return 1; }
    
    // Parameter control
    void set_params(const Params& params) { params_ = params; invalidate_cache(); }
    const Params& get_params() const { return params_; }
    
private:
    TextureHandle input_texture_ = 0;
    TextureHandle output_texture_ = 0;
    Params params_;
    
    void invalidate_cache() { cached_hash_ = 0; }
    uint64_t calculate_hash() const;
};

// Blur Node Implementation (two-pass Gaussian)
class BlurNode : public EffectNode {
public:
    struct Params {
        float radius = 5.0f;
        int quality = 1;  // 0=fast, 1=normal, 2=high
    };
    
    BlurNode();
    ~BlurNode() override = default;
    
    void render(RenderContext& ctx) override;
    void set_input(int slot, TextureHandle texture) override;
    TextureHandle get_output() const override { return output_texture_; }
    uint64_t get_hash() const override;
    
    std::string get_name() const override { return "GaussianBlur"; }
    int get_input_count() const override { return 1; }
    int get_output_count() const override { return 1; }
    
    void set_params(const Params& params) { params_ = params; invalidate_cache(); }
    const Params& get_params() const { return params_; }
    
private:
    TextureHandle input_texture_ = 0;
    TextureHandle intermediate_texture_ = 0;  // For two-pass blur
    TextureHandle output_texture_ = 0;
    Params params_;
    
    void invalidate_cache() { cached_hash_ = 0; }
    uint64_t calculate_hash() const;
};

// Transform Node Implementation
class TransformNode : public EffectNode {
public:
    struct Params {
        float scale[2] = {1.0f, 1.0f};
        float rotation = 0.0f;  // radians
        float translation[2] = {0.0f, 0.0f};
        float anchor_point[2] = {0.5f, 0.5f};
        float crop_rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};  // x, y, width, height
    };
    
    TransformNode();
    ~TransformNode() override = default;
    
    void render(RenderContext& ctx) override;
    void set_input(int slot, TextureHandle texture) override;
    TextureHandle get_output() const override { return output_texture_; }
    uint64_t get_hash() const override;
    
    std::string get_name() const override { return "Transform"; }
    int get_input_count() const override { return 1; }
    int get_output_count() const override { return 1; }
    
    void set_params(const Params& params) { params_ = params; invalidate_cache(); }
    const Params& get_params() const { return params_; }
    
private:
    TextureHandle input_texture_ = 0;
    TextureHandle output_texture_ = 0;
    Params params_;
    
    void invalidate_cache() { cached_hash_ = 0; }
    uint64_t calculate_hash() const;
    float* get_transform_matrix() const;  // Calculate 4x4 matrix from params
};

// LUT Node Implementation
class LUTNode : public EffectNode {
public:
    struct Params {
        float strength = 1.0f;  // 0.0 to 1.0 blend
    };
    
    LUTNode();
    ~LUTNode() override = default;
    
    void render(RenderContext& ctx) override;
    void set_input(int slot, TextureHandle texture) override;
    TextureHandle get_output() const override { return output_texture_; }
    uint64_t get_hash() const override;
    
    std::string get_name() const override { return "3DLUT"; }
    int get_input_count() const override { return 2; }  // Input texture + LUT texture
    int get_output_count() const override { return 1; }
    
    void set_params(const Params& params) { params_ = params; invalidate_cache(); }
    const Params& get_params() const { return params_; }
    void set_lut_texture(TextureHandle lut) { lut_texture_ = lut; invalidate_cache(); }
    
private:
    TextureHandle input_texture_ = 0;
    TextureHandle lut_texture_ = 0;
    TextureHandle output_texture_ = 0;
    Params params_;
    
    void invalidate_cache() { cached_hash_ = 0; }
    uint64_t calculate_hash() const;
};

// Mix/Blend Node for combining multiple inputs
class MixNode : public EffectNode {
public:
    enum class BlendMode {
        NORMAL,
        MULTIPLY,
        SCREEN,
        OVERLAY,
        SOFT_LIGHT,
        HARD_LIGHT,
        ADD,
        SUBTRACT
    };
    
    struct Params {
        BlendMode blend_mode = BlendMode::NORMAL;
        float opacity = 1.0f;  // 0.0 to 1.0
    };
    
    MixNode();
    ~MixNode() override = default;
    
    void render(RenderContext& ctx) override;
    void set_input(int slot, TextureHandle texture) override;
    TextureHandle get_output() const override { return output_texture_; }
    uint64_t get_hash() const override;
    
    std::string get_name() const override { return "Mix"; }
    int get_input_count() const override { return 2; }  // Base + Overlay
    int get_output_count() const override { return 1; }
    
    void set_params(const Params& params) { params_ = params; invalidate_cache(); }
    const Params& get_params() const { return params_; }
    
private:
    TextureHandle base_texture_ = 0;
    TextureHandle overlay_texture_ = 0;
    TextureHandle output_texture_ = 0;
    Params params_;
    
    void invalidate_cache() { cached_hash_ = 0; }
    uint64_t calculate_hash() const;
};

} // namespace ve::gfx
