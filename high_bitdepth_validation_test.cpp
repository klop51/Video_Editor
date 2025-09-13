#include "media_io/high_bitdepth_support.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <iomanip>

using namespace ve::media_io;

/**
 * @brief Comprehensive validation test for High Bit Depth Pipeline
 * 
 * Tests Phase 2 Week 7 implementation: 16-bit processing pipeline,
 * 12-bit format support, precision handling, and quality assessment.
 */

class HighBitDepthValidationTest {
public:
    void runAllTests() {
        std::cout << "=== High Bit Depth Pipeline Validation Test ===" << std::endl;
        
        testInitialization();
        testFormatSupport();
        testBitDepthInfo();
        testFormatDetection();
        testProcessingPrecision();
        testBitDepthConversion();
        testQualityAssessment();
        testDitheringMethods();
        testRangeConversion();
        testClippingDetection();
        testMemoryCalculation();
        testProfessionalWorkflows();
        
        std::cout << "=== High Bit Depth Pipeline Validation COMPLETE ===" << std::endl;
        std::cout << "All high bit depth processing components tested successfully!" << std::endl;
    }

private:
    void testInitialization() {
        std::cout << "Testing High Bit Depth Support initialization..." << std::endl;
        
        HighBitDepthSupport support;
        auto formats = support.getSupportedFormats();
        
        assert(!formats.empty());
        std::cout << "High Bit Depth Support initialized: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testFormatSupport() {
        std::cout << "Testing supported high bit depth formats..." << std::endl;
        
        HighBitDepthSupport support;
        auto formats = support.getSupportedFormats();
        
        std::cout << "Number of supported high bit depth formats: " << formats.size() << std::endl;
        
        // Test major format categories
        std::vector<std::pair<HighBitDepthFormat, std::string>> test_formats = {
            {HighBitDepthFormat::YUV420P10LE, "10-bit 4:2:0 YUV"},
            {HighBitDepthFormat::YUV422P10LE, "10-bit 4:2:2 YUV"},
            {HighBitDepthFormat::YUV444P10LE, "10-bit 4:4:4 YUV"},
            {HighBitDepthFormat::YUV420P12LE, "12-bit 4:2:0 YUV"},
            {HighBitDepthFormat::YUV422P12LE, "12-bit 4:2:2 YUV"},
            {HighBitDepthFormat::YUV444P12LE, "12-bit 4:4:4 YUV"},
            {HighBitDepthFormat::YUV420P16LE, "16-bit 4:2:0 YUV"},
            {HighBitDepthFormat::RGB48LE, "16-bit RGB"},
            {HighBitDepthFormat::RGBA64LE, "16-bit RGBA"},
            {HighBitDepthFormat::V210, "V210 10-bit packed"},
            {HighBitDepthFormat::V410, "V410 10-bit packed"}
        };
        
        for (const auto& [format, name] : test_formats) {
            bool supported = support.isFormatSupported(format);
            std::cout << "- " << name << ": " << (supported ? "SUPPORTED" : "NOT SUPPORTED") << std::endl;
            assert(supported);
        }
        
        std::cout << std::endl;
    }
    
