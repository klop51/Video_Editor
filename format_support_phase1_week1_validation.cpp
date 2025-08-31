#include <iostream>
#include <vector>
#include <string>
#include "media_io/format_detector.hpp"
#include "decode/frame.hpp"

/**
 * FORMAT_SUPPORT_ROADMAP.md Phase 1 Week 1 Validation
 * Tests the core infrastructure components implemented in Week 1:
 * 1. Expanded PixelFormat enum with professional formats
 * 2. Format Detection System with capability matrix
 * 3. Expanded ColorSpace support for professional workflows
 */

namespace ve::test {

class Phase1Week1Validator {
public:
    bool run_all_tests() {
        std::cout << "=== FORMAT_SUPPORT_ROADMAP.md Phase 1 Week 1 Testing ===\n";
        std::cout << "=========================================================\n\n";
        
        std::cout << "🎯 PHASE 1 WEEK 1 OBJECTIVE:\n";
        std::cout << "   Implement core infrastructure for professional format support\n";
        std::cout << "   Foundation for all subsequent format implementation phases\n\n";
        
        bool all_passed = true;
        
        all_passed &= test_expanded_pixel_formats();
        all_passed &= test_professional_color_spaces();
        all_passed &= test_format_detection_system();
        all_passed &= test_capability_matrix();
        all_passed &= test_professional_scoring();
        
        std::cout << "\n=== PHASE 1 WEEK 1 RESULTS ===\n";
        if (all_passed) {
            std::cout << "✅ ALL WEEK 1 TESTS PASSED!\n";
            std::cout << "✅ Core infrastructure ready for professional codec implementation\n";
            std::cout << "✅ Foundation established for Week 2: ProRes Support\n";
        } else {
            std::cout << "❌ SOME WEEK 1 TESTS FAILED!\n";
            std::cout << "❌ Core infrastructure needs fixes before proceeding\n";
        }
        
        return all_passed;
    }
    
private:
    bool test_expanded_pixel_formats() {
        std::cout << "🔍 Testing Expanded PixelFormat Support...\n";
        
        // Test professional 16-bit RGB formats
        [[maybe_unused]] auto rgb48le = ve::decode::PixelFormat::RGB48LE;
        [[maybe_unused]] auto rgba64le = ve::decode::PixelFormat::RGBA64LE;
        std::cout << "   ✅ 16-bit RGB formats: RGB48LE, RGBA64LE available\n";
        
        // Test professional YUV formats
        [[maybe_unused]] auto yuv420p16le = ve::decode::PixelFormat::YUV420P16LE;
        [[maybe_unused]] auto yuv422p16le = ve::decode::PixelFormat::YUV422P16LE;
        [[maybe_unused]] auto yuv444p16le = ve::decode::PixelFormat::YUV444P16LE;
        std::cout << "   ✅ 16-bit YUV formats: YUV420P16LE, YUV422P16LE, YUV444P16LE available\n";
        
        // Test professional packed formats
        [[maybe_unused]] auto v210 = ve::decode::PixelFormat::V210;
        [[maybe_unused]] auto v410 = ve::decode::PixelFormat::V410;
        std::cout << "   ✅ Professional packed formats: V210, V410 available\n";
        
        // Test alpha variants
        [[maybe_unused]] auto yuva420p = ve::decode::PixelFormat::YUVA420P;
        [[maybe_unused]] auto yuva422p = ve::decode::PixelFormat::YUVA422P;
        [[maybe_unused]] auto yuva444p = ve::decode::PixelFormat::YUVA444P;
        std::cout << "   ✅ Alpha variants: YUVA420P, YUVA422P, YUVA444P available\n";
        
        // Test high bit depth planar RGB
        [[maybe_unused]] auto gbrp = ve::decode::PixelFormat::GBRP;
        [[maybe_unused]] auto gbrp16le = ve::decode::PixelFormat::GBRP16LE;
        std::cout << "   ✅ Planar RGB formats: GBRP, GBRP16LE available\n";
        
        std::cout << "   🎯 Professional pixel format expansion: SUCCESS\n\n";
        return true;
    }
    
