// Advanced Temporal Effects - Professional Motion Blur and Temporal Processing
// Supports both HLSL (D3D11) and can be cross-compiled to SPIR-V for Vulkan

#ifndef TEMPORAL_EFFECTS_HLSL
#define TEMPORAL_EFFECTS_HLSL

// Temporal Effects Constants
cbuffer TemporalEffectsConstants : register(b0)
{
    // Motion Blur Parameters
    float4 motion_blur_settings;        // sample_count, shutter_angle, max_blur_radius, quality_level
    float4 motion_blur_advanced;        // depth_threshold, velocity_scale, noise_reduction, adaptive_samples
    float2 global_motion_vector;        // camera motion compensation
    float2 motion_blur_center;          // center point for radial blur
    
    // Optical Flow Parameters  
    float4 optical_flow_params;         // flow_strength, smoothing, edge_threshold, temporal_consistency
    float4 flow_quality_settings;       // pyramid_levels, search_radius, correlation_threshold, subpixel_accuracy
    
    // Frame Blending/Interpolation
    float4 frame_blend_params;          // blend_factor, motion_compensation, ghosting_reduction, sharpening
    float4 interpolation_settings;      // interpolation_mode, temporal_window, adaptive_blending, quality
    
    // Temporal Denoising
    float4 temporal_denoise_params;     // strength, motion_threshold, detail_preservation, noise_profile
    float4 denoise_advanced;            // spatial_radius, temporal_radius, edge_sensitivity, chroma_denoise
    
    // Frame Information
    float4 frame_timing;                // current_time, delta_time, frame_rate, temporal_position
    float4 viewport_info;               // width, height, aspect_ratio, pixel_scale
    
    // Camera Motion (for global compensation)
    float4x4 previous_view_matrix;
    float4x4 current_view_matrix;
    float4x4 inverse_projection_matrix;
};

// Temporal texture inputs
Texture2D current_frame : register(t0);
Texture2D previous_frame : register(t1);
Texture2D next_frame : register(t2);
Texture2D motion_vectors : register(t3);
Texture2D depth_buffer : register(t4);
Texture2D velocity_buffer : register(t5);
Texture2D temporal_history : register(t6);
Texture2D noise_texture : register(t7);

SamplerState linear_sampler : register(s0);
SamplerState point_sampler : register(s1);
SamplerState linear_clamp_sampler : register(s2);

// Utility functions for temporal processing

// High-quality motion vector sampling with sub-pixel accuracy
float2 sample_motion_vector(float2 uv)
{
    // Sample motion vectors with bilinear filtering for smooth motion
    float2 motion = motion_vectors.Sample(linear_sampler, uv).xy;
    
    // Decode from storage format (assumes [-1,1] range stored as [0,1])
    motion = motion * 2.0 - 1.0;
    
    // Scale by velocity and apply global motion compensation
    motion *= motion_blur_advanced.y; // velocity_scale
    motion += global_motion_vector;
    
    return motion;
}

// Calculate depth-aware motion vectors
float2 calculate_depth_motion(float2 uv, float depth)
{
    // Convert screen space to world space
    float4 clip_pos = float4(uv * 2.0 - 1.0, depth, 1.0);
    clip_pos.y *= -1.0; // Flip Y for D3D11
    
    float4 world_pos = mul(clip_pos, inverse_projection_matrix);
    world_pos /= world_pos.w;
    
    // Transform to previous frame
    float4 prev_clip = mul(world_pos, previous_view_matrix);
    prev_clip /= prev_clip.w;
    
    // Calculate screen space motion
    float2 prev_screen = prev_clip.xy * 0.5 + 0.5;
    prev_screen.y = 1.0 - prev_screen.y; // Flip Y back
    
    return uv - prev_screen;
}

