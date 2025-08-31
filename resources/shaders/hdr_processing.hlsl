// Week 9: HDR Processing Shaders
// Comprehensive HLSL shaders for HDR and wide color gamut processing

// =============================================================================
// HDR Transfer Functions
// =============================================================================

// PQ (SMPTE ST 2084) transfer function
float3 linear_to_pq(float3 linear_color, float max_luminance)
{
    // Normalize to 0-1 range based on max luminance
    float3 normalized = linear_color / max_luminance;
    
    // Apply PQ curve
    float3 powed = pow(abs(normalized), 0.1593017578125); // m1
    float3 numerator = 0.8359375 + 18.8515625 * powed;   // c1 + c2 * powed
    float3 denominator = 1.0 + 18.6875 * powed;          // 1 + c3 * powed
    
    return pow(abs(numerator / denominator), 78.84375);   // m2
}

float3 pq_to_linear(float3 pq_color, float max_luminance)
{
    // Inverse PQ curve
    float3 powed = pow(abs(pq_color), 1.0 / 78.84375);   // 1/m2
    float3 numerator = max(powed - 0.8359375, 0.0);      // max(powed - c1, 0)
    float3 denominator = 18.8515625 - 18.6875 * powed;   // c2 - c3 * powed
    
    float3 linear_norm = pow(abs(numerator / denominator), 1.0 / 0.1593017578125); // 1/m1
    return linear_norm * max_luminance;
}

// HLG (Hybrid Log-Gamma) transfer function
float3 linear_to_hlg(float3 linear_color, float system_gamma)
{
    float3 result;
    float a = 0.17883277;
    float b = 0.28466892;
    float c = 0.55991073;
    
    for (int i = 0; i < 3; i++)
    {
        float E = linear_color[i];
        
        if (E <= 1.0 / 12.0)
        {
            result[i] = sqrt(3.0 * E);
        }
        else
        {
            result[i] = a * log(12.0 * E - b) + c;
        }
    }
    
    // Apply system gamma
    return pow(abs(result), 1.0 / system_gamma);
}

float3 hlg_to_linear(float3 hlg_color, float system_gamma)
{
    // Remove system gamma
    float3 E_prime = pow(abs(hlg_color), system_gamma);
    
    float3 result;
    float a = 0.17883277;
    float b = 0.28466892;
    float c = 0.55991073;
    
    for (int i = 0; i < 3; i++)
    {
        if (E_prime[i] <= 0.5)
        {
            result[i] = (E_prime[i] * E_prime[i]) / 3.0;
        }
        else
        {
            result[i] = (exp((E_prime[i] - c) / a) + b) / 12.0;
        }
    }
    
    return result;
}

// Log curves (simplified ARRI LogC)
float3 linear_to_log_c(float3 linear_color)
{
    float a = 5.555556;
    float b = 0.047996;
    float c = 0.244161;
    float d = 0.386036;
    float cut = 0.010591;
    
    float3 result;
    for (int i = 0; i < 3; i++)
    {
        if (linear_color[i] > cut)
        {
            result[i] = c * log10(a * linear_color[i] + b) + d;
        }
        else
        {
            result[i] = 5.367655 * linear_color[i] + 0.092809;
        }
    }
    
    return result;
}

float3 log_c_to_linear(float3 log_color)
{
    float a = 5.555556;
    float b = 0.047996;
    float c = 0.244161;
    float d = 0.386036;
    float cut = 0.010591;
    float e = 5.367655;
    float f = 0.092809;
    
    float3 result;
    for (int i = 0; i < 3; i++)
    {
        if (log_color[i] > e * cut + f)
        {
            result[i] = (pow(10.0, (log_color[i] - d) / c) - b) / a;
        }
        else
        {
            result[i] = (log_color[i] - f) / e;
        }
    }
    
    return result;
}

// =============================================================================
// Color Space Conversion Matrices
// =============================================================================

// BT.709 to BT.2020 conversion matrix
static const float3x3 BT709_TO_BT2020 = {
    0.6274, 0.3293, 0.0433,
    0.0691, 0.9195, 0.0114,
    0.0164, 0.0880, 0.8956
};

// BT.2020 to BT.709 conversion matrix
static const float3x3 BT2020_TO_BT709 = {
    1.7166511, -0.3556708, -0.2533663,
    -0.6666844, 1.6164812, 0.0157685,
    0.0176399, -0.0427706, 0.9421031
};

// BT.709 to DCI-P3 conversion matrix
static const float3x3 BT709_TO_DCI_P3 = {
    0.8224621, 0.1775380, 0.0000000,
    0.0331941, 0.9668058, 0.0000000,
    0.0170827, 0.0723974, 0.9105199
};

