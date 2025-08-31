// Week 9: Color Grading LUT Generator Implementation
// Complete implementation of professional 3D LUT generation and color grading tools

#include "gfx/color_grading_lut_generator.hpp"
#include "core/logger.hpp"
#include "core/exceptions.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <execution>
#include <random>

namespace VideoEditor::GFX {

// =============================================================================
// Utility Functions
// =============================================================================

namespace {
    // Interpolation functions
    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }
    
    RGB lerp_rgb(const RGB& a, const RGB& b, float t) {
        return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t)};
    }
    
    // Trilinear interpolation for 3D LUT
    RGB trilinear_interpolate(const std::vector<float>& lut_data, int lut_size, const RGB& input) {
        // Scale input to LUT coordinates
        float r_coord = input.r * (lut_size - 1);
        float g_coord = input.g * (lut_size - 1);
        float b_coord = input.b * (lut_size - 1);
        
        // Find surrounding cube vertices
        int r0 = static_cast<int>(std::floor(r_coord));
        int g0 = static_cast<int>(std::floor(g_coord));
        int b0 = static_cast<int>(std::floor(b_coord));
        
        int r1 = std::min(r0 + 1, lut_size - 1);
        int g1 = std::min(g0 + 1, lut_size - 1);
        int b1 = std::min(b0 + 1, lut_size - 1);
        
        // Calculate interpolation weights
        float dr = r_coord - r0;
        float dg = g_coord - g0;
        float db = b_coord - b0;
        
        // Get 8 corner values
        auto get_lut_value = [&](int r, int g, int b) -> RGB {
            int index = ((r * lut_size + g) * lut_size + b) * 3;
            return {lut_data[index], lut_data[index + 1], lut_data[index + 2]};
        };
        
        RGB c000 = get_lut_value(r0, g0, b0);
        RGB c001 = get_lut_value(r0, g0, b1);
        RGB c010 = get_lut_value(r0, g1, b0);
        RGB c011 = get_lut_value(r0, g1, b1);
        RGB c100 = get_lut_value(r1, g0, b0);
        RGB c101 = get_lut_value(r1, g0, b1);
        RGB c110 = get_lut_value(r1, g1, b0);
        RGB c111 = get_lut_value(r1, g1, b1);
        
        // Trilinear interpolation
        RGB c00 = lerp_rgb(c000, c001, db);
        RGB c01 = lerp_rgb(c010, c011, db);
        RGB c10 = lerp_rgb(c100, c101, db);
        RGB c11 = lerp_rgb(c110, c111, db);
        
        RGB c0 = lerp_rgb(c00, c01, dg);
        RGB c1 = lerp_rgb(c10, c11, dg);
        
        return lerp_rgb(c0, c1, dr);
    }
    
    // Color space conversion matrices
    constexpr ColorMatrix3x3 REC709_TO_XYZ = {{
        {{0.4124f, 0.3576f, 0.1805f}},
        {{0.2126f, 0.7152f, 0.0722f}},
        {{0.0193f, 0.1192f, 0.9505f}}
    }};
    
    constexpr ColorMatrix3x3 XYZ_TO_REC709 = {{
        {{ 3.2406f, -1.5372f, -0.4986f}},
        {{-0.9689f,  1.8758f,  0.0415f}},
        {{ 0.0557f, -0.2040f,  1.0570f}}
    }};
    
    // Film emulation curves
    RGB apply_kodak_5218_emulation(const RGB& input) {
        // Simplified Kodak Vision3 5218 film emulation
        RGB result;
        result.r = std::pow(input.r, 0.6f) * 1.1f;
        result.g = std::pow(input.g, 0.55f) * 1.05f;
        result.b = std::pow(input.b, 0.65f) * 0.95f;
        
        // Slight color shift towards warm tones
        result.r = std::min(1.0f, result.r + 0.02f);
        result.b = std::max(0.0f, result.b - 0.01f);
        
        return result;
    }
    
    RGB apply_fuji_8592_emulation(const RGB& input) {
        // Simplified Fuji Eterna 8592 film emulation
        RGB result;
        result.r = std::pow(input.r, 0.7f) * 0.98f;
        result.g = std::pow(input.g, 0.65f) * 1.02f;
        result.b = std::pow(input.b, 0.6f) * 1.05f;
        
        // Slight color shift towards cool tones
        result.b = std::min(1.0f, result.b + 0.03f);
        result.r = std::max(0.0f, result.r - 0.01f);
        
        return result;
    }
}

