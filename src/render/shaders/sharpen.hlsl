// Sharpening Filter Shaders
// Week 6: Professional sharpening effects

cbuffer SharpenConstants : register(b0) {
    float sharpen_strength;     // 0.0 to 2.0 (0.0 = no effect, 1.0 = moderate, 2.0 = strong)
    float edge_threshold;       // 0.0 to 1.0, prevents over-sharpening of noise
    float2 texture_size;        // Input texture dimensions
    float clamp_strength;       // Prevents overshooting
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

PSInput VS_Sharpen(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

// Convert RGB to luminance for edge detection
float get_luminance(float3 color) {
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

// Unsharp mask sharpening
float4 unsharp_mask(float2 uv) {
    float2 texel_size = 1.0f / texture_size;
    
    // Sample center pixel
    float4 center = inputTexture.Sample(linearSampler, uv);
    
    // Sample surrounding pixels for blur approximation
    float4 blur = float4(0.0f, 0.0f, 0.0f, 0.0f);
    blur += inputTexture.Sample(linearSampler, uv + float2(-texel_size.x, -texel_size.y)) * 0.0625f; // Top-left
    blur += inputTexture.Sample(linearSampler, uv + float2(0.0f, -texel_size.y)) * 0.125f;         // Top
    blur += inputTexture.Sample(linearSampler, uv + float2(texel_size.x, -texel_size.y)) * 0.0625f; // Top-right
    blur += inputTexture.Sample(linearSampler, uv + float2(-texel_size.x, 0.0f)) * 0.125f;         // Left
    blur += inputTexture.Sample(linearSampler, uv) * 0.25f;                                         // Center
    blur += inputTexture.Sample(linearSampler, uv + float2(texel_size.x, 0.0f)) * 0.125f;          // Right
    blur += inputTexture.Sample(linearSampler, uv + float2(-texel_size.x, texel_size.y)) * 0.0625f; // Bottom-left
    blur += inputTexture.Sample(linearSampler, uv + float2(0.0f, texel_size.y)) * 0.125f;          // Bottom
    blur += inputTexture.Sample(linearSampler, uv + float2(texel_size.x, texel_size.y)) * 0.0625f;  // Bottom-right
    
    // Calculate high-frequency detail
    float4 detail = center - blur;
    
    // Edge detection to prevent noise amplification
    float edge_strength = length(detail.rgb);
    float adaptive_strength = edge_strength > edge_threshold ? sharpen_strength : sharpen_strength * 0.5f;
    
    // Apply sharpening with clamping
    float4 sharpened = center + detail * adaptive_strength;
    sharpened = clamp(sharpened, center - clamp_strength, center + clamp_strength);
    
    return float4(saturate(sharpened.rgb), center.a);
}

// Laplacian sharpening kernel
float4 laplacian_sharpen(float2 uv) {
    float2 texel_size = 1.0f / texture_size;
    
    // Laplacian kernel:
    // [0 -1  0]
    // [-1 5 -1]
    // [0 -1  0]
    
    float4 center = inputTexture.Sample(linearSampler, uv);
    float4 top    = inputTexture.Sample(linearSampler, uv + float2(0.0f, -texel_size.y));
    float4 bottom = inputTexture.Sample(linearSampler, uv + float2(0.0f, texel_size.y));
    float4 left   = inputTexture.Sample(linearSampler, uv + float2(-texel_size.x, 0.0f));
    float4 right  = inputTexture.Sample(linearSampler, uv + float2(texel_size.x, 0.0f));
    
    // Apply Laplacian kernel
    float4 laplacian = center * 5.0f - top - bottom - left - right;
    
    // Blend with original based on strength
    float4 result = center + laplacian * sharpen_strength * 0.2f;
    
    return float4(saturate(result.rgb), center.a);
}

// High-quality sharpening with edge preservation
float4 edge_preserving_sharpen(float2 uv) {
    float2 texel_size = 1.0f / texture_size;
    
    // Sample 3x3 neighborhood
    float4 samples[9];
    samples[0] = inputTexture.Sample(linearSampler, uv + float2(-texel_size.x, -texel_size.y));
    samples[1] = inputTexture.Sample(linearSampler, uv + float2(0.0f, -texel_size.y));
    samples[2] = inputTexture.Sample(linearSampler, uv + float2(texel_size.x, -texel_size.y));
    samples[3] = inputTexture.Sample(linearSampler, uv + float2(-texel_size.x, 0.0f));
    samples[4] = inputTexture.Sample(linearSampler, uv); // Center
    samples[5] = inputTexture.Sample(linearSampler, uv + float2(texel_size.x, 0.0f));
    samples[6] = inputTexture.Sample(linearSampler, uv + float2(-texel_size.x, texel_size.y));
    samples[7] = inputTexture.Sample(linearSampler, uv + float2(0.0f, texel_size.y));
    samples[8] = inputTexture.Sample(linearSampler, uv + float2(texel_size.x, texel_size.y));
    
    float4 center = samples[4];
    
    // Calculate local variance to detect edges
    float4 mean = float4(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 9; i++) {
        mean += samples[i];
    }
    mean /= 9.0f;
    
    float variance = 0.0f;
    for (int j = 0; j < 9; j++) {
        float4 diff = samples[j] - mean;
        variance += dot(diff.rgb, diff.rgb);
    }
    variance /= 9.0f;
    
    // Adaptive sharpening based on local variance
    float edge_factor = saturate(variance * 10.0f);
    float adaptive_strength = sharpen_strength * edge_factor;
    
    // Simple sharpening kernel
    float4 sharpened = center * 1.5f - (samples[1] + samples[3] + samples[5] + samples[7]) * 0.125f;
    
    return lerp(center, sharpened, adaptive_strength);
}

float4 PS_UnsharpMask(PSInput input) : SV_TARGET {
    return unsharp_mask(input.texcoord);
}

float4 PS_LaplacianSharpen(PSInput input) : SV_TARGET {
    return laplacian_sharpen(input.texcoord);
}

float4 PS_EdgePreservingSharpen(PSInput input) : SV_TARGET {
    return edge_preserving_sharpen(input.texcoord);
}
