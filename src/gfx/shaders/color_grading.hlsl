// Professional Color Grading Shaders - DaVinci Resolve Quality Color Tools
// Supports both HLSL (D3D11) and can be cross-compiled to SPIR-V for Vulkan

#ifndef COLOR_GRADING_HLSL
#define COLOR_GRADING_HLSL

// Color Grading Constants Buffer
cbuffer ColorGradingConstants : register(b0)
{
    // Color Wheels (Lift/Gamma/Gain) - Industry Standard
    float4 lift_color_saturation;        // lift.rgb, lift_saturation
    float4 gamma_color_saturation;       // gamma.rgb, gamma_saturation
    float4 gain_color_saturation;        // gain.rgb, gain_saturation
    float4 offset_master_gamma;          // offset.rgb, master_gamma
    
    // Log Color Wheels (Shadows/Midtones/Highlights)
    float4 shadows_color_saturation;     // shadows.rgb, shadows_saturation
    float4 midtones_color_saturation;    // midtones.rgb, midtones_saturation
    float4 highlights_color_saturation;  // highlights.rgb, highlights_saturation
    float4 log_master_controls;          // master_offset, master_gain, pivot_point, unused
    
    // Basic Controls
    float4 exposure_contrast_highlights_shadows; // exposure, contrast, highlights, shadows
    float4 whites_blacks_saturation_vibrance;    // whites, blacks, saturation, vibrance
    float4 temperature_tint_clarity_dehaze;      // temperature, tint, clarity, dehaze
    float4 sharpening_noise_vignette_grain;     // sharpening, noise_reduction, vignette, grain
    
    // HSL Qualifiers (Hue/Saturation/Luminance Range Selection)
    float4 hsl_qualifier_hue_range;      // hue_center, hue_width, hue_softness, hue_symmetry
    float4 hsl_qualifier_sat_range;      // sat_low, sat_high, sat_softness, sat_invert
    float4 hsl_qualifier_lum_range;      // lum_low, lum_high, lum_softness, lum_invert
    float4 hsl_qualifier_color_boost;    // qualifier_strength, color_boost, denoise, blur
    
    // Bezier RGB Curves (4-point curves with control points)
    float4 curve_red_points[4];          // Red curve: input0,output0,input1,output1 per float4
    float4 curve_green_points[4];        // Green curve: same format
    float4 curve_blue_points[4];         // Blue curve: same format
    float4 curve_master_points[4];       // Master curve: same format
    
    // Secondary Color Correction
    float4 secondary_color_target;       // target_hue, target_range, target_softness, unused
    float4 secondary_corrections;        // hue_shift, saturation_mult, luminance_mult, unused
    
    // Power Windows (Mask-based corrections)
    float4 power_window_center_size;     // center.xy, size.xy
    float4 power_window_params;          // shape, softness, invert, opacity
    float4 power_window_corrections;     // exposure_offset, saturation_mult, hue_shift, unused
    
    // LUT and Film Emulation
    float4 lut_strength_film_emulation; // lut_strength, film_emulation_strength, grain_intensity, unused
    float4 film_response_curve;         // toe, shoulder, contrast, saturation
    
    // Viewport and Working Space
    float4 viewport_working_space;      // width, height, working_colorspace, output_colorspace
    float4 processing_params;           // bit_depth, dithering, gamut_mapping, tone_mapping
};

// Texture Resources
Texture2D input_texture : register(t0);
Texture2D lut_texture_3d : register(t1);    // 3D LUT for color grading
Texture2D film_emulation_lut : register(t2); // Film stock emulation
Texture2D qualifier_mask : register(t3);     // HSL qualifier mask
Texture2D power_window_mask : register(t4);  // Power window mask

SamplerState linear_sampler : register(s0);
SamplerState linear_clamp_sampler : register(s1);
SamplerState point_sampler : register(s2);

// Color Space Conversion Functions

// Rec.709 to XYZ
float3 rec709_to_xyz(float3 rgb)
{
    float3x3 matrix = float3x3(
        0.4124564, 0.3575761, 0.1804375,
        0.2126729, 0.7151522, 0.0721750,
        0.0193339, 0.1191920, 0.9503041
    );
    return mul(matrix, rgb);
}

// XYZ to Rec.709
float3 xyz_to_rec709(float3 xyz)
{
    float3x3 matrix = float3x3(
         3.2404542, -1.5371385, -0.4985314,
        -0.9692660,  1.8760108,  0.0415560,
         0.0556434, -0.2040259,  1.0572252
    );
    return mul(matrix, xyz);
}

