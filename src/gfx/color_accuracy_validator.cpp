// Week 9: Color Accuracy Validator Implementation
// Complete implementation of professional color accuracy measurement and validation

#include "gfx/color_accuracy_validator.hpp"
#include "core/logger.hpp"
#include "core/exceptions.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace VideoEditor::GFX {

// =============================================================================
// CIE Color Space Conversion Functions
// =============================================================================

namespace {
    // CIE standard illuminants
    constexpr CIExyY D65_WHITE_POINT = {0.31271f, 0.32902f, 1.0f};
    constexpr CIExyY D50_WHITE_POINT = {0.34567f, 0.35850f, 1.0f};
    
    // XYZ to RGB matrices (sRGB/Rec.709)
    constexpr ColorMatrix3x3 XYZ_TO_SRGB = {{
        {{ 3.2406f, -1.5372f, -0.4986f}},
        {{-0.9689f,  1.8758f,  0.0415f}},
        {{ 0.0557f, -0.2040f,  1.0570f}}
    }};
    
    constexpr ColorMatrix3x3 SRGB_TO_XYZ = {{
        {{0.4124f, 0.3576f, 0.1805f}},
        {{0.2126f, 0.7152f, 0.0722f}},
        {{0.0193f, 0.1192f, 0.9505f}}
    }};
    
    // Gamma correction functions
    float linear_to_srgb_gamma(float linear) {
        if (linear <= 0.0031308f) {
            return 12.92f * linear;
        } else {
            return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
        }
    }
    
    float srgb_gamma_to_linear(float gamma) {
        if (gamma <= 0.04045f) {
            return gamma / 12.92f;
        } else {
            return std::pow((gamma + 0.055f) / 1.055f, 2.4f);
        }
    }
}

CIEColor convert_xyz_to_lab(const CIEColor& xyz, const CIExyY& white_point) {
    // Convert white point to XYZ
    float Yn = white_point.Y;
    float Xn = white_point.x * Yn / white_point.y;
    float Zn = (1.0f - white_point.x - white_point.y) * Yn / white_point.y;
    
    // Normalize XYZ by white point
    float fx = xyz.X / Xn;
    float fy = xyz.Y / Yn;
    float fz = xyz.Z / Zn;
    
    // Apply LAB transformation
    auto f = [](float t) -> float {
        const float delta = 6.0f / 29.0f;
        if (t > delta * delta * delta) {
            return std::pow(t, 1.0f / 3.0f);
        } else {
            return t / (3.0f * delta * delta) + 4.0f / 29.0f;
        }
    };
    
    fx = f(fx);
    fy = f(fy);
    fz = f(fz);
    
    CIEColor lab;
    lab.L = 116.0f * fy - 16.0f;
    lab.a = 500.0f * (fx - fy);
    lab.b = 200.0f * (fy - fz);
    
    return lab;
}

CIEColor convert_lab_to_xyz(const CIEColor& lab, const CIExyY& white_point) {
    // Convert white point to XYZ
    float Yn = white_point.Y;
    float Xn = white_point.x * Yn / white_point.y;
    float Zn = (1.0f - white_point.x - white_point.y) * Yn / white_point.y;
    
    float fy = (lab.L + 16.0f) / 116.0f;
    float fx = lab.a / 500.0f + fy;
    float fz = fy - lab.b / 200.0f;
    
    auto f_inv = [](float t) -> float {
        const float delta = 6.0f / 29.0f;
        if (t > delta) {
            return t * t * t;
        } else {
            return 3.0f * delta * delta * (t - 4.0f / 29.0f);
        }
    };
    
    CIEColor xyz;
    xyz.X = Xn * f_inv(fx);
    xyz.Y = Yn * f_inv(fy);
    xyz.Z = Zn * f_inv(fz);
    
    return xyz;
}

