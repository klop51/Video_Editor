#include "media_io/color_management.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <iomanip>

using namespace ve::media_io;
using namespace ve::media_io::color_utils;

/**
 * @brief Comprehensive validation test for Color Management Integration
 * 
 * Tests Phase 2 Week 8 implementation: color space conversion matrices,
 * gamut mapping, display adaptation, and professional color accuracy.
 */

class ColorManagementValidationTest {
public:
    void runAllTests() {
        std::cout << "=== Color Management Integration Validation Test ===" << std::endl;
        
        testInitialization();
        testColorSpaceSupport();
        testColorSpaceInfo();
        testConversionMatrices();
        testColorSpaceConversion();
        testGamutMapping();
        testDisplayAdaptation();
        testWhitePointAdaptation();
        testToneMapping();
        testColorAccuracy();
        testProfessionalWorkflows();
        testUtilityFunctions();
        
        std::cout << "=== Color Management Integration Validation COMPLETE ===" << std::endl;
        std::cout << "All color management components tested successfully!" << std::endl;
    }

private:
    void testInitialization() {
        std::cout << "Testing Color Management initialization..." << std::endl;
        
        ColorManagement cm;
        auto supported_spaces = cm.getSupportedColorSpaces();
        
        assert(!supported_spaces.empty());
        std::cout << "Color Management initialized: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testColorSpaceSupport() {
        std::cout << "Testing supported color spaces..." << std::endl;
        
        ColorManagement cm;
        auto spaces = cm.getSupportedColorSpaces();
        
        std::cout << "Number of supported color spaces: " << spaces.size() << std::endl;
        
        // Test major color space categories
        std::vector<std::pair<ColorSpace, std::string>> test_spaces = {
            {ColorSpace::BT709, "Rec. 709 (HD standard)"},
            {ColorSpace::SRGB, "sRGB (computer graphics)"},
            {ColorSpace::BT2020, "Rec. 2020 (UHD standard)"},
            {ColorSpace::DCI_P3, "DCI-P3 (digital cinema)"},
            {ColorSpace::DISPLAY_P3, "Display P3 (Apple displays)"},
            {ColorSpace::ADOBE_RGB, "Adobe RGB (wide gamut)"},
            {ColorSpace::LINEAR_BT709, "Linear Rec. 709"},
            {ColorSpace::LINEAR_BT2020, "Linear Rec. 2020"},
            {ColorSpace::ACES_CG, "ACES Color Grading space"}
        };
        
        for (const auto& [space, name] : test_spaces) {
            bool supported = cm.isColorSpaceSupported(space);
            std::cout << "- " << name << ": " << (supported ? "SUPPORTED" : "NOT SUPPORTED") << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void testColorSpaceInfo() {
        std::cout << "Testing color space information..." << std::endl;
        
        ColorManagement cm;
        
        // Test detailed information for key color spaces
        std::vector<ColorSpace> test_spaces = {
            ColorSpace::BT709,
            ColorSpace::BT2020,
            ColorSpace::DCI_P3,
            ColorSpace::ADOBE_RGB
        };
        
        for (ColorSpace space : test_spaces) {
            ColorSpaceInfo info = cm.getColorSpaceInfo(space);
            
            std::cout << info.name << ":" << std::endl;
            std::cout << "  White point: " << static_cast<int>(info.white_point) << std::endl;
            std::cout << "  Gamma: " << std::fixed << std::setprecision(1) << info.gamma << std::endl;
            std::cout << "  Is linear: " << (info.is_linear ? "YES" : "NO") << std::endl;
            std::cout << "  Wide gamut: " << (info.is_wide_gamut ? "YES" : "NO") << std::endl;
            std::cout << "  Description: " << info.description << std::endl;
            
            assert(!info.name.empty());
            assert(info.gamma > 0.0);
        }
        
        std::cout << std::endl;
    }
    
    void testConversionMatrices() {
        std::cout << "Testing color space conversion matrices..." << std::endl;
        
        ColorManagement cm;
        
        // Test identity matrix for same color space
        auto identity = cm.getConversionMatrix(ColorSpace::BT709, ColorSpace::BT709);
        assert(std::abs(identity[0][0] - 1.0) < 0.001);
        assert(std::abs(identity[1][1] - 1.0) < 0.001);
        assert(std::abs(identity[2][2] - 1.0) < 0.001);
        
        std::cout << "  Identity matrix: SUCCESS" << std::endl;
        
        // Test conversion matrices for major transformations
        std::vector<std::pair<ColorSpace, ColorSpace>> conversions = {
            {ColorSpace::BT709, ColorSpace::BT2020},
            {ColorSpace::BT2020, ColorSpace::BT709},
            {ColorSpace::SRGB, ColorSpace::DCI_P3},
            {ColorSpace::DCI_P3, ColorSpace::ADOBE_RGB}
        };
        
        for (const auto& [from, to] : conversions) {
            auto matrix = cm.getConversionMatrix(from, to);
            
            // Verify matrix is not zero
            bool non_zero = false;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    if (std::abs(matrix[i][j]) > 0.001) {
                        non_zero = true;
                        break;
                    }
                }
            }
            assert(non_zero);
        }
        
        std::cout << "  Conversion matrices: SUCCESS" << std::endl;
        
        // Test chromatic adaptation
        auto adaptation = cm.getChromaticAdaptationMatrix(WhitePoint::D50, WhitePoint::D65);
        assert(std::abs(adaptation[0][0]) > 0.5);  // Should have reasonable values
        
        std::cout << "  Chromatic adaptation: SUCCESS" << std::endl;
        
        std::cout << std::endl;
    }
    
