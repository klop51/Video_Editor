// Advanced Spatial Effects - Professional Lens Correction and Geometric Transformation
// Supports both HLSL (D3D11) and can be cross-compiled to SPIR-V for Vulkan

#ifndef SPATIAL_EFFECTS_HLSL
#define SPATIAL_EFFECTS_HLSL

// Spatial Effects Constants
cbuffer SpatialEffectsConstants : register(b0)
{
    // Lens Distortion Correction
    float4 barrel_pincushion_asym_zoom;      // barrel, pincushion, asymmetric, zoom
    float4 lens_distortion_coeffs;           // k1, k2, k3, k4 (radial distortion)
    float4 tangential_distortion;            // p1, p2, unused, unused
    float2 distortion_center_offset;         // center offset from 0.5, 0.5
    
    // Keystone Correction (Perspective Transform)
    float4 keystone_corners_tl_tr;           // top_left.xy, top_right.xy
    float4 keystone_corners_bl_br;           // bottom_left.xy, bottom_right.xy
    float4 keystone_strength_feather;        // strength, edge_feather, grid_alpha, unused
    
    // Rolling Shutter Correction
    float4 rolling_shutter_params;           // strength, scan_direction, sync_offset, unused
    float4 rolling_shutter_curve;            // curve_power, frequency, phase, amplitude
    
    // Stabilization
    float4 stabilization_transform[4];       // 4x4 transformation matrix
    float4 stabilization_params;             // smoothing, crop_factor, edge_mode, motion_scale
    
    // Lens Characteristics
    float4 lens_characteristics;             // focal_length, sensor_width, f_stop, focus_distance
    float4 optical_corrections;              // vignette_compensation, distortion_compensation, unused, unused
    
    // Viewport and frame info
    float4 viewport_time_frame;              // width, height, time, frame_number
    float4 source_resolution;                // source_width, source_height, pixel_aspect, unused
};

// Texture inputs
Texture2D input_texture : register(t0);
Texture2D stabilization_mask : register(t1);
Texture2D optical_flow_texture : register(t2);
Texture2D reference_grid_texture : register(t3);

SamplerState linear_sampler : register(s0);
SamplerState linear_clamp_sampler : register(s1);
SamplerState point_sampler : register(s2);

// Utility functions for geometric transformations

// Bilinear interpolation for high-quality sampling
float4 sample_bilinear(Texture2D tex, float2 uv)
{
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return float4(0.0, 0.0, 0.0, 0.0);
    
    return tex.Sample(linear_clamp_sampler, uv);
}

// Bicubic interpolation for premium quality (computationally expensive)
float4 sample_bicubic(Texture2D tex, float2 uv)
{
    float2 texture_size = float2(source_resolution.xy);
    float2 pixel_pos = uv * texture_size - 0.5;
    float2 pixel_frac = frac(pixel_pos);
    float2 pixel_int = floor(pixel_pos);
    
    // Cubic interpolation weights
    float2 w0 = pixel_frac * (-0.5 + pixel_frac * (1.0 - 0.5 * pixel_frac));
    float2 w1 = 1.0 + pixel_frac * pixel_frac * (-2.5 + 1.5 * pixel_frac);
    float2 w2 = pixel_frac * (0.5 + pixel_frac * (2.0 - 1.5 * pixel_frac));
    float2 w3 = pixel_frac * pixel_frac * (-0.5 + 0.5 * pixel_frac);
    
    float2 s0 = w0 + w1;
    float2 s1 = w2 + w3;
    float2 f0 = w1 / s0;
    float2 f1 = w3 / s1;
    
    float2 t0 = (pixel_int - 1.0 + f0) / texture_size;
    float2 t1 = (pixel_int + 1.0 + f1) / texture_size;
    
    float4 tex00 = sample_bilinear(tex, float2(t0.x, t0.y));
    float4 tex10 = sample_bilinear(tex, float2(t1.x, t0.y));
    float4 tex01 = sample_bilinear(tex, float2(t0.x, t1.y));
    float4 tex11 = sample_bilinear(tex, float2(t1.x, t1.y));
    
    float4 result = lerp(lerp(tex00, tex10, s1.x), lerp(tex01, tex11, s1.x), s1.y);
    return result;
}

