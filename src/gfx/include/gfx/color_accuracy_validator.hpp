// Week 9: Color Accuracy Validation Tools
// Professional color accuracy measurement and validation for video workflows

#pragma once

#include "core/base_types.hpp"
#include "core/result.hpp"
#include "gfx/hdr_processor.hpp"
#include "gfx/wide_color_gamut_support.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <chrono>

namespace VideoEditor::GFX {

// =============================================================================
// Color Accuracy Standards and References
// =============================================================================

// CIE Color Space Definitions
struct CIExyY {
    float x, y, Y;
    
    CIExyY() = default;
    CIExyY(float x_, float y_, float Y_ = 1.0f) : x(x_), y(y_), Y(Y_) {}
    
    bool is_valid() const noexcept;
};

struct CIEXYZ {
    float X, Y, Z;
    
    CIEXYZ() = default;
    CIEXYZ(float X_, float Y_, float Z_) : X(X_), Y(Y_), Z(Z_) {}
    
    static CIEXYZ from_xyY(const CIExyY& xyY);
    CIExyY to_xyY() const;
    bool is_valid() const noexcept;
};

struct CIELab {
    float L, a, b;
    
    CIELab() = default;
    CIELab(float L_, float a_, float b_) : L(L_), a(a_), b(b_) {}
    
    static CIELab from_XYZ(const CIEXYZ& xyz, const CIEXYZ& white_point);
    CIEXYZ to_XYZ(const CIEXYZ& white_point) const;
    bool is_valid() const noexcept;
};

// Standard Illuminants
enum class StandardIlluminant : uint8_t {
    D50,    // 5003K - Printing, graphics arts
    D55,    // 5503K - Photography
    D65,    // 6504K - Television, sRGB
    D75,    // 7504K - Photography
    A,      // 2856K - Incandescent
    B,      // 4874K - Direct sunlight
    C,      // 6774K - Average daylight
    E,      // Equal energy
    F2,     // 4230K - Fluorescent
    F7,     // 6500K - Fluorescent
    F11     // 4000K - Fluorescent
};

// Color Difference Metrics
enum class ColorDifferenceMetric : uint8_t {
    DELTA_E_76,     // CIE 1976 Delta E
    DELTA_E_94,     // CIE 1994 Delta E
    DELTA_E_2000,   // CIE 2000 Delta E (CIEDE2000)
    DELTA_E_CMC,    // CMC l:c Delta E
    DELTA_RGB,      // Simple RGB Euclidean distance
    DELTA_ITP       // ICtCp color space delta
};

// Color Checker Standards
enum class ColorCheckerType : uint8_t {
    MACBETH_24,         // X-Rite ColorChecker Classic 24
    MACBETH_140,        // X-Rite ColorChecker Digital SG 140
    IT8_7_4,            // IT8.7/4 transparency target
    XRITE_PASSPORT,     // X-Rite ColorChecker Passport
    DATACOLOR_SPYDER,   // Datacolor SpyderCHECKR
    CUSTOM              // User-defined reference
};

// =============================================================================
// Color Measurement and Analysis
// =============================================================================

struct ColorSample {
    CIEXYZ xyz_measured;        // Measured XYZ values
    CIEXYZ xyz_reference;       // Reference XYZ values
    CIELab lab_measured;        // Measured LAB values
    CIELab lab_reference;       // Reference LAB values
    
    float delta_e_76 = 0.0f;    // CIE 1976 Delta E
    float delta_e_94 = 0.0f;    // CIE 1994 Delta E
    float delta_e_2000 = 0.0f;  // CIE 2000 Delta E
    
    std::string patch_name;     // Color patch identifier
    uint32_t patch_id = 0;      // Numeric patch ID
    
    bool is_valid() const noexcept;
};

struct ColorAccuracyResult {
    // Overall statistics
    float mean_delta_e_76 = 0.0f;
    float mean_delta_e_94 = 0.0f;
    float mean_delta_e_2000 = 0.0f;
    float max_delta_e_2000 = 0.0f;
    float percentile_95_delta_e = 0.0f;
    
