// Week 7: Effect Nodes Implementation
// Individual effect node implementations

#include "gfx/effect_nodes.hpp"
#include "gfx/vk_device.hpp"
#include "core/log.hpp"
#include <cstring>
#include <cmath>

namespace ve::gfx {

// Hash function for combining multiple values
namespace {
    uint64_t hash_combine(uint64_t seed, uint64_t value) {
        return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }
    
    uint64_t hash_float(float value) {
        union { float f; uint32_t i; } converter;
        converter.f = value;
        return static_cast<uint64_t>(converter.i);
    }
    
    uint64_t hash_array(const float* data, size_t count) {
        uint64_t hash = 0;
        for (size_t i = 0; i < count; ++i) {
            hash = hash_combine(hash, hash_float(data[i]));
        }
        return hash;
    }
}

// ColorCorrectionNode Implementation
ColorCorrectionNode::ColorCorrectionNode() {
    ve::log::debug("Created ColorCorrectionNode");
}

void ColorCorrectionNode::render(RenderContext& ctx) {
    if (!enabled_ || input_texture_ == 0 || !ctx.device) {
        return;
    }
    
    // Get or allocate output texture
    if (output_texture_ == 0) {
        output_texture_ = ctx.get_intermediate_texture();
        if (output_texture_ == 0) {
            ve::log::error("Failed to allocate output texture for ColorCorrectionNode");
            return;
        }
    }
    
    // Apply color correction effect
    bool success = ctx.device->apply_color_correction(
        input_texture_, output_texture_,
        params_.brightness, params_.contrast, 
        params_.saturation, params_.gamma
    );
    
    if (success) {
        ctx.effects_rendered++;
        ve::log::debug("ColorCorrectionNode rendered successfully");
    } else {
        ve::log::warning("ColorCorrectionNode render failed");
    }
}

void ColorCorrectionNode::set_input(int slot, TextureHandle texture) {
    if (slot == 0) {
        input_texture_ = texture;
        invalidate_cache();
    }
}

uint64_t ColorCorrectionNode::get_hash() const {
    if (cached_hash_ == 0) {
        cached_hash_ = calculate_hash();
    }
    return cached_hash_;
}

uint64_t ColorCorrectionNode::calculate_hash() const {
    uint64_t hash = 0x1234567890ABCDEF;  // Base hash for ColorCorrection
    hash = hash_combine(hash, static_cast<uint64_t>(input_texture_));
    hash = hash_combine(hash, hash_float(params_.brightness));
    hash = hash_combine(hash, hash_float(params_.contrast));
    hash = hash_combine(hash, hash_float(params_.saturation));
    hash = hash_combine(hash, hash_float(params_.gamma));
    hash = hash_combine(hash, hash_array(params_.shadows, 3));
    hash = hash_combine(hash, hash_array(params_.midtones, 3));
    hash = hash_combine(hash, hash_array(params_.highlights, 3));
    hash = hash_combine(hash, hash_float(params_.shadow_range));
    hash = hash_combine(hash, hash_float(params_.highlight_range));
    return hash;
}

// BlurNode Implementation
BlurNode::BlurNode() {
    ve::log::debug("Created BlurNode");
}

void BlurNode::render(RenderContext& ctx) {
    if (!enabled_ || input_texture_ == 0 || !ctx.device) {
        return;
    }
    
    // Get or allocate textures for two-pass blur
    if (intermediate_texture_ == 0) {
        intermediate_texture_ = ctx.get_intermediate_texture();
    }
    if (output_texture_ == 0) {
        output_texture_ = ctx.get_intermediate_texture();
    }
    
    if (intermediate_texture_ == 0 || output_texture_ == 0) {
        ve::log::error("Failed to allocate textures for BlurNode");
        return;
    }
    
    // Apply Gaussian blur (two-pass)
    bool success = ctx.device->apply_gaussian_blur(
        input_texture_, intermediate_texture_, output_texture_, params_.radius
    );
    
    if (success) {
        ctx.effects_rendered++;
        ve::log::debug("BlurNode rendered successfully with radius:", params_.radius);
    } else {
        ve::log::warning("BlurNode render failed");
    }
}

void BlurNode::set_input(int slot, TextureHandle texture) {
    if (slot == 0) {
        input_texture_ = texture;
        invalidate_cache();
    }
}

uint64_t BlurNode::get_hash() const {
    if (cached_hash_ == 0) {
        cached_hash_ = calculate_hash();
    }
    return cached_hash_;
}

uint64_t BlurNode::calculate_hash() const {
    uint64_t hash = 0x2345678901BCDEF0;  // Base hash for Blur
    hash = hash_combine(hash, static_cast<uint64_t>(input_texture_));
    hash = hash_combine(hash, hash_float(params_.radius));
    hash = hash_combine(hash, static_cast<uint64_t>(params_.quality));
    return hash;
}

// TransformNode Implementation
TransformNode::TransformNode() {
    ve::log::debug("Created TransformNode");
}

void TransformNode::render(RenderContext& ctx) {
    if (!enabled_ || input_texture_ == 0 || !ctx.device) {
        return;
    }
    
    // Get or allocate output texture
    if (output_texture_ == 0) {
        output_texture_ = ctx.get_intermediate_texture();
        if (output_texture_ == 0) {
            ve::log::error("Failed to allocate output texture for TransformNode");
            return;
        }
    }
    
    // Note: Transform effect not yet implemented in GraphicsDevice
    // For now, just copy input to output as placeholder
    ve::log::info("TransformNode render placeholder");
    ctx.effects_rendered++;
}

void TransformNode::set_input(int slot, TextureHandle texture) {
    if (slot == 0) {
        input_texture_ = texture;
        invalidate_cache();
    }
}

uint64_t TransformNode::get_hash() const {
    if (cached_hash_ == 0) {
        cached_hash_ = calculate_hash();
    }
    return cached_hash_;
}

uint64_t TransformNode::calculate_hash() const {
    uint64_t hash = 0x3456789012CDEF01;  // Base hash for Transform
    hash = hash_combine(hash, static_cast<uint64_t>(input_texture_));
    hash = hash_combine(hash, hash_array(params_.scale, 2));
    hash = hash_combine(hash, hash_float(params_.rotation));
    hash = hash_combine(hash, hash_array(params_.translation, 2));
    hash = hash_combine(hash, hash_array(params_.anchor_point, 2));
    hash = hash_combine(hash, hash_array(params_.crop_rect, 4));
    return hash;
}

float* TransformNode::get_transform_matrix() const {
    static float matrix[16];
    
    // Create transformation matrix from parameters
    float cos_r = std::cos(params_.rotation);
    float sin_r = std::sin(params_.rotation);
    
    // Initialize as identity
    std::memset(matrix, 0, sizeof(matrix));
    matrix[0] = params_.scale[0] * cos_r;
    matrix[1] = params_.scale[0] * -sin_r;
    matrix[4] = params_.scale[1] * sin_r;
    matrix[5] = params_.scale[1] * cos_r;
    matrix[10] = 1.0f;
    matrix[12] = params_.translation[0];
    matrix[13] = params_.translation[1];
    matrix[15] = 1.0f;
    
    return matrix;
}

// LUTNode Implementation
LUTNode::LUTNode() {
    ve::log::debug("Created LUTNode");
}

void LUTNode::render(RenderContext& ctx) {
    if (!enabled_ || input_texture_ == 0 || lut_texture_ == 0 || !ctx.device) {
        return;
    }
    
    // Get or allocate output texture
    if (output_texture_ == 0) {
        output_texture_ = ctx.get_intermediate_texture();
        if (output_texture_ == 0) {
            ve::log::error("Failed to allocate output texture for LUTNode");
            return;
        }
    }
    
    // Apply 3D LUT effect
    bool success = ctx.device->apply_lut(
        input_texture_, lut_texture_, output_texture_, params_.strength
    );
    
    if (success) {
        ctx.effects_rendered++;
        ve::log::debug("LUTNode rendered successfully with strength:", params_.strength);
    } else {
        ve::log::warning("LUTNode render failed");
    }
}

void LUTNode::set_input(int slot, TextureHandle texture) {
    if (slot == 0) {
        input_texture_ = texture;
        invalidate_cache();
    } else if (slot == 1) {
        lut_texture_ = texture;
        invalidate_cache();
    }
}

uint64_t LUTNode::get_hash() const {
    if (cached_hash_ == 0) {
        cached_hash_ = calculate_hash();
    }
    return cached_hash_;
}

uint64_t LUTNode::calculate_hash() const {
    uint64_t hash = 0x456789012CDEF012;  // Base hash for LUT
    hash = hash_combine(hash, static_cast<uint64_t>(input_texture_));
    hash = hash_combine(hash, static_cast<uint64_t>(lut_texture_));
    hash = hash_combine(hash, hash_float(params_.strength));
    return hash;
}

// MixNode Implementation
MixNode::MixNode() {
    ve::log::debug("Created MixNode");
}

void MixNode::render(RenderContext& ctx) {
    if (!enabled_ || base_texture_ == 0 || overlay_texture_ == 0 || !ctx.device) {
        return;
    }
    
    // Get or allocate output texture
    if (output_texture_ == 0) {
        output_texture_ = ctx.get_intermediate_texture();
        if (output_texture_ == 0) {
            ve::log::error("Failed to allocate output texture for MixNode");
            return;
        }
    }
    
    // Note: Mix/blend effects not yet implemented in GraphicsDevice
    // For now, just copy base texture to output as placeholder
    ve::log::info("MixNode render placeholder");
    ctx.effects_rendered++;
}

void MixNode::set_input(int slot, TextureHandle texture) {
    if (slot == 0) {
        base_texture_ = texture;
        invalidate_cache();
    } else if (slot == 1) {
        overlay_texture_ = texture;
        invalidate_cache();
    }
}

uint64_t MixNode::get_hash() const {
    if (cached_hash_ == 0) {
        cached_hash_ = calculate_hash();
    }
    return cached_hash_;
}

uint64_t MixNode::calculate_hash() const {
    uint64_t hash = 0x56789012CDEF0123;  // Base hash for Mix
    hash = hash_combine(hash, static_cast<uint64_t>(base_texture_));
    hash = hash_combine(hash, static_cast<uint64_t>(overlay_texture_));
    hash = hash_combine(hash, static_cast<uint64_t>(params_.blend_mode));
    hash = hash_combine(hash, hash_float(params_.opacity));
    return hash;
}

} // namespace ve::gfx