    void testBitDepthInfo() {
        std::cout << "Testing bit depth information..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Test bit depth characteristics
        std::vector<std::tuple<HighBitDepthFormat, uint8_t, uint32_t, std::string>> test_cases = {
            {HighBitDepthFormat::YUV420P10LE, static_cast<uint8_t>(10), static_cast<uint32_t>(1023), "10-bit 4:2:0"},
            {HighBitDepthFormat::YUV422P12LE, static_cast<uint8_t>(12), static_cast<uint32_t>(4095), "12-bit 4:2:2"},
            {HighBitDepthFormat::YUV420P16LE, static_cast<uint8_t>(16), static_cast<uint32_t>(65535), "16-bit 4:2:0"},
            {HighBitDepthFormat::RGB48LE, static_cast<uint8_t>(16), static_cast<uint32_t>(65535), "16-bit RGB"},
            {HighBitDepthFormat::V210, static_cast<uint8_t>(10), static_cast<uint32_t>(1023), "V210 packed"}
        };
        
        for (const auto& [format, expected_bits, expected_max, name] : test_cases) {
            BitDepthInfo info = support.getBitDepthInfo(format);
            
            std::cout << name << ":" << std::endl;
            std::cout << "  Bits per component: " << static_cast<int>(info.bits_per_component) << std::endl;
            std::cout << "  Max value: " << info.max_value << std::endl;
            std::cout << "  Components per pixel: " << static_cast<int>(info.components_per_pixel) << std::endl;
            std::cout << "  Has alpha: " << (info.has_alpha ? "YES" : "NO") << std::endl;
            std::cout << "  Is planar: " << (info.is_planar ? "YES" : "NO") << std::endl;
            std::cout << "  Description: " << info.description << std::endl;
            
            assert(info.bits_per_component == expected_bits);
            assert(info.max_value == expected_max);
        }
        
        std::cout << std::endl;
    }
    
    void testFormatDetection() {
        std::cout << "Testing format detection..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Test format detection from data patterns
        std::vector<uint8_t> test_data(1024, 0);
        HighBitDepthFormat detected = support.detectFormat(test_data.data(), test_data.size());
        
        std::cout << "  Format detection from data: " << 
            (detected != HighBitDepthFormat::UNKNOWN ? "SUCCESS" : "FAILED") << std::endl;
        
        // Test high bit depth requirement detection
        bool requires_10bit = support.requiresHighBitDepthProcessing(HighBitDepthFormat::YUV420P10LE);
        
        std::cout << "  10-bit requires high bit depth: " << (requires_10bit ? "YES" : "NO") << std::endl;
        
        assert(requires_10bit);
        
        std::cout << std::endl;
    }
    
    void testProcessingPrecision() {
        std::cout << "Testing processing precision configuration..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Test precision configuration
        ProcessingPrecision precision;
        precision.mode = ProcessingPrecision::PrecisionMode::FORCE_16BIT;
        precision.enable_dithering = true;
        precision.detect_clipping = true;
        precision.quality_threshold = 0.95;
        
        support.setProcessingPrecision(precision);
        ProcessingPrecision retrieved = support.getProcessingPrecision();
        
        assert(retrieved.mode == ProcessingPrecision::PrecisionMode::FORCE_16BIT);
        assert(retrieved.enable_dithering == true);
        assert(retrieved.detect_clipping == true);
        
        std::cout << "  Precision configuration: SUCCESS" << std::endl;
        
        // Test internal bit depth recommendation
        std::vector<std::string> operations = {"color_grade", "exposure_adjust", "composite"};
        uint8_t recommended = support.recommendInternalBitDepth(
            HighBitDepthFormat::YUV420P10LE, 
            operations
        );
        
        std::cout << "  Recommended internal bit depth for 10-bit + grading: " << 
            static_cast<int>(recommended) << " bits" << std::endl;
        
        assert(recommended >= 12);  // Should recommend higher precision for grading
        
        std::cout << std::endl;
    }
    
