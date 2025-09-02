#pragma once

#include "core/frame.hpp"
#include "core/color_types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ve::gfx {

/**
 * Advanced HDR Processing with HDR10+ Dynamic Metadata
 * Comprehensive HDR support including dynamic tone mapping and metadata handling
 */

enum class HDRStandard {
    NONE = 0,               // Standard Dynamic Range (SDR)
    HDR10,                  // HDR10 (ST.2084 PQ + Rec.2020)
    HDR10_PLUS,             // HDR10+ with dynamic metadata
    DOLBY_VISION,           // Dolby Vision (proprietary)
    HLG,                    // Hybrid Log-Gamma (BBC/NHK)
    SL_HDR1,                // SL-HDR1 (Philips/Technicolor)
    SL_HDR2,                // SL-HDR2 (advanced)
    SL_HDR3,                // SL-HDR3 (latest)
    ADVANCED_HDR           // Next-generation HDR formats
};

enum class TransferFunction {
    BT709 = 1,              // Rec. 709 (SDR)
    BT2020 = 14,            // Rec. 2020 (same as 709 for compatibility)
    SMPTE_ST2084 = 16,      // PQ (Perceptual Quantizer)
    SMPTE_ST428 = 17,       // Cinema DCI-P3
    HLG = 18,               // Hybrid Log-Gamma (ARIB STD-B67)
    LINEAR = 8,             // Linear transfer function
    GAMMA_2_2 = 4,          // Simple gamma 2.2
    GAMMA_2_4 = 5,          // Simple gamma 2.4
    GAMMA_2_8 = 6,          // Simple gamma 2.8
    LOG_100 = 9,            // Logarithmic (100:1 range)
    LOG_316 = 10,           // Logarithmic (316:1 range)
    SRGB = 13               // sRGB transfer function
};

enum class ColorPrimaries {
    BT709 = 1,              // Rec. 709 / sRGB
    BT470_M = 4,            // NTSC 1953
    BT470_BG = 5,           // PAL/SECAM
    SMPTE_170M = 6,         // NTSC 1987
    SMPTE_240M = 7,         // SMPTE-240M
    FILM = 8,               // Generic film
    BT2020 = 9,             // Rec. 2020
    SMPTE_ST431_2 = 11,     // DCI-P3
    SMPTE_ST432_1 = 12,     // P3 D65
    SMPTE_EG432_2 = 22,     // EBU Tech 3213-E
    JEDEC_P22 = 22          // JEDEC P22 phosphors
};

struct HDRColorSpace {
    ColorPrimaries primaries = ColorPrimaries::BT2020;
    TransferFunction transfer_function = TransferFunction::SMPTE_ST2084;
    uint32_t matrix_coefficients = 9;  // BT.2020 non-constant luminance
    bool full_range = false;            // Limited range by default
    
    // White point and primaries (CIE 1931 chromaticity coordinates)
    struct {
        double white_x = 0.3127, white_y = 0.3290;     // D65
        double red_x = 0.708, red_y = 0.292;           // Rec.2020 red
        double green_x = 0.170, green_y = 0.797;       // Rec.2020 green
        double blue_x = 0.131, blue_y = 0.046;         // Rec.2020 blue
    } chromaticity;
};

struct HDRMasteringDisplayMetadata {
    // Display primaries (CIE 1931 chromaticity coordinates * 50000)
    uint16_t display_primaries_x[3] = {0};  // R, G, B x coordinates
    uint16_t display_primaries_y[3] = {0};  // R, G, B y coordinates
    uint16_t white_point_x = 0;             // White point x coordinate
    uint16_t white_point_y = 0;             // White point y coordinate
    
    // Luminance values in 0.0001 cd/m² units
    uint32_t max_display_mastering_luminance = 0;  // Peak luminance
    uint32_t min_display_mastering_luminance = 0;  // Minimum luminance
    
    // Additional mastering display info
    std::string mastering_display_name;
    std::string color_grading_software;
    std::string colorist_name;
    bool has_valid_metadata = false;
};

struct HDRContentLightLevelInfo {
    uint16_t max_content_light_level = 0;          // MaxCLL in cd/m²
    uint16_t max_frame_average_light_level = 0;    // MaxFALL in cd/m²
    
    // Extended light level statistics
    uint16_t percentile_99_9_light_level = 0;      // 99.9th percentile
    uint16_t percentile_99_light_level = 0;        // 99th percentile
    uint16_t percentile_95_light_level = 0;        // 95th percentile
    uint16_t median_light_level = 0;               // 50th percentile
    
    bool has_valid_info = false;
};

struct HDR10PlusDynamicMetadata {
    // Application identifier
    uint8_t application_identifier = 4;            // 4 = HDR10+
    uint8_t application_version = 1;
    
    // Processing window information
    uint32_t num_windows = 1;
    
