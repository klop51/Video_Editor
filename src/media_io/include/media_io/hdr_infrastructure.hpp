#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>
#include <array>

/**
 * HDR Infrastructure Implementation for Phase 2 Week 5
 * FORMAT_SUPPORT_ROADMAP.md - Critical for modern production
 * 
 * Comprehensive HDR support including HDR10, HDR10+, Dolby Vision, and HLG
 * Essential for professional video workflows and consumer delivery
 */

namespace ve::media_io {

/**
 * HDR Standards Support
 * Comprehensive support for all major HDR formats
 */
enum class HDRStandard {
    NONE,           // Standard Dynamic Range (SDR)
    HDR10,          // BT.2020 + PQ (Perceptual Quantizer)
    HDR10_PLUS,     // HDR10 + Dynamic metadata
    DOLBY_VISION,   // Proprietary Dolby enhancement
    HLG,            // Hybrid Log-Gamma (broadcast standard)
    HDR_VIVID,      // Chinese HDR standard
    SL_HDR1,        // Philips SL-HDR1
    SL_HDR2,        // Philips SL-HDR2
    TECHNICOLOR_HDR // Technicolor HDR
};

/**
 * Transfer Function Implementation
 * Mathematical curves for luminance encoding/decoding
 */
enum class TransferFunction {
    UNKNOWN,
    LINEAR,         // Linear light (for processing)
    SRGB,           // Standard RGB (gamma 2.2)
    BT709,          // Rec. 709 (HD television)
    BT2020,         // Rec. 2020 (UHD)
    PQ,             // Perceptual Quantizer (SMPTE ST 2084) - HDR10
    HLG,            // Hybrid Log-Gamma (ITU-R BT.2100)
    LOG,            // Logarithmic (camera log formats)
    GAMMA22,        // Pure gamma 2.2
    GAMMA28,        // Pure gamma 2.8
    DCI_P3,         // DCI-P3 gamma 2.6
    DISPLAY_P3      // Display P3 gamma 2.2
};

/**
 * Color Primaries for Wide Color Gamut
 * Defines the color space boundaries
 */
enum class ColorPrimaries {
    UNKNOWN,
    BT709,          // Rec. 709 (HD television)
    BT2020,         // Rec. 2020 (UHD/HDR)
    DCI_P3,         // DCI-P3 (cinema)
    DISPLAY_P3,     // Display P3 (Apple/consumer)
    ADOBE_RGB,      // Adobe RGB (1998)
    SRGB,           // sRGB
    PROPHOTO_RGB,   // ProPhoto RGB
    BT601_525,      // Rec. 601 NTSC
    BT601_625,      // Rec. 601 PAL
    BT470_M,        // BT.470 System M
    BT470_BG,       // BT.470 System B/G
    SMPTE240M,      // SMPTE-240M
    GENERIC_FILM,   // Generic film
    BT2020_NCL,     // BT.2020 non-constant luminance
    BT2020_CL       // BT.2020 constant luminance
};

/**
 * HDR Metadata Structure
 * Contains all HDR-related metadata for proper display
 */
struct HDRMetadata {
    // Basic HDR information
    HDRStandard hdr_standard = HDRStandard::NONE;
    TransferFunction transfer_function = TransferFunction::UNKNOWN;
    ColorPrimaries color_primaries = ColorPrimaries::UNKNOWN;
    
    // Mastering display information (for HDR10)
    struct MasteringDisplayInfo {
        // Display primaries (x, y coordinates in CIE 1931)
        std::array<std::array<float, 2>, 3> display_primaries = {}; // R, G, B
        std::array<float, 2> white_point = {0.3127f, 0.3290f}; // D65 default
        
        // Luminance levels (in cd/m²)
        float max_display_mastering_luminance = 1000.0f;  // nits
        float min_display_mastering_luminance = 0.01f;    // nits
    } mastering_display;
    
    // Content light level info (for HDR10)
    struct ContentLightLevelInfo {
        uint16_t max_content_light_level = 0;     // MaxCLL in nits
        uint16_t max_frame_average_light_level = 0; // MaxFALL in nits
    } content_light_level;
    
    // Dynamic metadata (for HDR10+ and Dolby Vision)
    struct DynamicMetadata {
        bool has_dynamic_metadata = false;
        uint32_t frame_number = 0;
        
        // HDR10+ specific
        std::vector<uint8_t> hdr10_plus_data;
        
        // Dolby Vision specific
        std::vector<uint8_t> dolby_vision_rpu;  // Reference Processing Unit
        std::vector<uint8_t> dolby_vision_el;   // Enhancement Layer
    } dynamic_metadata;
    