    void testColorSpaceConversion() {
        std::cout << "Testing color space conversion..." << std::endl;
        
        ColorManagement cm;
        
        // Test single color conversion
        RGBColor red_709(1.0, 0.0, 0.0);
        RGBColor red_2020 = cm.convertSingleColor(red_709, ColorSpace::BT709, ColorSpace::BT2020);
        
        assert(red_2020.R > 0.5);  // Should still be primarily red
        assert(red_2020.G >= 0.0 && red_2020.G <= 1.0);
        assert(red_2020.B >= 0.0 && red_2020.B <= 1.0);
        
        std::cout << "  Single color conversion: SUCCESS" << std::endl;
        std::cout << "    BT.709 red (1.0, 0.0, 0.0) -> BT.2020 red (" << 
            std::fixed << std::setprecision(3) << 
            red_2020.R << ", " << red_2020.G << ", " << red_2020.B << ")" << std::endl;
        
        // Test batch conversion
        std::vector<RGBColor> test_colors = {
            {1.0, 0.0, 0.0},  // Red
            {0.0, 1.0, 0.0},  // Green
            {0.0, 0.0, 1.0},  // Blue
            {1.0, 1.0, 1.0},  // White
            {0.5, 0.5, 0.5}   // Gray
        };
        
        ColorConversionConfig config;
        config.source_space = ColorSpace::BT709;
        config.target_space = ColorSpace::BT2020;
        config.gamut_method = GamutMappingMethod::PERCEPTUAL;
        config.preserve_blacks = true;
        config.chromatic_adaptation = true;
        config.gamut_compression_factor = 0.8;
        
        auto result = cm.convertColorSpace(test_colors, config);
        
        assert(result.conversion_successful);
        assert(result.converted_colors.size() == test_colors.size());
        assert(result.gamut_coverage >= 0.0 && result.gamut_coverage <= 1.0);
        
        std::cout << "  Batch conversion: SUCCESS" << std::endl;
        std::cout << "    Converted " << result.converted_colors.size() << " colors" << std::endl;
        std::cout << "    Gamut coverage: " << std::fixed << std::setprecision(1) << 
            (result.gamut_coverage * 100.0) << "%" << std::endl;
        std::cout << "    Average Delta E: " << std::fixed << std::setprecision(2) << 
            result.color_accuracy_delta_e << std::endl;
        
        std::cout << std::endl;
    }
    
