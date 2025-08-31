// Week 9: HDR Processing Compute Shaders
// GPU-accelerated HDR and wide color gamut processing compute shaders

// =============================================================================
// Common Structures and Constants
// =============================================================================

struct HDRProcessingParams
{
    uint input_transfer_function;      // HDRTransferFunction enum
    uint output_transfer_function;     // HDRTransferFunction enum
    uint input_color_space;           // ColorSpace enum
    uint output_color_space;          // ColorSpace enum
    
    uint tone_mapping_operator;       // ToneMappingOperator enum
    float white_point;                // Tone mapping white point
    float exposure_bias;              // Exposure adjustment
    float shoulder_strength;          // Filmic tone mapping parameter
    
    float linear_start;               // Filmic tone mapping parameter
    float linear_length;              // Filmic tone mapping parameter
    float black_tightness;            // Filmic tone mapping parameter
    float max_luminance;              // Peak luminance (nits)
    
    float2 source_primaries[3];       // Source color space primaries
    float2 target_primaries[3];       // Target color space primaries
    float2 source_white_point;        // Source white point
    float2 target_white_point;        // Target white point
};

struct GamutMappingParams
{
    uint mapping_method;              // GamutMapping enum
    float compression_factor;         // Compression strength
    float saturation_preservation;    // Perceptual mapping parameter
    uint target_gamut;               // Target gamut space
    
    float3x3 conversion_matrix;       // Pre-calculated conversion matrix
    float gamut_boundary[256];        // Gamut boundary LUT
    uint boundary_size;               // Size of boundary LUT
    uint padding;                     // Alignment padding
};

// Input/Output textures
Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

// Constant buffers
cbuffer HDRParams : register(b0)
{
    HDRProcessingParams hdr_params;
};

cbuffer GamutParams : register(b1)
{
    GamutMappingParams gamut_params;
};

// Structured buffers for LUTs
StructuredBuffer<float3> ColorSpaceLUT : register(t1);
StructuredBuffer<float> ToneMappingLUT : register(t2);

// Samplers
SamplerState LinearSampler : register(s0);

// =============================================================================
// Transfer Function Implementations
// =============================================================================

float apply_pq_oetf(float linear_value, float max_luminance)
{
    const float m1 = 0.1593017578125;    // 2610.0 / 16384.0
    const float m2 = 78.84375;           // 2523.0 / 32.0
    const float c1 = 0.8359375;          // 3424.0 / 4096.0
    const float c2 = 18.8515625;         // 2413.0 / 128.0
    const float c3 = 18.6875;            // 2392.0 / 128.0
    
    float normalized = saturate(linear_value / max_luminance);
    float powed = pow(normalized, m1);
    float numerator = c1 + c2 * powed;
    float denominator = 1.0 + c3 * powed;
    
    return pow(numerator / denominator, m2);
}

float apply_pq_eotf(float pq_value, float max_luminance)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    
    float powed = pow(saturate(pq_value), 1.0 / m2);
    float numerator = max(powed - c1, 0.0);
    float denominator = c2 - c3 * powed;
    
    if (denominator <= 0.0) return 0.0;
    
    float linear_norm = pow(numerator / denominator, 1.0 / m1);
    return linear_norm * max_luminance;
}

float apply_hlg_oetf(float linear_value)
{
    const float a = 0.17883277;
    const float b = 0.28466892;
    const float c = 0.55991073;
    
    float E = saturate(linear_value);
    
    if (E <= 1.0 / 12.0)
    {
        return sqrt(3.0 * E);
    }
    else
    {
        return a * log(12.0 * E - b) + c;
    }
}

float apply_hlg_eotf(float hlg_value)
{
    const float a = 0.17883277;
    const float b = 0.28466892;
    const float c = 0.55991073;
    
    float E_prime = saturate(hlg_value);
    
    if (E_prime <= 0.5)
    {
        return (E_prime * E_prime) / 3.0;
    }
    else
    {
        return (exp((E_prime - c) / a) + b) / 12.0;
    }
}

float3 apply_transfer_function(float3 linear_color, uint function_type, float max_luminance)
{
    switch (function_type)
    {
        case 0: // LINEAR
            return linear_color;
            
        case 1: // GAMMA_2_2
            return pow(max(linear_color, 0.0), 1.0 / 2.2);
            
        case 2: // PQ_2084
            return float3(
                apply_pq_oetf(linear_color.r, max_luminance),
                apply_pq_oetf(linear_color.g, max_luminance),
                apply_pq_oetf(linear_color.b, max_luminance)
            );
            
        case 3: // HLG_ARIB
            return float3(
                apply_hlg_oetf(linear_color.r),
                apply_hlg_oetf(linear_color.g),
                apply_hlg_oetf(linear_color.b)
            );
            
        case 4: // SRGB
            return linear_color <= 0.0031308 ? 
                   12.92 * linear_color : 
                   1.055 * pow(linear_color, 1.0 / 2.4) - 0.055;
            
        default:
            return linear_color;
    }
}

