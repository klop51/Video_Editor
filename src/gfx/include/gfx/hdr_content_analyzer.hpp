// Week 9: HDR Content Analyzer
// Comprehensive HDR content analysis and validation tools

#pragma once

#include "core/base_types.hpp"
#include "core/result.hpp"
#include "gfx/hdr_processor.hpp"
#include "gfx/hdr_metadata_parser.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <chrono>

namespace VideoEditor::GFX {

// =============================================================================
// HDR Content Analysis Types
// =============================================================================

// HDR Content Classification
enum class HDRContentType : uint8_t {
    SDR = 0,                    // Standard Dynamic Range
    HDR10,                      // Static HDR (ST.2086 + ST.2094-10)
    HDR10_PLUS,                 // Dynamic HDR (ST.2094-40)
    DOLBY_VISION,               // Dolby Vision
    HLG,                        // Hybrid Log-Gamma
    SL_HDR1,                    // Technicolor Single Layer HDR1
    SL_HDR2,                    // Technicolor Single Layer HDR2
    UNKNOWN                     // Cannot determine
};

// Content Analysis Categories
enum class ContentCategory : uint8_t {
    NATURAL_SCENE,              // Real-world photography/video
    SYNTHETIC_CGI,              // Computer generated imagery
    MIXED_CONTENT,              // Combination of natural and synthetic
    GRAPHICS_OVERLAY,           // UI elements, text, graphics
    BLACK_LEVEL_CONTENT,        // Predominantly dark content
    BRIGHT_CONTENT,             // Predominantly bright content
    HIGH_CONTRAST,              // High dynamic range within frame
    LOW_CONTRAST,               // Low dynamic range within frame
    SKIN_TONE_HEAVY,            // Significant skin tone content
    UNKNOWN_CATEGORY
};

// Luminance Distribution Analysis
struct LuminanceHistogram {
    std::vector<uint32_t> bins;         // Histogram bins
    float bin_width_nits = 0.0f;        // Width of each bin in nits
    float min_luminance = 0.0f;         // Minimum luminance in dataset
    float max_luminance = 0.0f;         // Maximum luminance in dataset
    uint64_t total_pixels = 0;          // Total number of pixels analyzed
    
    // Statistical measures
    float mean_luminance = 0.0f;
    float median_luminance = 0.0f;
    float std_deviation = 0.0f;
    float skewness = 0.0f;              // Measure of asymmetry
    float kurtosis = 0.0f;              // Measure of tail heaviness
    
    // Percentiles
    float percentile_1 = 0.0f;
    float percentile_5 = 0.0f;
    float percentile_10 = 0.0f;
    float percentile_50 = 0.0f;         // Median
    float percentile_90 = 0.0f;
    float percentile_95 = 0.0f;
    float percentile_99 = 0.0f;
    
    bool is_valid() const noexcept;
    float calculate_dynamic_range() const noexcept;
    std::string to_string() const;
};

// Color Gamut Analysis
struct ColorGamutAnalysis {
    float bt709_coverage = 0.0f;        // % of BT.709 gamut covered
    float dci_p3_coverage = 0.0f;       // % of DCI-P3 gamut covered
    float bt2020_coverage = 0.0f;       // % of BT.2020 gamut covered
    float adobe_rgb_coverage = 0.0f;    // % of Adobe RGB gamut covered
    
    float bt709_exceedance = 0.0f;      // % of pixels outside BT.709
    float dci_p3_exceedance = 0.0f;     // % of pixels outside DCI-P3
    
    uint64_t total_pixels = 0;
    uint64_t out_of_gamut_pixels = 0;
    
    float max_saturation = 0.0f;        // Maximum chroma in content
    float avg_saturation = 0.0f;        // Average chroma in content
    
    bool requires_wide_gamut = false;   // True if significant WCG content
    bool has_highly_saturated_colors = false;
    