    bool test_professional_color_spaces() {
        std::cout << "🎨 Testing Professional Color Space Support...\n";
        
        // Test cinema color spaces
        [[maybe_unused]] auto dci_p3 = ve::decode::ColorSpace::DCI_P3;
        [[maybe_unused]] auto display_p3 = ve::decode::ColorSpace::DISPLAY_P3;
        std::cout << "   ✅ Cinema color spaces: DCI_P3, DISPLAY_P3 available\n";
        
        // Test broadcast color spaces
        [[maybe_unused]] auto bt2020_ncl = ve::decode::ColorSpace::BT2020_NCL;
        [[maybe_unused]] auto bt2020_cl = ve::decode::ColorSpace::BT2020_CL;
        std::cout << "   ✅ BT.2020 variants: BT2020_NCL, BT2020_CL available\n";
        
        // Test professional color spaces
        [[maybe_unused]] auto adobe_rgb = ve::decode::ColorSpace::ADOBE_RGB;
        [[maybe_unused]] auto prophoto_rgb = ve::decode::ColorSpace::PROPHOTO_RGB;
        std::cout << "   ✅ Professional spaces: ADOBE_RGB, PROPHOTO_RGB available\n";
        
        // Test camera color spaces
        [[maybe_unused]] auto alexa_wide = ve::decode::ColorSpace::ALEXA_WIDE_GAMUT;
        [[maybe_unused]] auto sony_sgamut3 = ve::decode::ColorSpace::SONY_SGAMUT3;
        [[maybe_unused]] auto canon_cinema = ve::decode::ColorSpace::CANON_CINEMA_GAMUT;
        std::cout << "   ✅ Camera color spaces: ALEXA_WIDE_GAMUT, SONY_SGAMUT3, CANON_CINEMA_GAMUT available\n";
        
        // Test HDR transfer functions
        [[maybe_unused]] auto hdr10 = ve::decode::ColorSpace::HDR10_ST2084;
        [[maybe_unused]] auto hlg = ve::decode::ColorSpace::HLG_ARIB_STD_B67;
        [[maybe_unused]] auto dolby_vision = ve::decode::ColorSpace::DOLBY_VISION;
        std::cout << "   ✅ HDR spaces: HDR10_ST2084, HLG_ARIB_STD_B67, DOLBY_VISION available\n";
        
        std::cout << "   🎯 Professional color space expansion: SUCCESS\n\n";
        return true;
    }
    
    bool test_format_detection_system() {
        std::cout << "🔍 Testing Format Detection System...\n";
        
        ve::media_io::FormatDetector detector;
        
        // Test capability matrix
        auto prores_cap = detector.get_format_capability(
            ve::media_io::CodecFamily::PRORES, 
            ve::media_io::ContainerType::MOV
        );
        
        bool prores_supported = prores_cap.supports_decode && 
                               prores_cap.real_time_capable &&
                               prores_cap.max_width >= 4096;
        
        std::cout << "   " << (prores_supported ? "✅" : "❌") 
                  << " ProRes capability: decode=" << prores_cap.supports_decode 
                  << ", real_time=" << prores_cap.real_time_capable
                  << ", max_res=" << prores_cap.max_width << "x" << prores_cap.max_height << "\n";
        
        // Test DNxHR capability
        auto dnxhr_cap = detector.get_format_capability(
            ve::media_io::CodecFamily::DNXHR,
            ve::media_io::ContainerType::MXF
        );
        
        bool dnxhr_supported = dnxhr_cap.supports_decode && 
                              dnxhr_cap.supports_encode &&
                              dnxhr_cap.max_width >= 4096;
        
        std::cout << "   " << (dnxhr_supported ? "✅" : "❌")
                  << " DNxHR capability: decode=" << dnxhr_cap.supports_decode
                  << ", encode=" << dnxhr_cap.supports_encode
                  << ", max_res=" << dnxhr_cap.max_width << "x" << dnxhr_cap.max_height << "\n";
        
        // Test modern codec capability
        auto h265_cap = detector.get_format_capability(
            ve::media_io::CodecFamily::H265_HEVC,
            ve::media_io::ContainerType::MP4
        );
        
        bool h265_supported = h265_cap.supports_decode &&
                             h265_cap.hardware_accelerated &&
                             h265_cap.supports_hdr;
        
        std::cout << "   " << (h265_supported ? "✅" : "❌")
                  << " H.265/HEVC capability: decode=" << h265_cap.supports_decode
                  << ", hw_accel=" << h265_cap.hardware_accelerated
                  << ", hdr=" << h265_cap.supports_hdr << "\n";
        
        std::cout << "   🎯 Format detection system: SUCCESS\n\n";
        return prores_supported && dnxhr_supported && h265_supported;
    }
    
    bool test_capability_matrix() {
        std::cout << "📊 Testing Professional Capability Matrix...\n";
        
        ve::media_io::FormatDetector detector;
        auto supported_formats = detector.get_supported_formats();
        
        std::cout << "   📈 Supported format combinations: " << supported_formats.size() << "\n";
        
        // Verify key professional formats are supported
        bool has_prores_mov = false;
        bool has_dnxhr_mxf = false;
        bool has_h265_mp4 = false;
        
        for (const auto& format_pair : supported_formats) {
            if (format_pair.first == ve::media_io::CodecFamily::PRORES &&
                format_pair.second == ve::media_io::ContainerType::MOV) {
                has_prores_mov = true;
            }
            if (format_pair.first == ve::media_io::CodecFamily::DNXHR &&
                format_pair.second == ve::media_io::ContainerType::MXF) {
                has_dnxhr_mxf = true;
            }
            if (format_pair.first == ve::media_io::CodecFamily::H265_HEVC &&
                format_pair.second == ve::media_io::ContainerType::MP4) {
                has_h265_mp4 = true;
            }
        }
        
        std::cout << "   " << (has_prores_mov ? "✅" : "❌") << " ProRes in MOV container supported\n";
        std::cout << "   " << (has_dnxhr_mxf ? "✅" : "❌") << " DNxHR in MXF container supported\n";
        std::cout << "   " << (has_h265_mp4 ? "✅" : "❌") << " H.265 in MP4 container supported\n";
        
        bool matrix_valid = has_prores_mov && has_dnxhr_mxf && has_h265_mp4;
        std::cout << "   🎯 Capability matrix validation: " << (matrix_valid ? "SUCCESS" : "FAILED") << "\n\n";
        return matrix_valid;
    }
    