CIExyY convert_xyz_to_xyy(const CIEColor& xyz) {
    float sum = xyz.X + xyz.Y + xyz.Z;
    if (sum == 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    
    return {xyz.X / sum, xyz.Y / sum, xyz.Y};
}

CIEColor convert_xyy_to_xyz(const CIExyY& xyy) {
    if (xyy.y == 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    
    CIEColor xyz;
    xyz.Y = xyy.Y;
    xyz.X = xyy.x * xyy.Y / xyy.y;
    xyz.Z = (1.0f - xyy.x - xyy.y) * xyy.Y / xyy.y;
    
    return xyz;
}

CIEColor convert_rgb_to_xyz(const RGB& rgb, const ColorMatrix3x3& matrix) {
    // Apply gamma correction
    float r_linear = srgb_gamma_to_linear(rgb.r);
    float g_linear = srgb_gamma_to_linear(rgb.g);
    float b_linear = srgb_gamma_to_linear(rgb.b);
    
    // Matrix multiplication
    CIEColor xyz;
    xyz.X = matrix.m[0][0] * r_linear + matrix.m[0][1] * g_linear + matrix.m[0][2] * b_linear;
    xyz.Y = matrix.m[1][0] * r_linear + matrix.m[1][1] * g_linear + matrix.m[1][2] * b_linear;
    xyz.Z = matrix.m[2][0] * r_linear + matrix.m[2][1] * g_linear + matrix.m[2][2] * b_linear;
    
    return xyz;
}

RGB convert_xyz_to_rgb(const CIEColor& xyz, const ColorMatrix3x3& matrix) {
    // Matrix multiplication
    float r_linear = matrix.m[0][0] * xyz.X + matrix.m[0][1] * xyz.Y + matrix.m[0][2] * xyz.Z;
    float g_linear = matrix.m[1][0] * xyz.X + matrix.m[1][1] * xyz.Y + matrix.m[1][2] * xyz.Z;
    float b_linear = matrix.m[2][0] * xyz.X + matrix.m[2][1] * xyz.Y + matrix.m[2][2] * xyz.Z;
    
    // Apply gamma correction
    RGB rgb;
    rgb.r = linear_to_srgb_gamma(std::max(0.0f, std::min(1.0f, r_linear)));
    rgb.g = linear_to_srgb_gamma(std::max(0.0f, std::min(1.0f, g_linear)));
    rgb.b = linear_to_srgb_gamma(std::max(0.0f, std::min(1.0f, b_linear)));
    
    return rgb;
}

// =============================================================================
// Delta E Calculation Functions
// =============================================================================

float calculate_delta_e_1976(const CIEColor& lab1, const CIEColor& lab2) {
    float dL = lab1.L - lab2.L;
    float da = lab1.a - lab2.a;
    float db = lab1.b - lab2.b;
    
    return std::sqrt(dL * dL + da * da + db * db);
}

float calculate_delta_e_1994(const CIEColor& lab1, const CIEColor& lab2, 
                             float kL, float kC, float kH) {
    float dL = lab1.L - lab2.L;
    float da = lab1.a - lab2.a;
    float db = lab1.b - lab2.b;
    
    float C1 = std::sqrt(lab1.a * lab1.a + lab1.b * lab1.b);
    float C2 = std::sqrt(lab2.a * lab2.a + lab2.b * lab2.b);
    float dC = C1 - C2;
    
    float dH_squared = da * da + db * db - dC * dC;
    float dH = (dH_squared > 0) ? std::sqrt(dH_squared) : 0.0f;
    
    float SL = 1.0f;
    float SC = 1.0f + 0.045f * C1;
    float SH = 1.0f + 0.015f * C1;
    
    float dL_term = dL / (kL * SL);
    float dC_term = dC / (kC * SC);
    float dH_term = dH / (kH * SH);
    
    return std::sqrt(dL_term * dL_term + dC_term * dC_term + dH_term * dH_term);
}

float calculate_delta_e_2000(const CIEColor& lab1, const CIEColor& lab2, 
                             float kL, float kC, float kH) {
    float L1 = lab1.L, a1 = lab1.a, b1 = lab1.b;
    float L2 = lab2.L, a2 = lab2.a, b2 = lab2.b;
    
    float C1 = std::sqrt(a1 * a1 + b1 * b1);
    float C2 = std::sqrt(a2 * a2 + b2 * b2);
    float C_bar = (C1 + C2) / 2.0f;
    
    float G = 0.5f * (1.0f - std::sqrt(std::pow(C_bar, 7) / (std::pow(C_bar, 7) + std::pow(25.0f, 7))));
    
    float a1_prime = (1.0f + G) * a1;
    float a2_prime = (1.0f + G) * a2;
    
    float C1_prime = std::sqrt(a1_prime * a1_prime + b1 * b1);
    float C2_prime = std::sqrt(a2_prime * a2_prime + b2 * b2);
    
    float h1_prime = std::atan2(b1, a1_prime) * 180.0f / M_PI;
    float h2_prime = std::atan2(b2, a2_prime) * 180.0f / M_PI;
    
    if (h1_prime < 0) h1_prime += 360.0f;
    if (h2_prime < 0) h2_prime += 360.0f;
    
    float dL_prime = L2 - L1;
    float dC_prime = C2_prime - C1_prime;
    
    float dh_prime;
    if (C1_prime * C2_prime == 0) {
        dh_prime = 0;
    } else if (std::abs(h2_prime - h1_prime) <= 180.0f) {
        dh_prime = h2_prime - h1_prime;
    } else if (h2_prime - h1_prime > 180.0f) {
        dh_prime = h2_prime - h1_prime - 360.0f;
    } else {
        dh_prime = h2_prime - h1_prime + 360.0f;
    }
    
    float dH_prime = 2.0f * std::sqrt(C1_prime * C2_prime) * std::sin(dh_prime * M_PI / 360.0f);
    
    float L_bar_prime = (L1 + L2) / 2.0f;
    float C_bar_prime = (C1_prime + C2_prime) / 2.0f;
    
    float h_bar_prime;
    if (C1_prime * C2_prime == 0) {
        h_bar_prime = h1_prime + h2_prime;
    } else if (std::abs(h1_prime - h2_prime) <= 180.0f) {
        h_bar_prime = (h1_prime + h2_prime) / 2.0f;
    } else if (std::abs(h1_prime - h2_prime) > 180.0f && (h1_prime + h2_prime) < 360.0f) {
        h_bar_prime = (h1_prime + h2_prime + 360.0f) / 2.0f;
    } else {
        h_bar_prime = (h1_prime + h2_prime - 360.0f) / 2.0f;
    }
    
    float T = 1.0f - 0.17f * std::cos((h_bar_prime - 30.0f) * M_PI / 180.0f) +
              0.24f * std::cos(2.0f * h_bar_prime * M_PI / 180.0f) +
              0.32f * std::cos((3.0f * h_bar_prime + 6.0f) * M_PI / 180.0f) -
              0.20f * std::cos((4.0f * h_bar_prime - 63.0f) * M_PI / 180.0f);
    
    float dTheta = 30.0f * std::exp(-std::pow((h_bar_prime - 275.0f) / 25.0f, 2));
    
    float RC = 2.0f * std::sqrt(std::pow(C_bar_prime, 7) / (std::pow(C_bar_prime, 7) + std::pow(25.0f, 7)));
    
    float SL = 1.0f + (0.015f * std::pow(L_bar_prime - 50.0f, 2)) / 
               std::sqrt(20.0f + std::pow(L_bar_prime - 50.0f, 2));
    float SC = 1.0f + 0.045f * C_bar_prime;
    float SH = 1.0f + 0.015f * C_bar_prime * T;
    
    float RT = -std::sin(2.0f * dTheta * M_PI / 180.0f) * RC;
    
    float dL_term = dL_prime / (kL * SL);
    float dC_term = dC_prime / (kC * SC);
    float dH_term = dH_prime / (kH * SH);
    
    return std::sqrt(dL_term * dL_term + dC_term * dC_term + dH_term * dH_term + 
                     RT * dC_term * dH_term);
}

// =============================================================================
// Color Checker Implementation
// =============================================================================

std::vector<ColorPatchReference> ColorChecker::create_x_rite_color_checker() {
    std::vector<ColorPatchReference> patches;
    patches.reserve(24);
    
    // X-Rite ColorChecker reference values (CIE LAB under D65)
    std::vector<std::tuple<std::string, float, float, float>> reference_data = {
        {"Dark Skin", 37.99f, 13.56f, 14.06f},
        {"Light Skin", 65.71f, 18.13f, 17.81f},
        {"Blue Sky", 49.93f, -4.88f, -21.93f},
        {"Foliage", 43.14f, -13.10f, 21.61f},
        {"Blue Flower", 55.11f, 8.84f, -25.40f},
        {"Bluish Green", 70.72f, -33.40f, -0.20f},
        {"Orange", 62.66f, 36.07f, 57.10f},
        {"Purplish Blue", 40.02f, 10.41f, -45.96f},
        {"Moderate Red", 51.12f, 48.24f, 16.25f},
        {"Purple", 30.32f, 22.98f, -21.59f},
        {"Yellow Green", 72.53f, -23.71f, 57.26f},
        {"Orange Yellow", 71.94f, 19.36f, 67.86f},
        {"Blue", 28.78f, 14.18f, -50.30f},
        {"Green", 55.26f, -38.34f, 31.37f},
        {"Red", 42.10f, 53.38f, 28.19f},
        {"Yellow", 81.73f, 4.04f, 79.82f},
        {"Magenta", 51.94f, 49.99f, -14.57f},
        {"Cyan", 51.04f, -28.63f, -28.71f},
        {"White 9.5", 96.54f, -0.43f, 1.19f},
        {"Neutral 8", 81.26f, -0.64f, -0.34f},
        {"Neutral 6.5", 66.77f, -0.73f, -0.50f},
        {"Neutral 5", 50.87f, -0.15f, -0.27f},
        {"Neutral 3.5", 35.66f, -0.42f, -1.23f},
        {"Black 2", 20.46f, -0.08f, -0.97f}
    };
    
    for (size_t i = 0; i < reference_data.size(); ++i) {
        ColorPatchReference patch;
        patch.name = std::get<0>(reference_data[i]);
        patch.patch_id = static_cast<int>(i + 1);
        patch.reference_lab.L = std::get<1>(reference_data[i]);
        patch.reference_lab.a = std::get<2>(reference_data[i]);
        patch.reference_lab.b = std::get<3>(reference_data[i]);
        patch.reference_xyz = convert_lab_to_xyz(patch.reference_lab, D65_WHITE_POINT);
        patch.tolerance_delta_e = 2.0f; // Typical tolerance for color matching
        
        patches.push_back(patch);
    }
    
    return patches;
}

std::vector<ColorPatchReference> ColorChecker::create_it8_chart() {
    std::vector<ColorPatchReference> patches;
    
    // IT8.7/4 standard reference chart (simplified version)
    // Full implementation would include all 264 patches
    
    // Create a subset of representative patches
    std::vector<std::tuple<std::string, float, float, float>> reference_data = {
        // Primary colors
        {"Red Primary", 25.43f, 67.99f, 55.33f},
        {"Green Primary", 46.23f, -51.70f, 49.90f},
        {"Blue Primary", 25.64f, 19.29f, -57.42f},
        {"Cyan Primary", 54.01f, -37.00f, -40.49f},
        {"Magenta Primary", 42.24f, 58.93f, -10.89f},
        {"Yellow Primary", 77.27f, -5.04f, 78.84f},
        
        // Neutral patches
        {"White", 95.05f, -0.17f, 2.25f},
        {"Light Gray", 81.29f, -0.64f, -0.34f},
        {"Medium Gray", 59.06f, -0.31f, -0.40f},
        {"Dark Gray", 36.54f, -0.31f, -1.24f},
        {"Black", 16.44f, -0.06f, -1.30f},
        
        // Skin tones
        {"Light Skin", 72.73f, 2.90f, 15.09f},
        {"Medium Skin", 54.38f, 9.67f, 18.83f},
        {"Dark Skin", 31.29f, 6.24f, 12.85f}
    };
    
    for (size_t i = 0; i < reference_data.size(); ++i) {
        ColorPatchReference patch;
        patch.name = std::get<0>(reference_data[i]);
        patch.patch_id = static_cast<int>(i + 1);
        patch.reference_lab.L = std::get<1>(reference_data[i]);
        patch.reference_lab.a = std::get<2>(reference_data[i]);
        patch.reference_lab.b = std::get<3>(reference_data[i]);
        patch.reference_xyz = convert_lab_to_xyz(patch.reference_lab, D65_WHITE_POINT);
        patch.tolerance_delta_e = 1.5f; // Tighter tolerance for IT8
        
        patches.push_back(patch);
    }
    
    return patches;
}

// =============================================================================
// Color Accuracy Validator Implementation
// =============================================================================

ColorAccuracyValidator::ColorAccuracyValidator() {
    // Initialize default color checker
    color_checker_patches_ = ColorChecker::create_x_rite_color_checker();
    
    // Initialize default measurement settings
    measurement_settings_.delta_e_formula = DeltaEFormula::CIE_2000;
    measurement_settings_.illuminant = D65_WHITE_POINT;
    measurement_settings_.observer_angle = ObserverAngle::DEGREE_2;
    measurement_settings_.measurement_geometry = MeasurementGeometry::SPHERE_D8;
    measurement_settings_.num_measurements = 5;
    measurement_settings_.stabilization_time_ms = 1000;
}

Core::Result<ColorMeasurement> ColorAccuracyValidator::measure_color_patch(
    const RGB& measured_rgb, int patch_id) const {
    
    try {
        // Find reference patch
        auto patch_it = std::find_if(color_checker_patches_.begin(), color_checker_patches_.end(),
            [patch_id](const ColorPatchReference& patch) { return patch.patch_id == patch_id; });
        
        if (patch_it == color_checker_patches_.end()) {
            return Core::Result<ColorMeasurement>::error("Patch ID not found");
        }
        
        const ColorPatchReference& reference = *patch_it;
        
        // Convert measured RGB to CIE LAB
        CIEColor measured_xyz = convert_rgb_to_xyz(measured_rgb, SRGB_TO_XYZ);
        CIEColor measured_lab = convert_xyz_to_lab(measured_xyz, measurement_settings_.illuminant);
        
        // Calculate Delta E based on selected formula
        float delta_e = 0.0f;
        switch (measurement_settings_.delta_e_formula) {
            case DeltaEFormula::CIE_1976:
                delta_e = calculate_delta_e_1976(measured_lab, reference.reference_lab);
                break;
            case DeltaEFormula::CIE_1994:
                delta_e = calculate_delta_e_1994(measured_lab, reference.reference_lab);
                break;
            case DeltaEFormula::CIE_2000:
                delta_e = calculate_delta_e_2000(measured_lab, reference.reference_lab);
                break;
        }
        
        // Create measurement result
        ColorMeasurement measurement;
        measurement.patch_id = patch_id;
        measurement.patch_name = reference.name;
        measurement.measured_rgb = measured_rgb;
        measurement.measured_lab = measured_lab;
        measurement.measured_xyz = measured_xyz;
        measurement.reference_lab = reference.reference_lab;
        measurement.reference_xyz = reference.reference_xyz;
        measurement.delta_e = delta_e;
        measurement.is_within_tolerance = (delta_e <= reference.tolerance_delta_e);
        measurement.measurement_timestamp = std::chrono::steady_clock::now();
        
        return Core::Result<ColorMeasurement>::success(measurement);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Color measurement failed: {}", e.what());
        return Core::Result<ColorMeasurement>::error("Color measurement failed");
    }
}

Core::Result<ColorAccuracyReport> ColorAccuracyValidator::validate_color_accuracy(
    const std::vector<RGB>& measured_colors) const {
    
    try {
        if (measured_colors.size() != color_checker_patches_.size()) {
            return Core::Result<ColorAccuracyReport>::error(
                "Number of measured colors does not match color checker patches");
        }
        
        ColorAccuracyReport report;
        report.measurement_settings = measurement_settings_;
        report.total_patches = static_cast<int>(measured_colors.size());
        report.measurement_timestamp = std::chrono::steady_clock::now();
        
        std::vector<float> delta_e_values;
        delta_e_values.reserve(measured_colors.size());
        
        // Measure each patch
        for (size_t i = 0; i < measured_colors.size(); ++i) {
            auto measurement_result = measure_color_patch(measured_colors[i], 
                                                         color_checker_patches_[i].patch_id);
            
            if (measurement_result.is_error()) {
                return Core::Result<ColorAccuracyReport>::error(
                    "Failed to measure patch " + std::to_string(i + 1));
            }
            
            ColorMeasurement measurement = measurement_result.value();
            report.measurements.push_back(measurement);
            delta_e_values.push_back(measurement.delta_e);
            
            if (measurement.is_within_tolerance) {
                report.patches_within_tolerance++;
            }
        }
        
        // Calculate statistics
        if (!delta_e_values.empty()) {
            report.average_delta_e = std::accumulate(delta_e_values.begin(), 
                                                   delta_e_values.end(), 0.0f) / delta_e_values.size();
            
            auto minmax = std::minmax_element(delta_e_values.begin(), delta_e_values.end());
            report.min_delta_e = *minmax.first;
            report.max_delta_e = *minmax.second;
            
            // Calculate standard deviation
            float variance = 0.0f;
            for (float delta_e : delta_e_values) {
                variance += (delta_e - report.average_delta_e) * (delta_e - report.average_delta_e);
            }
            report.standard_deviation = std::sqrt(variance / delta_e_values.size());
            
            // Calculate percentile values
            std::vector<float> sorted_values = delta_e_values;
            std::sort(sorted_values.begin(), sorted_values.end());
            
            size_t p90_index = static_cast<size_t>(sorted_values.size() * 0.90);
            size_t p95_index = static_cast<size_t>(sorted_values.size() * 0.95);
            
            report.delta_e_90th_percentile = sorted_values[std::min(p90_index, sorted_values.size() - 1)];
            report.delta_e_95th_percentile = sorted_values[std::min(p95_index, sorted_values.size() - 1)];
        }
        
        // Determine overall accuracy grade
        report.accuracy_grade = determine_accuracy_grade(report);
        
        return Core::Result<ColorAccuracyReport>::success(report);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Color accuracy validation failed: {}", e.what());
        return Core::Result<ColorAccuracyReport>::error("Color accuracy validation failed");
    }
}

AccuracyGrade ColorAccuracyValidator::determine_accuracy_grade(const ColorAccuracyReport& report) const {
    // Industry standard grading based on average Delta E
    if (report.average_delta_e <= 1.0f) {
        return AccuracyGrade::EXCELLENT;
    } else if (report.average_delta_e <= 2.0f) {
        return AccuracyGrade::GOOD;
    } else if (report.average_delta_e <= 3.0f) {
        return AccuracyGrade::ACCEPTABLE;
    } else if (report.average_delta_e <= 5.0f) {
        return AccuracyGrade::POOR;
    } else {
        return AccuracyGrade::UNACCEPTABLE;
    }
}

Core::Result<void> ColorAccuracyValidator::load_color_checker_reference(
    const std::string& file_path) {
    
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return Core::Result<void>::error("Failed to open reference file");
        }
        
        color_checker_patches_.clear();
        std::string line;
        
        // Skip header line
        std::getline(file, line);
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string token;
            
            ColorPatchReference patch;
            
            // Parse CSV format: ID,Name,L,a,b,tolerance
            if (std::getline(iss, token, ',')) {
                patch.patch_id = std::stoi(token);
            }
            
            if (std::getline(iss, token, ',')) {
                patch.name = token;
            }
            
            if (std::getline(iss, token, ',')) {
                patch.reference_lab.L = std::stof(token);
            }
            
            if (std::getline(iss, token, ',')) {
                patch.reference_lab.a = std::stof(token);
            }
            
            if (std::getline(iss, token, ',')) {
                patch.reference_lab.b = std::stof(token);
            }
            
            if (std::getline(iss, token, ',')) {
                patch.tolerance_delta_e = std::stof(token);
            }
            
            // Convert LAB to XYZ
            patch.reference_xyz = convert_lab_to_xyz(patch.reference_lab, D65_WHITE_POINT);
            
            color_checker_patches_.push_back(patch);
        }
        
        LOG_INFO("Loaded {} color checker patches from {}", 
                color_checker_patches_.size(), file_path);
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load color checker reference: {}", e.what());
        return Core::Result<void>::error("Failed to load color checker reference");
    }
}