    void testGamutMapping() {
        std::cout << "Testing gamut mapping..." << std::endl;
        
        ColorManagement cm;
        
        // Test out-of-gamut color
        RGBColor out_of_gamut(1.5, -0.2, 0.8);  // Invalid RGB values
        
        // Test different gamut mapping methods
        std::vector<std::pair<GamutMappingMethod, std::string>> methods = {
            {GamutMappingMethod::CLIP, "Hard clipping"},
            {GamutMappingMethod::PERCEPTUAL, "Perceptual mapping"},
            {GamutMappingMethod::SATURATION, "Saturation preserving"},
            {GamutMappingMethod::RELATIVE_COLORIMETRIC, "Relative colorimetric"}
        };
        
        for (const auto& [method, name] : methods) {
            RGBColor mapped = cm.applyGamutMapping(out_of_gamut, ColorSpace::BT709, method);
            
            // Verify mapped color is in valid range
            assert(mapped.R >= 0.0 && mapped.R <= 1.0);
            assert(mapped.G >= 0.0 && mapped.G <= 1.0);
            assert(mapped.B >= 0.0 && mapped.B <= 1.0);
            
            std::cout << "  " << name << ": SUCCESS" << std::endl;
        }
        
        // Test gamut boundary calculation
        auto boundary = cm.calculateGamutBoundary(ColorSpace::BT709);
        assert(!boundary.boundary_points.empty());
        assert(boundary.area > 0.0);
        
        std::cout << "  Gamut boundary calculation: SUCCESS" << std::endl;
        
        // Test gamut coverage calculation
        double coverage_709_to_2020 = cm.calculateGamutCoverage(ColorSpace::BT709, ColorSpace::BT2020);
        double coverage_2020_to_709 = cm.calculateGamutCoverage(ColorSpace::BT2020, ColorSpace::BT709);
        
        std::cout << "  BT.709 -> BT.2020 coverage: " << std::fixed << std::setprecision(1) << 
            (coverage_709_to_2020 * 100.0) << "%" << std::endl;
        std::cout << "  BT.2020 -> BT.709 coverage: " << std::fixed << std::setprecision(1) << 
            (coverage_2020_to_709 * 100.0) << "%" << std::endl;
        
        assert(coverage_709_to_2020 < 1.0);  // BT.709 should not fully cover BT.2020
        assert(coverage_2020_to_709 >= 0.9);  // BT.2020 should mostly cover BT.709
        
        std::cout << std::endl;
    }
    
    void testDisplayAdaptation() {
        std::cout << "Testing display adaptation..." << std::endl;
        
        ColorManagement cm;
        
        // Configure test display
        DisplayConfig display;
        display.native_color_space = ColorSpace::SRGB;
        display.white_point = WhitePoint::D65;
        display.max_luminance = 100.0;
        display.min_luminance = 0.1;
        display.hdr_capable = false;
        display.wide_gamut = false;
        
        cm.setDisplayConfig(display);
        auto retrieved_config = cm.getDisplayConfig();
        
        assert(retrieved_config.native_color_space == ColorSpace::SRGB);
        assert(retrieved_config.max_luminance == 100.0);
        
        std::cout << "  Display configuration: SUCCESS" << std::endl;
        
        // Test display adaptation
        RGBColor bt2020_color(0.8, 0.9, 0.7);
        RGBColor adapted = cm.adaptForDisplay(bt2020_color, ColorSpace::BT2020);
        
        assert(adapted.R >= 0.0 && adapted.R <= 1.0);
        assert(adapted.G >= 0.0 && adapted.G <= 1.0);
        assert(adapted.B >= 0.0 && adapted.B <= 1.0);
        
        std::cout << "  Display adaptation: SUCCESS" << std::endl;
        std::cout << "    BT.2020 color (" << std::fixed << std::setprecision(3) << 
            bt2020_color.R << ", " << bt2020_color.G << ", " << bt2020_color.B << 
            ") -> sRGB (" << adapted.R << ", " << adapted.G << ", " << adapted.B << ")" << std::endl;
        
        std::cout << std::endl;
    }
    
