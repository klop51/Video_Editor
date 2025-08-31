// Week 9: Professional Monitor Calibration System
// Display calibration and color management for professional video monitoring

#pragma once

#include "core/base_types.hpp"
#include "core/result.hpp"
#include "gfx/color_accuracy_validator.hpp"
#include "gfx/wide_color_gamut_support.hpp"
#include <vector>
#include <memory>
#include <string>
#include <chrono>

namespace VideoEditor::GFX {

// =============================================================================
// Display Technology Types
// =============================================================================

enum class DisplayTechnology : uint8_t {
    LCD_IPS,            // IPS LCD panel
    LCD_VA,             // VA LCD panel
    LCD_TN,             // TN LCD panel
    OLED,               // OLED display
    QLED,               // Quantum Dot LED
    MICROLED,           // MicroLED display
    PLASMA,             // Plasma display (legacy)
    CRT,                // CRT monitor (legacy)
    PROJECTOR_LCD,      // LCD projector
    PROJECTOR_DLP,      // DLP projector
    PROJECTOR_LASER,    // Laser projector
    UNKNOWN
};

enum class DisplayPurpose : uint8_t {
    BROADCAST_MONITORING,   // Broadcast video monitoring
    CINEMA_MASTERING,       // Cinema content mastering
    HDR_GRADING,           // HDR color grading
    SDR_GRADING,           // SDR color grading
    CONSUMER_PREVIEW,       // Consumer device preview
    CLIENT_REVIEW,          // Client review/approval
    GENERAL_EDITING,        // General video editing
    GRAPHICS_DESIGN,        // Graphics and design work
    PHOTOGRAPHY,            // Photography workflow
    UNKNOWN_PURPOSE
};

// =============================================================================
// Monitor Specifications
// =============================================================================

struct MonitorSpecifications {
    std::string manufacturer;
    std::string model;
    std::string serial_number;
    
    DisplayTechnology technology = DisplayTechnology::UNKNOWN;
    DisplayPurpose primary_purpose = DisplayPurpose::UNKNOWN_PURPOSE;
    
    // Physical characteristics
    float diagonal_size_inches = 0.0f;
    uint32_t native_width = 0;
    uint32_t native_height = 0;
    float pixel_pitch_mm = 0.0f;
    
    // Luminance capabilities
    float peak_luminance_nits = 0.0f;        // Maximum brightness
    float min_luminance_nits = 0.0f;         // Black level
    float typical_luminance_nits = 0.0f;     // Typical working brightness
    
    // Color gamut specifications
    RGBWorkingSpace native_color_space = RGBWorkingSpace::SRGB;
    float bt709_coverage = 0.0f;             // % BT.709 coverage
    float dci_p3_coverage = 0.0f;            // % DCI-P3 coverage
    float bt2020_coverage = 0.0f;            // % BT.2020 coverage
    float adobe_rgb_coverage = 0.0f;         // % Adobe RGB coverage
    
    // HDR capabilities
    bool supports_hdr10 = false;
    bool supports_hdr10_plus = false;
    bool supports_dolby_vision = false;
    bool supports_hlg = false;
    
    // Color depth and precision
    uint8_t bit_depth = 8;                   // Native bit depth
    bool supports_10bit = false;
    bool supports_12bit = false;
    bool hardware_lut_available = false;
    uint32_t hardware_lut_size = 0;          // 3D LUT size if available
    
    // Professional features
    bool has_calibration_probe_support = false;
    bool has_hardware_calibration = false;
    bool has_uniform_luminance = false;
    std::vector<std::string> supported_calibration_standards;
    
    bool is_valid() const noexcept;
    std::string to_string() const;
};

// =============================================================================
// Calibration Standards and Targets
// =============================================================================

enum class CalibrationStandard : uint8_t {
    REC_709,                // ITU-R Rec. 709
    REC_2020,               // ITU-R Rec. 2020
    DCI_P3,                 // DCI-P3 cinema standard
    SMPTE_C,                // SMPTE-C standard
    EBU_3213,               // EBU Tech 3213
    ADOBE_RGB,              // Adobe RGB standard
    SRGB,                   // sRGB standard
    CUSTOM                  // User-defined standard
};

struct CalibrationStandardSpec {
    CalibrationStandard standard = CalibrationStandard::CUSTOM;
    