    struct ProcessingWindow {
        // Window coordinates (normalized 0.0-1.0)
        double window_upper_left_corner_x = 0.0;
        double window_upper_left_corner_y = 0.0;
        double window_lower_right_corner_x = 1.0;
        double window_lower_right_corner_y = 1.0;
        
        // Center of mass coordinates
        uint16_t center_of_ellipse_x = 0;
        uint16_t center_of_ellipse_y = 0;
        uint8_t rotation_angle = 0;
        uint16_t semimajor_axis_internal_ellipse = 0;
        uint16_t semimajor_axis_external_ellipse = 0;
        uint16_t semiminor_axis_external_ellipse = 0;
        uint8_t overlap_process_option = 0;
        
        // Luminance parameters
        uint32_t maxscl[3] = {0};                   // Maximum per-color component
        uint32_t average_maxrgb = 0;                // Average of maximum RGB
        uint8_t num_distribution_maxrgb_percentiles = 0;
        uint8_t distribution_maxrgb_percentages[15] = {0};
        uint32_t distribution_maxrgb_percentiles[15] = {0};
        
        // Tone mapping parameters
        uint16_t fraction_bright_pixels = 0;
        
        // Knee function parameters (if present)
        bool tone_mapping_flag = false;
        uint16_t knee_point_x = 0;
        uint16_t knee_point_y = 0;
        uint8_t num_bezier_curve_anchors = 0;
        uint16_t bezier_curve_anchors[15] = {0};
        
        // Color saturation mapping (if present)
        bool color_saturation_mapping_flag = false;
        uint8_t color_saturation_weight = 0;
    } windows[3];  // Maximum 3 windows supported
    
    // Global parameters
    bool mastering_display_actual_peak_luminance_flag = false;
    uint8_t num_rows_mastering_display_actual_peak_luminance = 0;
    uint8_t num_cols_mastering_display_actual_peak_luminance = 0;
    uint8_t mastering_display_actual_peak_luminance[25][25] = {{0}};
    
    // Validation
    bool is_valid = false;
    uint32_t frame_number = 0;  // Frame this metadata applies to
};

struct DolbyVisionMetadata {
    // Dolby Vision RPU (Reference Processing Unit) data
    uint8_t rpu_format = 0;
    uint8_t rpu_data_mapping_idc = 0;
    uint8_t rpu_data_chroma_resampling_explicit_filter_flag = 0;
    uint8_t coefficient_data_type = 0;
    
    // Color mapping parameters
    struct {
        uint32_t mmr_order_minus1[3] = {0};         // Mapping polynomial order
        uint64_t mmr_constant_int[3] = {0};         // Constant coefficients
        uint64_t mmr_coeff_int[3][8][8] = {{{0}}}; // Polynomial coefficients
        
        // Chroma scaling
        uint8_t chroma_resampling_explicit_filter_flag = 0;
        int8_t chroma_filter_coeffs[4] = {0};
    } color_mapping;
    
    // Tone mapping
    struct {
        uint16_t targeted_system_display_maximum_luminance = 0;
        bool targeted_system_display_actual_peak_luminance_flag = false;
        uint8_t num_rows_targeted_system_display_actual_peak_luminance = 0;
        uint8_t num_cols_targeted_system_display_actual_peak_luminance = 0;
        uint8_t targeted_system_display_actual_peak_luminance[25][25] = {{0}};
    } tone_mapping;
    
    // Extended metadata
    std::vector<uint8_t> extension_metadata;
    
    bool is_valid = false;
};

/**
 * Advanced HDR Processor
 * Handles HDR content analysis, tone mapping, and metadata processing
 */
class AdvancedHDRProcessor {
public:
    AdvancedHDRProcessor();
    ~AdvancedHDRProcessor();
    
    // HDR detection and analysis
    HDRStandard detectHDRStandard(const Frame& frame);
    HDRColorSpace analyzeColorSpace(const Frame& frame);
    HDRContentLightLevelInfo analyzeContentLightLevels(const Frame& frame);
    
    // Dynamic metadata extraction
    HDR10PlusDynamicMetadata extractHDR10PlusMetadata(const Frame& frame);
    DolbyVisionMetadata extractDolbyVisionMetadata(const Frame& frame);
    
    // Tone mapping
    Frame toneMapHDRToSDR(const Frame& hdr_frame, const std::string& tone_map_method = "hable");
    Frame toneMapSDRToHDR(const Frame& sdr_frame, const HDRColorSpace& target_color_space);
    Frame adaptiveToneMap(const Frame& input_frame, const HDR10PlusDynamicMetadata& dynamic_metadata);
    
    // Color space conversion
    Frame convertColorSpace(const Frame& input_frame, 
                          const HDRColorSpace& source_space,
                          const HDRColorSpace& target_space);
    