    // Color categories
    float skin_tone_accuracy = 0.0f;      // Average Delta E for skin tones
    float neutral_accuracy = 0.0f;        // Average Delta E for grays
    float primary_accuracy = 0.0f;        // Average Delta E for primaries
    float saturated_accuracy = 0.0f;      // Average Delta E for saturated colors
    
    // Gamut analysis
    float gamut_coverage_srgb = 0.0f;      // % of sRGB gamut covered
    float gamut_coverage_adobe_rgb = 0.0f; // % of Adobe RGB gamut covered
    float gamut_coverage_dci_p3 = 0.0f;    // % of DCI-P3 gamut covered
    float gamut_coverage_bt2020 = 0.0f;    // % of BT.2020 gamut covered
    
    // White point analysis
    CIExyY measured_white_point;
    CIExyY reference_white_point;
    float white_point_deviation = 0.0f;    // Delta uv
    
    // Tone response
    float gamma_measured = 0.0f;
    float gamma_reference = 2.2f;
    float gamma_deviation = 0.0f;
    
    // Pass/fail criteria
    bool passes_broadcast_standard = false;  // Delta E < 3.0
    bool passes_cinema_standard = false;     // Delta E < 2.0
    bool passes_professional_standard = false; // Delta E < 1.0
    
    // Individual samples
    std::vector<ColorSample> color_samples;
    
    // Timing
    std::chrono::milliseconds measurement_duration{0};
    
    std::string generate_report() const;
    bool export_to_csv(const std::string& filename) const;
};

// =============================================================================
// Color Accuracy Validator
// =============================================================================

class ColorAccuracyValidator {
public:
    explicit ColorAccuracyValidator() = default;
    ~ColorAccuracyValidator() = default;
    
    // Initialize with reference data
    Core::Result<void> load_color_checker_reference(ColorCheckerType checker_type);
    Core::Result<void> load_custom_reference(const std::vector<ColorSample>& reference_colors);
    Core::Result<void> load_reference_from_file(const std::string& filename);
    
    // Color measurement and validation
    Core::Result<ColorAccuracyResult> validate_frame_accuracy(
        const FrameData& frame,
        const std::vector<BoundingBox>& color_patch_regions,
        ColorSpace frame_color_space = ColorSpace::SRGB) const;
    
    Core::Result<ColorAccuracyResult> validate_display_accuracy(
        const std::vector<ColorSample>& measured_samples) const;
    
    // Automatic color checker detection
    Core::Result<std::vector<BoundingBox>> detect_color_checker_patches(
        const FrameData& frame,
        ColorCheckerType checker_type) const;
    
    // Color space analysis
    Core::Result<float> calculate_gamut_coverage(
        const std::vector<CIEXYZ>& measured_colors,
        RGBWorkingSpace target_gamut) const;
    
    Core::Result<CIExyY> calculate_white_point(
        const FrameData& frame,
        const BoundingBox& white_patch_region) const;
    
    // Tone response analysis
    struct ToneResponseResult {
        float measured_gamma = 0.0f;
        float reference_gamma = 2.2f;
        float correlation_coefficient = 0.0f;
        std::vector<float> input_levels;    // 0.0 - 1.0
        std::vector<float> output_levels;   // Measured luminance
        std::vector<float> reference_curve; // Expected luminance
        
        bool is_linear_response() const { return std::abs(measured_gamma - 1.0f) < 0.1f; }
        bool is_valid_gamma() const { return measured_gamma > 0.5f && measured_gamma < 4.0f; }
    };
    
    Core::Result<ToneResponseResult> analyze_tone_response(
        const FrameData& frame,
        const std::vector<BoundingBox>& gray_patch_regions) const;
    