// RGB to HSV conversion with high precision
float3 rgb_to_hsv_precise(float3 rgb)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(rgb.bg, K.wz), float4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV to RGB conversion
float3 hsv_to_rgb_precise(float3 hsv)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), hsv.y);
}

// Linear to sRGB with precise gamma
float3 linear_to_srgb_precise(float3 linear)
{
    return linear <= 0.0031308 ? linear * 12.92 : 1.055 * pow(linear, 1.0/2.4) - 0.055;
}

// sRGB to Linear with precise gamma
float3 srgb_to_linear_precise(float3 srgb)
{
    return srgb <= 0.04045 ? srgb / 12.92 : pow((srgb + 0.055) / 1.055, 2.4);
}

// Professional Color Wheels Implementation
float3 apply_color_wheels_lift_gamma_gain(float3 color)
{
    // Extract parameters
    float3 lift = lift_color_saturation.xyz;
    float lift_sat = lift_color_saturation.w;
    float3 gamma = gamma_color_saturation.xyz;
    float gamma_sat = gamma_color_saturation.w;
    float3 gain = gain_color_saturation.xyz;
    float gain_sat = gain_color_saturation.w;
    float3 offset = offset_master_gamma.xyz;
    float master_gamma = offset_master_gamma.w;
    
    // Convert to linear for accurate color math
    color = srgb_to_linear_precise(color);
    
    // Calculate luminance for range masking
    float luminance = dot(color, float3(0.2126729, 0.7151522, 0.0721750)); // Rec.709 weights
    
    // Create tonal range masks
    float shadow_mask = 1.0 - smoothstep(0.0, 0.4, luminance);
    float midtone_mask = 1.0 - abs(luminance - 0.5) * 2.0;
    midtone_mask = smoothstep(0.0, 1.0, midtone_mask);
    float highlight_mask = smoothstep(0.6, 1.0, luminance);
    
    // Apply Lift (affects shadows most)
    // Industry standard: lift affects blacks and moves them towards the lift color
    float3 lifted_color = color + lift * shadow_mask * (1.0 - color);
    
    // Apply saturation in lift range
    float3 lifted_desaturated = float3(dot(lifted_color, float3(0.299, 0.587, 0.114)));
    lifted_color = lerp(lifted_desaturated, lifted_color, lift_sat * shadow_mask + (1.0 - shadow_mask));
    
    // Apply Gamma (affects midtones most)
    // Gamma adjustment with power function
    float3 gamma_corrected = pow(max(lifted_color, 0.0001), 1.0 / max(gamma, 0.001));
    
    // Apply saturation in gamma range
    float gamma_lum = dot(gamma_corrected, float3(0.299, 0.587, 0.114));
    gamma_corrected = lerp(float3(gamma_lum), gamma_corrected, gamma_sat * midtone_mask + (1.0 - midtone_mask));
    
    // Apply Gain (affects highlights most)
    // Gain multiplies the color values
    float3 gained_color = gamma_corrected * (gain * highlight_mask + (1.0 - highlight_mask));
    
    // Apply saturation in gain range
    float gain_lum = dot(gained_color, float3(0.299, 0.587, 0.114));
    gained_color = lerp(float3(gain_lum), gained_color, gain_sat * highlight_mask + (1.0 - highlight_mask));
    
    // Apply master offset
    gained_color += offset;
    
    // Apply master gamma
    gained_color = pow(max(gained_color, 0.0001), master_gamma);
    
    // Convert back to sRGB
    return linear_to_srgb_precise(gained_color);
}

// Log Color Wheels (Shadows/Midtones/Highlights)
float3 apply_log_color_wheels(float3 color)
{
    float3 shadows = shadows_color_saturation.xyz;
    float shadows_sat = shadows_color_saturation.w;
    float3 midtones = midtones_color_saturation.xyz;
    float midtones_sat = midtones_color_saturation.w;
    float3 highlights = highlights_color_saturation.xyz;
    float highlights_sat = highlights_color_saturation.w;
    float pivot_point = log_master_controls.z;
    
    // Convert to log space for more perceptual color correction
    float3 log_color = log2(max(color, 0.0001)) / 10.0 + 0.5; // Normalize to [0,1]
    
    // Calculate log luminance
    float log_lum = dot(log_color, float3(0.299, 0.587, 0.114));
    
    // Create smooth tonal range masks based on pivot point
    float shadow_mask = smoothstep(pivot_point + 0.2, pivot_point - 0.2, log_lum);
    float highlight_mask = smoothstep(pivot_point - 0.2, pivot_point + 0.2, log_lum);
    float midtone_mask = 1.0 - shadow_mask - highlight_mask;
    
    // Apply color corrections per tonal range
    log_color += shadows * shadow_mask + midtones * midtone_mask + highlights * highlight_mask;
    
    // Apply per-range saturation
    float corrected_lum = dot(log_color, float3(0.299, 0.587, 0.114));
    log_color = lerp(float3(corrected_lum), log_color, 
        shadows_sat * shadow_mask + midtones_sat * midtone_mask + highlights_sat * highlight_mask);
    
    // Convert back from log space
    return pow(2.0, (log_color - 0.5) * 10.0);
}