    // HLG specific parameters
    struct HLGParameters {
        float hlg_ootf_gamma = 1.2f;    // OOTF gamma for HLG
        bool hlg_system_start_code = false;
    } hlg_params;
    
    // Additional metadata
    std::string color_space_name;
    std::string transfer_characteristics_name;
    std::string matrix_coefficients_name;
    bool video_full_range = false;
    
    // Validation flags
    bool is_valid = false;
    std::vector<std::string> validation_warnings;
};

/**
 * HDR Processing Configuration
 * Settings for HDR processing pipeline
 */
struct HDRProcessingConfig {
    // Input configuration
    HDRStandard input_hdr_standard = HDRStandard::NONE;
    TransferFunction input_transfer_function = TransferFunction::UNKNOWN;
    ColorPrimaries input_color_primaries = ColorPrimaries::UNKNOWN;
    
    // Output configuration
    HDRStandard output_hdr_standard = HDRStandard::NONE;
    TransferFunction output_transfer_function = TransferFunction::UNKNOWN;
    ColorPrimaries output_color_primaries = ColorPrimaries::UNKNOWN;
    
    // Processing options
    bool enable_tone_mapping = true;
    bool preserve_dynamic_metadata = true;
    bool enable_gamut_mapping = true;
    
    // Tone mapping parameters
    struct ToneMappingParams {
        float target_peak_luminance = 100.0f;    // nits for SDR display
        float source_peak_luminance = 1000.0f;   // nits from content
        float adaptation_level = 0.4f;           // Adaptation factor
        bool use_reinhard = false;               // Use Reinhard tone mapping
        bool use_aces = true;                    // Use ACES tone mapping
        bool use_hable = false;                  // Use Hable tone mapping
    } tone_mapping;
    
    // Color space conversion
    struct ColorSpaceConversion {
        bool enable_conversion = true;
        float conversion_matrix[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}}; // Identity default
        bool use_chromatic_adaptation = true;
        std::array<float, 2> source_white_point = {0.3127f, 0.3290f};
        std::array<float, 2> target_white_point = {0.3127f, 0.3290f};
    } color_conversion;
    
    // Performance settings
    bool enable_gpu_acceleration = true;
    bool enable_lut_optimization = true;
    uint32_t lut_size = 33;                     // 33x33x33 LUT
};

/**
 * HDR Capability Information
 * Describes HDR capabilities of the system/display
 */
struct HDRCapabilityInfo {
    // Display capabilities
    bool supports_hdr10 = false;
    bool supports_hdr10_plus = false;
    bool supports_dolby_vision = false;
    bool supports_hlg = false;
    
    // Alternative field names for test compatibility
    bool hdr10_supported = false;
    bool hlg_supported = false;
    bool dolby_vision_supported = false;
    bool hardware_tone_mapping_available = false;
    
    // Display characteristics
    float max_luminance = 100.0f;              // nits
    float min_luminance = 0.1f;                // nits
    float max_average_luminance = 80.0f;       // nits
    
    // Color gamut support
    ColorPrimaries native_color_primaries = ColorPrimaries::BT709;
    float color_gamut_coverage_bt2020 = 0.0f;  // Percentage of BT.2020
    float color_gamut_coverage_dci_p3 = 0.0f;  // Percentage of DCI-P3
    
    // Hardware features
    bool has_hardware_hdr_processing = false;
    bool has_tone_mapping_hardware = false;
    bool has_color_management_hardware = false;
    
    // Supported transfer functions
    std::vector<TransferFunction> supported_transfer_functions;
    
    // Performance characteristics
    bool real_time_hdr_processing = false;
    uint32_t max_hdr_resolution_width = 0;
    uint32_t max_hdr_resolution_height = 0;
};

/**
 * HDR Infrastructure Manager
 * Core class for HDR processing and management
 */
class HDRInfrastructure {
public:
    /**
     * Initialize HDR infrastructure
     * @param enable_hardware_acceleration Enable GPU-accelerated HDR processing
     * @return True if initialization successful
     */
    static bool initialize(bool enable_hardware_acceleration = true);
    
    /**
     * Detect HDR metadata from video stream
     * @param stream_data Raw video stream data
     * @param data_size Size of stream data
     * @param codec_hint Codec family hint for parsing
     * @return Detected HDR metadata structure
     */
    static HDRMetadata detect_hdr_metadata(const uint8_t* stream_data, 
                                           size_t data_size,
                                           int codec_hint = 0);
    