// =============================================================================
// Basic 3D LUT Implementation
// =============================================================================

Basic3DLUT::Basic3DLUT(int size) : size_(size) {
    data_.resize(size * size * size * 3);
    
    // Initialize with identity transformation
    for (int r = 0; r < size; ++r) {
        for (int g = 0; g < size; ++g) {
            for (int b = 0; b < size; ++b) {
                int index = ((r * size + g) * size + b) * 3;
                data_[index + 0] = static_cast<float>(r) / (size - 1);
                data_[index + 1] = static_cast<float>(g) / (size - 1);
                data_[index + 2] = static_cast<float>(b) / (size - 1);
            }
        }
    }
}

RGB Basic3DLUT::apply(const RGB& input) const {
    return trilinear_interpolate(data_, size_, input);
}

void Basic3DLUT::set_entry(int r, int g, int b, const RGB& color) {
    if (r >= 0 && r < size_ && g >= 0 && g < size_ && b >= 0 && b < size_) {
        int index = ((r * size_ + g) * size_ + b) * 3;
        data_[index + 0] = color.r;
        data_[index + 1] = color.g;
        data_[index + 2] = color.b;
    }
}

RGB Basic3DLUT::get_entry(int r, int g, int b) const {
    if (r >= 0 && r < size_ && g >= 0 && g < size_ && b >= 0 && b < size_) {
        int index = ((r * size_ + g) * size_ + b) * 3;
        return {data_[index + 0], data_[index + 1], data_[index + 2]};
    }
    return {0.0f, 0.0f, 0.0f};
}

void Basic3DLUT::apply_function(const std::function<RGB(const RGB&)>& func) {
    for (int r = 0; r < size_; ++r) {
        for (int g = 0; g < size_; ++g) {
            for (int b = 0; b < size_; ++b) {
                RGB input{
                    static_cast<float>(r) / (size_ - 1),
                    static_cast<float>(g) / (size_ - 1),
                    static_cast<float>(b) / (size_ - 1)
                };
                
                RGB output = func(input);
                set_entry(r, g, b, output);
            }
        }
    }
}

// =============================================================================
// Color Grading LUT Generator Implementation
// =============================================================================

ColorGradingLUTGenerator::ColorGradingLUTGenerator() {
    // Initialize default grading parameters
    grading_params_.exposure = 0.0f;
    grading_params_.contrast = 1.0f;
    grading_params_.saturation = 1.0f;
    grading_params_.temperature = 0.0f;
    grading_params_.tint = 0.0f;
    grading_params_.highlights = 0.0f;
    grading_params_.shadows = 0.0f;
    grading_params_.whites = 0.0f;
    grading_params_.blacks = 0.0f;
    grading_params_.gamma = 1.0f;
    
    // Initialize lift-gamma-gain
    grading_params_.lift = {0.0f, 0.0f, 0.0f};
    grading_params_.gamma_rgb = {1.0f, 1.0f, 1.0f};
    grading_params_.gain = {1.0f, 1.0f, 1.0f};
    
    // Initialize color wheels
    grading_params_.shadows_wheel = {0.0f, 0.0f, 0.0f};
    grading_params_.midtones_wheel = {0.0f, 0.0f, 0.0f};
    grading_params_.highlights_wheel = {0.0f, 0.0f, 0.0f};
}

Core::Result<Basic3DLUT> ColorGradingLUTGenerator::generate_basic_lut(
    const ColorGradingParams& params, int lut_size) const {
    
    try {
        Basic3DLUT lut(lut_size);
        
        // Apply color grading transformation to each LUT entry
        lut.apply_function([&](const RGB& input) -> RGB {
            return apply_color_grading(input, params);
        });
        
        return Core::Result<Basic3DLUT>::success(lut);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Basic LUT generation failed: {}", e.what());
        return Core::Result<Basic3DLUT>::error("Basic LUT generation failed");
    }
}