    void testWhitePointAdaptation() {
        std::cout << "Testing white point adaptation..." << std::endl;
        
        ColorManagement cm;
        
        // Test white point adaptation
        XYZColor d50_white(0.9642, 1.0000, 0.8251);
        XYZColor d65_adapted = cm.adaptWhitePoint(d50_white, WhitePoint::D50, WhitePoint::D65);
        
        // D65 should be different from D50
        assert(std::abs(d65_adapted.X - d50_white.X) > 0.01 || 
               std::abs(d65_adapted.Z - d50_white.Z) > 0.01);
        
        std::cout << "  D50 -> D65 adaptation: SUCCESS" << std::endl;
        
        // Test Bradford adaptation matrix
        auto bradford = cm.bradfordAdaptation(WhitePoint::D50, WhitePoint::D65);
        assert(std::abs(bradford[0][0]) > 0.5);  // Should have reasonable values
        
        std::cout << "  Bradford adaptation matrix: SUCCESS" << std::endl;
        
        std::cout << std::endl;
    }
    
    void testToneMapping() {
        std::cout << "Testing tone mapping..." << std::endl;
        
        ColorManagement cm;
        
        // Test HDR to SDR tone mapping
        RGBColor hdr_color(2.0, 1.5, 1.8);  // HDR values > 1.0
        RGBColor sdr_mapped = cm.toneMapForSDR(hdr_color, 100.0);
        
        assert(sdr_mapped.R >= 0.0 && sdr_mapped.R <= 1.0);
        assert(sdr_mapped.G >= 0.0 && sdr_mapped.G <= 1.0);
        assert(sdr_mapped.B >= 0.0 && sdr_mapped.B <= 1.0);
        
        std::cout << "  HDR to SDR tone mapping: SUCCESS" << std::endl;
        std::cout << "    HDR (" << std::fixed << std::setprecision(1) << 
            hdr_color.R << ", " << hdr_color.G << ", " << hdr_color.B << 
            ") -> SDR (" << std::setprecision(3) << sdr_mapped.R << ", " << 
            sdr_mapped.G << ", " << sdr_mapped.B << ")" << std::endl;
        
        // Test SDR to HDR expansion
        RGBColor sdr_color(0.8, 0.9, 0.7);
        RGBColor hdr_expanded = cm.expandToHDR(sdr_color, 1000.0);
        
        // HDR values should be larger than SDR
        assert(hdr_expanded.R >= sdr_color.R || hdr_expanded.G >= sdr_color.G || hdr_expanded.B >= sdr_color.B);
        
        std::cout << "  SDR to HDR expansion: SUCCESS" << std::endl;
        
        std::cout << std::endl;
    }
    
    void testColorAccuracy() {
        std::cout << "Testing color accuracy metrics..." << std::endl;
        
        ColorManagement cm;
        
        // Test Delta E calculation
        RGBColor color1(1.0, 0.0, 0.0);
        RGBColor color2(0.9, 0.1, 0.0);
        
        double delta_e = cm.calculateDeltaE(color1, color2);
        assert(delta_e >= 0.0);
        
        std::cout << "  Delta E calculation: SUCCESS" << std::endl;
        std::cout << "    Delta E between similar reds: " << std::fixed << std::setprecision(2) << 
            delta_e << std::endl;
        
        // Test gamut utilization
        std::vector<RGBColor> diverse_colors = {
            {0.0, 0.0, 0.0},  // Black
            {1.0, 1.0, 1.0},  // White
            {1.0, 0.0, 0.0},  // Red
            {0.0, 1.0, 0.0},  // Green
            {0.0, 0.0, 1.0},  // Blue
            {0.5, 0.5, 0.5}   // Gray
        };
        
        double utilization = cm.calculateGamutUtilization(diverse_colors, ColorSpace::BT709);
        assert(utilization >= 0.0 && utilization <= 1.0);
        
        std::cout << "  Gamut utilization: " << std::fixed << std::setprecision(1) << 
            (utilization * 100.0) << "%" << std::endl;
        
        // Test color accuracy validation
        std::vector<RGBColor> reference = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
        std::vector<RGBColor> processed = {{0.98, 0.02, 0.0}, {0.02, 0.98, 0.0}};
        
        bool accurate = cm.validateColorAccuracy(reference, processed, 5.0);
        assert(accurate);  // Should be within acceptable range
        
        std::cout << "  Color accuracy validation: SUCCESS" << std::endl;
        
        std::cout << std::endl;
    }
    
