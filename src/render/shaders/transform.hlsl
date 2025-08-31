// Transform Effects Shader
// Week 6: Geometric transformations and cropping

cbuffer TransformConstants : register(b0) {
    float4x4 transform_matrix;  // Combined scale, rotation, translation
    float2 anchor_point;        // Rotation center (0-1 range)
    float4 crop_rect;          // x, y, width, height (0-1 range)
    float2 texture_size;       // Input texture dimensions
    float2 output_size;        // Output render target dimensions
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

PSInput VS_Transform(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

// Apply 2D transformation matrix to UV coordinates
float2 apply_transform(float2 uv, float4x4 matrix, float2 anchor) {
    // Convert UV to centered coordinates (-0.5 to 0.5)
    float2 centered_uv = uv - 0.5f;
    
    // Adjust for anchor point
    centered_uv = centered_uv - (anchor - 0.5f);
    
    // Apply transformation matrix
    float4 transformed = mul(float4(centered_uv, 0.0f, 1.0f), matrix);
    
    // Convert back to UV space (0 to 1)
    return transformed.xy + 0.5f;
}

// Check if UV coordinates are within crop rectangle
bool is_within_crop(float2 uv, float4 crop) {
    return uv.x >= crop.x && 
           uv.x <= (crop.x + crop.z) && 
           uv.y >= crop.y && 
           uv.y <= (crop.y + crop.w);
}

// Bilinear filtering for smooth scaling
float4 sample_bilinear(Texture2D tex, SamplerState samp, float2 uv) {
    return tex.Sample(samp, uv);
}

float4 PS_Transform(PSInput input) : SV_TARGET {
    float2 uv = input.texcoord;
    
    // Apply geometric transformation
    float2 transformed_uv = apply_transform(uv, transform_matrix, anchor_point);
    
    // Check if the transformed UV is within bounds
    if (transformed_uv.x < 0.0f || transformed_uv.x > 1.0f || 
        transformed_uv.y < 0.0f || transformed_uv.y > 1.0f) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent outside bounds
    }
    
    // Apply cropping
    if (!is_within_crop(transformed_uv, crop_rect)) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent outside crop
    }
    
    // Sample the input texture
    return sample_bilinear(inputTexture, linearSampler, transformed_uv);
}

// Utility functions for creating transformation matrices (used from C++)
/*
float4x4 create_scale_matrix(float2 scale) {
    return float4x4(
        scale.x, 0.0f,    0.0f, 0.0f,
        0.0f,    scale.y, 0.0f, 0.0f,
        0.0f,    0.0f,    1.0f, 0.0f,
        0.0f,    0.0f,    0.0f, 1.0f
    );
}

float4x4 create_rotation_matrix(float angle_radians) {
    float cos_a = cos(angle_radians);
    float sin_a = sin(angle_radians);
    return float4x4(
        cos_a,  -sin_a, 0.0f, 0.0f,
        sin_a,   cos_a, 0.0f, 0.0f,
        0.0f,    0.0f,  1.0f, 0.0f,
        0.0f,    0.0f,  0.0f, 1.0f
    );
}

float4x4 create_translation_matrix(float2 translation) {
    return float4x4(
        1.0f, 0.0f, 0.0f, translation.x,
        0.0f, 1.0f, 0.0f, translation.y,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}
*/