std::string ColorAccuracyValidator::generate_detailed_report(const ColorAccuracyReport& report) const {
    std::ostringstream oss;
    
    // Header
    oss << "======================================\n";
    oss << "     COLOR ACCURACY VALIDATION REPORT\n";
    oss << "======================================\n\n";
    
    // Summary
    oss << "SUMMARY:\n";
    oss << "--------\n";
    oss << "Total Patches: " << report.total_patches << "\n";
    oss << "Patches within tolerance: " << report.patches_within_tolerance 
        << " (" << std::fixed << std::setprecision(1) 
        << (100.0f * report.patches_within_tolerance / report.total_patches) << "%)\n";
    oss << "Overall Grade: " << accuracy_grade_to_string(report.accuracy_grade) << "\n\n";
    
    // Statistics
    oss << "DELTA E STATISTICS:\n";
    oss << "-------------------\n";
    oss << "Average: " << std::fixed << std::setprecision(2) << report.average_delta_e << "\n";
    oss << "Minimum: " << report.min_delta_e << "\n";
    oss << "Maximum: " << report.max_delta_e << "\n";
    oss << "Std Dev: " << report.standard_deviation << "\n";
    oss << "90th Percentile: " << report.delta_e_90th_percentile << "\n";
    oss << "95th Percentile: " << report.delta_e_95th_percentile << "\n\n";
    
    // Individual measurements
    oss << "INDIVIDUAL MEASUREMENTS:\n";
    oss << "------------------------\n";
    oss << std::left << std::setw(20) << "Patch Name" 
        << std::setw(8) << "Delta E" 
        << std::setw(8) << "Pass" 
        << "RGB (Measured)" << "\n";
    oss << std::string(60, '-') << "\n";
    
    for (const auto& measurement : report.measurements) {
        oss << std::left << std::setw(20) << measurement.patch_name
            << std::setw(8) << std::fixed << std::setprecision(2) << measurement.delta_e
            << std::setw(8) << (measurement.is_within_tolerance ? "PASS" : "FAIL")
            << "(" << std::setprecision(3) << measurement.measured_rgb.r 
            << ", " << measurement.measured_rgb.g 
            << ", " << measurement.measured_rgb.b << ")\n";
    }
    
    return oss.str();
}