float3 apply_transfer_function_inverse(float3 encoded_color, uint function_type, float max_luminance)
{
    switch (function_type)
    {
        case 0: // LINEAR
            return encoded_color;
            
        case 1: // GAMMA_2_2
            return pow(max(encoded_color, 0.0), 2.2);
            
        case 2: // PQ_2084
            return float3(
                apply_pq_eotf(encoded_color.r, max_luminance),
                apply_pq_eotf(encoded_color.g, max_luminance),
                apply_pq_eotf(encoded_color.b, max_luminance)
            );
            
        case 3: // HLG_ARIB
            return float3(
                apply_hlg_eotf(encoded_color.r),
                apply_hlg_eotf(encoded_color.g),
                apply_hlg_eotf(encoded_color.b)
            );
            
        case 4: // SRGB
            return encoded_color <= 0.04045 ? 
                   encoded_color / 12.92 : 
                   pow((encoded_color + 0.055) / 1.055, 2.4);
            
        default:
            return encoded_color;
    }
}

// =============================================================================
// Tone Mapping Operators
// =============================================================================

float3 tone_map_reinhard(float3 hdr_color, float white_point)
{
    return hdr_color * (1.0 + hdr_color / (white_point * white_point)) / (1.0 + hdr_color);
}

float3 tone_map_hable(float3 hdr_color, float exposure_bias)
{
    const float A = 0.15; // Shoulder strength
    const float B = 0.50; // Linear strength
    const float C = 0.10; // Linear angle
    const float D = 0.20; // Toe strength
    const float E = 0.02; // Toe numerator
    const float F = 0.30; // Toe denominator
    
    float3 hable_partial = ((hdr_color * (A * hdr_color + C * B) + D * E) / 
                           (hdr_color * (A * hdr_color + B) + D * F)) - E / F;
    
    float3 curr = hable_partial * exposure_bias;
    float3 white_scale = 1.0 / (((11.2 * (A * 11.2 + C * B) + D * E) / 
                                (11.2 * (A * 11.2 + B) + D * F)) - E / F);
    
    return curr * white_scale;
}

float3 tone_map_aces(float3 hdr_color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    
    return saturate((hdr_color * (a * hdr_color + b)) / (hdr_color * (c * hdr_color + d) + e));
}

float3 tone_map_filmic(float3 hdr_color, float shoulder, float linear_start, 
                      float linear_length, float black_tightness)
{
    float3 x = max(0.0, hdr_color - black_tightness);
    float3 linear_part = linear_start + linear_length * x;
    float3 shoulder_part = pow(x, shoulder);
    
    float3 t = saturate((x - linear_start) / linear_length);
    return lerp(linear_part, shoulder_part, t);
}

float3 apply_tone_mapping(float3 hdr_color, uint operator_type, HDRProcessingParams params)
{
    switch (operator_type)
    {
        case 0: // NONE
            return hdr_color;
            
        case 1: // REINHARD
            return tone_map_reinhard(hdr_color, params.white_point);
            
        case 2: // HABLE
            return tone_map_hable(hdr_color, params.exposure_bias);
            
        case 3: // ACES
            return tone_map_aces(hdr_color);
            
        case 4: // FILMIC
            return tone_map_filmic(hdr_color, params.shoulder_strength, 
                                 params.linear_start, params.linear_length, 
                                 params.black_tightness);
            
        default:
            return hdr_color;
    }
}

// =============================================================================
// Color Space Conversion
// =============================================================================

float3 apply_color_matrix(float3 color, float3x3 matrix)
{
    return float3(
        dot(matrix[0], color),
        dot(matrix[1], color),
        dot(matrix[2], color)
    );
}