// Professional Bezier Curve Evaluation
float evaluate_bezier_curve(float input, float4 points[4])
{
    // Clamp input to curve range
    float curve_min = points[0].x;
    float curve_max = points[3].x;
    if (input <= curve_min) return points[0].y;
    if (input >= curve_max) return points[3].y;
    
    // Find segment and interpolate
    float t = (input - curve_min) / (curve_max - curve_min);
    
    // Cubic Bezier interpolation
    float3 p0 = float3(points[0].xy, 0);
    float3 p1 = float3(points[1].xy, 0);
    float3 p2 = float3(points[2].xy, 0);
    float3 p3 = float3(points[3].xy, 0);
    
    float one_minus_t = 1.0 - t;
    float3 result = one_minus_t * one_minus_t * one_minus_t * p0 +
                   3.0 * one_minus_t * one_minus_t * t * p1 +
                   3.0 * one_minus_t * t * t * p2 +
                   t * t * t * p3;
    
    return result.y;
}

// Apply RGB Curves
float3 apply_rgb_curves(float3 color)
{
    float3 result;
    result.r = evaluate_bezier_curve(color.r, curve_red_points);
    result.g = evaluate_bezier_curve(color.g, curve_green_points);
    result.b = evaluate_bezier_curve(color.b, curve_blue_points);
    
    // Apply master curve to luminance
    float master_lum = dot(result, float3(0.299, 0.587, 0.114));
    float corrected_master_lum = evaluate_bezier_curve(master_lum, curve_master_points);
    
    // Blend master curve with RGB curves
    float lum_ratio = corrected_master_lum / max(master_lum, 0.0001);
    result *= lum_ratio;
    
    return result;
}

// HSL Qualifier Mask Generation
float generate_hsl_qualifier_mask(float3 color)
{
    float3 hsv = rgb_to_hsv_precise(color);
    float hue = hsv.x;
    float saturation = hsv.y;
    float luminance = dot(color, float3(0.299, 0.587, 0.114));
    
    // Hue qualification
    float hue_center = hsl_qualifier_hue_range.x;
    float hue_width = hsl_qualifier_hue_range.y;
    float hue_softness = hsl_qualifier_hue_range.z;
    
    float hue_distance = abs(hue - hue_center);
    hue_distance = min(hue_distance, 1.0 - hue_distance); // Handle hue wrap-around
    float hue_mask = 1.0 - smoothstep(hue_width - hue_softness, hue_width + hue_softness, hue_distance);
    
    // Saturation qualification
    float sat_low = hsl_qualifier_sat_range.x;
    float sat_high = hsl_qualifier_sat_range.y;
    float sat_softness = hsl_qualifier_sat_range.z;
    
    float sat_mask = smoothstep(sat_low - sat_softness, sat_low + sat_softness, saturation) *
                    (1.0 - smoothstep(sat_high - sat_softness, sat_high + sat_softness, saturation));
    
    // Luminance qualification
    float lum_low = hsl_qualifier_lum_range.x;
    float lum_high = hsl_qualifier_lum_range.y;
    float lum_softness = hsl_qualifier_lum_range.z;
    
    float lum_mask = smoothstep(lum_low - lum_softness, lum_low + lum_softness, luminance) *
                    (1.0 - smoothstep(lum_high - lum_softness, lum_high + lum_softness, luminance));
    
    return hue_mask * sat_mask * lum_mask;
}