std::string ColorAccuracyValidator::accuracy_grade_to_string(AccuracyGrade grade) const {
    switch (grade) {
        case AccuracyGrade::EXCELLENT: return "Excellent (ΔE ≤ 1.0)";
        case AccuracyGrade::GOOD: return "Good (ΔE ≤ 2.0)";
        case AccuracyGrade::ACCEPTABLE: return "Acceptable (ΔE ≤ 3.0)";
        case AccuracyGrade::POOR: return "Poor (ΔE ≤ 5.0)";
        case AccuracyGrade::UNACCEPTABLE: return "Unacceptable (ΔE > 5.0)";
        default: return "Unknown";
    }
}

// =============================================================================
// Calibration System Implementation
// =============================================================================

Core::Result<CalibrationLUT> CalibrationSystem::generate_3d_lut(
    const std::vector<ColorMeasurement>& measurements,
    int lut_size) const {
    
    try {
        CalibrationLUT lut;
        lut.size = lut_size;
        lut.lut_data.resize(lut_size * lut_size * lut_size * 3);
        
        // Initialize LUT with identity transformation
        for (int r = 0; r < lut_size; ++r) {
            for (int g = 0; g < lut_size; ++g) {
                for (int b = 0; b < lut_size; ++b) {
                    int index = ((r * lut_size + g) * lut_size + b) * 3;
                    
                    lut.lut_data[index + 0] = static_cast<float>(r) / (lut_size - 1);
                    lut.lut_data[index + 1] = static_cast<float>(g) / (lut_size - 1);
                    lut.lut_data[index + 2] = static_cast<float>(b) / (lut_size - 1);
                }
            }
        }
        
        // Apply corrections based on measurements
        for (const auto& measurement : measurements) {
            // Calculate correction factors
            RGB measured = measurement.measured_rgb;
            RGB reference = convert_xyz_to_rgb(measurement.reference_xyz, XYZ_TO_SRGB);
            
            RGB correction;
            correction.r = (measured.r != 0.0f) ? reference.r / measured.r : 1.0f;
            correction.g = (measured.g != 0.0f) ? reference.g / measured.g : 1.0f;
            correction.b = (measured.b != 0.0f) ? reference.b / measured.b : 1.0f;
            
            // Apply correction to nearby LUT entries
            apply_local_correction(lut, measured, correction, lut_size);
        }
        
        lut.creation_timestamp = std::chrono::steady_clock::now();
        
        return Core::Result<CalibrationLUT>::success(lut);
        
    } catch (const std::exception& e) {
        LOG_ERROR("3D LUT generation failed: {}", e.what());
        return Core::Result<CalibrationLUT>::error("3D LUT generation failed");
    }
}