    void testProfessionalWorkflows() {
        std::cout << "Testing professional workflow scenarios..." << std::endl;
        
        // Test workflow recommendations
        auto netflix_rec = getWorkflowRecommendation(ColorSpace::BT709, "netflix", false);
        auto cinema_rec = getWorkflowRecommendation(ColorSpace::DCI_P3, "cinema", false);
        auto broadcast_rec = getWorkflowRecommendation(ColorSpace::BT709, "broadcast", false);
        
        std::cout << "Netflix workflow:" << std::endl;
        std::cout << "  Working space: " << static_cast<int>(netflix_rec.working_space) << std::endl;
        std::cout << "  Output space: " << static_cast<int>(netflix_rec.output_space) << std::endl;
        std::cout << "  Reasoning: " << netflix_rec.reasoning << std::endl;
        
        std::cout << "Cinema workflow:" << std::endl;
        std::cout << "  Working space: " << static_cast<int>(cinema_rec.working_space) << std::endl;
        std::cout << "  Output space: " << static_cast<int>(cinema_rec.output_space) << std::endl;
        
        std::cout << "Broadcast workflow:" << std::endl;
        std::cout << "  Working space: " << static_cast<int>(broadcast_rec.working_space) << std::endl;
        std::cout << "  Output space: " << static_cast<int>(broadcast_rec.output_space) << std::endl;
        
        // Test codec-based color space detection
        ColorSpace hevc_space = detectFromCodecInfo("HEVC Main10", "bt2020");
        ColorSpace h264_space = detectFromCodecInfo("H.264", "bt709");
        
        assert(hevc_space == ColorSpace::BT2020);
        assert(h264_space == ColorSpace::BT709);
        
        std::cout << "  Codec-based detection: SUCCESS" << std::endl;
        
        // Test workflow validation
        std::vector<RGBColor> test_colors = {
            {0.8, 0.2, 0.1}, {0.1, 0.9, 0.2}, {0.2, 0.1, 0.8}
        };
        
        auto accuracy_report = validateWorkflow(
            test_colors, 
            ColorSpace::BT2020, 
            ColorSpace::BT709, 
            GamutMappingMethod::PERCEPTUAL
        );
        
        std::cout << "Workflow validation report:" << std::endl;
        std::cout << "  Average Delta E: " << std::fixed << std::setprecision(2) << 
            accuracy_report.average_delta_e << std::endl;
        std::cout << "  Max Delta E: " << accuracy_report.max_delta_e << std::endl;
        std::cout << "  Color fidelity score: " << std::setprecision(1) << 
            (accuracy_report.color_fidelity_score * 100.0) << "%" << std::endl;
        std::cout << "  Recommendations: " << accuracy_report.recommendations.size() << " items" << std::endl;
        
        assert(accuracy_report.average_delta_e >= 0.0);
        assert(accuracy_report.color_fidelity_score >= 0.0 && accuracy_report.color_fidelity_score <= 1.0);
        
        std::cout << std::endl;
    }
    
    void testUtilityFunctions() {
        std::cout << "Testing utility functions..." << std::endl;
        
        // Test common color space conversions
        RGBColor bt709_red(1.0, 0.0, 0.0);
        
        RGBColor bt2020_red = bt709ToBt2020(bt709_red);
        assert(bt2020_red.R > 0.5);
        
        RGBColor back_to_709 = bt2020ToBt709(bt2020_red);
        assert(std::abs(back_to_709.R - 1.0) < 0.1);  // Should be close to original
        
        std::cout << "  BT.709 <-> BT.2020 conversion: SUCCESS" << std::endl;
        
        // Test P3 conversions
        RGBColor dci_green(0.0, 1.0, 0.0);
        RGBColor display_p3_green = dciP3ToDisplayP3(dci_green);
        assert(display_p3_green.G > 0.5);
        
        std::cout << "  DCI-P3 -> Display P3 conversion: SUCCESS" << std::endl;
        
        // Test sRGB to BT.709
        RGBColor srgb_blue(0.0, 0.0, 1.0);
        RGBColor bt709_blue = srgbToBt709(srgb_blue);
        assert(bt709_blue.B > 0.5);
        
        std::cout << "  sRGB -> BT.709 conversion: SUCCESS" << std::endl;
        
        std::cout << std::endl;
    }
};

int main() {
    try {
        ColorManagementValidationTest test;
        test.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