    // Dynamic range optimization
    Frame optimizeDynamicRange(const Frame& hdr_frame, 
                             const HDRMasteringDisplayMetadata& mastering_metadata,
                             const HDRContentLightLevelInfo& content_info);
    
    // HDR validation and compliance
    bool validateHDRCompliance(const Frame& hdr_frame, HDRStandard standard);
    std::vector<std::string> getComplianceIssues(const Frame& hdr_frame, HDRStandard standard);
    
    // Metadata generation
    HDR10PlusDynamicMetadata generateHDR10PlusMetadata(const Frame& frame, 
                                                      uint32_t frame_number = 0);
    HDRMasteringDisplayMetadata generateMasteringDisplayMetadata(const std::vector<Frame>& frames);
    HDRContentLightLevelInfo generateContentLightLevelInfo(const std::vector<Frame>& frames);
    
    // Advanced tone mapping methods
    void setToneMappingMethod(const std::string& method);
    std::vector<std::string> getAvailableToneMappingMethods() const;
    
    // Display adaptation
    Frame adaptForDisplay(const Frame& hdr_frame,
                        const HDRColorSpace& display_capabilities,
                        double peak_brightness_nits = 1000.0);

private:
    struct HDRProcessorImpl;
    std::unique_ptr<HDRProcessorImpl> impl_;
    
    // Tone mapping algorithms
    Frame hableToneMap(const Frame& hdr_frame, double exposure = 1.0);
    Frame aces2020ToneMap(const Frame& hdr_frame);
    Frame reinhardToneMap(const Frame& hdr_frame, double white_point = 1.0);
    Frame uncharted2ToneMap(const Frame& hdr_frame);
    Frame agxToneMap(const Frame& hdr_frame);  // Latest ACES-based tone mapper
    
    // Color space transformation matrices
    Matrix3f getColorSpaceTransform(ColorPrimaries from, ColorPrimaries to);
    Matrix3f getTransferFunctionTransform(TransferFunction from, TransferFunction to);
    
    // HDR analysis helpers
    double calculatePeakLuminance(const Frame& frame);
    double calculateAverageLuminance(const Frame& frame);
    std::vector<double> calculateLuminanceHistogram(const Frame& frame, uint32_t bins = 256);
    
    // Dynamic metadata helpers
    void analyzeRegionOfInterest(const Frame& frame, 
                               double x, double y, double width, double height,
                               HDR10PlusDynamicMetadata::ProcessingWindow& window);
    
    // Validation helpers
    bool validateColorPrimaries(const Frame& frame, ColorPrimaries primaries);
    bool validateTransferFunction(const Frame& frame, TransferFunction transfer_func);
    bool validateLuminanceRange(const Frame& frame, double min_nits, double max_nits);
};

/**
 * HDR Content Analyzer
 * Advanced analysis tools for HDR content optimization
 */
class HDRContentAnalyzer {
public:
    struct HDRAnalysisReport {
        HDRStandard detected_standard = HDRStandard::NONE;
        HDRColorSpace color_space;
        HDRMasteringDisplayMetadata mastering_metadata;
        HDRContentLightLevelInfo content_light_info;
        
        // Content characteristics
        double peak_luminance_nits = 0.0;
        double average_luminance_nits = 0.0;
        double min_luminance_nits = 0.0;
        double luminance_range_ratio = 0.0;
        
        // Dynamic range utilization
        double hdr_utilization_percentage = 0.0;  // How much of HDR range is used
        double color_gamut_coverage = 0.0;        // Rec.2020 gamut coverage
        double temporal_consistency = 0.0;         // Frame-to-frame consistency
        
        // Quality metrics
        double hdr_quality_score = 0.0;           // Overall HDR quality (0-100)
        std::vector<std::string> quality_issues;
        std::vector<std::string> optimization_recommendations;
        
        // Tone mapping recommendations
        std::string recommended_tone_map_method;
        double recommended_exposure_adjustment = 0.0;
        
        // Display compatibility
        std::map<std::string, bool> display_compatibility;  // Device compatibility
    };
    
    static HDRAnalysisReport analyzeHDRContent(const std::vector<Frame>& frames);
    static HDRAnalysisReport analyzeHDRSequence(const std::vector<Frame>& frames,
                                               const std::vector<HDR10PlusDynamicMetadata>& metadata);
    
    // Content optimization recommendations
    static std::vector<std::string> getOptimizationRecommendations(const HDRAnalysisReport& report);
    static HDRColorSpace getOptimalColorSpace(const HDRAnalysisReport& report);
    static double getOptimalPeakLuminance(const HDRAnalysisReport& report);
    
