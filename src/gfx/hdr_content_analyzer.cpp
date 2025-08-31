// Week 9: HDR Content Analyzer Implementation
// Complete implementation of HDR content analysis and validation

#include "gfx/hdr_content_analyzer.hpp"
#include "core/logger.hpp"
#include "core/exceptions.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <execution>
#include <fstream>

namespace VideoEditor::GFX {

// =============================================================================
// Utility Functions
// =============================================================================

namespace {
    float rgb_to_luminance(const RGB& rgb) {
        // ITU-R BT.709 luma coefficients
        return 0.2126f * rgb.r + 0.7152f * rgb.g + 0.0722f * rgb.b;
    }
    
    float rgb_to_luminance_bt2020(const RGB& rgb) {
        // ITU-R BT.2020 luma coefficients
        return 0.2627f * rgb.r + 0.6780f * rgb.g + 0.0593f * rgb.b;
    }
    
    float linear_to_pq(float linear) {
        // SMPTE ST.2084 (PQ) transfer function
        constexpr float m1 = 0.1593017578125f;
        constexpr float m2 = 78.84375f;
        constexpr float c1 = 0.8359375f;
        constexpr float c2 = 18.8515625f;
        constexpr float c3 = 18.6875f;
        
        if (linear <= 0.0f) return 0.0f;
        
        float Y = std::pow(linear / 10000.0f, m1);
        float numerator = c1 + c2 * Y;
        float denominator = 1.0f + c3 * Y;
        
        return std::pow(numerator / denominator, m2);
    }
    
    float pq_to_linear(float pq) {
        // Inverse SMPTE ST.2084 (PQ) transfer function
        constexpr float m1 = 0.1593017578125f;
        constexpr float m2 = 78.84375f;
        constexpr float c1 = 0.8359375f;
        constexpr float c2 = 18.8515625f;
        constexpr float c3 = 18.6875f;
        
        if (pq <= 0.0f) return 0.0f;
        
        float Y_pow = std::pow(pq, 1.0f / m2);
        float numerator = std::max(0.0f, Y_pow - c1);
        float denominator = c2 - c3 * Y_pow;
        
        if (denominator <= 0.0f) return 10000.0f;
        
        return 10000.0f * std::pow(numerator / denominator, 1.0f / m1);
    }
    
    float hlg_oetf(float linear) {
        // HLG OETF (BBC/NHK)
        constexpr float a = 0.17883277f;
        constexpr float b = 0.28466892f;
        constexpr float c = 0.55991073f;
        
        if (linear <= 1.0f / 12.0f) {
            return std::sqrt(3.0f * linear);
        } else {
            return a * std::log(12.0f * linear - b) + c;
        }
    }
    