    /**
     * Validate HDR metadata consistency
     * @param metadata HDR metadata to validate
     * @return True if metadata is valid and consistent
     */
    static bool validate_hdr_metadata(HDRMetadata& metadata);
    
    /**
     * Get system HDR capabilities
     * @return HDR capability information for current system
     */
    static HDRCapabilityInfo get_system_hdr_capabilities();
    
    /**
     * Create HDR processing configuration
     * @param input_metadata Source HDR metadata
     * @param target_display Target display capabilities
     * @return Optimized processing configuration
     */
    static HDRProcessingConfig create_processing_config(const HDRMetadata& input_metadata,
                                                        const HDRCapabilityInfo& target_display);
    
    /**
     * Convert between HDR standards
     * @param source_metadata Source HDR metadata
     * @param target_standard Target HDR standard
     * @return Converted HDR metadata
     */
    static HDRMetadata convert_hdr_standard(const HDRMetadata& source_metadata,
                                           HDRStandard target_standard);
    
    /**
     * Generate tone mapping LUT
     * @param config HDR processing configuration
     * @return 3D LUT data for tone mapping
     */
    static std::vector<float> generate_tone_mapping_lut(const HDRProcessingConfig& config);
    
    /**
     * Apply HDR processing to frame data
     * @param input_data Source frame data
     * @param input_size Size of input data
     * @param metadata HDR metadata
     * @param config Processing configuration
     * @param output_data Processed frame data
     * @return True if processing successful
     */
    bool process_hdr_frame(const uint8_t* input_data,
                          size_t input_size,
                          const HDRMetadata& metadata,
                          const HDRProcessingConfig& config,
                          std::vector<uint8_t>& output_data);
    
    /**
     * Get HDR standard name
     * @param standard HDR standard enum
     * @return Human-readable name
     */
    static std::string get_hdr_standard_name(HDRStandard standard);
    
    /**
     * Get transfer function name
     * @param transfer_func Transfer function enum
     * @return Human-readable name
     */
    static std::string get_transfer_function_name(TransferFunction transfer_func);
    
    /**
     * Get color primaries name
     * @param primaries Color primaries enum
     * @return Human-readable name
     */
    static std::string get_color_primaries_name(ColorPrimaries primaries);
    
    /**
     * Check if HDR standard requires dynamic metadata
     * @param standard HDR standard to check
     * @return True if dynamic metadata is required
     */
    static bool requires_dynamic_metadata(HDRStandard standard);
    
    /**
     * Calculate color gamut coverage
     * @param source_primaries Source color primaries
     * @param target_primaries Target color primaries
     * @return Coverage percentage (0.0-1.0)
     */
    static float calculate_gamut_coverage(ColorPrimaries source_primaries,
                                         ColorPrimaries target_primaries);
    
    /**
     * Get recommended processing pipeline for HDR workflow
     * @param input_metadata Source HDR metadata
     * @param target_capabilities Target display capabilities
     * @return Processing recommendations
     */
    struct HDRProcessingRecommendations {
        bool tone_mapping_required = false;
        bool gamut_mapping_required = false;
        bool transfer_function_conversion_required = false;
        HDRStandard recommended_output_standard = HDRStandard::NONE;
        std::vector<std::string> workflow_notes;
        float estimated_quality_preservation = 1.0f;
    };
    
    static HDRProcessingRecommendations get_processing_recommendations(
        const HDRMetadata& input_metadata,
        const HDRCapabilityInfo& target_capabilities);

    /**
     * Detect HDR standard from stream data
     * @param stream_data Raw stream data
     * @return Detected HDR standard
     */
    HDRStandard detect_hdr_standard(const std::vector<uint8_t>& stream_data);

    /**
     * Parse HDR metadata from stream data
     * @param stream_data Raw stream data
     * @return Parsed HDR metadata
     */
    HDRMetadata parse_hdr_metadata(const std::vector<uint8_t>& stream_data);

    /**
     * Convert color space between HDR standards
     * @param source_metadata Source HDR metadata
     * @param target_primaries Target color primaries
     * @param target_transfer Target transfer function
     * @return Converted HDR metadata
     */
    HDRMetadata convert_color_space(const HDRMetadata& source_metadata,
                                          ColorPrimaries target_primaries,
                                          TransferFunction target_transfer);
                                          