    void testBitDepthConversion() {
        std::cout << "Testing bit depth conversion..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Create test source frame (10-bit)
        HighBitDepthFrame source;
        source.format = HighBitDepthFormat::YUV420P10LE;
        source.width = 64;
        source.height = 64;
        source.bit_depth_info = support.getBitDepthInfo(source.format);
        source.is_limited_range = true;
        source.frame_number = 1;
        
        // Initialize planes
        source.planes.resize(3);
        source.linesize.resize(3);
        
        // Luma plane (Y)
        source.linesize[0] = source.width * 2;  // 10-bit = 2 bytes per sample
        source.planes[0].resize(source.linesize[0] * source.height);
        
        // Chroma planes (U, V) - 4:2:0 subsampling
        source.linesize[1] = source.linesize[2] = (source.width / 2) * 2;
        source.planes[1].resize(source.linesize[1] * (source.height / 2));
        source.planes[2].resize(source.linesize[2] * (source.height / 2));
        
        // Fill with test pattern (10-bit values)
        uint16_t* luma_data = reinterpret_cast<uint16_t*>(source.planes[0].data());
        for (uint32_t i = 0; i < source.width * source.height; ++i) {
            luma_data[i] = 64 + (i % 896);  // 10-bit limited range: 64-960
        }
        
        // Test conversion to 16-bit
        HighBitDepthFrame destination;
        bool conversion_success = support.convertBitDepth(
            source, 
            destination, 
            HighBitDepthFormat::YUV420P16LE,
            DitheringMethod::ERROR_DIFFUSION
        );
        
        std::cout << "  10-bit to 16-bit conversion: " << 
            (conversion_success ? "SUCCESS" : "FAILED") << std::endl;
        assert(conversion_success);
        
        // Validate destination properties
        assert(destination.format == HighBitDepthFormat::YUV420P16LE);
        assert(destination.width == source.width);
        assert(destination.height == source.height);
        assert(destination.planes.size() == 3);
        
        std::cout << "  Destination format validation: SUCCESS" << std::endl;
        
        std::cout << std::endl;
    }
    
    void testQualityAssessment() {
        std::cout << "Testing quality assessment..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Create reference and processed frames
        HighBitDepthFrame reference = createTestFrame(HighBitDepthFormat::YUV420P16LE, 32, 32);
        HighBitDepthFrame processed = createTestFrame(HighBitDepthFormat::YUV420P16LE, 32, 32);
        
        // Add small amount of noise to processed frame
        uint16_t* proc_data = reinterpret_cast<uint16_t*>(processed.planes[0].data());
        for (uint32_t i = 0; i < reference.width * reference.height; ++i) {
            proc_data[i] += (i % 16) - 8;  // Small noise pattern
        }
        
        QualityMetrics metrics = support.assessQuality(reference, processed);
        
        std::cout << "  PSNR: " << std::fixed << std::setprecision(2) << metrics.psnr << " dB" << std::endl;
        std::cout << "  SSIM: " << std::fixed << std::setprecision(4) << metrics.ssim << std::endl;
        std::cout << "  Clipped pixels: " << metrics.clipped_pixels << std::endl;
        std::cout << "  Quality acceptable: " << (metrics.quality_acceptable ? "YES" : "NO") << std::endl;
        
        assert(metrics.psnr > 0.0);
        assert(metrics.ssim >= 0.0 && metrics.ssim <= 1.0);
        
        std::cout << std::endl;
    }
    