    // Delta E calculations
    float calculate_delta_e_76(const CIELab& lab1, const CIELab& lab2) const noexcept;
    float calculate_delta_e_94(const CIELab& lab1, const CIELab& lab2,
                               float kL = 1.0f, float kC = 1.0f, float kH = 1.0f) const noexcept;
    float calculate_delta_e_2000(const CIELab& lab1, const CIELab& lab2,
                                 float kL = 1.0f, float kC = 1.0f, float kH = 1.0f) const noexcept;
    
    // Color space conversions
    static CIEXYZ rgb_to_xyz(float r, float g, float b, 
                            RGBWorkingSpace color_space,
                            StandardIlluminant illuminant = StandardIlluminant::D65);
    
    static void xyz_to_rgb(const CIEXYZ& xyz, float& r, float& g, float& b,
                          RGBWorkingSpace color_space,
                          StandardIlluminant illuminant = StandardIlluminant::D65);
    
    static CIELab xyz_to_lab(const CIEXYZ& xyz, StandardIlluminant illuminant = StandardIlluminant::D65);
    static CIEXYZ lab_to_xyz(const CIELab& lab, StandardIlluminant illuminant = StandardIlluminant::D65);
    
    // Utility functions
    static CIEXYZ get_illuminant_xyz(StandardIlluminant illuminant);
    static CIExyY get_illuminant_xyy(StandardIlluminant illuminant);
    
private:
    std::vector<ColorSample> reference_colors_;
    ColorCheckerType current_checker_type_ = ColorCheckerType::MACBETH_24;
    
    // Internal analysis helpers
    ColorSample analyze_color_patch(const FrameData& frame,
                                   const BoundingBox& patch_region,
                                   ColorSpace frame_color_space) const;
    
    float calculate_patch_average_luminance(const FrameData& frame,
                                           const BoundingBox& region) const;
    
    CIEXYZ calculate_patch_average_xyz(const FrameData& frame,
                                      const BoundingBox& region,
                                      ColorSpace frame_color_space) const;
    
    // Color checker pattern recognition
    struct ColorCheckerPattern {
        uint32_t rows, cols;
        std::vector<CIEXYZ> reference_colors;
        std::vector<std::string> patch_names;
    };
    
    ColorCheckerPattern get_color_checker_pattern(ColorCheckerType checker_type) const;
    
    // Statistical analysis
    struct ColorStatistics {
        float mean = 0.0f;
        float median = 0.0f;
        float std_dev = 0.0f;
        float min_value = 0.0f;
        float max_value = 0.0f;
        float percentile_95 = 0.0f;
    };
    
    ColorStatistics calculate_delta_e_statistics(const std::vector<float>& delta_e_values) const;
    
    // Classification helpers
    bool is_skin_tone_patch(const std::string& patch_name) const;
    bool is_neutral_patch(const std::string& patch_name) const;
    bool is_primary_patch(const std::string& patch_name) const;
    bool is_saturated_patch(const std::string& patch_name) const;
};

// =============================================================================
// Color Calibration System
// =============================================================================

struct CalibrationTarget {
    RGBWorkingSpace target_color_space = RGBWorkingSpace::SRGB;
    StandardIlluminant target_white_point = StandardIlluminant::D65;
    float target_gamma = 2.2f;
    float target_luminance = 100.0f;  // cd/m²
    
    // Quality targets
    float max_delta_e_target = 2.0f;
    float mean_delta_e_target = 1.0f;
    float white_point_tolerance = 0.003f;  // Delta uv
    float gamma_tolerance = 0.1f;
    
    bool is_valid() const noexcept;
};

struct CalibrationCorrection {
    // 3D LUT correction
    std::vector<float> correction_lut_3d;  // RGB triplets for 3D LUT
    uint32_t lut_size = 33;                // LUT dimension (33x33x33)
    
    // Matrix correction
    float correction_matrix[9] = {1,0,0, 0,1,0, 0,0,1}; // 3x3 color matrix
    
    // Gamma correction
    float gamma_correction[3] = {1.0f, 1.0f, 1.0f};     // R, G, B gamma
    
    // White point correction
    float white_point_correction[2] = {0.0f, 0.0f};     // x, y offset
    