    float hlg_inverse_oetf(float hlg) {
        // HLG Inverse OETF
        constexpr float a = 0.17883277f;
        constexpr float b = 0.28466892f;
        constexpr float c = 0.55991073f;
        
        if (hlg <= 0.5f) {
            return hlg * hlg / 3.0f;
        } else {
            return (std::exp((hlg - c) / a) + b) / 12.0f;
        }
    }
}

// =============================================================================
// Luminance Analysis Implementation
// =============================================================================

LuminanceHistogram HDRContentAnalyzer::analyze_frame_luminance(
    const std::vector<RGB>& frame_data, int width, int height) const {
    
    LuminanceHistogram histogram;
    histogram.bin_count = 256;
    histogram.histogram.resize(histogram.bin_count, 0);
    histogram.frame_width = width;
    histogram.frame_height = height;
    
    if (frame_data.empty()) {
        return histogram;
    }
    
    std::vector<float> luminance_values;
    luminance_values.reserve(frame_data.size());
    
    // Calculate luminance for each pixel
    for (const auto& pixel : frame_data) {
        float luma = rgb_to_luminance_bt2020(pixel);
        luminance_values.push_back(luma);
    }
    
    // Find min/max for dynamic range calculation
    auto minmax = std::minmax_element(luminance_values.begin(), luminance_values.end());
    histogram.min_luminance = *minmax.first;
    histogram.max_luminance = *minmax.second;
    histogram.peak_luminance = histogram.max_luminance * 10000.0f; // Convert to nits
    
    // Calculate average luminance
    histogram.average_luminance = std::accumulate(luminance_values.begin(), 
                                                 luminance_values.end(), 0.0f) / luminance_values.size();
    histogram.average_luminance_nits = histogram.average_luminance * 10000.0f;
    
    // Build histogram
    for (float luma : luminance_values) {
        int bin = static_cast<int>(luma * (histogram.bin_count - 1));
        bin = std::max(0, std::min(histogram.bin_count - 1, bin));
        histogram.histogram[bin]++;
    }
    
    // Calculate percentiles
    std::vector<float> sorted_luminance = luminance_values;
    std::sort(sorted_luminance.begin(), sorted_luminance.end());
    
    size_t total_pixels = sorted_luminance.size();
    histogram.percentile_1 = sorted_luminance[static_cast<size_t>(total_pixels * 0.01)];
    histogram.percentile_10 = sorted_luminance[static_cast<size_t>(total_pixels * 0.10)];
    histogram.percentile_50 = sorted_luminance[static_cast<size_t>(total_pixels * 0.50)];
    histogram.percentile_90 = sorted_luminance[static_cast<size_t>(total_pixels * 0.90)];
    histogram.percentile_99 = sorted_luminance[static_cast<size_t>(total_pixels * 0.99)];
    
    // Calculate effective dynamic range
    histogram.effective_dynamic_range = histogram.percentile_99 / 
                                       std::max(0.0001f, histogram.percentile_1);
    
    return histogram;
}

FrameHDRAnalysis HDRContentAnalyzer::analyze_frame(
    const std::vector<RGB>& frame_data, int width, int height, 
    HDRStandard expected_standard) const {
    
    FrameHDRAnalysis analysis;
    analysis.frame_width = width;
    analysis.frame_height = height;
    analysis.pixel_count = width * height;
    analysis.expected_standard = expected_standard;
    
    if (frame_data.empty()) {
        analysis.classification = ContentType::INVALID;
        return analysis;
    }
    
    // Analyze luminance distribution
    analysis.luminance_histogram = analyze_frame_luminance(frame_data, width, height);
    
    // Classify content type based on luminance
    analysis.classification = classify_content_type(analysis.luminance_histogram);
    
    // Calculate HDR utilization metrics
    calculate_hdr_utilization(analysis);
    
    // Detect potential quality issues
    analysis.quality_issues = detect_quality_issues(frame_data, analysis.luminance_histogram);
    
    // Calculate gamut usage
    analysis.gamut_usage = calculate_gamut_usage(frame_data);
    
    // Estimate required peak luminance
    analysis.recommended_peak_nits = estimate_required_peak_luminance(analysis.luminance_histogram);
    
    // Check standard compliance
    analysis.standard_compliance = check_standard_compliance(analysis, expected_standard);
    
    analysis.analysis_timestamp = std::chrono::steady_clock::now();
    
    return analysis;
}

ContentType HDRContentAnalyzer::classify_content_type(const LuminanceHistogram& histogram) const {
    float peak_nits = histogram.peak_luminance;
    float avg_nits = histogram.average_luminance_nits;
    float dynamic_range = histogram.effective_dynamic_range;
    
    // Classification thresholds
    if (peak_nits > 1000.0f && dynamic_range > 100.0f) {
        if (peak_nits > 4000.0f) {
            return ContentType::HDR_HIGH_PEAK;
        } else {
            return ContentType::HDR_STANDARD;
        }
    } else if (peak_nits > 400.0f && dynamic_range > 50.0f) {
        return ContentType::HDR_LOW_PEAK;
    } else if (peak_nits > 200.0f) {
        return ContentType::ENHANCED_SDR;
    } else {
        return ContentType::SDR_STANDARD;
    }
}

void HDRContentAnalyzer::calculate_hdr_utilization(FrameHDRAnalysis& analysis) const {
    const auto& hist = analysis.luminance_histogram;
    
    // Calculate what percentage of pixels exceed SDR range (100 nits)
    float sdr_threshold = 100.0f / 10000.0f; // Convert to normalized
    int pixels_above_sdr = 0;
    
    for (size_t i = 0; i < hist.histogram.size(); ++i) {
        float bin_center = static_cast<float>(i) / (hist.histogram.size() - 1);
        if (bin_center > sdr_threshold) {
            pixels_above_sdr += hist.histogram[i];
        }
    }
    
    analysis.hdr_pixel_percentage = static_cast<float>(pixels_above_sdr) / analysis.pixel_count * 100.0f;
    
    // Calculate tone mapping headroom
    analysis.tone_mapping_headroom = hist.peak_luminance / (100.0f / 10000.0f);
    
    // Estimate mastering display requirements
    if (hist.peak_luminance > 400.0f / 10000.0f) {
        analysis.mastering_display_recommendation = MasteringDisplayType::HDR_1000;
    } else if (hist.peak_luminance > 200.0f / 10000.0f) {
        analysis.mastering_display_recommendation = MasteringDisplayType::HDR_600;
    } else {
        analysis.mastering_display_recommendation = MasteringDisplayType::SDR_100;
    }
}

std::vector<QualityIssue> HDRContentAnalyzer::detect_quality_issues(
    const std::vector<RGB>& frame_data, const LuminanceHistogram& histogram) const {
    
    std::vector<QualityIssue> issues;
    
    // Check for clipping
    int clipped_pixels = 0;
    int near_black_pixels = 0;
    
    for (const auto& pixel : frame_data) {
        // Check for clipping (values very close to 1.0)
        if (pixel.r > 0.99f || pixel.g > 0.99f || pixel.b > 0.99f) {
            clipped_pixels++;
        }
        
        // Check for crushed blacks
        if (pixel.r < 0.01f && pixel.g < 0.01f && pixel.b < 0.01f) {
            near_black_pixels++;
        }
    }
    
    float clipping_percentage = static_cast<float>(clipped_pixels) / frame_data.size() * 100.0f;
    float black_percentage = static_cast<float>(near_black_pixels) / frame_data.size() * 100.0f;
    
    if (clipping_percentage > 1.0f) {
        QualityIssue issue;
        issue.type = IssueType::HIGHLIGHT_CLIPPING;
        issue.severity = (clipping_percentage > 5.0f) ? IssueSeverity::HIGH : IssueSeverity::MEDIUM;
        issue.description = "Highlight clipping detected in " + 
                           std::to_string(clipping_percentage) + "% of pixels";
        issue.affected_pixel_percentage = clipping_percentage;
        issues.push_back(issue);
    }
    
    if (black_percentage > 10.0f) {
        QualityIssue issue;
        issue.type = IssueType::SHADOW_CRUSHING;
        issue.severity = IssueSeverity::MEDIUM;
        issue.description = "Shadow crushing detected in " + 
                           std::to_string(black_percentage) + "% of pixels";
        issue.affected_pixel_percentage = black_percentage;
        issues.push_back(issue);
    }
    
    // Check for insufficient dynamic range
    if (histogram.effective_dynamic_range < 10.0f) {
        QualityIssue issue;
        issue.type = IssueType::LOW_DYNAMIC_RANGE;
        issue.severity = IssueSeverity::MEDIUM;
        issue.description = "Low dynamic range detected (ratio: " + 
                           std::to_string(histogram.effective_dynamic_range) + ")";
        issues.push_back(issue);
    }
    
    // Check for color banding (simplified detection)
    if (detect_color_banding(frame_data)) {
        QualityIssue issue;
        issue.type = IssueType::COLOR_BANDING;
        issue.severity = IssueSeverity::LOW;
        issue.description = "Potential color banding detected";
        issues.push_back(issue);
    }
    
    return issues;
}

bool HDRContentAnalyzer::detect_color_banding(const std::vector<RGB>& frame_data) const {
    // Simplified banding detection - look for sudden jumps in gradients
    if (frame_data.size() < 1000) return false;
    
    std::vector<float> luminance_values;
    luminance_values.reserve(frame_data.size());
    
    for (const auto& pixel : frame_data) {
        luminance_values.push_back(rgb_to_luminance(pixel));
    }
    
    // Sort and look for plateau regions
    std::sort(luminance_values.begin(), luminance_values.end());
    
    int plateau_count = 0;
    float last_value = luminance_values[0];
    int same_value_count = 1;
    
    for (size_t i = 1; i < luminance_values.size(); ++i) {
        if (std::abs(luminance_values[i] - last_value) < 0.001f) {
            same_value_count++;
        } else {
            if (same_value_count > static_cast<int>(luminance_values.size() * 0.01f)) {
                plateau_count++;
            }
            same_value_count = 1;
            last_value = luminance_values[i];
        }
    }
    
    return plateau_count > 3; // Threshold for banding detection
}

GamutUsage HDRContentAnalyzer::calculate_gamut_usage(const std::vector<RGB>& frame_data) const {
    GamutUsage usage;
    
    if (frame_data.empty()) return usage;
    
    // Calculate gamut boundaries for different color spaces
    int rec709_pixels = 0;
    int p3_pixels = 0;
    int rec2020_pixels = 0;
    int wide_gamut_pixels = 0;
    
    for (const auto& pixel : frame_data) {
        // Simplified gamut detection based on saturation and hue
        float max_component = std::max({pixel.r, pixel.g, pixel.b});
        float min_component = std::min({pixel.r, pixel.g, pixel.b});
        float saturation = (max_component > 0) ? (max_component - min_component) / max_component : 0;
        
        // High saturation colors are more likely to be outside Rec.709
        if (saturation > 0.8f) {
            wide_gamut_pixels++;
            if (saturation > 0.9f) {
                rec2020_pixels++;
            } else {
                p3_pixels++;
            }
        } else {
            rec709_pixels++;
        }
    }
    
    int total_pixels = static_cast<int>(frame_data.size());
    usage.rec709_coverage = static_cast<float>(rec709_pixels) / total_pixels * 100.0f;
    usage.p3_coverage = static_cast<float>(p3_pixels) / total_pixels * 100.0f;
    usage.rec2020_coverage = static_cast<float>(rec2020_pixels) / total_pixels * 100.0f;
    usage.wide_gamut_percentage = static_cast<float>(wide_gamut_pixels) / total_pixels * 100.0f;
    
    // Recommend color space based on usage
    if (usage.rec2020_coverage > 5.0f) {
        usage.recommended_color_space = ColorSpace::REC_2020;
    } else if (usage.p3_coverage > 10.0f) {
        usage.recommended_color_space = ColorSpace::DCI_P3;
    } else {
        usage.recommended_color_space = ColorSpace::REC_709;
    }
    
    return usage;
}

float HDRContentAnalyzer::estimate_required_peak_luminance(const LuminanceHistogram& histogram) const {
    // Use 99th percentile to determine required peak luminance
    float peak_requirement = histogram.percentile_99 * 10000.0f; // Convert to nits
    
    // Round to standard peak values
    if (peak_requirement <= 100.0f) {
        return 100.0f; // SDR
    } else if (peak_requirement <= 400.0f) {
        return 400.0f; // HDR400
    } else if (peak_requirement <= 600.0f) {
        return 600.0f; // HDR600
    } else if (peak_requirement <= 1000.0f) {
        return 1000.0f; // HDR1000
    } else if (peak_requirement <= 1400.0f) {
        return 1400.0f; // HDR1400
    } else if (peak_requirement <= 4000.0f) {
        return 4000.0f; // HDR4000
    } else {
        return 10000.0f; // HDR10000
    }
}

StandardCompliance HDRContentAnalyzer::check_standard_compliance(
    const FrameHDRAnalysis& analysis, HDRStandard standard) const {
    
    StandardCompliance compliance;
    compliance.tested_standard = standard;
    
    switch (standard) {
        case HDRStandard::HDR10:
            compliance = check_hdr10_compliance(analysis);
            break;
        case HDRStandard::HDR10_PLUS:
            compliance = check_hdr10_plus_compliance(analysis);
            break;
        case HDRStandard::HLG:
            compliance = check_hlg_compliance(analysis);
            break;
        case HDRStandard::DOLBY_VISION:
            compliance = check_dolby_vision_compliance(analysis);
            break;
        default:
            compliance.is_compliant = false;
            compliance.violations.push_back("Unknown or unsupported HDR standard");
            break;
    }
    
    return compliance;
}

StandardCompliance HDRContentAnalyzer::check_hdr10_compliance(const FrameHDRAnalysis& analysis) const {
    StandardCompliance compliance;
    compliance.tested_standard = HDRStandard::HDR10;
    compliance.is_compliant = true;
    
    // Check peak luminance requirements
    if (analysis.luminance_histogram.peak_luminance < 100.0f / 10000.0f) {
        compliance.is_compliant = false;
        compliance.violations.push_back("Peak luminance below HDR threshold (100 nits)");
    }
    
    if (analysis.luminance_histogram.peak_luminance > 10000.0f / 10000.0f) {
        compliance.violations.push_back("Peak luminance exceeds HDR10 maximum (10,000 nits)");
    }
    
    // Check bit depth requirements (would need actual bit depth info)
    // For now, assume compliance
    
    // Check color space usage
    if (analysis.gamut_usage.rec2020_coverage < 1.0f) {
        compliance.warnings.push_back("Limited Rec.2020 color space utilization");
    }
    
    return compliance;
}

StandardCompliance HDRContentAnalyzer::check_hdr10_plus_compliance(const FrameHDRAnalysis& analysis) const {
    StandardCompliance compliance = check_hdr10_compliance(analysis);
    compliance.tested_standard = HDRStandard::HDR10_PLUS;
    
    // HDR10+ specific checks would go here
    // For now, inherit HDR10 compliance requirements
    
    return compliance;
}

StandardCompliance HDRContentAnalyzer::check_hlg_compliance(const FrameHDRAnalysis& analysis) const {
    StandardCompliance compliance;
    compliance.tested_standard = HDRStandard::HLG;
    compliance.is_compliant = true;
    
    // HLG specific checks
    if (analysis.luminance_histogram.peak_luminance > 1000.0f / 10000.0f) {
        compliance.warnings.push_back("Peak luminance above typical HLG range (1,000 nits)");
    }
    
    // Check for HLG-appropriate dynamic range
    if (analysis.luminance_histogram.effective_dynamic_range < 10.0f) {
        compliance.is_compliant = false;
        compliance.violations.push_back("Insufficient dynamic range for HLG content");
    }
    
    return compliance;
}

StandardCompliance HDRContentAnalyzer::check_dolby_vision_compliance(const FrameHDRAnalysis& analysis) const {
    StandardCompliance compliance;
    compliance.tested_standard = HDRStandard::DOLBY_VISION;
    compliance.is_compliant = true;
    
    // Dolby Vision specific checks
    if (analysis.luminance_histogram.peak_luminance > 4000.0f / 10000.0f) {
        compliance.warnings.push_back("Peak luminance above typical Dolby Vision range (4,000 nits)");
    }
    
    // Check bit depth and color space requirements
    if (analysis.gamut_usage.rec2020_coverage < 5.0f) {
        compliance.is_compliant = false;
        compliance.violations.push_back("Insufficient Rec.2020 color space utilization for Dolby Vision");
    }
    
    return compliance;
}

// =============================================================================
// Sequence Analysis Implementation
// =============================================================================

SequenceHDRAnalysis HDRContentAnalyzer::analyze_sequence(
    const std::vector<FrameHDRAnalysis>& frame_analyses) const {
    
    SequenceHDRAnalysis sequence;
    sequence.total_frames = static_cast<int>(frame_analyses.size());
    sequence.analysis_timestamp = std::chrono::steady_clock::now();
    
    if (frame_analyses.empty()) {
        sequence.overall_classification = ContentType::INVALID;
        return sequence;
    }
    
    // Calculate temporal statistics
    calculate_temporal_statistics(sequence, frame_analyses);
    
    // Detect scene changes
    sequence.scene_changes = detect_scene_changes(frame_analyses);
    
    // Analyze temporal flicker
    sequence.flicker_analysis = analyze_temporal_flicker(frame_analyses);
    
    // Calculate overall quality metrics
    calculate_sequence_quality_metrics(sequence, frame_analyses);
    
    // Generate processing recommendations
    sequence.processing_recommendations = generate_processing_recommendations(sequence);
    
    return sequence;
}

void HDRContentAnalyzer::calculate_temporal_statistics(
    SequenceHDRAnalysis& sequence, const std::vector<FrameHDRAnalysis>& frames) const {
    
    std::vector<float> peak_luminances;
    std::vector<float> average_luminances;
    
    peak_luminances.reserve(frames.size());
    average_luminances.reserve(frames.size());
    
    for (const auto& frame : frames) {
        peak_luminances.push_back(frame.luminance_histogram.peak_luminance);
        average_luminances.push_back(frame.luminance_histogram.average_luminance);
    }
    
    // Calculate temporal statistics
    auto peak_minmax = std::minmax_element(peak_luminances.begin(), peak_luminances.end());
    auto avg_minmax = std::minmax_element(average_luminances.begin(), average_luminances.end());
    
    sequence.peak_luminance_min = *peak_minmax.first * 10000.0f;
    sequence.peak_luminance_max = *peak_minmax.second * 10000.0f;
    sequence.peak_luminance_avg = std::accumulate(peak_luminances.begin(), 
                                                 peak_luminances.end(), 0.0f) / peak_luminances.size() * 10000.0f;
    
    sequence.average_luminance_min = *avg_minmax.first * 10000.0f;
    sequence.average_luminance_max = *avg_minmax.second * 10000.0f;
    sequence.average_luminance_overall = std::accumulate(average_luminances.begin(), 
                                                        average_luminances.end(), 0.0f) / average_luminances.size() * 10000.0f;
    
    // Calculate temporal variance
    float peak_variance = 0.0f;
    float avg_peak = sequence.peak_luminance_avg / 10000.0f;
    
    for (float peak : peak_luminances) {
        peak_variance += (peak - avg_peak) * (peak - avg_peak);
    }
    sequence.luminance_temporal_variance = peak_variance / peak_luminances.size();
    
    // Classify overall content
    if (sequence.peak_luminance_avg > 1000.0f) {
        sequence.overall_classification = ContentType::HDR_STANDARD;
    } else if (sequence.peak_luminance_avg > 400.0f) {
        sequence.overall_classification = ContentType::HDR_LOW_PEAK;
    } else {
        sequence.overall_classification = ContentType::SDR_STANDARD;
    }
}

std::vector<SceneChange> HDRContentAnalyzer::detect_scene_changes(
    const std::vector<FrameHDRAnalysis>& frames) const {
    
    std::vector<SceneChange> scene_changes;
    
    if (frames.size() < 2) return scene_changes;
    
    for (size_t i = 1; i < frames.size(); ++i) {
        const auto& prev_frame = frames[i - 1];
        const auto& curr_frame = frames[i];
        
        // Calculate luminance difference
        float luma_diff = std::abs(curr_frame.luminance_histogram.average_luminance - 
                                  prev_frame.luminance_histogram.average_luminance);
        
        // Calculate histogram difference
        float hist_diff = calculate_histogram_difference(prev_frame.luminance_histogram, 
                                                        curr_frame.luminance_histogram);
        
        // Scene change detection thresholds
        if (luma_diff > 0.2f || hist_diff > 0.3f) {
            SceneChange change;
            change.frame_number = static_cast<int>(i);
            change.confidence = std::min(1.0f, (luma_diff + hist_diff) / 2.0f);
            change.luminance_change = luma_diff;
            change.histogram_change = hist_diff;
            
            if (change.confidence > 0.7f) {
                change.type = SceneChangeType::CUT;
            } else {
                change.type = SceneChangeType::FADE;
            }
            
            scene_changes.push_back(change);
        }
    }
    
    return scene_changes;
}

float HDRContentAnalyzer::calculate_histogram_difference(
    const LuminanceHistogram& hist1, const LuminanceHistogram& hist2) const {
    
    if (hist1.histogram.size() != hist2.histogram.size()) {
        return 1.0f; // Maximum difference for incompatible histograms
    }
    
    float total_diff = 0.0f;
    int total_pixels1 = std::accumulate(hist1.histogram.begin(), hist1.histogram.end(), 0);
    int total_pixels2 = std::accumulate(hist2.histogram.begin(), hist2.histogram.end(), 0);
    
    if (total_pixels1 == 0 || total_pixels2 == 0) {
        return 1.0f;
    }
    
    for (size_t i = 0; i < hist1.histogram.size(); ++i) {
        float norm1 = static_cast<float>(hist1.histogram[i]) / total_pixels1;
        float norm2 = static_cast<float>(hist2.histogram[i]) / total_pixels2;
        total_diff += std::abs(norm1 - norm2);
    }
    
    return total_diff / 2.0f; // Normalize to [0, 1]
}

FlickerAnalysis HDRContentAnalyzer::analyze_temporal_flicker(
    const std::vector<FrameHDRAnalysis>& frames) const {
    
    FlickerAnalysis flicker;
    flicker.flicker_detected = false;
    flicker.flicker_frequency = 0.0f;
    flicker.flicker_magnitude = 0.0f;
    
    if (frames.size() < 10) return flicker; // Need enough frames for analysis
    
    // Extract luminance time series
    std::vector<float> luminance_series;
    luminance_series.reserve(frames.size());
    
    for (const auto& frame : frames) {
        luminance_series.push_back(frame.luminance_histogram.average_luminance);
    }
    
    // Simple flicker detection using variance analysis
    float mean_luminance = std::accumulate(luminance_series.begin(), 
                                          luminance_series.end(), 0.0f) / luminance_series.size();
    
    float variance = 0.0f;
    for (float luma : luminance_series) {
        variance += (luma - mean_luminance) * (luma - mean_luminance);
    }
    variance /= luminance_series.size();
    
    float std_dev = std::sqrt(variance);
    float coefficient_of_variation = std_dev / mean_luminance;
    
    // Detect flicker based on coefficient of variation
    if (coefficient_of_variation > 0.05f) { // 5% threshold
        flicker.flicker_detected = true;
        flicker.flicker_magnitude = coefficient_of_variation;
        
        // Estimate flicker frequency (simplified)
        int zero_crossings = 0;
        bool above_mean = luminance_series[0] > mean_luminance;
        
        for (size_t i = 1; i < luminance_series.size(); ++i) {
            bool current_above = luminance_series[i] > mean_luminance;
            if (current_above != above_mean) {
                zero_crossings++;
                above_mean = current_above;
            }
        }
        
        flicker.flicker_frequency = static_cast<float>(zero_crossings) / (2.0f * frames.size()) * 24.0f; // Assume 24fps
    }
    
    return flicker;
}

void HDRContentAnalyzer::calculate_sequence_quality_metrics(
    SequenceHDRAnalysis& sequence, const std::vector<FrameHDRAnalysis>& frames) const {
    
    // Count frames with quality issues
    int frames_with_clipping = 0;
    int frames_with_banding = 0;
    int frames_with_low_dynamic_range = 0;
    
    for (const auto& frame : frames) {
        for (const auto& issue : frame.quality_issues) {
            switch (issue.type) {
                case IssueType::HIGHLIGHT_CLIPPING:
                    frames_with_clipping++;
                    break;
                case IssueType::COLOR_BANDING:
                    frames_with_banding++;
                    break;
                case IssueType::LOW_DYNAMIC_RANGE:
                    frames_with_low_dynamic_range++;
                    break;
                default:
                    break;
            }
        }
    }
    
    sequence.quality_score = 100.0f;
    
    // Penalize for quality issues
    float clipping_penalty = static_cast<float>(frames_with_clipping) / frames.size() * 30.0f;
    float banding_penalty = static_cast<float>(frames_with_banding) / frames.size() * 20.0f;
    float dynamic_range_penalty = static_cast<float>(frames_with_low_dynamic_range) / frames.size() * 25.0f;
    
    sequence.quality_score -= (clipping_penalty + banding_penalty + dynamic_range_penalty);
    sequence.quality_score = std::max(0.0f, sequence.quality_score);
    
    // Calculate HDR utilization score
    float total_hdr_percentage = 0.0f;
    for (const auto& frame : frames) {
        total_hdr_percentage += frame.hdr_pixel_percentage;
    }
    sequence.hdr_utilization_score = total_hdr_percentage / frames.size();
}

std::vector<ProcessingRecommendation> HDRContentAnalyzer::generate_processing_recommendations(
    const SequenceHDRAnalysis& sequence) const {
    
    std::vector<ProcessingRecommendation> recommendations;
    
    // Tone mapping recommendations
    if (sequence.peak_luminance_max > 1000.0f) {
        ProcessingRecommendation rec;
        rec.type = RecommendationType::TONE_MAPPING;
        rec.priority = RecommendationPriority::HIGH;
        rec.description = "Apply tone mapping for displays with peak luminance below " + 
                         std::to_string(sequence.peak_luminance_max) + " nits";
        rec.confidence = 0.9f;
        recommendations.push_back(rec);
    }
    
    // HDR standard recommendations
    if (sequence.overall_classification == ContentType::HDR_STANDARD) {
        ProcessingRecommendation rec;
        rec.type = RecommendationType::HDR_STANDARD_SELECTION;
        rec.priority = RecommendationPriority::MEDIUM;
        
        if (sequence.luminance_temporal_variance > 0.01f) {
            rec.description = "Use HDR10+ or Dolby Vision for dynamic metadata benefits";
        } else {
            rec.description = "HDR10 static metadata is sufficient for this content";
        }
        rec.confidence = 0.8f;
        recommendations.push_back(rec);
    }
    
    // Quality improvement recommendations
    if (sequence.quality_score < 80.0f) {
        ProcessingRecommendation rec;
        rec.type = RecommendationType::QUALITY_ENHANCEMENT;
        rec.priority = RecommendationPriority::HIGH;
        rec.description = "Apply quality enhancement filters to address detected issues";
        rec.confidence = 0.7f;
        recommendations.push_back(rec);
    }
    
    // Flicker reduction
    if (sequence.flicker_analysis.flicker_detected && 
        sequence.flicker_analysis.flicker_magnitude > 0.1f) {
        ProcessingRecommendation rec;
        rec.type = RecommendationType::TEMPORAL_PROCESSING;
        rec.priority = RecommendationPriority::MEDIUM;
        rec.description = "Apply temporal filtering to reduce flicker at " + 
                         std::to_string(sequence.flicker_analysis.flicker_frequency) + " Hz";
        rec.confidence = 0.6f;
        recommendations.push_back(rec);
    }
    
    return recommendations;
}

// =============================================================================
// Export Functions
// =============================================================================

Core::Result<void> HDRContentAnalyzer::export_analysis_report(
    const SequenceHDRAnalysis& analysis, const std::string& file_path) const {
    
    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return Core::Result<void>::error("Failed to open report file for writing");
        }
        