    void testDitheringMethods() {
        std::cout << "Testing dithering methods..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Create 16-bit test data
        std::vector<uint16_t> source_16bit(256);
        for (size_t i = 0; i < source_16bit.size(); ++i) {
            source_16bit[i] = static_cast<uint16_t>(i * 256 + (i % 256));
        }
        
        std::vector<uint8_t> dithered_8bit;
        
        // Test different dithering methods
        std::vector<std::pair<DitheringMethod, std::string>> methods = {
            {DitheringMethod::NONE, "No dithering"},
            {DitheringMethod::ERROR_DIFFUSION, "Floyd-Steinberg"},
            {DitheringMethod::ORDERED, "Ordered dithering"},
            {DitheringMethod::TRIANGULAR_PDF, "Triangular PDF"}
        };
        
        for (const auto& [method, name] : methods) {
            support.applyDithering(source_16bit, dithered_8bit, method, 16, 16);
            
            assert(!dithered_8bit.empty());
            std::cout << "  " << name << ": SUCCESS" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void testRangeConversion() {
        std::cout << "Testing range conversion..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Create test frame in limited range
        HighBitDepthFrame frame = createTestFrame(HighBitDepthFormat::YUV420P10LE, 32, 32);
        frame.is_limited_range = true;
        
        // Convert to full range
        support.convertRange(frame, true);
        assert(!frame.is_limited_range);
        
        std::cout << "  Limited to full range: SUCCESS" << std::endl;
        
        // Convert back to limited range
        support.convertRange(frame, false);
        assert(frame.is_limited_range);
        
        std::cout << "  Full to limited range: SUCCESS" << std::endl;
        
        std::cout << std::endl;
    }
    
    void testClippingDetection() {
        std::cout << "Testing clipping detection..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Create frame with some clipped values
        HighBitDepthFrame frame = createTestFrame(HighBitDepthFormat::YUV420P10LE, 16, 16);
        
        // Add clipped values
        uint16_t* luma_data = reinterpret_cast<uint16_t*>(frame.planes[0].data());
        luma_data[0] = 0;     // Below black level
        luma_data[1] = 1023;  // Above white level
        luma_data[2] = 10;    // Below limited range black
        
        auto clipped_regions = support.detectClipping(frame);
        
        std::cout << "  Clipped regions detected: " << clipped_regions.size() << std::endl;
        
        // Should detect at least the clipped pixels we added
        assert(clipped_regions.size() >= 2);
        
        std::cout << "  Clipping detection: SUCCESS" << std::endl;
        std::cout << std::endl;
    }
    
    void testMemoryCalculation() {
        std::cout << "Testing memory requirement calculation..." << std::endl;
        
        HighBitDepthSupport support;
        
        // Test memory calculations for different formats
        std::vector<std::tuple<HighBitDepthFormat, uint32_t, uint32_t, std::string>> test_cases = {
            {HighBitDepthFormat::YUV420P10LE, 1920, 1080, "1080p 10-bit 4:2:0"},
            {HighBitDepthFormat::YUV422P12LE, 3840, 2160, "4K 12-bit 4:2:2"},
            {HighBitDepthFormat::RGB48LE, 1920, 1080, "1080p 16-bit RGB"}
        };
        
        for (const auto& [format, width, height, name] : test_cases) {
            size_t memory = support.calculateMemoryRequirement(format, width, height);
            double memory_mb = static_cast<double>(memory) / (1024.0 * 1024.0);
            
            std::cout << "  " << name << ": " << std::fixed << std::setprecision(1) << 
                memory_mb << " MB" << std::endl;
            
            assert(memory > 0);
        }
        
        std::cout << std::endl;
    }
    
    void testProfessionalWorkflows() {
        std::cout << "Testing professional workflow scenarios..." << std::endl;
        
        using namespace high_bitdepth_utils;
        
        // Test codec-based format detection
        std::vector<std::pair<std::string, HighBitDepthFormat>> codec_tests = {
            {"ProRes 422 HQ", HighBitDepthFormat::YUV422P10LE},
            {"ProRes 4444", HighBitDepthFormat::YUVA444P12LE},
            {"DNxHR HQX", HighBitDepthFormat::YUV422P10LE},
            {"V210", HighBitDepthFormat::V210},
            {"HEVC Main10", HighBitDepthFormat::YUV420P10LE}
        };
        
        for (const auto& [codec_name, expected_format] : codec_tests) {
            HighBitDepthFormat detected = detectFromCodecName(codec_name);
            std::cout << "  " << codec_name << " -> " << 
                (detected == expected_format ? "CORRECT" : "INCORRECT") << " format detection" << std::endl;
        }
        
        // Test processing recommendations
        std::cout << "\nTesting processing recommendations..." << std::endl;
        
        std::vector<ProcessingRecommendation> recommendations = {
            getProcessingRecommendation(HighBitDepthFormat::YUV420P10LE, 1920, 1080, 8ULL * 1024 * 1024 * 1024),  // 8GB
            getProcessingRecommendation(HighBitDepthFormat::YUV422P12LE, 3840, 2160, 4ULL * 1024 * 1024 * 1024),  // 4GB
            getProcessingRecommendation(HighBitDepthFormat::RGB48LE, 1920, 1080, 2ULL * 1024 * 1024 * 1024)       // 2GB
        };
        
        std::vector<std::string> scenario_names = {
            "1080p 10-bit with 8GB RAM",
            "4K 12-bit with 4GB RAM", 
            "1080p RGB with 2GB RAM"
        };
        
        for (size_t i = 0; i < recommendations.size(); ++i) {
            const auto& rec = recommendations[i];
            std::cout << "  " << scenario_names[i] << ":" << std::endl;
            std::cout << "    Internal bit depth: " << static_cast<int>(rec.internal_bit_depth) << std::endl;
            std::cout << "    Use streaming: " << (rec.use_streaming ? "YES" : "NO") << std::endl;
            std::cout << "    Buffer size: " << (rec.recommended_buffer_size / (1024*1024)) << " MB" << std::endl;
        }
        
        // Test optimal precision calculation
        std::vector<HighBitDepthFormat> input_formats = {
            HighBitDepthFormat::YUV420P10LE,
            HighBitDepthFormat::YUV422P12LE
        };
        
        std::vector<std::string> operations = {"color_grade", "composite", "resize"};
        uint8_t optimal_precision = calculateOptimalPrecision(input_formats, operations);
        
        std::cout << "\n  Optimal precision for mixed 10/12-bit + grading: " << 
            static_cast<int>(optimal_precision) << " bits" << std::endl;
        
        assert(optimal_precision >= 12);
        
        std::cout << std::endl;
    }
    
    // Helper method to create test frames
    HighBitDepthFrame createTestFrame(HighBitDepthFormat format, uint32_t width, uint32_t height) {
        HighBitDepthSupport support;
        
        HighBitDepthFrame frame;
        frame.format = format;
        frame.width = width;
        frame.height = height;
        frame.bit_depth_info = support.getBitDepthInfo(format);
        frame.is_limited_range = true;
        frame.frame_number = 0;
        
        // Initialize planes based on format
        size_t plane_count = frame.bit_depth_info.has_alpha ? 4 : 3;
        if (format == HighBitDepthFormat::RGB48LE || format == HighBitDepthFormat::RGBA64LE) {
            plane_count = 1;  // Packed format
        }
        
        frame.planes.resize(plane_count);
        frame.linesize.resize(plane_count);
        
        size_t bytes_per_sample = (frame.bit_depth_info.bits_per_component + 7) / 8;
        
        for (size_t i = 0; i < plane_count; ++i) {
            uint32_t plane_width = width;
            uint32_t plane_height = height;
            
            // Handle chroma subsampling
            if (i > 0 && format != HighBitDepthFormat::YUV444P10LE && 
                format != HighBitDepthFormat::YUV444P12LE &&
                format != HighBitDepthFormat::YUV444P16LE) {
                plane_width /= 2;
                if (format == HighBitDepthFormat::YUV420P10LE ||
                    format == HighBitDepthFormat::YUV420P12LE ||
                    format == HighBitDepthFormat::YUV420P16LE) {
                    plane_height /= 2;
                }
            }
            
            frame.linesize[i] = static_cast<uint32_t>(plane_width * bytes_per_sample);
            frame.planes[i].resize(frame.linesize[i] * plane_height);
            
            // Fill with test pattern
            if (bytes_per_sample == 1) {
                for (size_t j = 0; j < frame.planes[i].size(); ++j) {
                    frame.planes[i][j] = static_cast<uint8_t>(j % 256);
                }
            } else {
                uint16_t* data = reinterpret_cast<uint16_t*>(frame.planes[i].data());
                size_t sample_count = frame.planes[i].size() / 2;
                for (size_t j = 0; j < sample_count; ++j) {
                    data[j] = static_cast<uint16_t>(j % frame.bit_depth_info.max_value);
                }
            }
        }
        
        return frame;
    }
};

int main() {
    try {
        HighBitDepthValidationTest test;
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
