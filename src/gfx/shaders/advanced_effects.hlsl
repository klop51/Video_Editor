// Advanced Shader Effects - Professional Cinematic Effects
// Supports both HLSL (D3D11) and can be cross-compiled to SPIR-V for Vulkan

#ifndef ADVANCED_EFFECTS_HLSL
#define ADVANCED_EFFECTS_HLSL

// Common constants and structures
cbuffer EffectsConstants : register(b0)
{
    // Film Grain Parameters
    float4 film_grain_params;     // intensity, size, color_amount, response_curve
    float4 film_grain_multipliers; // red, green, blue, random_seed
    
    // Vignette Parameters  
    float4 vignette_params;       // radius, softness, strength, roundness
    float4 vignette_center_color; // center.xy, color_tint.rg
    float2 vignette_color_b_feather; // color_tint.b, feather
    
    // Chromatic Aberration Parameters
    float4 chromatic_aberration;  // strength, edge_falloff, unused, unused
    float4 red_blue_offsets;      // red_offset.xy, blue_offset.xy
    
    // Lens Distortion Parameters
    float4 lens_distortion_params; // barrel, asymmetric, chromatic, zoom
    float2 distortion_center;      // center offset
    
    // Color Grading - Color Wheels
    float4 lift_rgb_sat;          // lift.rgb, lift_saturation
    float4 gamma_rgb_sat;         // gamma.rgb, gamma_saturation  
    float4 gain_rgb_sat;          // gain.rgb, gain_saturation
    float4 offset_power_slope;    // offset.rgb, power
    
    // Color Grading - Basic Adjustments
    float4 exposure_contrast_highlights_shadows; // exposure, contrast, highlights, shadows
    float4 whites_blacks_saturation_vibrance;    // whites, blacks, saturation, vibrance
    float4 temperature_tint_clarity_dehaze;      // temperature, tint, clarity, dehaze
    
    // Motion Blur Parameters
    float4 motion_blur_params;    // sample_count, shutter_angle, max_radius, depth_threshold
    float2 global_motion_vector;
    
    // Viewport and timing
    float4 viewport_size_time;    // width, height, time, frame_count
    float4x4 view_projection_matrix;
};

// Texture samplers
Texture2D input_texture : register(t0);
Texture2D motion_vectors_texture : register(t1);
Texture2D depth_texture : register(t2);
Texture2D previous_frame_texture : register(t3);
Texture2D lut_texture : register(t4);
Texture2D noise_texture : register(t5);

SamplerState linear_sampler : register(s0);
SamplerState point_sampler : register(s1);
SamplerState linear_clamp_sampler : register(s2);

// Utility functions
float3 rgb_to_hsv(float3 rgb)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(rgb.bg, K.wz), float4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 hsv_to_rgb(float3 hsv)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}

float3 linear_to_srgb(float3 linear_color)
{
    return pow(max(linear_color, 0.0), 1.0 / 2.2);
}

float3 srgb_to_linear(float3 srgb_color)
{
    return pow(max(srgb_color, 0.0), 2.2);
}

// Professional Film Grain Effect
float4 apply_film_grain(float4 color, float2 uv)
{
    float intensity = film_grain_params.x;
    float size = film_grain_params.y;
    float color_amount = film_grain_params.z;
    float response_curve = film_grain_params.w;
    
    if (intensity <= 0.0)
        return color;
    
    // Generate high-quality noise using multiple octaves
    float2 grain_uv = uv * viewport_size_time.xy / size;
    float grain_time = viewport_size_time.z * 0.1 + film_grain_multipliers.w;
    
    // Sample noise texture with temporal variation
    float3 noise = noise_texture.Sample(linear_sampler, grain_uv + float2(grain_time, -grain_time * 0.7)).rgb;
    noise = (noise - 0.5) * 2.0; // Convert to [-1, 1]
    
    // Apply response curve (film characteristic)
    float luminance = dot(color.rgb, float3(0.299, 0.587, 0.114));
    float grain_strength = intensity * pow(luminance, response_curve);
    
    // Reduce grain in highlights (realistic film behavior)
    grain_strength *= (1.0 - smoothstep(0.8, 1.0, luminance) * 0.7);
    
    // Color vs monochrome grain blending
    float mono_grain = (noise.r + noise.g + noise.b) / 3.0;
    float3 final_grain = lerp(float3(mono_grain, mono_grain, mono_grain), noise, color_amount);
    
    // Apply per-channel multipliers for different film stocks
    final_grain *= film_grain_multipliers.rgb;
    
    // Blend with original color
    color.rgb += final_grain * grain_strength;
    
    return color;
}