    /**
     * Convert RGB color values between color spaces
     * @param source_rgb Source RGB values
     * @param source_primaries Source color primaries
     * @param target_primaries Target color primaries
     * @param target_rgb Output RGB values
     * @return Success status
     */
    bool convert_color_space(const std::vector<float>& source_rgb,
                           ColorPrimaries source_primaries,
                           ColorPrimaries target_primaries,
                           std::vector<float>& target_rgb);

private:
    // HDR detection helpers
    static HDRStandard detect_hdr_standard_from_data(const uint8_t* data, size_t size);
    static TransferFunction detect_transfer_function(const uint8_t* data, size_t size);
    static ColorPrimaries detect_color_primaries(const uint8_t* data, size_t size);
    
    // Metadata parsers
    static bool parse_hdr10_metadata(const uint8_t* data, size_t size, HDRMetadata& metadata);
    static bool parse_dolby_vision_metadata(const uint8_t* data, size_t size, HDRMetadata& metadata);
    static bool parse_hlg_metadata(const uint8_t* data, size_t size, HDRMetadata& metadata);
    
    // Color space mathematics
    static void calculate_color_transform_matrix(ColorPrimaries source, ColorPrimaries target, 
                                               float matrix[3][3]);
    static std::array<float, 2> get_white_point_for_primaries(ColorPrimaries primaries);
    static std::array<std::array<float, 2>, 3> get_primaries_coordinates(ColorPrimaries primaries);
    
    // Tone mapping algorithms
    static float apply_reinhard_tone_mapping(float luminance, float max_luminance);
    static float apply_aces_tone_mapping(float luminance);
    static float apply_hable_tone_mapping(float luminance);
    
    // Transfer function implementations
    static float apply_pq_eotf(float signal);
    static float apply_pq_oetf(float luminance);
    static float apply_hlg_eotf(float signal);
    static float apply_hlg_oetf(float luminance);
    
    // System capability detection
    static bool detect_hardware_hdr_support();
    static HDRCapabilityInfo detect_display_capabilities();
    
    // Validation helpers
    static bool validate_luminance_levels(const HDRMetadata::MasteringDisplayInfo& info);
    static bool validate_color_primaries(const HDRMetadata::MasteringDisplayInfo& info);
    static bool validate_dynamic_metadata(const HDRMetadata::DynamicMetadata& metadata);
};

/**
 * HDR Workflow Utilities
 * Helper functions for common HDR workflows
 */
namespace hdr_utils {
    
    /**
     * Create HDR10 metadata from basic parameters
     * @param max_luminance Maximum display luminance
     * @param min_luminance Minimum display luminance
     * @param max_cll Maximum content light level
     * @param max_fall Maximum frame average light level
     * @return HDR10 metadata structure
     */
    HDRMetadata create_hdr10_metadata(float max_luminance, float min_luminance,
                                     uint16_t max_cll, uint16_t max_fall);
    
    /**
     * Convert SDR to HDR metadata
     * @param sdr_metadata Standard dynamic range metadata
     * @param target_standard Target HDR standard
     * @return Converted HDR metadata
     */
    HDRMetadata convert_sdr_to_hdr(const HDRMetadata& sdr_metadata, HDRStandard target_standard);
    
    /**
     * Check HDR compatibility between formats
     * @param source_metadata Source HDR metadata
     * @param target_metadata Target HDR metadata
     * @return Compatibility assessment
     */
    struct HDRCompatibilityInfo {
        bool fully_compatible = false;
        bool requires_conversion = false;
        bool quality_loss_expected = false;
        std::vector<std::string> compatibility_notes;
    };
    
    HDRCompatibilityInfo check_hdr_compatibility(const HDRMetadata& source_metadata,
                                                const HDRMetadata& target_metadata);
    
    /**
     * Get standard HDR configurations for common workflows
     */
    HDRProcessingConfig get_youtube_hdr_config();
    HDRProcessingConfig get_netflix_hdr_config();
    HDRProcessingConfig get_broadcast_hlg_config();
    HDRProcessingConfig get_cinema_dci_p3_config();
    HDRProcessingConfig get_apple_dolby_vision_config();
    
    /**
     * Validate HDR workflow for streaming platform
     * @param metadata HDR metadata to validate
     * @param platform_name Target streaming platform
     * @return Validation result with recommendations
     */
    struct StreamingValidationResult {
        bool meets_requirements = false;
        std::vector<std::string> requirements_met;
        std::vector<std::string> requirements_failed;
        std::vector<std::string> recommendations;
    };
    
    StreamingValidationResult validate_for_streaming_platform(const HDRMetadata& metadata,
                                                             const std::string& platform_name);
}

} // namespace ve::media_io