    // Gain/offset correction
    float gain_correction[3] = {1.0f, 1.0f, 1.0f};      // R, G, B gain
    float offset_correction[3] = {0.0f, 0.0f, 0.0f};    // R, G, B offset
    
    // Validation
    bool is_identity() const noexcept;
    float calculate_correction_strength() const noexcept;
};

class ColorCalibrationSystem {
public:
    explicit ColorCalibrationSystem() = default;
    ~ColorCalibrationSystem() = default;
    
    // Calibration workflow
    Core::Result<CalibrationCorrection> generate_calibration(
        const ColorAccuracyResult& measurement_result,
        const CalibrationTarget& target) const;
    
    Core::Result<ColorAccuracyResult> validate_calibration(
        const CalibrationCorrection& correction,
        const ColorAccuracyResult& original_measurement) const;
    
    // Apply calibration corrections
    Core::Result<void> apply_matrix_correction(FrameData& frame,
                                              const float matrix[9]) const;
    
    Core::Result<void> apply_3d_lut_correction(FrameData& frame,
                                              const std::vector<float>& lut_3d,
                                              uint32_t lut_size) const;
    
    Core::Result<void> apply_gamma_correction(FrameData& frame,
                                             const float gamma[3]) const;
    
    // Calibration optimization
    Core::Result<CalibrationCorrection> optimize_calibration_iterative(
        const ColorAccuracyResult& measurement_result,
        const CalibrationTarget& target,
        uint32_t max_iterations = 10) const;
    
    // Export calibration data
    Core::Result<void> export_3d_lut(const CalibrationCorrection& correction,
                                     const std::string& filename,
                                     const std::string& format = "cube") const;
    
    Core::Result<void> export_icc_profile(const CalibrationCorrection& correction,
                                          const CalibrationTarget& target,
                                          const std::string& filename) const;
    
private:
    // Correction generation algorithms
    CalibrationCorrection generate_matrix_correction(
        const ColorAccuracyResult& measurement,
        const CalibrationTarget& target) const;
    
    CalibrationCorrection generate_3d_lut_correction(
        const ColorAccuracyResult& measurement,
        const CalibrationTarget& target,
        uint32_t lut_size) const;
    
    CalibrationCorrection generate_gamma_correction(
        const ColorAccuracyResult& measurement,
        const CalibrationTarget& target) const;
    
    // Optimization helpers
    float calculate_calibration_error(const CalibrationCorrection& correction,
                                     const ColorAccuracyResult& measurement,
                                     const CalibrationTarget& target) const;
    
    CalibrationCorrection refine_correction(const CalibrationCorrection& base_correction,
                                           const ColorAccuracyResult& measurement,
                                           const CalibrationTarget& target) const;
    
    // LUT utilities
    void interpolate_3d_lut(float r, float g, float b,
                           const std::vector<float>& lut_3d,
                           uint32_t lut_size,
                           float& out_r, float& out_g, float& out_b) const;
    
    size_t get_lut_index(uint32_t r, uint32_t g, uint32_t b, uint32_t lut_size) const;
};

// =============================================================================
// Utility Structures
// =============================================================================

struct BoundingBox {
    uint32_t x, y, width, height;
    
    BoundingBox() = default;
    BoundingBox(uint32_t x_, uint32_t y_, uint32_t w_, uint32_t h_) 
        : x(x_), y(y_), width(w_), height(h_) {}
    
    bool is_valid() const noexcept {
        return width > 0 && height > 0;
    }
    
    bool contains(uint32_t px, uint32_t py) const noexcept {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
    
    uint32_t area() const noexcept {
        return width * height;
    }
};

// Color accuracy reporting
struct AccuracyReport {
    ColorAccuracyResult accuracy_result;
    CalibrationTarget target;
    std::string device_info;
    std::string measurement_conditions;
    std::chrono::system_clock::time_point timestamp;
    
    bool export_to_json(const std::string& filename) const;
    bool export_to_xml(const std::string& filename) const;
    std::string generate_html_report() const;
};

} // namespace VideoEditor::GFX