float3 convert_color_space(float3 color, uint from_space, uint to_space)
{
    // Predefined color space conversion matrices
    static const float3x3 bt709_to_bt2020 = {
        0.6274, 0.3293, 0.0433,
        0.0691, 0.9195, 0.0114,
        0.0164, 0.0880, 0.8956
    };
    
    static const float3x3 bt2020_to_bt709 = {
        1.7166511, -0.3556708, -0.2533663,
        -0.6666844, 1.6164812, 0.0157685,
        0.0176399, -0.0427706, 0.9421031
    };
    
    static const float3x3 bt709_to_dci_p3 = {
        0.8224621, 0.1775380, 0.0000000,
        0.0331941, 0.9668058, 0.0000000,
        0.0170827, 0.0723974, 0.9105199
    };
    
    static const float3x3 dci_p3_to_bt709 = {
        1.2249401, -0.2249404, 0.0000000,
        -0.0420569, 1.0420571, 0.0000000,
        -0.0196376, -0.0786361, 1.0982735
    };
    
    // Apply appropriate conversion matrix
    if (from_space == 0 && to_space == 1) // BT.709 to BT.2020
        return apply_color_matrix(color, bt709_to_bt2020);
    else if (from_space == 1 && to_space == 0) // BT.2020 to BT.709
        return apply_color_matrix(color, bt2020_to_bt709);
    else if (from_space == 0 && to_space == 2) // BT.709 to DCI-P3
        return apply_color_matrix(color, bt709_to_dci_p3);
    else if (from_space == 2 && to_space == 0) // DCI-P3 to BT.709
        return apply_color_matrix(color, dci_p3_to_bt709);
    else
        return color; // Identity conversion
}

// =============================================================================
// Gamut Mapping Functions
// =============================================================================

float3 apply_gamut_clipping(float3 color)
{
    return saturate(color);
}

float3 apply_gamut_compression(float3 color, float compression_factor)
{
    float max_component = max(max(color.r, color.g), color.b);
    
    if (max_component > 1.0)
    {
        float compression_ratio = 1.0 + (max_component - 1.0) * compression_factor;
        float scale = compression_ratio / max_component;
        color *= scale;
    }
    
    return color;
}

float3 apply_perceptual_gamut_mapping(float3 color, float saturation_preservation)
{
    // Calculate luminance (BT.709 weights)
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    
    // Calculate chromaticity
    float3 chromaticity = color - luminance;
    float chroma_magnitude = length(chromaticity);
    
    // Estimate gamut boundary
    float max_chroma = min(1.0 - luminance, luminance);
    
    if (chroma_magnitude > max_chroma && chroma_magnitude > 0.0)
    {
        float compression = lerp(max_chroma / chroma_magnitude, 1.0, saturation_preservation);
        chromaticity *= compression;
    }
    
    return luminance + chromaticity;
}

float3 apply_gamut_mapping(float3 color, GamutMappingParams params)
{
    switch (params.mapping_method)
    {
        case 0: // CLIP
            return apply_gamut_clipping(color);
            
        case 1: // COMPRESS
            return apply_gamut_compression(color, params.compression_factor);
            
        case 2: // PERCEPTUAL
            return apply_perceptual_gamut_mapping(color, params.saturation_preservation);
            
        case 3: // RELATIVE_COLORIMETRIC
            // Simplified relative colorimetric mapping
            return apply_gamut_clipping(color);
            
        default:
            return color;
    }
}

// =============================================================================
// HDR Analysis Functions
// =============================================================================

float calculate_luminance(float3 color, uint color_space)
{
    switch (color_space)
    {
        case 0: // BT.709
            return dot(color, float3(0.2126, 0.7152, 0.0722));
            
        case 1: // BT.2020
            return dot(color, float3(0.2627, 0.6780, 0.0593));
            
        case 2: // DCI-P3
            return dot(color, float3(0.209, 0.721, 0.070));
            
        default:
            return dot(color, float3(0.2126, 0.7152, 0.0722));
    }
}

bool is_hdr_content(float3 color, float threshold)
{
    float luminance = calculate_luminance(color, 0);
    return luminance > threshold;
}

// =============================================================================
// Main Compute Shaders
// =============================================================================

[numthreads(16, 16, 1)]
void HDRProcessingCS(uint3 id : SV_DispatchThreadID)
{
    uint2 texture_size;
    InputTexture.GetDimensions(texture_size.x, texture_size.y);
    
    if (id.x >= texture_size.x || id.y >= texture_size.y)
        return;
    
    // Sample input pixel
    float4 input_pixel = InputTexture[id.xy];
    float3 color = input_pixel.rgb;
    
    // Apply inverse input transfer function (convert to linear)
    color = apply_transfer_function_inverse(color, hdr_params.input_transfer_function, 
                                          hdr_params.max_luminance);
    
    // Convert input color space to working space
    if (hdr_params.input_color_space != hdr_params.output_color_space)
    {
        color = convert_color_space(color, hdr_params.input_color_space, 
                                  hdr_params.output_color_space);
    }
    
    // Apply tone mapping
    color = apply_tone_mapping(color, hdr_params.tone_mapping_operator, hdr_params);
    
    // Apply output transfer function
    color = apply_transfer_function(color, hdr_params.output_transfer_function, 
                                  hdr_params.max_luminance);
    
    // Write result
    OutputTexture[id.xy] = float4(color, input_pixel.a);
}

