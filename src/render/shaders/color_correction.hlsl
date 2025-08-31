// Advanced Color Correction Shaders
// Week 6: Professional video effects capability

cbuffer ColorCorrectionConstants : register(b0) {
    float brightness;        // -1.0 to 1.0
    float contrast;         // 0.0 to 2.0 (1.0 = no change)
    float saturation;       // 0.0 to 2.0 (1.0 = no change)
    float gamma;           // 0.1 to 3.0 (1.0 = no change)
    float3 shadows;        // RGB multipliers for shadows
    float3 midtones;       // RGB multipliers for midtones  
    float3 highlights;     // RGB multipliers for highlights
    float shadow_range;    // 0.0 to 1.0, defines shadow cutoff
    float highlight_range; // 0.0 to 1.0, defines highlight cutoff
    float _padding[2];
};

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct VSInput {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PSInput VS_ColorCorrection(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

// Convert RGB to luminance for shadow/midtone/highlight isolation
float get_luminance(float3 color) {
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

// Smooth step function for blend regions
float smooth_step_range(float value, float low, float high) {
    float t = saturate((value - low) / (high - low));
    return t * t * (3.0f - 2.0f * t);
}

// Apply lift, gamma, gain (shadows, midtones, highlights)
float3 apply_lift_gamma_gain(float3 color, float3 lift, float3 gamma_val, float3 gain) {
    // Lift affects shadows (adds to color)
    color = color + lift;
    
    // Gamma affects midtones (power curve)
    color = pow(max(color, 0.0001f), 1.0f / gamma_val);
    
    // Gain affects highlights (multiplies color)
    color = color * gain;
    
    return color;
}

// Shadow/midtone/highlight color grading
float3 apply_three_way_color_grading(float3 color) {
    float luma = get_luminance(color);
    
    // Calculate blend weights for shadows, midtones, highlights
    float shadow_weight = 1.0f - smooth_step_range(luma, 0.0f, shadow_range);
    float highlight_weight = smooth_step_range(luma, 1.0f - highlight_range, 1.0f);
    float midtone_weight = 1.0f - shadow_weight - highlight_weight;
    
    // Apply color corrections with proper weighting
    float3 result = color;
    result = lerp(result, result * shadows, shadow_weight);
    result = lerp(result, result * midtones, midtone_weight);
    result = lerp(result, result * highlights, highlight_weight);
    
    return result;
}

// HSV saturation adjustment
float3 adjust_saturation(float3 color, float sat) {
    float luma = get_luminance(color);
    return lerp(float3(luma, luma, luma), color, sat);
}

// Main color correction function
float4 apply_color_correction(float4 input_color) {
    float3 color = input_color.rgb;
    
    // 1. Apply brightness (simple add)
    color = color + brightness;
    
    // 2. Apply contrast (scale around 0.5)
    color = (color - 0.5f) * contrast + 0.5f;
    
    // 3. Apply gamma correction
    color = pow(max(color, 0.0001f), 1.0f / gamma);
    
    // 4. Apply three-way color grading
    color = apply_three_way_color_grading(color);
    
    // 5. Apply saturation adjustment
    color = adjust_saturation(color, saturation);
    
    // 6. Clamp to valid range
    color = saturate(color);
    
    return float4(color, input_color.a);
}

float4 PS_ColorCorrection(PSInput input) : SV_TARGET {
    float4 inputColor = inputTexture.Sample(linearSampler, input.texcoord);
    return apply_color_correction(inputColor);
}