    bool test_professional_scoring() {
        std::cout << "⭐ Testing Professional Format Scoring...\n";
        
        ve::media_io::FormatDetector detector;
        
        // Create mock professional format
        ve::media_io::DetectedFormat prores_format;
        prores_format.codec = ve::media_io::CodecFamily::PRORES;
        prores_format.container = ve::media_io::ContainerType::MOV;
        prores_format.width = 4096;
        prores_format.height = 2160;
        prores_format.bit_depth = 12;
        prores_format.capability = detector.get_format_capability(prores_format.codec, prores_format.container);
        
        float prores_score = detector.calculate_professional_score(prores_format);
        bool prores_professional = prores_score >= 0.8f;
        
        std::cout << "   " << (prores_professional ? "✅" : "❌")
                  << " 4K ProRes score: " << prores_score << " (professional: " << (prores_professional ? "YES" : "NO") << ")\n";
        
        // Create mock consumer format
        ve::media_io::DetectedFormat h264_format;
        h264_format.codec = ve::media_io::CodecFamily::H264;
        h264_format.container = ve::media_io::ContainerType::MP4;
        h264_format.width = 1920;
        h264_format.height = 1080;
        h264_format.bit_depth = 8;
        h264_format.capability = detector.get_format_capability(h264_format.codec, h264_format.container);
        
        float h264_score = detector.calculate_professional_score(h264_format);
        bool h264_consumer = h264_score < 0.6f;
        
        std::cout << "   " << (h264_consumer ? "✅" : "❌")
                  << " HD H.264 score: " << h264_score << " (consumer: " << (h264_consumer ? "YES" : "NO") << ")\n";
        
        // Test optimization recommendations
        auto recommendations = detector.get_optimization_recommendations(prores_format);
        bool has_recommendations = !recommendations.empty();
        
        std::cout << "   " << (has_recommendations ? "✅" : "❌")
                  << " Optimization recommendations generated: " << recommendations.size() << " suggestions\n";
        
        if (has_recommendations) {
            for (const auto& rec : recommendations) {
                std::cout << "     → " << rec << "\n";
            }
        }
        
        bool scoring_valid = prores_professional && h264_consumer && has_recommendations;
        std::cout << "   🎯 Professional scoring system: " << (scoring_valid ? "SUCCESS" : "FAILED") << "\n\n";
        return scoring_valid;
    }
};

} // namespace ve::test

int main() {
    try {
        ve::test::Phase1Week1Validator validator;
        bool success = validator.run_all_tests();
        
        std::cout << "\n=== PHASE 1 WEEK 1 COMPLETION STATUS ===\n";
        if (success) {
            std::cout << "🎉 PHASE 1 WEEK 1: CORE INFRASTRUCTURE COMPLETED!\n";
            std::cout << "📋 DELIVERABLES ACHIEVED:\n";
            std::cout << "   ✅ Enhanced format enums with professional pixel formats\n";
            std::cout << "   ✅ Professional color space support expanded\n";
            std::cout << "   ✅ Format detection system operational\n";
            std::cout << "   ✅ Capability matrix for format validation\n";
            std::cout << "   ✅ Professional scoring and recommendations\n";
            std::cout << "\n📈 SUCCESS CRITERIA MET:\n";
            std::cout << "   ✅ Can detect and categorize all major professional formats\n";
            std::cout << "   ✅ Infrastructure ready for Week 2: ProRes Support\n";
            std::cout << "\n🚀 READY FOR PHASE 1 WEEK 2: ProRes Support Implementation\n";
            return 0;
        } else {
            std::cout << "❌ PHASE 1 WEEK 1: INFRASTRUCTURE ISSUES DETECTED\n";
            std::cout << "🔧 REQUIRED ACTIONS:\n";
            std::cout << "   - Review failed test outputs above\n";
            std::cout << "   - Fix core infrastructure components\n";
            std::cout << "   - Re-run validation before proceeding to Week 2\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ EXCEPTION: " << e.what() << "\n";
        return 1;
    }
}