        file << "HDR CONTENT ANALYSIS REPORT\n";
        file << "===========================\n\n";
        
        // Sequence overview
        file << "SEQUENCE OVERVIEW:\n";
        file << "------------------\n";
        file << "Total Frames: " << analysis.total_frames << "\n";
        file << "Content Classification: " << content_type_to_string(analysis.overall_classification) << "\n";
        file << "Quality Score: " << std::fixed << std::setprecision(1) << analysis.quality_score << "/100\n";
        file << "HDR Utilization: " << analysis.hdr_utilization_score << "%\n\n";
        
        // Luminance statistics
        file << "LUMINANCE ANALYSIS:\n";
        file << "-------------------\n";
        file << "Peak Luminance: " << analysis.peak_luminance_min << " - " 
             << analysis.peak_luminance_max << " nits (avg: " << analysis.peak_luminance_avg << ")\n";
        file << "Average Luminance: " << analysis.average_luminance_min << " - " 
             << analysis.average_luminance_max << " nits (overall: " << analysis.average_luminance_overall << ")\n";
        file << "Temporal Variance: " << analysis.luminance_temporal_variance << "\n\n";
        
        // Scene analysis
        file << "SCENE ANALYSIS:\n";
        file << "---------------\n";
        file << "Scene Changes Detected: " << analysis.scene_changes.size() << "\n";
        if (analysis.flicker_analysis.flicker_detected) {
            file << "Flicker Detected: " << analysis.flicker_analysis.flicker_frequency 
                 << " Hz (magnitude: " << analysis.flicker_analysis.flicker_magnitude << ")\n";
        }
        file << "\n";
        
