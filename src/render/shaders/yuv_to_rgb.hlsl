// YUV to RGB Conversion Shaders
// Supports YUV420P, YUV422P, YUV444P formats with BT.709 color space

cbuffer ColorSpaceConstants : register(b0)
{
    float4x4 yuv_to_rgb_matrix;
    float4 black_level;      // [y_black, u_black, v_black, unused]
    float4 white_level;      // [y_white, u_white, v_white, unused]
    float2 chroma_scale;     // UV plane scaling factors
    float2 luma_scale;       // Y plane scaling factors
};

// YUV texture planes
Texture2D Y_texture : register(t0);
Texture2D U_texture : register(t1);
Texture2D V_texture : register(t2);

// Combined NV12/NV21 textures (alternative input)
Texture2D YUV_luma : register(t3);
Texture2D YUV_chroma : register(t4);

SamplerState linear_sampler : register(s0);
SamplerState point_sampler : register(s1);

struct VS_INPUT
{
    float2 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// Vertex Shader - unchanged from previous implementations
PS_INPUT VS_YUVtoRGB(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}

// YUV420P/422P/444P Pixel Shader (planar formats)
float4 PS_YUVtoRGB_Planar(PS_INPUT input) : SV_Target
{
    float2 uv = input.texcoord;
    
    // Sample YUV planes with appropriate scaling
    float y = Y_texture.Sample(linear_sampler, uv * luma_scale).r;
    float u = U_texture.Sample(linear_sampler, uv * chroma_scale).r;
    float v = V_texture.Sample(linear_sampler, uv * chroma_scale).r;
    
    // Normalize to [0,1] range accounting for black/white levels
    y = (y - black_level.x) / (white_level.x - black_level.x);
    u = (u - black_level.y) / (white_level.y - black_level.y) - 0.5f;
    v = (v - black_level.z) / (white_level.z - black_level.z) - 0.5f;
    
    // Apply YUV to RGB conversion matrix
    float3 yuv = float3(y, u, v);
    float3 rgb = mul(yuv_to_rgb_matrix, float4(yuv, 1.0f)).rgb;
    
    // Clamp to valid range
    rgb = saturate(rgb);
    
    return float4(rgb, 1.0f);
}

// NV12/NV21 Pixel Shader (semi-planar formats)
float4 PS_YUVtoRGB_NV12(PS_INPUT input) : SV_Target
{
    float2 uv = input.texcoord;
    
    // Sample luma plane
    float y = YUV_luma.Sample(linear_sampler, uv).r;
    
    // Sample chroma plane (UV interleaved)
    float2 chroma = YUV_chroma.Sample(linear_sampler, uv * chroma_scale).rg;
    float u = chroma.r;
    float v = chroma.g;
    
    // Normalize values
    y = (y - black_level.x) / (white_level.x - black_level.x);
    u = (u - black_level.y) / (white_level.y - black_level.y) - 0.5f;
    v = (v - black_level.z) / (white_level.z - black_level.z) - 0.5f;
    
    // Convert to RGB
    float3 yuv = float3(y, u, v);
    float3 rgb = mul(yuv_to_rgb_matrix, float4(yuv, 1.0f)).rgb;
    
    return float4(saturate(rgb), 1.0f);
}

// BT.709 High Definition color space (most common for modern video)
// Can be overridden via constant buffer for other color spaces
// 
// Default BT.709 matrix:
// R = Y + 1.5748 * V
// G = Y - 0.1873 * U - 0.4681 * V  
// B = Y + 1.8556 * U
//
// Matrix form:
// [R]   [1.0000  0.0000  1.5748] [Y]
// [G] = [1.0000 -0.1873 -0.4681] [U-0.5]
// [B]   [1.0000  1.8556  0.0000] [V-0.5]
