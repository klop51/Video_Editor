// 3D LUT Application Shader
// Week 6: Professional color grading with Look-Up Tables

cbuffer LUTConstants : register(b0) {
    float lut_strength;         // 0.0 to 1.0, blend between original and LUT
    float lut_size;            // Size of the 3D LUT (typically 32 or 64)
    float2 _padding;
};

Texture2D inputTexture : register(t0);    // Source video frame
Texture3D lutTexture : register(t1);      // 3D LUT texture
SamplerState linearSampler : register(s0);
SamplerState lutSampler : register(s1);

struct VSInput {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PSInput VS_LUT(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

// Apply 3D LUT with trilinear interpolation
float4 apply_3d_lut(float4 input_color) {
    // Clamp input to valid range
    float3 color = saturate(input_color.rgb);
    
    // Scale color to LUT coordinate space
    // LUT textures typically use (0, lut_size-1) range
    float3 lut_coord = color * (lut_size - 1.0f) / lut_size + 0.5f / lut_size;
    
    // Sample the 3D LUT with trilinear interpolation
    float3 lut_color = lutTexture.Sample(lutSampler, lut_coord).rgb;
    
    return float4(lut_color, input_color.a);
}

// Apply 3D LUT with manual interpolation for precision
float4 apply_3d_lut_precise(float4 input_color) {
    float3 color = saturate(input_color.rgb);
    
    // Calculate LUT coordinates
    float3 scaled = color * (lut_size - 1.0f);
    float3 base_coord = floor(scaled);
    float3 fract_coord = scaled - base_coord;
    
    // Convert to texture coordinates
    float3 uv0 = (base_coord + 0.5f) / lut_size;
    float3 uv1 = (base_coord + 1.5f) / lut_size;
    
    // Sample 8 corners of the cube for trilinear interpolation
    float3 c000 = lutTexture.Sample(lutSampler, float3(uv0.x, uv0.y, uv0.z)).rgb;
    float3 c001 = lutTexture.Sample(lutSampler, float3(uv0.x, uv0.y, uv1.z)).rgb;
    float3 c010 = lutTexture.Sample(lutSampler, float3(uv0.x, uv1.y, uv0.z)).rgb;
    float3 c011 = lutTexture.Sample(lutSampler, float3(uv0.x, uv1.y, uv1.z)).rgb;
    float3 c100 = lutTexture.Sample(lutSampler, float3(uv1.x, uv0.y, uv0.z)).rgb;
    float3 c101 = lutTexture.Sample(lutSampler, float3(uv1.x, uv0.y, uv1.z)).rgb;
    float3 c110 = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv0.z)).rgb;
    float3 c111 = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
    
    // Trilinear interpolation
    float3 c00 = lerp(c000, c001, fract_coord.z);
    float3 c01 = lerp(c010, c011, fract_coord.z);
    float3 c10 = lerp(c100, c101, fract_coord.z);
    float3 c11 = lerp(c110, c111, fract_coord.z);
    
    float3 c0 = lerp(c00, c01, fract_coord.y);
    float3 c1 = lerp(c10, c11, fract_coord.y);
    
    float3 result = lerp(c0, c1, fract_coord.x);
    
    return float4(result, input_color.a);
}

// Apply LUT with strength blending
float4 apply_lut_with_strength(float4 input_color) {
    float4 lut_result = apply_3d_lut(input_color);
    return lerp(input_color, lut_result, lut_strength);
}