    // Display adaptation analysis
    static std::map<std::string, double> analyzeDisplayCompatibility(const HDRAnalysisReport& report);
    static std::vector<std::string> getDisplayAdaptationRequirements(const HDRAnalysisReport& report,
                                                                    const std::string& target_display);

private:
    static double calculateHDRUtilization(const std::vector<Frame>& frames);
    static double calculateColorGamutCoverage(const std::vector<Frame>& frames, ColorPrimaries gamut);
    static double calculateTemporalConsistency(const std::vector<Frame>& frames);
    static double calculateOverallQualityScore(const HDRAnalysisReport& report);
};

/**
 * HDR Metadata Manager
 * Comprehensive metadata handling for all HDR standards
 */
class HDRMetadataManager {
public:
    // Metadata serialization/deserialization
    static std::vector<uint8_t> serializeHDR10PlusMetadata(const HDR10PlusDynamicMetadata& metadata);
    static HDR10PlusDynamicMetadata deserializeHDR10PlusMetadata(const std::vector<uint8_t>& data);
    
    static std::vector<uint8_t> serializeDolbyVisionMetadata(const DolbyVisionMetadata& metadata);
    static DolbyVisionMetadata deserializeDolbyVisionMetadata(const std::vector<uint8_t>& data);
    
    // SEI message handling (H.264/H.265)
    static std::vector<uint8_t> createHDR10SEIMessage(const HDRMasteringDisplayMetadata& mastering,
                                                     const HDRContentLightLevelInfo& content_info);
    static std::vector<uint8_t> createHDR10PlusSEIMessage(const HDR10PlusDynamicMetadata& metadata);
    
    // Container metadata (MP4, MKV, etc.)
    static std::string generateMP4HDRMetadata(const HDRColorSpace& color_space,
                                            const HDRMasteringDisplayMetadata& mastering);
    static std::string generateMKVHDRMetadata(const HDRColorSpace& color_space,
                                            const HDRMasteringDisplayMetadata& mastering);
    
    // Platform-specific metadata
    static std::string generateYouTubeHDRMetadata(const HDRAnalysisReport& analysis);
    static std::string generateNetflixHDRMetadata(const HDRAnalysisReport& analysis);
    static std::string generateAmazonHDRMetadata(const HDRAnalysisReport& analysis);
    
    // Validation and compliance
    static bool validateMetadataCompliance(const HDR10PlusDynamicMetadata& metadata);
    static std::vector<std::string> getMetadataValidationErrors(const HDR10PlusDynamicMetadata& metadata);

private:
    static void encodeUnsignedVarInt(std::vector<uint8_t>& buffer, uint64_t value);
    static uint64_t decodeUnsignedVarInt(const std::vector<uint8_t>& buffer, size_t& offset);
};

/**
 * HDR Display Simulation
 * Simulates different HDR display capabilities for testing
 */
class HDRDisplaySimulator {
public:
    struct HDRDisplayProfile {
        std::string display_name;
        double peak_luminance_nits = 1000.0;
        double min_luminance_nits = 0.01;
        ColorPrimaries native_primaries = ColorPrimaries::BT2020;
        double color_gamut_coverage = 0.95;  // Percentage of target gamut
        bool supports_hdr10 = true;
        bool supports_hdr10_plus = false;
        bool supports_dolby_vision = false;
        bool supports_hlg = true;
        
        // Display characteristics
        double panel_reflectance = 0.02;     // Ambient light reflection
        double contrast_ratio = 0.0;         // 0 = infinite (OLED), >0 = LCD
        std::string panel_technology;        // "OLED", "LCD", "MicroLED", etc.
    };
    
    // Predefined display profiles
    static HDRDisplayProfile getDisplayProfile(const std::string& display_name);
    static std::vector<std::string> getAvailableDisplays();
    
    // Display simulation
    static Frame simulateDisplayOutput(const Frame& hdr_frame,
                                     const HDRDisplayProfile& display_profile,
                                     double ambient_light_nits = 5.0);
    
    // Display capabilities testing
    static bool canDisplayContent(const HDRAnalysisReport& content_analysis,
                                const HDRDisplayProfile& display_profile);
    static std::vector<std::string> getDisplayLimitations(const HDRAnalysisReport& content_analysis,
                                                         const HDRDisplayProfile& display_profile);
    
    // Custom display profile creation
    static HDRDisplayProfile createCustomProfile(const std::string& name,
                                                double peak_nits,
                                                double min_nits,
                                                ColorPrimaries primaries);

private:
    static std::map<std::string, HDRDisplayProfile> builtin_profiles_;
    static void initializeBuiltinProfiles();
    
    static Frame applyDisplayLimitations(const Frame& input_frame,
                                       const HDRDisplayProfile& profile);
    static Frame simulateAmbientLight(const Frame& display_frame,
                                    const HDRDisplayProfile& profile,
                                    double ambient_nits);
};

} // namespace ve::gfx
