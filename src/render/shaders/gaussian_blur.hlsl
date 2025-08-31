// Gaussian Blur Shaders - Two-pass separable filter
// Week 6: High-quality blur effects for professional video

cbuffer BlurConstants : register(b0) {
    float blur_radius;          // Blur radius in pixels
    float2 texture_size;        // Input texture dimensions  
    float2 blur_direction;      // (1,0) for horizontal, (0,1) for vertical
    int kernel_size;           // Number of samples (should be odd)
    float _padding[3];
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

PSInput VS_Blur(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

// Calculate Gaussian weight for given distance and sigma
float gaussian_weight(float x, float sigma) {
    float sigma_squared = sigma * sigma;
    return exp(-(x * x) / (2.0f * sigma_squared)) / sqrt(2.0f * 3.14159265f * sigma_squared);
}

// Optimized Gaussian blur using linear sampling
float4 gaussian_blur_optimized(float2 uv, float2 direction, float radius) {
    float2 texel_size = 1.0f / texture_size;
    float sigma = radius * 0.3f;  // Convert radius to sigma
    
    float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weight_sum = 0.0f;
    
    // Calculate the number of samples based on radius
    int samples = max(3, (int)(radius * 2.0f) + 1);
    if (samples % 2 == 0) samples++;  // Ensure odd number
    
    int half_samples = samples / 2;
    
    // Sample the texture with Gaussian weights
    for (int i = -half_samples; i <= half_samples; i++) {
        float weight = gaussian_weight(float(i), sigma);
        float2 sample_uv = uv + direction * texel_size * float(i);
        
        // Clamp UV coordinates to valid range
        sample_uv = saturate(sample_uv);
        
        float4 sample_color = inputTexture.Sample(linearSampler, sample_uv);
        result += sample_color * weight;
        weight_sum += weight;
    }
    
    // Normalize by total weight
    return result / weight_sum;
}

// High-quality Gaussian blur with more samples
float4 gaussian_blur_high_quality(float2 uv, float2 direction, float radius) {
    float2 texel_size = 1.0f / texture_size;
    float sigma = radius * 0.25f;
    
    float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weight_sum = 0.0f;
    
    // Use more samples for higher quality
    int samples = max(7, (int)(radius * 3.0f) + 1);
    if (samples % 2 == 0) samples++;
    samples = min(samples, 31);  // Limit for performance
    
    int half_samples = samples / 2;
    
    for (int i = -half_samples; i <= half_samples; i++) {
        float weight = gaussian_weight(float(i), sigma);
        float2 sample_uv = uv + direction * texel_size * float(i);
        sample_uv = saturate(sample_uv);
        
        float4 sample_color = inputTexture.Sample(linearSampler, sample_uv);
        result += sample_color * weight;
        weight_sum += weight;
    }
    
    return result / weight_sum;
}

// Box blur for fast approximation
float4 box_blur(float2 uv, float2 direction, float radius) {
    float2 texel_size = 1.0f / texture_size;
    
    float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int samples = max(3, (int)(radius * 2.0f) + 1);
    if (samples % 2 == 0) samples++;
    
    int half_samples = samples / 2;
    
    for (int i = -half_samples; i <= half_samples; i++) {
        float2 sample_uv = uv + direction * texel_size * float(i);
        sample_uv = saturate(sample_uv);
        result += inputTexture.Sample(linearSampler, sample_uv);
    }
    
    return result / float(samples);
}

float4 PS_GaussianBlur(PSInput input) : SV_TARGET {
    // Use optimized Gaussian blur for good quality/performance balance
    return gaussian_blur_optimized(input.texcoord, blur_direction, blur_radius);
}

float4 PS_GaussianBlurHQ(PSInput input) : SV_TARGET {
    // High-quality version for final output
    return gaussian_blur_high_quality(input.texcoord, blur_direction, blur_radius);
}

float4 PS_BoxBlur(PSInput input) : SV_TARGET {
    // Fast box blur for real-time preview
    return box_blur(input.texcoord, blur_direction, blur_radius);
}

// Horizontal blur pass (blur_direction should be (1,0))
float4 PS_BlurHorizontal(PSInput input) : SV_TARGET {
    return gaussian_blur_optimized(input.texcoord, float2(1.0f, 0.0f), blur_radius);
}

// Vertical blur pass (blur_direction should be (0,1))
float4 PS_BlurVertical(PSInput input) : SV_TARGET {
    return gaussian_blur_optimized(input.texcoord, float2(0.0f, 1.0f), blur_radius);
}