    // Target specifications
    RGBWorkingSpace color_space = RGBWorkingSpace::SRGB;
    StandardIlluminant white_point = StandardIlluminant::D65;
    float target_gamma = 2.2f;
    float target_luminance_nits = 100.0f;
    float target_black_level_nits = 0.3f;
    
    // Tolerance specifications
    float max_delta_e_tolerance = 2.0f;
    float mean_delta_e_tolerance = 1.0f;
    float white_point_tolerance = 0.003f;    // Delta uv
    float gamma_tolerance = 0.05f;
    float luminance_tolerance = 0.05f;       // ±5%
    float uniformity_tolerance = 0.05f;      // ±5% across screen
    
    // Environmental conditions
    float ambient_light_lux = 64.0f;         // Recommended ambient lighting
    float viewing_distance_meters = 1.0f;    // Optimal viewing distance
    
    bool is_valid() const noexcept;
    std::string get_standard_name() const;
    static CalibrationStandardSpec get_predefined_standard(CalibrationStandard standard);
};

// =============================================================================
// Calibration Measurement Results
// =============================================================================

struct UniformityMeasurement {
    std::vector<float> luminance_grid;       // Luminance at grid points
    std::vector<CIExyY> chromaticity_grid;   // Chromaticity at grid points
    uint32_t grid_width = 0;                 // Measurement grid width
    uint32_t grid_height = 0;                // Measurement grid height
    
    float max_luminance_deviation = 0.0f;    // Maximum deviation from center
    float avg_luminance_deviation = 0.0f;    // Average deviation from center
    float max_color_deviation = 0.0f;        // Maximum color shift
    float avg_color_deviation = 0.0f;        // Average color shift
    
    bool passes_uniformity_spec = false;
    float uniformity_percentage = 0.0f;      // Overall uniformity score
    
    std::string generate_report() const;
};

struct GammaTrackingMeasurement {
    std::vector<float> input_levels;         // 0.0 - 1.0 input values
    std::vector<float> measured_luminance;   // Measured luminance values
    std::vector<float> target_luminance;     // Target luminance values
    
    float measured_gamma = 0.0f;
    float target_gamma = 2.2f;
    float gamma_accuracy = 0.0f;             // How well it tracks target
    float correlation_coefficient = 0.0f;    // R² of gamma fit
    
    bool passes_gamma_spec = false;
    std::vector<float> deviation_points;     // Points with high deviation
    
    std::string generate_report() const;
};

struct ColorAccuracyMeasurement {
    std::vector<ColorSample> color_patches;  // Individual patch measurements
    
    float mean_delta_e_76 = 0.0f;
    float mean_delta_e_2000 = 0.0f;
    float max_delta_e_2000 = 0.0f;
    float percentile_95_delta_e = 0.0f;
    
    // Category-specific accuracy
    float skin_tone_accuracy = 0.0f;
    float neutral_accuracy = 0.0f;
    float primary_accuracy = 0.0f;
    float saturated_accuracy = 0.0f;
    
    bool passes_color_accuracy_spec = false;
    CIExyY measured_white_point;
    float white_point_deviation = 0.0f;
    
    std::string generate_report() const;
};

struct CalibrationMeasurementResult {
    MonitorSpecifications monitor_specs;
    CalibrationStandardSpec target_standard;
    
    ColorAccuracyMeasurement color_accuracy;
    GammaTrackingMeasurement gamma_tracking;
    UniformityMeasurement uniformity;
    
    // Overall assessment
    bool passes_all_specifications = false;
    float overall_quality_score = 0.0f;      // 0-100 composite score
    std::vector<std::string> failed_criteria;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    
    // Measurement metadata
    std::string measurement_technician;
    std::chrono::system_clock::time_point measurement_time;
    std::string measurement_conditions;
    std::string calibration_probe_model;
    
    std::string generate_comprehensive_report() const;
    bool export_to_pdf(const std::string& filename) const;
    bool export_to_json(const std::string& filename) const;
};

// =============================================================================
// Monitor Calibration System
// =============================================================================

class MonitorCalibrationSystem {
public:
    explicit MonitorCalibrationSystem() = default;
    ~MonitorCalibrationSystem() = default;
    
    // Calibration probe interface
    Core::Result<void> connect_calibration_probe(const std::string& probe_model);
    Core::Result<void> disconnect_calibration_probe();
    bool is_probe_connected() const { return probe_connected_; }
    std::string get_probe_model() const { return probe_model_; }
    