// Advanced motion blur with variable sample count and noise reduction
float4 apply_advanced_motion_blur(float2 uv)
{
    int sample_count = (int)motion_blur_settings.x;
    float shutter_angle = motion_blur_settings.y;
    float max_blur_radius = motion_blur_settings.z;
    float quality_level = motion_blur_settings.w;
    
    if (sample_count <= 1 || shutter_angle <= 0.0)
        return current_frame.Sample(linear_sampler, uv);
    
    // Sample motion vector and depth
    float2 motion_vector = sample_motion_vector(uv);
    float depth = depth_buffer.Sample(point_sampler, uv).r;
    
    // Depth-aware motion (optional enhancement)
    if (motion_blur_advanced.x > 0.0) // depth_threshold
    {
        float2 depth_motion = calculate_depth_motion(uv, depth);
        motion_vector = lerp(motion_vector, depth_motion, motion_blur_advanced.x);
    }
    
    // Scale motion by shutter angle and limit maximum blur
    motion_vector *= (shutter_angle / 360.0);
    float motion_length = length(motion_vector);
    if (motion_length > max_blur_radius)
        motion_vector = normalize(motion_vector) * max_blur_radius;
    
    // Adaptive sample count based on motion magnitude
    if (motion_blur_advanced.w > 0.0) // adaptive_samples
    {
        float motion_factor = motion_length / max_blur_radius;
        sample_count = (int)lerp(1.0, float(sample_count), motion_factor);
        sample_count = max(1, sample_count);
    }
    
    // Accumulate samples with advanced weighting
    float4 result = float4(0, 0, 0, 0);
    float total_weight = 0.0;
    float noise_reduction = motion_blur_advanced.z;
    
    // Generate jittered samples for noise reduction
    float2 jitter_seed = uv * 1000.0 + frame_timing.x;
    
    for (int i = 0; i < sample_count; i++)
    {
        float t = (float(i) + 0.5) / float(sample_count) - 0.5; // [-0.5, 0.5]
        
        // Add temporal jitter for noise reduction
        if (noise_reduction > 0.0)
        {
            float jitter = (noise_texture.Sample(point_sampler, jitter_seed + float2(i * 0.1, 0)).r - 0.5) * 0.5;
            t += jitter * noise_reduction / float(sample_count);
        }
        
        float2 sample_uv = uv + motion_vector * t;
        
        // Quality-based weight calculation
        float weight = 1.0;
        if (quality_level > 1.0)
        {
            // Gaussian weight for higher quality
            weight = exp(-t * t * 4.0);
        }
        else
        {
            // Simple triangle filter for performance
            weight = 1.0 - abs(t * 2.0);
        }
        
        // Sample with bounds checking
        if (sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && 
            sample_uv.y >= 0.0 && sample_uv.y <= 1.0)
        {
            float4 sample_color = current_frame.Sample(linear_clamp_sampler, sample_uv);
            result += sample_color * weight;
            total_weight += weight;
        }
    }
    
    return total_weight > 0.0 ? result / total_weight : current_frame.Sample(linear_sampler, uv);
}

// Optical flow estimation using Lucas-Kanade method
float2 estimate_optical_flow(float2 uv)
{
    float flow_strength = optical_flow_params.x;
    float smoothing = optical_flow_params.y;
    float edge_threshold = optical_flow_params.z;
    
    if (flow_strength <= 0.0)
        return float2(0, 0);
    
    float2 texel_size = 1.0 / viewport_info.xy;
    
    // Sample current and previous frames
    float3 current_color = current_frame.Sample(linear_sampler, uv).rgb;
    float3 previous_color = previous_frame.Sample(linear_sampler, uv).rgb;
    
    // Calculate spatial gradients using Sobel operator
    float3 grad_x = (
        current_frame.Sample(linear_sampler, uv + float2(texel_size.x, 0)).rgb -
        current_frame.Sample(linear_sampler, uv - float2(texel_size.x, 0)).rgb
    ) * 0.5;
    
    float3 grad_y = (
        current_frame.Sample(linear_sampler, uv + float2(0, texel_size.y)).rgb -
        current_frame.Sample(linear_sampler, uv - float2(0, texel_size.y)).rgb
    ) * 0.5;
    
    // Calculate temporal gradient
    float3 grad_t = current_color - previous_color;
    
    // Convert to luminance for flow calculation
    float Ix = dot(grad_x, float3(0.299, 0.587, 0.114));
    float Iy = dot(grad_y, float3(0.299, 0.587, 0.114));
    float It = dot(grad_t, float3(0.299, 0.587, 0.114));
    
    // Lucas-Kanade optical flow estimation
    float A11 = Ix * Ix;
    float A12 = Ix * Iy;
    float A22 = Iy * Iy;
    float b1 = -Ix * It;
    float b2 = -Iy * It;
    
    // Solve 2x2 system with regularization
    float det = A11 * A22 - A12 * A12 + 1e-6; // Add small epsilon for stability
    float2 flow = float2(
        (A22 * b1 - A12 * b2) / det,
        (A11 * b2 - A12 * b1) / det
    ) * flow_strength;
    
    // Edge detection to improve flow quality
    float edge_magnitude = sqrt(Ix * Ix + Iy * Iy);
    float edge_weight = smoothstep(edge_threshold * 0.5, edge_threshold, edge_magnitude);
    
    return flow * edge_weight;
}