// DCI-P3 to BT.709 conversion matrix
static const float3x3 DCI_P3_TO_BT709 = {
    1.2249401, -0.2249404, 0.0000000,
    -0.0420569, 1.0420571, 0.0000000,
    -0.0196376, -0.0786361, 1.0982735
};

// BT.2020 to DCI-P3 conversion matrix
static const float3x3 BT2020_TO_DCI_P3 = {
    1.3459433, -0.2556075, -0.0511118,
    -0.5445989, 1.5081673, 0.0202050,
    0.0000000, -0.0118732, 1.0118732
};

// ACES CG to BT.709 conversion matrix
static const float3x3 ACES_CG_TO_BT709 = {
    1.7050515, -0.6217923, -0.0832593,
    -0.1302597, 1.1408027, -0.0105430,
    -0.0240003, -0.1289687, 1.1529691
};

// Bradford chromatic adaptation matrix
static const float3x3 BRADFORD_MATRIX = {
    0.8951000, 0.2664000, -0.1614000,
    -0.7502000, 1.7135000, 0.0367000,
    0.0389000, -0.0685000, 1.0296000
};

static const float3x3 BRADFORD_INVERSE = {
    0.9869929, -0.1470543, 0.1599627,
    0.4323053, 0.5183603, 0.0492912,
    -0.0085287, 0.0400428, 0.9684867
};

// Apply color space conversion
float3 convert_color_space(float3 color, float3x3 conversion_matrix)
{
    return mul(conversion_matrix, color);
}

// =============================================================================
// Tone Mapping Operators
// =============================================================================

// Reinhard tone mapping
float3 tone_map_reinhard(float3 hdr_color, float white_point)
{
    return hdr_color * (1.0 + hdr_color / (white_point * white_point)) / (1.0 + hdr_color);
}

// Extended Reinhard tone mapping
float3 tone_map_reinhard_extended(float3 hdr_color, float white_point, float burn)
{
    float3 numerator = hdr_color * (1.0 + hdr_color / (white_point * white_point));
    float3 denominator = 1.0 + hdr_color;
    return pow(numerator / denominator, burn);
}

// John Hable (Uncharted 2) tone mapping
float3 hable_partial(float3 x)
{
    float A = 0.15; // Shoulder strength
    float B = 0.50; // Linear strength
    float C = 0.10; // Linear angle
    float D = 0.20; // Toe strength
    float E = 0.02; // Toe numerator
    float F = 0.30; // Toe denominator
    
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 tone_map_hable(float3 hdr_color, float exposure_bias)
{
    float3 curr = hable_partial(hdr_color * exposure_bias);
    float3 white_scale = 1.0 / hable_partial(11.2);
    return curr * white_scale;
}

// ACES tone mapping (approximation)
float3 tone_map_aces(float3 hdr_color)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    
    return saturate((hdr_color * (a * hdr_color + b)) / (hdr_color * (c * hdr_color + d) + e));
}

// ACES Hill tone curve
float3 tone_map_aces_hill(float3 hdr_color)
{
    // RRT (Reference Rendering Transform) approximation
    float3 a = hdr_color * (hdr_color + 0.0245786) - 0.000090537;
    float3 b = hdr_color * (0.983729 * hdr_color + 0.4329510) + 0.238081;
    return a / b;
}

// Filmic tone mapping
float3 tone_map_filmic(float3 hdr_color, float shoulder, float linear_start, float linear_length, float black_tightness)
{
    float3 x = max(0, hdr_color - black_tightness);
    float3 linear_part = linear_start + linear_length * x;
    float3 shoulder_part = pow(x, shoulder);
    
    return lerp(linear_part, shoulder_part, saturate((x - linear_start) / linear_length));
}

// GT (Gran Turismo) tone mapper
float3 tone_map_gt(float3 hdr_color, float P, float a, float m, float l, float c, float b)
{
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;
    
    float3 result;
    for (int i = 0; i < 3; i++)
    {
        float x = hdr_color[i];
        
        if (x < m)
        {
            result[i] = L0 + (L1 - L0) * (x / m);
        }
        else if (x < S0)
        {
            result[i] = m + a * (x - m);
        }
        else if (x < S1)
        {
            result[i] = m + a * l0 + (x - S0) / (S1 - S0) * (1.0 - m - a * l0);
        }
        else
        {
            result[i] = 1.0 + CP * (x - P);
        }
    }
    
    return result;
}

// =============================================================================
// Gamut Mapping Functions
// =============================================================================