// Professional Vignette Effect
float4 apply_vignette(float4 color, float2 uv)
{
    float radius = vignette_params.x;
    float softness = vignette_params.y;
    float strength = vignette_params.z;
    float roundness = vignette_params.w;
    
    if (strength <= 0.0)
        return color;
    
    float2 center = vignette_center_color.xy;
    float3 color_tint = float3(vignette_center_color.zw, vignette_color_b_feather.x);
    float feather = vignette_color_b_feather.y;
    
    // Calculate distance from center with aspect ratio compensation
    float2 centered_uv = uv - center;
    float aspect_ratio = viewport_size_time.x / viewport_size_time.y;
    centered_uv.x *= aspect_ratio;
    
    // Apply roundness (interpolate between circular and rectangular)
    float circular_dist = length(centered_uv);
    float rectangular_dist = max(abs(centered_uv.x), abs(centered_uv.y));
    float distance = lerp(rectangular_dist, circular_dist, roundness);
    
    // Calculate vignette mask with smooth falloff
    float vignette_mask = 1.0 - smoothstep(radius - softness, radius + softness + feather, distance);
    vignette_mask = pow(vignette_mask, 2.2); // Gamma correction for perceptual uniformity
    
    // Apply vignette effect
    float3 vignette_effect = lerp(color_tint, float3(1.0, 1.0, 1.0), vignette_mask);
    color.rgb *= lerp(float3(1.0, 1.0, 1.0), vignette_effect, strength);
    
    return color;
}

// High-Quality Chromatic Aberration
float4 apply_chromatic_aberration(float2 uv)
{
    float strength = chromatic_aberration.x;
    float edge_falloff = chromatic_aberration.y;
    
    if (strength <= 0.0)
        return input_texture.Sample(linear_sampler, uv);
    
    // Calculate distance from center for edge falloff
    float2 center_offset = uv - 0.5;
    float distance_from_center = length(center_offset);
    float edge_factor = pow(distance_from_center * 2.0, edge_falloff);
    
    // Calculate channel offsets with edge amplification
    float2 red_offset = red_blue_offsets.xy * strength * edge_factor;
    float2 blue_offset = red_blue_offsets.zw * strength * edge_factor;
    
    // Sample each color channel with offset
    float red = input_texture.Sample(linear_sampler, uv + red_offset).r;
    float green = input_texture.Sample(linear_sampler, uv).g;
    float blue = input_texture.Sample(linear_sampler, uv + blue_offset).b;
    float alpha = input_texture.Sample(linear_sampler, uv).a;
    
    return float4(red, green, blue, alpha);
}

// Professional Lens Distortion Correction
float2 apply_lens_distortion(float2 uv)
{
    float barrel_distortion = lens_distortion_params.x;
    float asymmetric_distortion = lens_distortion_params.y;
    float zoom = lens_distortion_params.w;
    
    if (abs(barrel_distortion) < 0.001 && abs(asymmetric_distortion) < 0.001)
        return uv;
    
    // Center coordinates
    float2 center = float2(0.5, 0.5) + distortion_center;
    float2 centered_uv = (uv - center);
    
    // Apply aspect ratio compensation
    float aspect_ratio = viewport_size_time.x / viewport_size_time.y;
    centered_uv.x *= aspect_ratio;
    
    // Calculate radial distance
    float r2 = dot(centered_uv, centered_uv);
    float r4 = r2 * r2;
    
    // Barrel/pincushion distortion formula
    float distortion_factor = 1.0 + barrel_distortion * r2 + barrel_distortion * barrel_distortion * r4;
    
    // Apply asymmetric distortion (different for X and Y)
    float2 distorted_uv = centered_uv * distortion_factor;
    distorted_uv.x *= (1.0 + asymmetric_distortion);
    
    // Restore aspect ratio and apply zoom compensation
    distorted_uv.x /= aspect_ratio;
    distorted_uv *= zoom;
    
    return distorted_uv + center;
}

// Professional Color Wheels (Lift/Gamma/Gain)
float3 apply_color_wheels(float3 color)
{
    float3 lift = lift_rgb_sat.xyz;
    float lift_saturation = lift_rgb_sat.w;
    float3 gamma = gamma_rgb_sat.xyz;
    float gamma_saturation = gamma_rgb_sat.w;
    float3 gain = gain_rgb_sat.xyz;
    float gain_saturation = gain_rgb_sat.w;
    float3 offset = offset_power_slope.xyz;
    float power = offset_power_slope.w;
    
    // Convert to linear space for accurate color math
    color = srgb_to_linear(color);
    
    // Apply lift (shadows) - affects dark areas more
    float3 lifted_color = color + lift * (1.0 - color);
    
    // Apply gamma (midtones) - affects middle values
    float3 gamma_corrected = pow(max(lifted_color, 0.0001), 1.0 / max(gamma, 0.001));
    
    // Apply gain (highlights) - affects bright areas more
    float3 gained_color = gamma_corrected * gain;
    
    // Apply offset (master lift)
    gained_color += offset;
    
    // Apply power (master gamma/contrast)
    gained_color = pow(max(gained_color, 0.0001), power);
    
    // Saturation adjustments per tonal range
    float luminance = dot(gained_color, float3(0.299, 0.587, 0.114));
    float shadow_mask = 1.0 - smoothstep(0.0, 0.3, luminance);
    float highlight_mask = smoothstep(0.7, 1.0, luminance);
    float midtone_mask = 1.0 - shadow_mask - highlight_mask;
    
    // Apply per-range saturation
    float3 desaturated = float3(luminance, luminance, luminance);
    gained_color = lerp(desaturated, gained_color, 
        shadow_mask * lift_saturation + 
        midtone_mask * gamma_saturation + 
        highlight_mask * gain_saturation);
    
    // Convert back to sRGB space
    return linear_to_srgb(gained_color);
}