[numthreads(16, 16, 1)]
void GamutMappingCS(uint3 id : SV_DispatchThreadID)
{
    uint2 texture_size;
    InputTexture.GetDimensions(texture_size.x, texture_size.y);
    
    if (id.x >= texture_size.x || id.y >= texture_size.y)
        return;
    
    // Sample input pixel
    float4 input_pixel = InputTexture[id.xy];
    float3 color = input_pixel.rgb;
    
    // Apply color space conversion if matrix is provided
    if (any(gamut_params.conversion_matrix[0]) || 
        any(gamut_params.conversion_matrix[1]) || 
        any(gamut_params.conversion_matrix[2]))
    {
        color = apply_color_matrix(color, gamut_params.conversion_matrix);
    }
    
    // Apply gamut mapping
    color = apply_gamut_mapping(color, gamut_params);
    
    // Write result
    OutputTexture[id.xy] = float4(color, input_pixel.a);
}

[numthreads(16, 16, 1)]
void ColorSpaceConversionCS(uint3 id : SV_DispatchThreadID)
{
    uint2 texture_size;
    InputTexture.GetDimensions(texture_size.x, texture_size.y);
    
    if (id.x >= texture_size.x || id.y >= texture_size.y)
        return;
    
    // Sample input pixel
    float4 input_pixel = InputTexture[id.xy];
    float3 color = input_pixel.rgb;
    
    // Convert color space
    color = convert_color_space(color, hdr_params.input_color_space, 
                              hdr_params.output_color_space);
    
    // Write result
    OutputTexture[id.xy] = float4(color, input_pixel.a);
}

[numthreads(16, 16, 1)]
void HDRAnalysisCS(uint3 id : SV_DispatchThreadID)
{
    uint2 texture_size;
    InputTexture.GetDimensions(texture_size.x, texture_size.y);
    
    if (id.x >= texture_size.x || id.y >= texture_size.y)
        return;
    
    // Sample input pixel
    float4 input_pixel = InputTexture[id.xy];
    float3 color = input_pixel.rgb;
    
    // Calculate luminance
    float luminance = calculate_luminance(color, hdr_params.input_color_space);
    
    // Check if HDR content
    bool is_hdr = is_hdr_content(color, 1.0);
    
    // Calculate saturation
    float max_component = max(max(color.r, color.g), color.b);
    float min_component = min(min(color.r, color.g), color.b);
    float saturation = (max_component > 0.0) ? (max_component - min_component) / max_component : 0.0;
    
    // Write analysis result (luminance, saturation, is_hdr flag, alpha)
    OutputTexture[id.xy] = float4(luminance, saturation, is_hdr ? 1.0 : 0.0, input_pixel.a);
}

// =============================================================================
// LUT Generation Compute Shader
// =============================================================================

[numthreads(8, 8, 8)]
void Generate3DLUTCS(uint3 id : SV_DispatchThreadID)
{
    uint lut_size = 64; // Typical 3D LUT size
    
    if (id.x >= lut_size || id.y >= lut_size || id.z >= lut_size)
        return;
    
    // Convert 3D index to normalized RGB coordinates
    float3 input_color = float3(id) / float(lut_size - 1);
    
    // Apply HDR processing pipeline
    float3 processed_color = input_color;
    
    // Apply transfer function inverse
    processed_color = apply_transfer_function_inverse(processed_color, 
                                                    hdr_params.input_transfer_function, 
                                                    hdr_params.max_luminance);
    
    // Apply color space conversion
    processed_color = convert_color_space(processed_color, 
                                        hdr_params.input_color_space, 
                                        hdr_params.output_color_space);
    
    // Apply tone mapping
    processed_color = apply_tone_mapping(processed_color, 
                                       hdr_params.tone_mapping_operator, 
                                       hdr_params);
    
    // Apply output transfer function
    processed_color = apply_transfer_function(processed_color, 
                                            hdr_params.output_transfer_function, 
                                            hdr_params.max_luminance);
    
    // Store in 3D LUT (flatten 3D index to 1D)
    uint flat_index = id.z * lut_size * lut_size + id.y * lut_size + id.x;
    
    // Write to structured buffer (assuming we have a RWStructuredBuffer for output)
    // This would typically be done through a UAV structured buffer
    // For demonstration, we'll assume the LUT is stored in a format compatible with our system
}