RGB ColorGradingLUTGenerator::apply_color_grading(const RGB& input, const ColorGradingParams& params) const {
    RGB result = input;
    
    // Apply exposure adjustment
    if (params.exposure != 0.0f) {
        float exposure_factor = std::pow(2.0f, params.exposure);
        result.r *= exposure_factor;
        result.g *= exposure_factor;
        result.b *= exposure_factor;
    }
    
    // Apply temperature and tint adjustments
    result = apply_white_balance(result, params.temperature, params.tint);
    
    // Apply lift-gamma-gain
    result = apply_lift_gamma_gain(result, params);
    
    // Apply contrast
    if (params.contrast != 1.0f) {
        result.r = apply_contrast_curve(result.r, params.contrast);
        result.g = apply_contrast_curve(result.g, params.contrast);
        result.b = apply_contrast_curve(result.b, params.contrast);
    }
    
    // Apply saturation
    if (params.saturation != 1.0f) {
        result = apply_saturation(result, params.saturation);
    }
    
    // Apply highlight/shadow adjustments
    result = apply_highlight_shadow_adjustments(result, params);
    
    // Apply color wheels
    result = apply_color_wheels(result, params);
    
    // Apply gamma correction
    if (params.gamma != 1.0f) {
        result.r = std::pow(std::max(0.0f, result.r), 1.0f / params.gamma);
        result.g = std::pow(std::max(0.0f, result.g), 1.0f / params.gamma);
        result.b = std::pow(std::max(0.0f, result.b), 1.0f / params.gamma);
    }
    
    // Clamp values
    result.r = std::max(0.0f, std::min(1.0f, result.r));
    result.g = std::max(0.0f, std::min(1.0f, result.g));
    result.b = std::max(0.0f, std::min(1.0f, result.b));
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_white_balance(const RGB& input, float temperature, float tint) const {
    if (temperature == 0.0f && tint == 0.0f) {
        return input;
    }
    
    // Convert temperature to color temperature scale
    float temp_factor = 1.0f + temperature * 0.1f; // Simplified temperature adjustment
    float tint_factor = 1.0f + tint * 0.1f;
    
    RGB result = input;
    
    // Apply temperature (affects red/blue balance)
    if (temperature > 0.0f) {
        // Warmer - increase red, decrease blue
        result.r *= temp_factor;
        result.b /= temp_factor;
    } else if (temperature < 0.0f) {
        // Cooler - decrease red, increase blue
        result.r /= temp_factor;
        result.b *= temp_factor;
    }
    
    // Apply tint (affects green/magenta balance)
    if (tint > 0.0f) {
        // More green
        result.g *= tint_factor;
    } else if (tint < 0.0f) {
        // More magenta
        result.r *= tint_factor;
        result.b *= tint_factor;
    }
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_lift_gamma_gain(const RGB& input, const ColorGradingParams& params) const {
    RGB result = input;
    
    // Apply lift (affects shadows/blacks)
    result.r = result.r + params.lift.r * (1.0f - result.r);
    result.g = result.g + params.lift.g * (1.0f - result.g);
    result.b = result.b + params.lift.b * (1.0f - result.b);
    
    // Apply gamma (affects midtones)
    if (params.gamma_rgb.r != 1.0f) {
        result.r = std::pow(std::max(0.0f, result.r), 1.0f / params.gamma_rgb.r);
    }
    if (params.gamma_rgb.g != 1.0f) {
        result.g = std::pow(std::max(0.0f, result.g), 1.0f / params.gamma_rgb.g);
    }
    if (params.gamma_rgb.b != 1.0f) {
        result.b = std::pow(std::max(0.0f, result.b), 1.0f / params.gamma_rgb.b);
    }
    
    // Apply gain (affects highlights/whites)
    result.r *= params.gain.r;
    result.g *= params.gain.g;
    result.b *= params.gain.b;
    
    return result;
}

float ColorGradingLUTGenerator::apply_contrast_curve(float value, float contrast) const {
    // S-curve contrast adjustment
    return (value - 0.5f) * contrast + 0.5f;
}

RGB ColorGradingLUTGenerator::apply_saturation(const RGB& input, float saturation) const {
    // Calculate luminance
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // Apply saturation
    RGB result;
    result.r = luminance + (input.r - luminance) * saturation;
    result.g = luminance + (input.g - luminance) * saturation;
    result.b = luminance + (input.b - luminance) * saturation;
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_highlight_shadow_adjustments(
    const RGB& input, const ColorGradingParams& params) const {
    
    RGB result = input;
    
    // Calculate luminance for masking
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // Apply highlights adjustment (affects bright areas)
    if (params.highlights != 0.0f) {
        float highlight_mask = luminance * luminance; // Quadratic mask for highlights
        float highlight_factor = 1.0f + params.highlights * 0.5f;
        
        result.r = result.r * (1.0f - highlight_mask) + result.r * highlight_factor * highlight_mask;
        result.g = result.g * (1.0f - highlight_mask) + result.g * highlight_factor * highlight_mask;
        result.b = result.b * (1.0f - highlight_mask) + result.b * highlight_factor * highlight_mask;
    }
    
    // Apply shadows adjustment (affects dark areas)
    if (params.shadows != 0.0f) {
        float shadow_mask = (1.0f - luminance) * (1.0f - luminance); // Inverse quadratic mask for shadows
        float shadow_factor = 1.0f + params.shadows * 0.5f;
        
        result.r = result.r * (1.0f - shadow_mask) + result.r * shadow_factor * shadow_mask;
        result.g = result.g * (1.0f - shadow_mask) + result.g * shadow_factor * shadow_mask;
        result.b = result.b * (1.0f - shadow_mask) + result.b * shadow_factor * shadow_mask;
    }
    
    // Apply whites adjustment (affects pure whites)
    if (params.whites != 0.0f) {
        float white_factor = 1.0f + params.whites * 0.3f;
        result.r *= white_factor;
        result.g *= white_factor;
        result.b *= white_factor;
    }
    
    // Apply blacks adjustment (affects pure blacks)
    if (params.blacks != 0.0f) {
        float black_offset = params.blacks * 0.1f;
        result.r += black_offset;
        result.g += black_offset;
        result.b += black_offset;
    }
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_color_wheels(const RGB& input, const ColorGradingParams& params) const {
    RGB result = input;
    
    // Calculate luminance for masking
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // Shadow color wheel (affects dark areas)
    float shadow_mask = std::pow(1.0f - luminance, 2.0f);
    result.r += params.shadows_wheel.r * shadow_mask * 0.1f;
    result.g += params.shadows_wheel.g * shadow_mask * 0.1f;
    result.b += params.shadows_wheel.b * shadow_mask * 0.1f;
    
    // Midtone color wheel (affects middle gray areas)
    float midtone_mask = 4.0f * luminance * (1.0f - luminance);
    result.r += params.midtones_wheel.r * midtone_mask * 0.1f;
    result.g += params.midtones_wheel.g * midtone_mask * 0.1f;
    result.b += params.midtones_wheel.b * midtone_mask * 0.1f;
    
    // Highlight color wheel (affects bright areas)
    float highlight_mask = std::pow(luminance, 2.0f);
    result.r += params.highlights_wheel.r * highlight_mask * 0.1f;
    result.g += params.highlights_wheel.g * highlight_mask * 0.1f;
    result.b += params.highlights_wheel.b * highlight_mask * 0.1f;
    
    return result;
}

// =============================================================================
// Film Emulation Implementation
// =============================================================================

Core::Result<Basic3DLUT> ColorGradingLUTGenerator::generate_film_emulation_lut(
    FilmStock film_stock, int lut_size) const {
    
    try {
        Basic3DLUT lut(lut_size);
        
        std::function<RGB(const RGB&)> film_function;
        
        switch (film_stock) {
            case FilmStock::KODAK_VISION3_5218:
                film_function = apply_kodak_5218_emulation;
                break;
            case FilmStock::FUJI_ETERNA_8592:
                film_function = apply_fuji_8592_emulation;
                break;
            case FilmStock::KODAK_PORTRA_400:
                film_function = [this](const RGB& input) -> RGB {
                    return apply_kodak_portra_emulation(input);
                };
                break;
            case FilmStock::FUJI_PROVIA_100F:
                film_function = [this](const RGB& input) -> RGB {
                    return apply_fuji_provia_emulation(input);
                };
                break;
            default:
                return Core::Result<Basic3DLUT>::error("Unsupported film stock");
        }
        
        lut.apply_function(film_function);
        
        return Core::Result<Basic3DLUT>::success(lut);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Film emulation LUT generation failed: {}", e.what());
        return Core::Result<Basic3DLUT>::error("Film emulation LUT generation failed");
    }
}

RGB ColorGradingLUTGenerator::apply_kodak_portra_emulation(const RGB& input) const {
    // Kodak Portra 400 film emulation
    RGB result;
    
    // Portra characteristics: smooth skin tones, warm colors, low contrast
    result.r = std::pow(input.r, 0.75f) * 1.05f;
    result.g = std::pow(input.g, 0.7f) * 1.02f;
    result.b = std::pow(input.b, 0.8f) * 0.98f;
    
    // Apply Portra's characteristic color grading
    float luminance = 0.2126f * result.r + 0.7152f * result.g + 0.0722f * result.b;
    
    // Enhance skin tones (orange/red regions)
    if (result.r > result.g && result.r > result.b) {
        float skin_enhance = (result.r - std::max(result.g, result.b)) * 0.1f;
        result.r += skin_enhance;
        result.g += skin_enhance * 0.5f;
    }
    
    // Soften highlights
    if (luminance > 0.7f) {
        float highlight_factor = 0.95f;
        result.r *= highlight_factor;
        result.g *= highlight_factor;
        result.b *= highlight_factor;
    }
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_fuji_provia_emulation(const RGB& input) const {
    // Fuji Provia 100F film emulation
    RGB result;
    
    // Provia characteristics: high saturation, punchy colors, good contrast
    result.r = std::pow(input.r, 0.8f) * 1.0f;
    result.g = std::pow(input.g, 0.75f) * 1.05f;
    result.b = std::pow(input.b, 0.7f) * 1.08f;
    
    // Enhance blues and greens (Provia's characteristic)
    float luminance = 0.2126f * result.r + 0.7152f * result.g + 0.0722f * result.b;
    
    // Boost blue skies
    if (result.b > result.r && result.b > result.g) {
        result.b = std::min(1.0f, result.b * 1.1f);
    }
    
    // Enhance greens
    if (result.g > result.r && result.g > result.b) {
        result.g = std::min(1.0f, result.g * 1.08f);
    }
    
    // Increase overall saturation
    float saturation_boost = 1.15f;
    result.r = luminance + (result.r - luminance) * saturation_boost;
    result.g = luminance + (result.g - luminance) * saturation_boost;
    result.b = luminance + (result.b - luminance) * saturation_boost;
    
    return result;
}

// =============================================================================
// Creative LUT Implementation
// =============================================================================

Core::Result<Basic3DLUT> ColorGradingLUTGenerator::generate_creative_lut(
    const CreativeLook& look, int lut_size) const {
    
    try {
        Basic3DLUT lut(lut_size);
        
        std::function<RGB(const RGB&)> creative_function;
        
        switch (look.style) {
            case CreativeStyle::CINEMATIC:
                creative_function = [&](const RGB& input) -> RGB {
                    return apply_cinematic_look(input, look);
                };
                break;
            case CreativeStyle::VINTAGE:
                creative_function = [&](const RGB& input) -> RGB {
                    return apply_vintage_look(input, look);
                };
                break;
            case CreativeStyle::BLEACH_BYPASS:
                creative_function = [&](const RGB& input) -> RGB {
                    return apply_bleach_bypass_look(input, look);
                };
                break;
            case CreativeStyle::TEAL_ORANGE:
                creative_function = [&](const RGB& input) -> RGB {
                    return apply_teal_orange_look(input, look);
                };
                break;
            case CreativeStyle::FILM_NOIR:
                creative_function = [&](const RGB& input) -> RGB {
                    return apply_film_noir_look(input, look);
                };
                break;
            default:
                return Core::Result<Basic3DLUT>::error("Unsupported creative style");
        }
        
        lut.apply_function(creative_function);
        
        return Core::Result<Basic3DLUT>::success(lut);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Creative LUT generation failed: {}", e.what());
        return Core::Result<Basic3DLUT>::error("Creative LUT generation failed");
    }
}

RGB ColorGradingLUTGenerator::apply_cinematic_look(const RGB& input, const CreativeLook& look) const {
    RGB result = input;
    
    // Cinematic look: slight desaturation, lifted blacks, film-like contrast
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // Lift blacks slightly
    result.r = result.r * 0.9f + 0.05f;
    result.g = result.g * 0.9f + 0.05f;
    result.b = result.b * 0.9f + 0.05f;
    
    // Apply S-curve for film-like contrast
    float contrast = 1.2f * look.intensity;
    result.r = apply_contrast_curve(result.r, contrast);
    result.g = apply_contrast_curve(result.g, contrast);
    result.b = apply_contrast_curve(result.b, contrast);
    
    // Slight desaturation
    float desaturation = 0.9f - (0.1f * look.intensity);
    result = apply_saturation(result, desaturation);
    
    // Warm color cast
    result.r *= 1.02f;
    result.b *= 0.98f;
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_vintage_look(const RGB& input, const CreativeLook& look) const {
    RGB result = input;
    
    // Vintage look: faded colors, lifted shadows, reduced contrast
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // Fade effect - lift shadows and pull down highlights
    float fade_amount = 0.15f * look.intensity;
    result.r = result.r * (1.0f - fade_amount) + fade_amount;
    result.g = result.g * (1.0f - fade_amount) + fade_amount;
    result.b = result.b * (1.0f - fade_amount) + fade_amount;
    
    // Reduce contrast
    float contrast = 0.8f - (0.2f * look.intensity);
    result.r = apply_contrast_curve(result.r, contrast);
    result.g = apply_contrast_curve(result.g, contrast);
    result.b = apply_contrast_curve(result.b, contrast);
    
    // Desaturate
    float saturation = 0.7f - (0.3f * look.intensity);
    result = apply_saturation(result, saturation);
    
    // Add vintage color cast (slightly yellow/warm)
    result.r *= 1.05f;
    result.g *= 1.02f;
    result.b *= 0.95f;
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_bleach_bypass_look(const RGB& input, const CreativeLook& look) const {
    RGB result = input;
    
    // Bleach bypass: high contrast, desaturated, silver retention effect
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // High contrast S-curve
    float contrast = 1.5f + (0.5f * look.intensity);
    result.r = apply_contrast_curve(result.r, contrast);
    result.g = apply_contrast_curve(result.g, contrast);
    result.b = apply_contrast_curve(result.b, contrast);
    
    // Heavy desaturation
    float saturation = 0.3f - (0.2f * look.intensity);
    result = apply_saturation(result, saturation);
    
    // Silver retention effect - blend with luminance
    float silver_blend = 0.3f * look.intensity;
    result.r = result.r * (1.0f - silver_blend) + luminance * silver_blend;
    result.g = result.g * (1.0f - silver_blend) + luminance * silver_blend;
    result.b = result.b * (1.0f - silver_blend) + luminance * silver_blend;
    
    // Slight cool cast
    result.b *= 1.02f;
    result.r *= 0.98f;
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_teal_orange_look(const RGB& input, const CreativeLook& look) const {
    RGB result = input;
    
    // Teal and Orange look: popular in modern cinema
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // Push highlights toward orange
    if (luminance > 0.5f) {
        float highlight_mask = (luminance - 0.5f) * 2.0f;
        result.r += highlight_mask * 0.1f * look.intensity;
        result.g += highlight_mask * 0.05f * look.intensity;
        result.b -= highlight_mask * 0.05f * look.intensity;
    }
    
    // Push shadows toward teal
    if (luminance < 0.5f) {
        float shadow_mask = (0.5f - luminance) * 2.0f;
        result.r -= shadow_mask * 0.05f * look.intensity;
        result.g += shadow_mask * 0.05f * look.intensity;
        result.b += shadow_mask * 0.1f * look.intensity;
    }
    
    // Increase saturation
    float saturation = 1.0f + (0.3f * look.intensity);
    result = apply_saturation(result, saturation);
    
    return result;
}

RGB ColorGradingLUTGenerator::apply_film_noir_look(const RGB& input, const CreativeLook& look) const {
    RGB result = input;
    
    // Film noir: high contrast black and white with optional blue tint
    float luminance = 0.2126f * input.r + 0.7152f * input.g + 0.0722f * input.b;
    
    // Convert to monochrome with high contrast
    float contrast = 1.8f + (0.5f * look.intensity);
    float enhanced_luma = apply_contrast_curve(luminance, contrast);
    
    // Create monochrome base
    result.r = enhanced_luma;
    result.g = enhanced_luma;
    result.b = enhanced_luma;
    
    // Optional blue tint for shadows
    if (luminance < 0.3f) {
        float shadow_mask = (0.3f - luminance) / 0.3f;
        result.b += shadow_mask * 0.1f * look.intensity;
    }
    
    return result;
}

// =============================================================================
// Realtime LUT Processor Implementation
// =============================================================================

Core::Result<void> RealtimeLUTProcessor::initialize_gpu_resources(ID3D11Device* device) {
    try {
        device_ = device;
        
        // Create 3D texture for LUT storage
        D3D11_TEXTURE3D_DESC texture_desc = {};
        texture_desc.Width = 64;  // Standard LUT size
        texture_desc.Height = 64;
        texture_desc.Depth = 64;
        texture_desc.MipLevels = 1;
        texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        HRESULT hr = device_->CreateTexture3D(&texture_desc, nullptr, &lut_texture_);
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create 3D LUT texture");
        }
        
        // Create shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = texture_desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        srv_desc.Texture3D.MipLevels = 1;
        
        hr = device_->CreateShaderResourceView(lut_texture_.Get(), &srv_desc, &lut_srv_);
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create LUT shader resource view");
        }
        
        // Create sampler state for LUT sampling
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        
        hr = device_->CreateSamplerState(&sampler_desc, &lut_sampler_);
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create LUT sampler state");
        }
        
        is_initialized_ = true;
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("GPU resource initialization failed: {}", e.what());
        return Core::Result<void>::error("GPU resource initialization failed");
    }
}

Core::Result<void> RealtimeLUTProcessor::upload_lut_to_gpu(const Basic3DLUT& lut) {
    try {
        if (!is_initialized_ || !device_) {
            return Core::Result<void>::error("Processor not initialized");
        }
        
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        device_->GetImmediateContext(&context);
        
        // Prepare LUT data for upload (convert to RGBA format)
        int lut_size = lut.get_size();
        std::vector<float> rgba_data(lut_size * lut_size * lut_size * 4);
        
        for (int r = 0; r < lut_size; ++r) {
            for (int g = 0; g < lut_size; ++g) {
                for (int b = 0; b < lut_size; ++b) {
                    RGB color = lut.get_entry(r, g, b);
                    int index = ((r * lut_size + g) * lut_size + b) * 4;
                    
                    rgba_data[index + 0] = color.r;
                    rgba_data[index + 1] = color.g;
                    rgba_data[index + 2] = color.b;
                    rgba_data[index + 3] = 1.0f; // Alpha
                }
            }
        }
        
        // Update 3D texture
        context->UpdateSubresource(lut_texture_.Get(), 0, nullptr, 
                                  rgba_data.data(), 
                                  lut_size * 4 * sizeof(float),
                                  lut_size * lut_size * 4 * sizeof(float));
        
        current_lut_size_ = lut_size;
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("LUT GPU upload failed: {}", e.what());
        return Core::Result<void>::error("LUT GPU upload failed");
    }
}

Core::Result<void> RealtimeLUTProcessor::apply_lut_to_texture(
    ID3D11ShaderResourceView* input_texture,
    ID3D11RenderTargetView* output_target) {
    
    try {
        if (!is_initialized_ || !lut_srv_) {
            return Core::Result<void>::error("Processor not initialized or no LUT loaded");
        }
        
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        device_->GetImmediateContext(&context);
        
        // Set render target
        context->OMSetRenderTargets(1, &output_target, nullptr);
        
        // Set LUT texture and sampler
        ID3D11ShaderResourceView* textures[] = {input_texture, lut_srv_.Get()};
        context->PSSetShaderResources(0, 2, textures);
        
        ID3D11SamplerState* samplers[] = {lut_sampler_.Get(), lut_sampler_.Get()};
        context->PSSetSamplers(0, 2, samplers);
        
        // Note: In a real implementation, you would also set vertex/pixel shaders
        // and draw a full-screen quad to apply the LUT
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("LUT application failed: {}", e.what());
        return Core::Result<void>::error("LUT application failed");
    }
}

// =============================================================================
// LUT File I/O Implementation
// =============================================================================

Core::Result<void> ColorGradingLUTGenerator::export_cube_lut(
    const Basic3DLUT& lut, const std::string& file_path) const {
    
    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return Core::Result<void>::error("Failed to open file for writing");
        }
        
        // Write .cube format header
        file << "# LUT size\n";
        file << "LUT_3D_SIZE " << lut.get_size() << "\n\n";
        file << "# LUT data\n";
        
        // Write LUT data in .cube format (blue fastest, red slowest)
        int size = lut.get_size();
        for (int r = 0; r < size; ++r) {
            for (int g = 0; g < size; ++g) {
                for (int b = 0; b < size; ++b) {
                    RGB color = lut.get_entry(r, g, b);
                    file << std::fixed << std::setprecision(6) 
                         << color.r << " " << color.g << " " << color.b << "\n";
                }
            }
        }
        
        LOG_INFO("Exported .cube LUT to: {}", file_path);
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to export .cube LUT: {}", e.what());
        return Core::Result<void>::error("Failed to export .cube LUT");
    }
}

Core::Result<Basic3DLUT> ColorGradingLUTGenerator::import_cube_lut(const std::string& file_path) const {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return Core::Result<Basic3DLUT>::error("Failed to open file for reading");
        }
        
        std::string line;
        int lut_size = 0;
        
        // Parse header
        while (std::getline(file, line)) {
            if (line.find("LUT_3D_SIZE") != std::string::npos) {
                std::istringstream iss(line);
                std::string keyword;
                iss >> keyword >> lut_size;
                break;
            }
        }
        
        if (lut_size == 0) {
            return Core::Result<Basic3DLUT>::error("Invalid LUT size");
        }
        
        Basic3DLUT lut(lut_size);
        
        // Read LUT data
        for (int r = 0; r < lut_size; ++r) {
            for (int g = 0; g < lut_size; ++g) {
                for (int b = 0; b < lut_size; ++b) {
                    if (!std::getline(file, line)) {
                        return Core::Result<Basic3DLUT>::error("Unexpected end of file");
                    }
                    
                    // Skip comment lines
                    if (line.empty() || line[0] == '#') {
                        --b; // Retry this entry
                        continue;
                    }
                    
                    std::istringstream iss(line);
                    RGB color;
                    iss >> color.r >> color.g >> color.b;
                    
                    lut.set_entry(r, g, b, color);
                }
            }
        }
        
        LOG_INFO("Imported .cube LUT from: {}", file_path);
        
        return Core::Result<Basic3DLUT>::success(lut);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to import .cube LUT: {}", e.what());
        return Core::Result<Basic3DLUT>::error("Failed to import .cube LUT");
    }
}

} // namespace VideoEditor::GFX