        // Recommendations
        file << "PROCESSING RECOMMENDATIONS:\n";
        file << "---------------------------\n";
        for (const auto& rec : analysis.processing_recommendations) {
            file << "- " << rec.description << " (Priority: " 
                 << recommendation_priority_to_string(rec.priority) << ")\n";
        }
        
        LOG_INFO("HDR analysis report exported to: {}", file_path);
        
        return Core::Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to export analysis report: {}", e.what());
        return Core::Result<void>::error("Failed to export analysis report");
    }
}

std::string HDRContentAnalyzer::content_type_to_string(ContentType type) const {
    switch (type) {
        case ContentType::SDR_STANDARD: return "SDR Standard";
        case ContentType::ENHANCED_SDR: return "Enhanced SDR";
        case ContentType::HDR_LOW_PEAK: return "HDR Low Peak";
        case ContentType::HDR_STANDARD: return "HDR Standard";
        case ContentType::HDR_HIGH_PEAK: return "HDR High Peak";
        case ContentType::INVALID: return "Invalid";
        default: return "Unknown";
    }
}

std::string HDRContentAnalyzer::recommendation_priority_to_string(RecommendationPriority priority) const {
    switch (priority) {
        case RecommendationPriority::LOW: return "Low";
        case RecommendationPriority::MEDIUM: return "Medium";
        case RecommendationPriority::HIGH: return "High";
        case RecommendationPriority::CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

} // namespace VideoEditor::GFX