    std::string primary_gamut_recommendation() const;
};

// Frame-level HDR Analysis
struct FrameHDRAnalysis {
    uint64_t frame_number = 0;
    std::chrono::microseconds timestamp{0};
    
    HDRContentType content_type = HDRContentType::UNKNOWN;
    ContentCategory category = ContentCategory::UNKNOWN_CATEGORY;
    
    LuminanceHistogram luminance_hist;
    ColorGamutAnalysis gamut_analysis;
    
    float peak_luminance_nits = 0.0f;           // Frame peak luminance
    float average_luminance_nits = 0.0f;        // Frame average luminance
    float min_luminance_nits = 0.0f;            // Frame minimum luminance
    
    float max_content_light_level = 0.0f;       // MaxCLL for this frame
    float frame_average_light_level = 0.0f;     // Frame average luminance
    
    // Spatial analysis
    float luminance_uniformity = 0.0f;          // 0-1, 1 = perfectly uniform
    float local_contrast_index = 0.0f;          // Measure of local contrast
    std::vector<float> luminance_gradient;      // Edge detection results
    
    // Temporal analysis (requires previous frames)
    float temporal_luminance_change = 0.0f;     // Change from previous frame
    float flicker_index = 0.0f;                 // Potential flicker detection
    
    // Quality metrics
    bool has_clipping = false;                  // True if values clipped
    bool has_noise = false;                     // True if significant noise detected
    float signal_to_noise_ratio = 0.0f;         // Estimated SNR
    
    // Metadata consistency
    bool metadata_consistent = true;            // True if metadata matches content
    std::vector<std::string> metadata_warnings;
    
    std::string summary() const;
};

// Sequence-level HDR Analysis
struct SequenceHDRAnalysis {
    std::vector<FrameHDRAnalysis> frame_analyses;
    
    // Overall statistics
    HDRContentType dominant_content_type = HDRContentType::UNKNOWN;
    float sequence_peak_luminance = 0.0f;       // Peak across all frames
    float sequence_avg_luminance = 0.0f;        // Average across all frames
    float sequence_min_luminance = 0.0f;        // Minimum across all frames
    
    // Temporal characteristics
    float max_luminance_change_rate = 0.0f;     // Fastest luminance change
    float avg_luminance_change_rate = 0.0f;     // Average temporal change
    std::vector<float> scene_cut_positions;     // Detected scene changes
    
    // Content classification
    std::map<ContentCategory, float> category_percentages;
    std::map<HDRContentType, size_t> content_type_distribution;
    
    // Quality assessment
    float overall_quality_score = 0.0f;         // 0-100 quality metric
    bool has_temporal_artifacts = false;        // Flicker, pumping, etc.
    bool has_spatial_artifacts = false;         // Banding, blocking, etc.
    
    // Recommendations
    HDRStandard recommended_hdr_standard = HDRStandard::INVALID;
    ToneMappingOperator recommended_tone_mapping = ToneMappingOperator::INVALID;
    ColorSpace recommended_color_space = ColorSpace::INVALID;
    
    std::string generate_report() const;
    bool export_to_json(const std::string& filename) const;
};

// =============================================================================
// HDR Content Analyzer
// =============================================================================

class HDRContentAnalyzer {
public:
    explicit HDRContentAnalyzer() = default;
    ~HDRContentAnalyzer() = default;
    
    // Frame-level analysis
    Core::Result<FrameHDRAnalysis> analyze_frame(
        const FrameData& frame,
        const std::optional<HDRMetadataPacket>& metadata = std::nullopt,
        ColorSpace frame_color_space = ColorSpace::SRGB) const;
    
    // Sequence-level analysis
    Core::Result<SequenceHDRAnalysis> analyze_sequence(
        const std::vector<FrameData>& frames,
        const std::vector<HDRMetadataPacket>& metadata_sequence = {}) const;
    