    // Monitor identification and setup
    Core::Result<MonitorSpecifications> detect_monitor_specifications() const;
    Core::Result<void> configure_monitor_for_calibration(const MonitorSpecifications& specs);
    
    // Calibration workflow
    Core::Result<CalibrationMeasurementResult> perform_full_calibration(
        const CalibrationStandardSpec& target_standard,
        const MonitorSpecifications& monitor_specs);
    
    Core::Result<ColorAccuracyMeasurement> measure_color_accuracy(
        const CalibrationStandardSpec& target_standard);
    
    Core::Result<GammaTrackingMeasurement> measure_gamma_tracking(
        float target_gamma = 2.2f,
        uint32_t measurement_points = 21);
    
    Core::Result<UniformityMeasurement> measure_uniformity(
        uint32_t grid_width = 9,
        uint32_t grid_height = 5);
    
    // Calibration correction generation
    Core::Result<CalibrationCorrection> generate_monitor_correction(
        const CalibrationMeasurementResult& measurement_result);
    
    Core::Result<void> apply_hardware_calibration(
        const CalibrationCorrection& correction,
        const MonitorSpecifications& monitor_specs);
    
    Core::Result<void> apply_software_calibration(
        const CalibrationCorrection& correction);
    
    // Validation and verification
    Core::Result<CalibrationMeasurementResult> validate_calibration(
        const CalibrationStandardSpec& target_standard,
        const CalibrationCorrection& applied_correction);
    
    Core::Result<bool> verify_calibration_stability(
        const CalibrationStandardSpec& target_standard,
        uint32_t measurement_interval_minutes = 30,
        uint32_t total_duration_hours = 8);
    
    // Quality assurance
    struct QualityAssessment {
        bool meets_broadcast_standards = false;
        bool meets_cinema_standards = false;
        bool meets_mastering_standards = false;
        
        float broadcast_compliance_score = 0.0f;    // 0-100
        float cinema_compliance_score = 0.0f;       // 0-100
        float mastering_compliance_score = 0.0f;    // 0-100
        
        std::vector<std::string> compliance_issues;
        std::vector<std::string> recommendations;
    };
    
    QualityAssessment assess_calibration_quality(
        const CalibrationMeasurementResult& measurement_result,
        DisplayPurpose intended_purpose) const;
    
    // Calibration maintenance
    Core::Result<void> schedule_recalibration_reminder(
        uint32_t interval_days = 30);
    
    Core::Result<std::vector<CalibrationMeasurementResult>> get_calibration_history(
        const std::string& monitor_serial) const;
    
    Core::Result<void> track_monitor_drift(
        const std::string& monitor_serial,
        const CalibrationStandardSpec& baseline_standard);
    
    // Ambient light compensation
    Core::Result<void> enable_ambient_light_compensation(bool enable);
    Core::Result<void> set_ambient_light_target(float target_lux);
    Core::Result<CalibrationCorrection> adjust_for_ambient_light(
        const CalibrationCorrection& base_correction,
        float current_ambient_lux);
    
    // Configuration and settings
    struct CalibrationSettings {
        uint32_t warmup_time_minutes = 30;          // Monitor warmup time
        uint32_t measurement_averaging = 5;         // Number of readings to average
        float measurement_timeout_seconds = 10.0f;  // Timeout per measurement
        bool auto_ambient_compensation = false;     // Automatic ambient adjustment
        bool save_measurement_history = true;       // Keep measurement records
        std::string default_probe_model;            // Default calibration probe
    };
    
    void set_calibration_settings(const CalibrationSettings& settings) { 
        settings_ = settings; 
    }
    const CalibrationSettings& get_calibration_settings() const { 
        return settings_; 
    }
    
private:
    bool probe_connected_ = false;
    std::string probe_model_;
    CalibrationSettings settings_;
    
    // Measurement automation
    Core::Result<CIEXYZ> measure_color_patch(float r, float g, float b,
                                            float measurement_time_seconds = 2.0f);
    
    Core::Result<float> measure_luminance(float gray_level,
                                         float measurement_time_seconds = 2.0f);
    
    Core::Result<CIExyY> measure_chromaticity(float r, float g, float b,
                                             float measurement_time_seconds = 2.0f);
    