// Professional Lens Distortion Correction
// Based on Brown-Conrady distortion model used in photogrammetry
float2 correct_lens_distortion(float2 uv)
{
    float k1 = lens_distortion_coeffs.x;
    float k2 = lens_distortion_coeffs.y; 
    float k3 = lens_distortion_coeffs.z;
    float k4 = lens_distortion_coeffs.w;
    float p1 = tangential_distortion.x;
    float p2 = tangential_distortion.y;
    
    // If no distortion coefficients, return unchanged
    if (abs(k1) < 0.0001 && abs(k2) < 0.0001 && abs(k3) < 0.0001 && 
        abs(k4) < 0.0001 && abs(p1) < 0.0001 && abs(p2) < 0.0001)
        return uv;
    
    // Center coordinates with offset
    float2 center = float2(0.5, 0.5) + distortion_center_offset;
    float2 normalized_coords = (uv - center);
    
    // Apply aspect ratio normalization
    float aspect_ratio = viewport_time_frame.x / viewport_time_frame.y;
    normalized_coords.x *= aspect_ratio;
    
    // Calculate radial distance
    float r2 = dot(normalized_coords, normalized_coords);
    float r4 = r2 * r2;
    float r6 = r4 * r2;
    float r8 = r4 * r4;
    
    // Radial distortion correction (Brown-Conrady model)
    float radial_distortion = 1.0 + k1 * r2 + k2 * r4 + k3 * r6 + k4 * r8;
    
    // Tangential distortion correction
    float2 tangential_correction = float2(
        2.0 * p1 * normalized_coords.x * normalized_coords.y + p2 * (r2 + 2.0 * normalized_coords.x * normalized_coords.x),
        p1 * (r2 + 2.0 * normalized_coords.y * normalized_coords.y) + 2.0 * p2 * normalized_coords.x * normalized_coords.y
    );
    
    // Apply corrections
    float2 corrected_coords = normalized_coords * radial_distortion + tangential_correction;
    
    // Restore aspect ratio and center
    corrected_coords.x /= aspect_ratio;
    corrected_coords += center;
    
    return corrected_coords;
}

// Professional Keystone/Perspective Correction
// Uses homogeneous coordinates for accurate perspective transformation
float2 correct_keystone(float2 uv)
{
    float strength = keystone_strength_feather.x;
    
    if (strength <= 0.0)
        return uv;
    
    // Define perspective transformation based on corner positions
    float2 tl = keystone_corners_tl_tr.xy;  // top-left
    float2 tr = keystone_corners_tl_tr.zw;  // top-right  
    float2 bl = keystone_corners_bl_br.xy;  // bottom-left
    float2 br = keystone_corners_bl_br.zw;  // bottom-right
    
    // Calculate perspective transform matrix using bilinear interpolation
    // This maps from unit square to arbitrary quadrilateral
    float2 p0 = bl;                           // (0,0) -> bottom-left
    float2 p1 = br - bl;                      // (1,0) -> bottom-right direction
    float2 p2 = tl - bl;                      // (0,1) -> top-left direction  
    float2 p3 = tl + br - tr - bl;           // (1,1) -> perspective correction
    
    // Solve for perspective-correct UV coordinates
    // This is a simplified perspective mapping
    float u = uv.x;
    float v = uv.y;
    float2 corrected_pos = p0 + u * p1 + v * p2 + u * v * p3;
    
    // Apply strength blending
    float2 final_uv = lerp(uv, corrected_pos, strength);
    
    return final_uv;
}

// Rolling Shutter Correction
float2 correct_rolling_shutter(float2 uv)
{
    float strength = rolling_shutter_params.x;
    float scan_direction = rolling_shutter_params.y; // 0=horizontal, 1=vertical
    float sync_offset = rolling_shutter_params.z;
    
    if (strength <= 0.0)
        return uv;
    
    // Calculate scan position (0 to 1 across scan direction)
    float scan_pos = (scan_direction > 0.5) ? uv.y : uv.x;
    scan_pos = frac(scan_pos + sync_offset);
    
    // Generate rolling shutter offset pattern
    float curve_power = rolling_shutter_curve.x;
    float frequency = rolling_shutter_curve.y;
    float phase = rolling_shutter_curve.z;
    float amplitude = rolling_shutter_curve.w;
    
    // Create smooth rolling pattern
    float offset_amount = sin(scan_pos * frequency * 2.0 * 3.14159 + phase);
    offset_amount = pow(abs(offset_amount), curve_power) * sign(offset_amount);
    offset_amount *= amplitude * strength;
    
    // Apply offset perpendicular to scan direction
    float2 corrected_uv = uv;
    if (scan_direction > 0.5)
        corrected_uv.x += offset_amount;
    else
        corrected_uv.y += offset_amount;
    
    return corrected_uv;
}

// Digital Stabilization with Motion Compensation
float2 apply_stabilization(float2 uv)
{
    float smoothing = stabilization_params.x;
    float crop_factor = stabilization_params.y;
    float edge_mode = stabilization_params.z;
    
    if (smoothing <= 0.0)
        return uv;
    
    // Apply stabilization transform matrix
    float4 homogeneous_coord = float4(uv.x, uv.y, 0.0, 1.0);
    
    float4 transformed = mul(homogeneous_coord, transpose(float4x4(
        stabilization_transform[0],
        stabilization_transform[1], 
        stabilization_transform[2],
        stabilization_transform[3]
    )));
    
    float2 stabilized_uv = transformed.xy / transformed.w;
    
    // Apply crop factor to zoom in and hide moving edges
    float2 center = float2(0.5, 0.5);
    stabilized_uv = (stabilized_uv - center) / crop_factor + center;
    
    return stabilized_uv;
}