// Professional Motion Blur with Variable Sample Count
float4 apply_motion_blur(float2 uv)
{
    int sample_count = (int)motion_blur_params.x;
    float shutter_angle = motion_blur_params.y;
    float max_radius = motion_blur_params.z;
    
    if (sample_count <= 1 || shutter_angle <= 0.0)
        return input_texture.Sample(linear_sampler, uv);
    
    // Sample motion vector
    float2 motion_vector = motion_vectors_texture.Sample(point_sampler, uv).xy;
    motion_vector += global_motion_vector;
    
    // Scale by shutter angle and limit maximum blur
    motion_vector *= (shutter_angle / 360.0);
    float motion_length = length(motion_vector);
    if (motion_length > max_radius)
        motion_vector = normalize(motion_vector) * max_radius;
    
    // Reconstruct motion blur using variable step sampling
    float4 result = float4(0, 0, 0, 0);
    float total_weight = 0.0;
    
    for (int i = 0; i < sample_count; i++)
    {
        float t = (float(i) + 0.5) / float(sample_count) - 0.5; // [-0.5, 0.5]
        float2 sample_uv = uv + motion_vector * t;
        
        // Weight samples using a smooth kernel (reduces noise)
        float weight = 1.0 - abs(t * 2.0); // Triangle filter
        weight = smoothstep(0.0, 1.0, weight);
        
        // Sample with bounds checking
        if (sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && 
            sample_uv.y >= 0.0 && sample_uv.y <= 1.0)
        {
            result += input_texture.Sample(linear_sampler, sample_uv) * weight;
            total_weight += weight;
        }
    }
    
    return total_weight > 0.0 ? result / total_weight : input_texture.Sample(linear_sampler, uv);
}

// Main effect application functions

// Vertex shader for fullscreen quad
struct VSOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VSOutput FullscreenVS(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    
    // Generate fullscreen triangle
    output.texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.texcoord * 2.0 - 1.0, 0.0, 1.0);
    output.position.y *= -1.0; // Flip Y for D3D11
    
    return output;
}

// Film Grain Pixel Shader
float4 FilmGrainPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    return apply_film_grain(color, input.texcoord);
}

// Vignette Pixel Shader
float4 VignettePS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    return apply_vignette(color, input.texcoord);
}

// Chromatic Aberration Pixel Shader
float4 ChromaticAberrationPS(VSOutput input) : SV_Target
{
    return apply_chromatic_aberration(input.texcoord);
}

// Lens Distortion Pixel Shader
float4 LensDistortionPS(VSOutput input) : SV_Target
{
    float2 distorted_uv = apply_lens_distortion(input.texcoord);
    
    // Check bounds and apply black border for out-of-bounds areas
    if (distorted_uv.x < 0.0 || distorted_uv.x > 1.0 || 
        distorted_uv.y < 0.0 || distorted_uv.y > 1.0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    
    return input_texture.Sample(linear_sampler, distorted_uv);
}

// Color Grading Pixel Shader
float4 ColorGradingPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    
    // Apply color wheels
    color.rgb = apply_color_wheels(color.rgb);
    
    // Apply basic adjustments
    float exposure = exposure_contrast_highlights_shadows.x;
    float contrast = exposure_contrast_highlights_shadows.y;
    float highlights = exposure_contrast_highlights_shadows.z;
    float shadows = exposure_contrast_highlights_shadows.w;
    
    // Exposure adjustment
    color.rgb *= pow(2.0, exposure);
    
    // Contrast adjustment around middle gray
    color.rgb = (color.rgb - 0.5) * (contrast + 1.0) + 0.5;
    
    // Highlights and shadows using luminance-based masks
    float luminance = dot(color.rgb, float3(0.299, 0.587, 0.114));
    float highlight_mask = smoothstep(0.5, 1.0, luminance);
    float shadow_mask = smoothstep(0.5, 0.0, luminance);
    
    color.rgb *= (1.0 + highlights * highlight_mask + shadows * shadow_mask);
    
    return color;
}

// Motion Blur Pixel Shader
float4 MotionBlurPS(VSOutput input) : SV_Target
{
    return apply_motion_blur(input.texcoord);
}

// Composite Effect Pixel Shader (applies multiple effects in sequence)
float4 CompositeEffectsPS(VSOutput input) : SV_Target
{
    float2 uv = input.texcoord;
    
    // Apply lens distortion first (affects UV coordinates)
    uv = apply_lens_distortion(uv);
    
    // Sample the distorted image
    float4 color = apply_chromatic_aberration(uv);
    
    // Apply color grading
    color.rgb = apply_color_wheels(color.rgb);
    
    // Apply cinematic effects
    color = apply_vignette(color, input.texcoord); // Use original UV for vignette
    color = apply_film_grain(color, input.texcoord);
    
    return color;
}

#endif // ADVANCED_EFFECTS_HLSL