    // Pattern generation
    Core::Result<void> display_color_patch(float r, float g, float b);
    Core::Result<void> display_gray_patch(float gray_level);
    Core::Result<void> display_uniformity_pattern(uint32_t grid_x, uint32_t grid_y,
                                                  uint32_t total_width, uint32_t total_height);
    
    // Hardware communication
    Core::Result<void> send_lut_to_monitor(const std::vector<float>& lut_data,
                                          const MonitorSpecifications& monitor_specs);
    
    Core::Result<MonitorSpecifications> query_monitor_capabilities();
    
    // Calibration algorithms
    CalibrationCorrection calculate_matrix_correction(
        const ColorAccuracyMeasurement& color_measurement,
        const CalibrationStandardSpec& target) const;
    
    CalibrationCorrection calculate_lut_correction(
        const CalibrationMeasurementResult& full_measurement,
        const CalibrationStandardSpec& target) const;
    
    CalibrationCorrection optimize_correction_iterative(
        const CalibrationMeasurementResult& measurement,
        const CalibrationStandardSpec& target,
        uint32_t max_iterations = 10) const;
    
    // Quality metrics
    float calculate_compliance_score(const CalibrationMeasurementResult& measurement,
                                   CalibrationStandard standard) const;
    
    std::vector<std::string> identify_compliance_issues(
        const CalibrationMeasurementResult& measurement,
        CalibrationStandard standard) const;
    
    // Data persistence
    Core::Result<void> save_measurement_result(const CalibrationMeasurementResult& result);
    Core::Result<CalibrationMeasurementResult> load_measurement_result(const std::string& filename);
    
    // Utility functions
    bool is_within_tolerance(float measured, float target, float tolerance) const;
    float calculate_delta_uv(const CIExyY& measured, const CIExyY& target) const;
    std::string generate_measurement_id() const;
    std::string format_measurement_conditions() const;
};

// =============================================================================
// Professional Monitor Profiles
// =============================================================================

struct ProfessionalMonitorProfile {
    std::string profile_name;
    MonitorSpecifications monitor_specs;
    CalibrationStandardSpec calibration_standard;
    CalibrationCorrection correction_data;
    
    // Validation data
    CalibrationMeasurementResult validation_measurement;
    std::chrono::system_clock::time_point last_calibrated;
    std::chrono::system_clock::time_point next_calibration_due;
    
    // Usage tracking
    uint64_t hours_in_use = 0;
    uint32_t calibration_cycles = 0;
    float stability_score = 0.0f;          // How stable the calibration remains
    
    bool is_valid() const noexcept;
    bool needs_recalibration() const noexcept;
    std::string get_status_summary() const;
};

class MonitorProfileManager {
public:
    explicit MonitorProfileManager() = default;
    ~MonitorProfileManager() = default;
    
    // Profile management
    Core::Result<void> create_monitor_profile(const ProfessionalMonitorProfile& profile);
    Core::Result<void> update_monitor_profile(const std::string& profile_name,
                                             const ProfessionalMonitorProfile& profile);
    Core::Result<void> delete_monitor_profile(const std::string& profile_name);
    
    Core::Result<ProfessionalMonitorProfile> get_monitor_profile(const std::string& profile_name) const;
    std::vector<std::string> list_monitor_profiles() const;
    
    // Active profile management
    Core::Result<void> activate_monitor_profile(const std::string& profile_name);
    std::string get_active_profile_name() const { return active_profile_name_; }
    Core::Result<ProfessionalMonitorProfile> get_active_profile() const;
    
    // Profile validation and maintenance
    Core::Result<std::vector<std::string>> check_profiles_needing_recalibration() const;
    Core::Result<void> update_profile_usage_hours(const std::string& profile_name, uint64_t hours);
    Core::Result<void> record_calibration_event(const std::string& profile_name,
                                               const CalibrationMeasurementResult& result);
    
    // Import/Export
    Core::Result<void> export_profile(const std::string& profile_name, const std::string& filename) const;
    Core::Result<void> import_profile(const std::string& filename);
    Core::Result<void> export_all_profiles(const std::string& directory) const;
    
private:
    std::map<std::string, ProfessionalMonitorProfile> profiles_;
    std::string active_profile_name_;
    
    // Profile persistence
    Core::Result<void> save_profiles_to_disk() const;
    Core::Result<void> load_profiles_from_disk();
    
    std::string get_profiles_directory() const;
    std::string get_profile_filename(const std::string& profile_name) const;
};

} // namespace VideoEditor::GFX