// Edge handling for out-of-bounds sampling
float4 handle_edge_sampling(Texture2D tex, float2 uv, float edge_mode)
{
    if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
    {
        return sample_bicubic(tex, uv);
    }
    
    // Edge mode: 0=black, 1=clamp, 2=mirror, 3=wrap
    if (edge_mode < 0.5)
    {
        // Black borders
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    else if (edge_mode < 1.5)
    {
        // Clamp/extend edges
        float2 clamped_uv = clamp(uv, 0.0, 1.0);
        return sample_bicubic(tex, clamped_uv);
    }
    else if (edge_mode < 2.5)
    {
        // Mirror edges
        float2 mirrored_uv = float2(
            uv.x < 0.0 ? -uv.x : (uv.x > 1.0 ? 2.0 - uv.x : uv.x),
            uv.y < 0.0 ? -uv.y : (uv.y > 1.0 ? 2.0 - uv.y : uv.y)
        );
        return sample_bicubic(tex, mirrored_uv);
    }
    else
    {
        // Wrap/repeat
        float2 wrapped_uv = frac(uv);
        return sample_bicubic(tex, wrapped_uv);
    }
}

// Vertex shader
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

// Lens Distortion Correction Pixel Shader
float4 LensDistortionCorrectionPS(VSOutput input) : SV_Target
{
    float2 corrected_uv = correct_lens_distortion(input.texcoord);
    return handle_edge_sampling(input_texture, corrected_uv, 1.0); // Clamp edges
}

// Keystone Correction Pixel Shader  
float4 KeystoneCorrectionPS(VSOutput input) : SV_Target
{
    float2 corrected_uv = correct_keystone(input.texcoord);
    
    // Apply edge feathering for smooth blending
    float edge_feather = keystone_strength_feather.y;
    float2 edge_distance = min(corrected_uv, 1.0 - corrected_uv);
    float edge_factor = smoothstep(0.0, edge_feather, min(edge_distance.x, edge_distance.y));
    
    float4 corrected_color = handle_edge_sampling(input_texture, corrected_uv, 0.0);
    float4 original_color = input_texture.Sample(linear_sampler, input.texcoord);
    
    return lerp(original_color, corrected_color, edge_factor);
}

// Rolling Shutter Correction Pixel Shader
float4 RollingShutterCorrectionPS(VSOutput input) : SV_Target
{
    float2 corrected_uv = correct_rolling_shutter(input.texcoord);
    return handle_edge_sampling(input_texture, corrected_uv, 2.0); // Mirror edges
}

// Stabilization Pixel Shader
float4 StabilizationPS(VSOutput input) : SV_Target
{
    float2 stabilized_uv = apply_stabilization(input.texcoord);
    float edge_mode = stabilization_params.z;
    return handle_edge_sampling(input_texture, stabilized_uv, edge_mode);
}

// Composite Spatial Correction Pixel Shader
float4 CompositeSpatialCorrectionPS(VSOutput input) : SV_Target
{
    float2 uv = input.texcoord;
    
    // Apply corrections in order: stabilization -> lens -> keystone -> rolling shutter
    uv = apply_stabilization(uv);
    uv = correct_lens_distortion(uv);
    uv = correct_keystone(uv);
    uv = correct_rolling_shutter(uv);
    
    // Sample with high-quality interpolation
    return handle_edge_sampling(input_texture, uv, stabilization_params.z);
}

// Debug Grid Overlay Pixel Shader (for alignment assistance)
float4 DebugGridPS(VSOutput input) : SV_Target
{
    float4 base_color = input_texture.Sample(linear_sampler, input.texcoord);
    float grid_alpha = keystone_strength_feather.z;
    
    if (grid_alpha <= 0.0)
        return base_color;
    
    // Generate grid pattern
    float2 grid_pos = input.texcoord * 20.0; // 20x20 grid
    float2 grid_lines = abs(frac(grid_pos) - 0.5);
    float grid_intensity = 1.0 - smoothstep(0.0, 0.05, min(grid_lines.x, grid_lines.y));
    
    // Overlay grid
    float3 grid_color = float3(1.0, 1.0, 1.0);
    return float4(lerp(base_color.rgb, grid_color, grid_intensity * grid_alpha), base_color.a);
}

#endif // SPATIAL_EFFECTS_HLSL