    // Streaming analysis (for real-time processing)
    Core::Result<void> start_streaming_analysis();
    Core::Result<FrameHDRAnalysis> analyze_streaming_frame(const FrameData& frame);
    Core::Result<SequenceHDRAnalysis> get_streaming_results();
    void reset_streaming_analysis();
    
    // Content classification
    HDRContentType classify_hdr_content(const FrameHDRAnalysis& frame_analysis) const;
    ContentCategory classify_content_category(const FrameData& frame) const;
    
    // Luminance analysis
    LuminanceHistogram calculate_luminance_histogram(
        const FrameData& frame,
        ColorSpace frame_color_space = ColorSpace::SRGB,
        uint32_t num_bins = 256) const;
    
    float calculate_local_contrast(const FrameData& frame,
                                  uint32_t window_size = 32) const;
    
    std::vector<float> calculate_luminance_gradient(const FrameData& frame) const;
    
    // Color gamut analysis
    ColorGamutAnalysis analyze_color_gamut(
        const FrameData& frame,
        ColorSpace frame_color_space = ColorSpace::SRGB) const;
    
    // Quality assessment
    float calculate_signal_to_noise_ratio(const FrameData& frame) const;
    bool detect_clipping(const FrameData& frame, float threshold = 0.99f) const;
    bool detect_noise(const FrameData& frame, float noise_threshold = 0.02f) const;
    
    // Temporal analysis
    float calculate_temporal_luminance_change(
        const FrameHDRAnalysis& current_frame,
        const FrameHDRAnalysis& previous_frame) const;
    
    std::vector<float> detect_scene_cuts(
        const std::vector<FrameHDRAnalysis>& frame_analyses,
        float threshold = 0.3f) const;
    
    float calculate_flicker_index(
        const std::vector<FrameHDRAnalysis>& frame_analyses,
        uint32_t window_size = 30) const;
    
    // Metadata validation
    bool validate_metadata_consistency(
        const FrameHDRAnalysis& frame_analysis,
        const HDRMetadataPacket& metadata) const;
    
    std::vector<std::string> check_metadata_accuracy(
        const SequenceHDRAnalysis& sequence_analysis,
        const std::vector<HDRMetadataPacket>& metadata_sequence) const;
    
    // Recommendation engine
    struct ProcessingRecommendations {
        HDRStandard recommended_standard = HDRStandard::INVALID;
        ToneMappingOperator recommended_tone_mapping = ToneMappingOperator::INVALID;
        ColorSpace recommended_color_space = ColorSpace::INVALID;
        
        bool requires_dynamic_metadata = false;
        bool requires_wide_color_gamut = false;
        bool requires_custom_tone_curve = false;
        
        float recommended_peak_luminance = 0.0f;
        float recommended_black_level = 0.0f;
        
        std::vector<std::string> processing_notes;
        std::vector<std::string> quality_warnings;
    };
    
    ProcessingRecommendations generate_processing_recommendations(
        const SequenceHDRAnalysis& sequence_analysis) const;
    
    // Configuration
    struct AnalysisConfig {
        bool enable_temporal_analysis = true;
        bool enable_noise_detection = true;
        bool enable_quality_assessment = true;
        bool enable_gamut_analysis = true;
        
        uint32_t histogram_bins = 256;
        uint32_t gradient_kernel_size = 3;
        uint32_t contrast_window_size = 32;
        
        float clipping_threshold = 0.99f;
        float noise_threshold = 0.02f;
        float scene_cut_threshold = 0.3f;
    };
    
    void set_analysis_config(const AnalysisConfig& config) { config_ = config; }
    const AnalysisConfig& get_analysis_config() const { return config_; }
    
private:
    AnalysisConfig config_;
    
    // Streaming analysis state
    bool streaming_active_ = false;
    std::vector<FrameHDRAnalysis> streaming_frames_;
    FrameHDRAnalysis previous_frame_analysis_;
    