void CalibrationSystem::apply_local_correction(CalibrationLUT& lut, const RGB& center, 
                                              const RGB& correction, int lut_size) const {
    
    const float influence_radius = 0.1f; // 10% of color space
    
    for (int r = 0; r < lut_size; ++r) {
        for (int g = 0; g < lut_size; ++g) {
            for (int b = 0; b < lut_size; ++b) {
                RGB lut_color;
                lut_color.r = static_cast<float>(r) / (lut_size - 1);
                lut_color.g = static_cast<float>(g) / (lut_size - 1);
                lut_color.b = static_cast<float>(b) / (lut_size - 1);
                
                // Calculate distance in RGB space
                float distance = std::sqrt(
                    (lut_color.r - center.r) * (lut_color.r - center.r) +
                    (lut_color.g - center.g) * (lut_color.g - center.g) +
                    (lut_color.b - center.b) * (lut_color.b - center.b)
                );
                
                if (distance <= influence_radius) {
                    // Calculate influence weight (gaussian falloff)
                    float weight = std::exp(-(distance * distance) / (2.0f * influence_radius * influence_radius * 0.25f));
                    
                    int index = ((r * lut_size + g) * lut_size + b) * 3;
                    
                    // Apply weighted correction
                    lut.lut_data[index + 0] *= (1.0f + weight * (correction.r - 1.0f));
                    lut.lut_data[index + 1] *= (1.0f + weight * (correction.g - 1.0f));
                    lut.lut_data[index + 2] *= (1.0f + weight * (correction.b - 1.0f));
                    
                    // Clamp values
                    lut.lut_data[index + 0] = std::max(0.0f, std::min(1.0f, lut.lut_data[index + 0]));
                    lut.lut_data[index + 1] = std::max(0.0f, std::min(1.0f, lut.lut_data[index + 1]));
                    lut.lut_data[index + 2] = std::max(0.0f, std::min(1.0f, lut.lut_data[index + 2]));
                }
            }
        }
    }
}