// Tetrahedral interpolation (more accurate than trilinear)
float4 apply_3d_lut_tetrahedral(float4 input_color) {
    float3 color = saturate(input_color.rgb);
    float3 scaled = color * (lut_size - 1.0f);
    float3 base_coord = floor(scaled);
    float3 fract_coord = scaled - base_coord;
    
    float3 uv0 = (base_coord + 0.5f) / lut_size;
    float3 uv1 = (base_coord + 1.5f) / lut_size;
    
    // Sample corner points
    float3 c000 = lutTexture.Sample(lutSampler, float3(uv0.x, uv0.y, uv0.z)).rgb;
    float3 c111 = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
    
    float3 result;
    
    // Determine which tetrahedron we're in based on fractional coordinates
    if (fract_coord.x >= fract_coord.y) {
        if (fract_coord.y >= fract_coord.z) {
            // Tetrahedron 1: x >= y >= z
            float3 c001 = lutTexture.Sample(lutSampler, float3(uv0.x, uv0.y, uv1.z)).rgb;
            float3 c011 = lutTexture.Sample(lutSampler, float3(uv0.x, uv1.y, uv1.z)).rgb;
            float3 c111_sample = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
            
            result = c000 + 
                     fract_coord.z * (c001 - c000) +
                     fract_coord.y * (c011 - c001) +
                     fract_coord.x * (c111_sample - c011);
        } else if (fract_coord.x >= fract_coord.z) {
            // Tetrahedron 2: x >= z >= y
            float3 c001 = lutTexture.Sample(lutSampler, float3(uv0.x, uv0.y, uv1.z)).rgb;
            float3 c101 = lutTexture.Sample(lutSampler, float3(uv1.x, uv0.y, uv1.z)).rgb;
            float3 c111_sample = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
            
            result = c000 + 
                     fract_coord.y * (c001 - c000) +
                     fract_coord.z * (c101 - c001) +
                     fract_coord.x * (c111_sample - c101);
        } else {
            // Tetrahedron 3: z >= x >= y
            float3 c100 = lutTexture.Sample(lutSampler, float3(uv1.x, uv0.y, uv0.z)).rgb;
            float3 c101 = lutTexture.Sample(lutSampler, float3(uv1.x, uv0.y, uv1.z)).rgb;
            float3 c111_sample = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
            
            result = c000 + 
                     fract_coord.y * (c100 - c000) +
                     fract_coord.x * (c101 - c100) +
                     fract_coord.z * (c111_sample - c101);
        }
    } else {
        if (fract_coord.z >= fract_coord.x) {
            // Tetrahedron 4: z >= y >= x
            float3 c010 = lutTexture.Sample(lutSampler, float3(uv0.x, uv1.y, uv0.z)).rgb;
            float3 c011 = lutTexture.Sample(lutSampler, float3(uv0.x, uv1.y, uv1.z)).rgb;
            float3 c111_sample = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
            
            result = c000 + 
                     fract_coord.x * (c010 - c000) +
                     fract_coord.y * (c011 - c010) +
                     fract_coord.z * (c111_sample - c011);
        } else if (fract_coord.z >= fract_coord.y) {
            // Tetrahedron 5: y >= z >= x
            float3 c010 = lutTexture.Sample(lutSampler, float3(uv0.x, uv1.y, uv0.z)).rgb;
            float3 c110 = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv0.z)).rgb;
            float3 c111_sample = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
            
            result = c000 + 
                     fract_coord.x * (c010 - c000) +
                     fract_coord.z * (c110 - c010) +
                     fract_coord.y * (c111_sample - c110);
        } else {
            // Tetrahedron 6: y >= x >= z
            float3 c100 = lutTexture.Sample(lutSampler, float3(uv1.x, uv0.y, uv0.z)).rgb;
            float3 c110 = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv0.z)).rgb;
            float3 c111_sample = lutTexture.Sample(lutSampler, float3(uv1.x, uv1.y, uv1.z)).rgb;
            
            result = c000 + 
                     fract_coord.z * (c100 - c000) +
                     fract_coord.x * (c110 - c100) +
                     fract_coord.y * (c111_sample - c110);
        }
    }
    
    return float4(result, input_color.a);
}

// Main pixel shader - standard 3D LUT application
float4 PS_Apply3DLUT(PSInput input) : SV_TARGET {
    float4 input_color = inputTexture.Sample(linearSampler, input.texcoord);
    return apply_lut_with_strength(input_color);
}

// High-precision LUT application
float4 PS_Apply3DLUTPrecise(PSInput input) : SV_TARGET {
    float4 input_color = inputTexture.Sample(linearSampler, input.texcoord);
    float4 lut_result = apply_3d_lut_precise(input_color);
    return lerp(input_color, lut_result, lut_strength);
}

// Tetrahedral interpolation for highest quality
float4 PS_Apply3DLUTTetrahedral(PSInput input) : SV_TARGET {
    float4 input_color = inputTexture.Sample(linearSampler, input.texcoord);
    float4 lut_result = apply_3d_lut_tetrahedral(input_color);
    return lerp(input_color, lut_result, lut_strength);
}