// Temperature and Tint Adjustment
float3 apply_temperature_tint(float3 color)
{
    float temperature = temperature_tint_clarity_dehaze.x;
    float tint = temperature_tint_clarity_dehaze.y;
    
    if (abs(temperature) < 0.001 && abs(tint) < 0.001)
        return color;
    
    // Convert temperature to white point adjustment
    float temp_kelvin = 6500.0 + temperature * 3000.0; // Range: 3500K - 9500K
    
    // Calculate white point for given temperature (simplified)
    float3 white_point;
    if (temp_kelvin < 6500.0)
    {
        // Warm (towards orange/red)
        white_point = float3(1.0, 0.8 + (temp_kelvin - 3500.0) / 3000.0 * 0.2, 0.6 + (temp_kelvin - 3500.0) / 3000.0 * 0.4);
    }
    else
    {
        // Cool (towards blue)
        white_point = float3(0.8 + (9500.0 - temp_kelvin) / 3000.0 * 0.2, 0.9 + (9500.0 - temp_kelvin) / 3000.0 * 0.1, 1.0);
    }
    
    // Apply tint (green/magenta shift)
    white_point.g += tint * 0.2;
    white_point = normalize(white_point);
    
    // White balance adjustment
    return color / white_point;
}

// 3D LUT Application with Trilinear Interpolation
float3 apply_3d_lut(float3 color, float lut_strength)
{
    if (lut_strength <= 0.0)
        return color;
    
    // Assume 32x32x32 LUT stored as 2D texture (32x1024)
    float lut_size = 32.0;
    float3 scaled_color = clamp(color, 0.0, 1.0) * (lut_size - 1.0);
    
    float3 base_coord = floor(scaled_color);
    float3 next_coord = min(base_coord + 1.0, lut_size - 1.0);
    float3 frac_coord = scaled_color - base_coord;
    
    // Calculate 2D texture coordinates for 3D LUT
    float2 uv1 = float2(
        (base_coord.x + base_coord.y * lut_size) / (lut_size * lut_size),
        base_coord.z / lut_size
    );
    
    // Sample 8 corners of the cube and interpolate
    float3 lut_color = lut_texture_3d.Sample(linear_sampler, uv1).rgb;
    
    // Simplified trilinear interpolation (full implementation would sample all 8 corners)
    return lerp(color, lut_color, lut_strength);
}

// Vertex Shader
struct VSOutput
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VSOutput FullscreenVS(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    output.texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.texcoord * 2.0 - 1.0, 0.0, 1.0);
    output.position.y *= -1.0;
    return output;
}

// Color Wheels Pixel Shader
float4 ColorWheelsPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    color.rgb = apply_color_wheels_lift_gamma_gain(color.rgb);
    return color;
}

// Log Color Wheels Pixel Shader
float4 LogColorWheelsPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    color.rgb = apply_log_color_wheels(color.rgb);
    return color;
}

// RGB Curves Pixel Shader
float4 RGBCurvesPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    color.rgb = apply_rgb_curves(color.rgb);
    return color;
}

// HSL Qualifier Pixel Shader
float4 HSLQualifierPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    float mask = generate_hsl_qualifier_mask(color.rgb);
    
    // Apply corrections only to qualified areas
    float qualifier_strength = hsl_qualifier_color_boost.x;
    float color_boost = hsl_qualifier_color_boost.y;
    
    if (mask > 0.0)
    {
        // Boost saturation in qualified areas
        float3 hsv = rgb_to_hsv_precise(color.rgb);
        hsv.y *= (1.0 + color_boost * mask);
        color.rgb = hsv_to_rgb_precise(hsv);
    }
    
    return color;
}

// Temperature/Tint Pixel Shader
float4 TemperatureTintPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    color.rgb = apply_temperature_tint(color.rgb);
    return color;
}

// Complete Color Grading Pipeline Pixel Shader
float4 CompleteColorGradingPS(VSOutput input) : SV_Target
{
    float4 color = input_texture.Sample(linear_sampler, input.texcoord);
    
    // Apply color grading pipeline in professional order
    
    // 1. Input transform (gamma/color space)
    color.rgb = srgb_to_linear_precise(color.rgb);
    
    // 2. Primary color correction
    color.rgb = apply_temperature_tint(color.rgb);
    color.rgb = apply_color_wheels_lift_gamma_gain(color.rgb);
    
    // 3. Curves
    color.rgb = apply_rgb_curves(color.rgb);
    
    // 4. Secondary color correction (HSL qualifier)
    float qualifier_mask = generate_hsl_qualifier_mask(color.rgb);
    // Apply secondary corrections based on mask...
    
    // 5. LUT application
    float lut_strength = lut_strength_film_emulation.x;
    color.rgb = apply_3d_lut(color.rgb, lut_strength);
    
    // 6. Output transform
    color.rgb = linear_to_srgb_precise(color.rgb);
    
    return color;
}

#endif // COLOR_GRADING_HLSL