Core::Result<void> CalibrationSystem::export_icc_profile(
    const CalibrationLUT& lut, const std::string& file_path) const {
    
    try {
        // This is a simplified ICC profile export
        // Full implementation would require ICC profile specification compliance
        
        std::ofstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return Core::Result<void>::error("Failed to open ICC profile file for writing");
        }
        
        // Write simplified ICC header (128 bytes)
        std::vector<uint8_t> header(128, 0);
        
        // Profile size (will be updated)
        uint32_t profile_size = 128 + static_cast<uint32_t>(lut.lut_data.size() * sizeof(float));
        header[0] = (profile_size >> 24) & 0xFF;
        header[1] = (profile_size >> 16) & 0xFF;
        header[2] = (profile_size >> 8) & 0xFF;
        header[3] = profile_size & 0xFF;
        
        // CMM type
        header[4] = 'V'; header[5] = 'E'; header[6] = 'D'; header[7] = 'T';
        
        // Profile version
        header[8] = 4; header[9] = 3; // Version 4.3
        
        // Device class: display
        header[12] = 'm'; header[13] = 'n'; header[14] = 't'; header[15] = 'r';
        
        // Color space: RGB
        header[16] = 'R'; header[17] = 'G'; header[18] = 'B'; header[19] = ' ';
        
        // Connection space: XYZ
        header[20] = 'X'; header[21] = 'Y'; header[22] = 'Z'; header[23] = ' ';
        
        file.write(reinterpret_cast<const char*>(header.data()), header.size());
        
        // Write LUT data (simplified)
        file.write(reinterpret_cast<const char*>(lut.lut_data.data()), 
                  lut.lut_data.size() * sizeof(float));
        
        LOG_INFO("ICC profile exported to: {}", file_path);
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("ICC profile export failed: {}", e.what());
        return Core::Result<void>::error("ICC profile export failed");
    }
}

} // namespace VideoEditor::GFX