// Convert RGB to LCH (Lightness, Chroma, Hue)
float3 rgb_to_lch(float3 rgb)
{
    // Simplified RGB to LCH conversion
    // In practice, this should go through LAB color space
    float L = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
    float a = 0.5 * (rgb.r - rgb.g);
    float b = 0.5 * (rgb.r + rgb.g - 2.0 * rgb.b);
    
    float C = sqrt(a * a + b * b);
    float H = atan2(b, a);
    
    return float3(L, C, H);
}

// Convert LCH back to RGB
float3 lch_to_rgb(float3 lch)
{
    float L = lch.x;
    float C = lch.y;
    float H = lch.z;
    
    float a = C * cos(H);
    float b = C * sin(H);
    
    // Simplified LCH to RGB conversion
    float r = L + 0.5 * a;
    float g = L - 0.5 * a;
    float bl = L - 0.5 * b;
    
    return float3(r, g, bl);
}

// Cusp compression gamut mapping
float3 gamut_map_cusp_compress(float3 rgb, float compression_factor)
{
    float3 lch = rgb_to_lch(rgb);
    
    float L = lch.x;
    float C = lch.y;
    float H = lch.z;
    
    // Find maximum chroma for this lightness and hue
    float max_chroma = 1.0; // Simplified - should be calculated based on gamut boundary
    
    // Apply compression
    if (C > max_chroma)
    {
        float compression = 1.0 - (1.0 - max_chroma / C) * compression_factor;
        C *= compression;
    }
    
    return lch_to_rgb(float3(L, C, H));
}

// Perceptual gamut mapping
float3 gamut_map_perceptual(float3 rgb, float saturation_preservation)
{
    // Preserve luminance and compress chromaticity
    float luminance = dot(rgb, float3(0.299, 0.587, 0.114));
    float3 chromaticity = rgb - luminance;
    
    float chroma_magnitude = length(chromaticity);
    float max_chroma = 1.0 - luminance; // Simplified gamut boundary
    
    if (chroma_magnitude > max_chroma)
    {
        float compression = lerp(max_chroma / chroma_magnitude, 1.0, saturation_preservation);
        chromaticity *= compression;
    }
    
    return luminance + chromaticity;
}

// Smooth gamut compression
float3 gamut_map_smooth_compress(float3 rgb, float threshold, float compression)
{
    float max_component = max(max(rgb.r, rgb.g), rgb.b);
    
    if (max_component > threshold)
    {
        float excess = max_component - threshold;
        float compressed_excess = excess * compression;
        float scale = (threshold + compressed_excess) / max_component;
        rgb *= scale;
    }
    
    return rgb;
}

// =============================================================================
// HDR Analysis Functions
// =============================================================================

// Calculate luminance
float calculate_luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126, 0.7152, 0.0722)); // BT.709 luminance weights
}

// Calculate BT.2020 luminance
float calculate_luminance_bt2020(float3 rgb)
{
    return dot(rgb, float3(0.2627, 0.6780, 0.0593)); // BT.2020 luminance weights
}

// Check if color is within BT.709 gamut
bool is_within_bt709_gamut(float3 rgb)
{
    return all(rgb >= 0.0) && all(rgb <= 1.0);
}

// Check if color is within DCI-P3 gamut (approximation)
bool is_within_dci_p3_gamut(float3 rgb)
{
    // Transform to DCI-P3 and check bounds
    float3 p3_color = mul(BT709_TO_DCI_P3, rgb);
    return all(p3_color >= 0.0) && all(p3_color <= 1.0);
}

// Calculate color temperature from white point
float calculate_color_temperature(float2 white_point_xy)
{
    float x = white_point_xy.x;
    float y = white_point_xy.y;
    
    // McCamy's approximation
    float n = (x - 0.3320) / (0.1858 - y);
    return 449.0 * n * n * n + 3525.0 * n * n + 6823.3 * n + 5520.33;
}

// =============================================================================
// Utility Functions
// =============================================================================

// Safe power function that handles negative values
float safe_pow(float base, float exponent)
{
    return pow(abs(base), exponent) * sign(base);
}

// Smooth step function
float smooth_step(float edge0, float edge1, float x)
{
    float t = saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

// Convert linear to sRGB gamma
float linear_to_srgb(float linear)
{
    if (linear <= 0.0031308)
    {
        return 12.92 * linear;
    }
    else
    {
        return 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
    }
}

// Convert sRGB gamma to linear
float srgb_to_linear(float srgb)
{
    if (srgb <= 0.04045)
    {
        return srgb / 12.92;
    }
    else
    {
        return pow((srgb + 0.055) / 1.055, 2.4);
    }
}

// Apply gamma curve
float3 apply_gamma(float3 color, float gamma)
{
    return pow(abs(color), 1.0 / gamma) * sign(color);
}

// Remove gamma curve
float3 remove_gamma(float3 color, float gamma)
{
    return pow(abs(color), gamma) * sign(color);
}