    // Internal analysis helpers
    float calculate_luminance_value(float r, float g, float b, ColorSpace color_space) const;
    
    std::vector<float> extract_luminance_values(const FrameData& frame,
                                               ColorSpace frame_color_space) const;
    
    float calculate_spatial_uniformity(const std::vector<float>& luminance_values,
                                      uint32_t width, uint32_t height) const;
    
    ContentCategory classify_content_by_luminance_distribution(
        const LuminanceHistogram& histogram) const;
    
    ContentCategory classify_content_by_frequency_analysis(const FrameData& frame) const;
    
    bool detect_skin_tones(const FrameData& frame, ColorSpace frame_color_space) const;
    
    // Statistical helpers
    struct BasicStatistics {
        float mean = 0.0f;
        float median = 0.0f;
        float std_dev = 0.0f;
        float min_val = 0.0f;
        float max_val = 0.0f;
        float skewness = 0.0f;
        float kurtosis = 0.0f;
    };
    
    BasicStatistics calculate_statistics(const std::vector<float>& data) const;
    std::vector<float> calculate_percentiles(const std::vector<float>& data,
                                            const std::vector<float>& percentile_points) const;
    
    // Image processing utilities
    std::vector<float> apply_sobel_operator(const std::vector<float>& image,
                                           uint32_t width, uint32_t height) const;
    
    std::vector<float> apply_gaussian_blur(const std::vector<float>& image,
                                          uint32_t width, uint32_t height,
                                          float sigma = 1.0f) const;
    
    float calculate_image_entropy(const std::vector<float>& image) const;
};

// =============================================================================
// HDR Quality Metrics
// =============================================================================

class HDRQualityMetrics {
public:
    explicit HDRQualityMetrics() = default;
    ~HDRQualityMetrics() = default;
    
    // Industry-standard quality metrics
    float calculate_hdr_vqm(const FrameData& reference, const FrameData& distorted) const;
    float calculate_hdr_vdp(const FrameData& reference, const FrameData& distorted,
                           float viewing_distance = 3.0f) const;
    
    // Tone mapping quality assessment
    float evaluate_tone_mapping_quality(const FrameData& hdr_source,
                                       const FrameData& tone_mapped,
                                       ToneMappingOperator operator_used) const;
    
    // Color accuracy in HDR
    float calculate_hdr_color_accuracy(const FrameData& frame,
                                      const std::vector<ColorSample>& reference_colors) const;
    
    // Temporal consistency metrics
    float calculate_temporal_consistency(const std::vector<FrameData>& sequence) const;
    
    // Artifact detection
    struct ArtifactDetectionResult {
        bool has_banding = false;
        bool has_blocking = false;
        bool has_temporal_pumping = false;
        bool has_color_shifts = false;
        bool has_haloing = false;
        
        float banding_severity = 0.0f;      // 0-1 scale
        float blocking_severity = 0.0f;     // 0-1 scale
        float pumping_severity = 0.0f;      // 0-1 scale
        float color_shift_severity = 0.0f;  // 0-1 scale
        float haloing_severity = 0.0f;      // 0-1 scale
        
        float overall_quality_score = 0.0f; // 0-100 scale
    };
    
    ArtifactDetectionResult detect_hdr_artifacts(const FrameData& frame) const;
    ArtifactDetectionResult detect_temporal_artifacts(const std::vector<FrameData>& sequence) const;
    
private:
    // Internal quality assessment helpers
    float calculate_gradient_based_sharpness(const FrameData& frame) const;
    float calculate_contrast_sensitivity(const FrameData& frame) const;
    float detect_false_contouring(const FrameData& frame) const;
    
    // Perceptual modeling
    std::vector<float> apply_hvs_model(const std::vector<float>& luminance_values) const;
    float calculate_jnd_threshold(float luminance) const; // Just Noticeable Difference
};

} // namespace VideoEditor::GFX