// Temporal frame blending with motion compensation
float4 apply_frame_blending(float2 uv)
{
    float blend_factor = frame_blend_params.x;
    float motion_compensation = frame_blend_params.y;
    float ghosting_reduction = frame_blend_params.z;
    
    if (blend_factor <= 0.0)
        return current_frame.Sample(linear_sampler, uv);
    
    float4 current_color = current_frame.Sample(linear_sampler, uv);
    
    // Motion-compensated sampling
    float2 motion_vector = float2(0, 0);
    if (motion_compensation > 0.0)
    {
        motion_vector = sample_motion_vector(uv) * motion_compensation;
    }
    
    float2 prev_uv = uv - motion_vector;
    float2 next_uv = uv + motion_vector;
    
    // Sample neighboring frames with bounds checking
    float4 previous_color = float4(0, 0, 0, 0);
    float4 next_color = float4(0, 0, 0, 0);
    float prev_weight = 0.0;
    float next_weight = 0.0;
    
    if (prev_uv.x >= 0.0 && prev_uv.x <= 1.0 && prev_uv.y >= 0.0 && prev_uv.y <= 1.0)
    {
        previous_color = previous_frame.Sample(linear_clamp_sampler, prev_uv);
        prev_weight = 1.0;
    }
    
    if (next_uv.x >= 0.0 && next_uv.x <= 1.0 && next_uv.y >= 0.0 && next_uv.y <= 1.0)
    {
        next_color = next_frame.Sample(linear_clamp_sampler, next_uv);
        next_weight = 1.0;
    }
    
    // Ghosting reduction using color difference thresholding
    if (ghosting_reduction > 0.0)
    {
        float prev_diff = length(current_color.rgb - previous_color.rgb);
        float next_diff = length(current_color.rgb - next_color.rgb);
        
        prev_weight *= smoothstep(ghosting_reduction, 0.0, prev_diff);
        next_weight *= smoothstep(ghosting_reduction, 0.0, next_diff);
    }
    
    // Blend frames
    float total_weight = 1.0 + prev_weight * blend_factor + next_weight * blend_factor;
    float4 blended_color = (current_color + 
                           previous_color * prev_weight * blend_factor + 
                           next_color * next_weight * blend_factor) / total_weight;
    
    return blended_color;
}

// Temporal denoising using motion-compensated filtering
float4 apply_temporal_denoising(float2 uv)
{
    float strength = temporal_denoise_params.x;
    float motion_threshold = temporal_denoise_params.y;
    float detail_preservation = temporal_denoise_params.z;
    
    if (strength <= 0.0)
        return current_frame.Sample(linear_sampler, uv);
    
    float4 current_color = current_frame.Sample(linear_sampler, uv);
    float2 motion_vector = sample_motion_vector(uv);
    float motion_magnitude = length(motion_vector);
    
    // Reduce denoising strength in high-motion areas
    float motion_factor = smoothstep(0.0, motion_threshold, motion_magnitude);
    float effective_strength = strength * (1.0 - motion_factor * 0.7);
    
    // Sample temporal history with motion compensation
    float2 history_uv = uv - motion_vector;
    float4 history_color = temporal_history.Sample(linear_clamp_sampler, history_uv);
    
    // Calculate temporal difference
    float color_diff = length(current_color.rgb - history_color.rgb);
    
    // Adaptive blending based on color similarity
    float temporal_weight = smoothstep(detail_preservation, 0.0, color_diff) * effective_strength;
    
    return lerp(current_color, history_color, temporal_weight);
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

// Motion Blur Pixel Shader
float4 MotionBlurPS(VSOutput input) : SV_Target
{
    return apply_advanced_motion_blur(input.texcoord);
}

// Optical Flow Estimation Pixel Shader
float4 OpticalFlowPS(VSOutput input) : SV_Target
{
    float2 flow = estimate_optical_flow(input.texcoord);
    
    // Encode flow to [0,1] range for storage
    flow = flow * 0.5 + 0.5;
    
    return float4(flow.x, flow.y, 0.0, 1.0);
}

// Frame Blending Pixel Shader
float4 FrameBlendingPS(VSOutput input) : SV_Target
{
    return apply_frame_blending(input.texcoord);
}

// Temporal Denoising Pixel Shader
float4 TemporalDenoisingPS(VSOutput input) : SV_Target
{
    return apply_temporal_denoising(input.texcoord);
}

// Composite Temporal Effects Pixel Shader
float4 CompositeTemporalPS(VSOutput input) : SV_Target
{
    float2 uv = input.texcoord;
    
    // Apply effects in order: denoising -> blending -> motion blur
    float4 color = apply_temporal_denoising(uv);
    
    // Update current frame texture for next operations
    // (In practice, this would be done through render targets)
    
    color = apply_frame_blending(uv);
    color = apply_advanced_motion_blur(uv);
    
    return color;
}

// Motion Vector Visualization (for debugging)
float4 MotionVectorVisPS(VSOutput input) : SV_Target
{
    float2 motion = sample_motion_vector(input.texcoord);
    float motion_magnitude = length(motion);
    
    // Normalize and visualize motion
    float2 normalized_motion = motion / max(motion_magnitude, 0.001);
    
    // Color-code direction and magnitude
    float3 color = float3(
        (normalized_motion.x + 1.0) * 0.5,  // Red: horizontal motion
        (normalized_motion.y + 1.0) * 0.5,  // Green: vertical motion
        motion_magnitude * 10.0             // Blue: magnitude
    );
    
    return float4(color, 1.0);
}

#endif // TEMPORAL_EFFECTS_HLSL
