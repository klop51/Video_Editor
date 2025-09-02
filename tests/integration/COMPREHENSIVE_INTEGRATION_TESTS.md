# Comprehensive Integration Test Suite

> Complete integration test framework for validating the professional video format support system.

## 📋 **Test Suite Overview**

This comprehensive test suite validates all aspects of the professional video format support system, ensuring production readiness through systematic testing of codecs, containers, quality validation, standards compliance, and performance characteristics.

### **Test Categories**
- **Format Support Tests**: Codec and container compatibility
- **Quality Validation Tests**: Professional standards compliance
- **Performance Tests**: Encoding/decoding speed and efficiency
- **Integration Tests**: Component interaction and workflow validation
- **Stress Tests**: System stability under load
- **Regression Tests**: Backward compatibility verification

---

## 🧪 **Core Test Framework**

### **Test Infrastructure**
```cpp
// File: tests/integration/include/integration_test_framework.hpp
#pragma once

#include "core/test_framework.hpp"
#include "media_io/format_info.hpp"
#include "quality/validation_result.hpp"
#include <chrono>
#include <memory>

namespace ve::tests::integration {

class IntegrationTestFramework {
public:
    struct TestConfiguration {
        std::string test_data_directory;
        std::string output_directory;
        std::string reference_directory;
        bool enable_hardware_acceleration = true;
        bool generate_detailed_reports = true;
        std::chrono::seconds test_timeout{300};
        uint32_t max_concurrent_tests = 4;
    };
    
    struct TestResult {
        std::string test_name;
        bool passed = false;
        std::chrono::milliseconds execution_time{0};
        std::string error_message;
        std::map<std::string, double> metrics;
        std::vector<std::string> warnings;
        quality::ValidationResult quality_result;
    };
    
    // Test execution
    static std::vector<TestResult> runAllTests(const TestConfiguration& config);
    static std::vector<TestResult> runTestCategory(const std::string& category,
                                                  const TestConfiguration& config);
    static TestResult runSingleTest(const std::string& test_name,
                                   const TestConfiguration& config);
    
    // Test discovery
    static std::vector<std::string> discoverTests();
    static std::vector<std::string> getTestCategories();
    static std::vector<std::string> getTestsInCategory(const std::string& category);
    
    // Report generation
    static std::string generateReport(const std::vector<TestResult>& results);
    static void generateHTMLReport(const std::vector<TestResult>& results,
                                  const std::string& output_file);
    static void generateJUnitReport(const std::vector<TestResult>& results,
                                   const std::string& output_file);

private:
    static bool setupTestEnvironment(const TestConfiguration& config);
    static void cleanupTestEnvironment();
    static std::vector<std::string> getTestFiles(const std::string& pattern);
};

} // namespace ve::tests::integration
```

---

## 🎬 **Format Support Integration Tests**

### **Codec Compatibility Tests**
```cpp
// File: tests/integration/codec_compatibility_tests.cpp
#include "integration_test_framework.hpp"
#include "decode/decoder_factory.hpp"
#include "render/encoder_factory.hpp"

namespace ve::tests::integration {

class CodecCompatibilityTests {
public:
    static std::vector<TestResult> runAllCodecTests(const TestConfiguration& config) {
        std::vector<TestResult> results;
        
        // Test all supported video codecs
        auto video_codecs = {
            "H.264", "HEVC", "AV1", "VP9", "VP8", "MPEG-2", "MPEG-4",
            "ProRes", "DNxHD", "XDCAM", "XAVC", "HAP", "Uncompressed"
        };
        
        for (const auto& codec : video_codecs) {
            results.push_back(testVideoCodecDecoding(codec, config));
            results.push_back(testVideoCodecEncoding(codec, config));
            results.push_back(testVideoCodecRoundTrip(codec, config));
        }
        
        // Test all supported audio codecs
        auto audio_codecs = {
            "AAC", "MP3", "FLAC", "PCM", "Opus", "Vorbis", "AC-3", "DTS",
            "ALAC", "WMA", "AMR"
        };
        
        for (const auto& codec : audio_codecs) {
            results.push_back(testAudioCodecDecoding(codec, config));
            results.push_back(testAudioCodecEncoding(codec, config));
            results.push_back(testAudioCodecRoundTrip(codec, config));
        }
        
        return results;
    }
    
private:
    static TestResult testVideoCodecDecoding(const std::string& codec_name,
                                           const TestConfiguration& config) {
        TestResult result;
        result.test_name = "VideoCodecDecoding_" + codec_name;
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            // Find test file for codec
            std::string test_file = findTestFile(codec_name, config.test_data_directory);
            if (test_file.empty()) {
                result.error_message = "No test file found for codec: " + codec_name;
                return result;
            }
            
            // Create decoder
            auto decoder = decode::DecoderFactory::createDecoder(codec_name);
            if (!decoder) {
                result.error_message = "Failed to create decoder for: " + codec_name;
                return result;
            }
            
            // Configure decoder
            decode::DecoderConfig config_dec;
            config_dec.input_file = test_file;
            config_dec.enable_hardware_acceleration = config.enable_hardware_acceleration;
            
            if (!decoder->initialize(config_dec)) {
                result.error_message = "Failed to initialize decoder for: " + codec_name;
                return result;
            }
            
            // Decode frames
            uint32_t frame_count = 0;
            uint64_t total_decode_time = 0;
            
            while (decoder->hasMoreFrames()) {
                auto frame_start = std::chrono::steady_clock::now();
                auto frame = decoder->decodeNextFrame();
                auto frame_end = std::chrono::steady_clock::now();
                
                if (!frame) {
                    result.error_message = "Failed to decode frame " + std::to_string(frame_count);
                    return result;
                }
                
                frame_count++;
                total_decode_time += std::chrono::duration_cast<std::chrono::microseconds>(
                    frame_end - frame_start).count();
                
                // Validate frame properties
                if (!validateDecodedFrame(frame.get(), codec_name)) {
                    result.error_message = "Invalid frame properties at frame " + std::to_string(frame_count);
                    return result;
                }
            }
            
            // Calculate metrics
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["frames_decoded"] = frame_count;
            result.metrics["avg_decode_time_us"] = static_cast<double>(total_decode_time) / frame_count;
            result.metrics["fps"] = frame_count * 1000.0 / result.execution_time.count();
            
            result.passed = true;
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during decoding: " + std::string(e.what());
        }
        
        return result;
    }
    
    static TestResult testVideoCodecEncoding(const std::string& codec_name,
                                           const TestConfiguration& config) {
        TestResult result;
        result.test_name = "VideoCodecEncoding_" + codec_name;
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            // Create encoder
            auto encoder = render::EncoderFactory::createEncoder(codec_name);
            if (!encoder) {
                result.error_message = "Failed to create encoder for: " + codec_name;
                return result;
            }
            
            // Configure encoder
            render::EncoderConfig config_enc;
            config_enc.output_file = config.output_directory + "/test_" + codec_name + ".mp4";
            config_enc.width = 1920;
            config_enc.height = 1080;
            config_enc.frame_rate = 30.0;
            config_enc.bitrate_kbps = 5000;
            config_enc.enable_hardware_acceleration = config.enable_hardware_acceleration;
            
            if (!encoder->initialize(config_enc)) {
                result.error_message = "Failed to initialize encoder for: " + codec_name;
                return result;
            }
            
            // Generate test frames and encode
            uint32_t frame_count = 60; // 2 seconds at 30fps
            uint64_t total_encode_time = 0;
            
            for (uint32_t i = 0; i < frame_count; ++i) {
                auto test_frame = generateTestFrame(1920, 1080, i);
                
                auto frame_start = std::chrono::steady_clock::now();
                auto encoded_data = encoder->encodeFrame(*test_frame);
                auto frame_end = std::chrono::steady_clock::now();
                
                if (encoded_data.data == nullptr || encoded_data.size == 0) {
                    result.error_message = "Failed to encode frame " + std::to_string(i);
                    return result;
                }
                
                total_encode_time += std::chrono::duration_cast<std::chrono::microseconds>(
                    frame_end - frame_start).count();
            }
            
            if (!encoder->finalize()) {
                result.error_message = "Failed to finalize encoder";
                return result;
            }
            
            // Calculate metrics
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["frames_encoded"] = frame_count;
            result.metrics["avg_encode_time_us"] = static_cast<double>(total_encode_time) / frame_count;
            result.metrics["fps"] = frame_count * 1000.0 / result.execution_time.count();
            
            // Validate output file
            if (!validateEncodedFile(config_enc.output_file, codec_name)) {
                result.error_message = "Output file validation failed";
                return result;
            }
            
            result.passed = true;
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during encoding: " + std::string(e.what());
        }
        
        return result;
    }
    
    static TestResult testVideoCodecRoundTrip(const std::string& codec_name,
                                            const TestConfiguration& config) {
        TestResult result;
        result.test_name = "VideoCodecRoundTrip_" + codec_name;
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            // Phase 1: Encode test sequence
            std::string encoded_file = config.output_directory + "/roundtrip_" + codec_name + ".mp4";
            
            auto encoder = render::EncoderFactory::createEncoder(codec_name);
            if (!encoder) {
                result.error_message = "Failed to create encoder";
                return result;
            }
            
            render::EncoderConfig enc_config;
            enc_config.output_file = encoded_file;
            enc_config.width = 1280;
            enc_config.height = 720;
            enc_config.frame_rate = 25.0;
            enc_config.bitrate_kbps = 3000;
            
            if (!encoder->initialize(enc_config)) {
                result.error_message = "Failed to initialize encoder";
                return result;
            }
            
            // Encode known test pattern
            std::vector<std::unique_ptr<core::VideoFrame>> original_frames;
            for (uint32_t i = 0; i < 50; ++i) {
                auto frame = generateTestPattern(1280, 720, i);
                original_frames.push_back(std::move(frame));
                encoder->encodeFrame(*original_frames.back());
            }
            
            encoder->finalize();
            
            // Phase 2: Decode and compare
            auto decoder = decode::DecoderFactory::createDecoder(codec_name);
            if (!decoder) {
                result.error_message = "Failed to create decoder";
                return result;
            }
            
            decode::DecoderConfig dec_config;
            dec_config.input_file = encoded_file;
            
            if (!decoder->initialize(dec_config)) {
                result.error_message = "Failed to initialize decoder";
                return result;
            }
            
            // Compare decoded frames with originals
            uint32_t frame_index = 0;
            double total_psnr = 0.0;
            double total_ssim = 0.0;
            
            while (decoder->hasMoreFrames() && frame_index < original_frames.size()) {
                auto decoded_frame = decoder->decodeNextFrame();
                if (!decoded_frame) {
                    result.error_message = "Failed to decode frame " + std::to_string(frame_index);
                    return result;
                }
                
                // Calculate quality metrics
                double psnr = calculatePSNR(original_frames[frame_index].get(), decoded_frame.get());
                double ssim = calculateSSIM(original_frames[frame_index].get(), decoded_frame.get());
                
                total_psnr += psnr;
                total_ssim += ssim;
                frame_index++;
            }
            
            // Calculate average quality
            double avg_psnr = total_psnr / frame_index;
            double avg_ssim = total_ssim / frame_index;
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["frames_processed"] = frame_index;
            result.metrics["avg_psnr"] = avg_psnr;
            result.metrics["avg_ssim"] = avg_ssim;
            
            // Quality thresholds (codec-dependent)
            double min_psnr = getMinimumPSNR(codec_name);
            double min_ssim = getMinimumSSIM(codec_name);
            
            if (avg_psnr < min_psnr) {
                result.error_message = "PSNR below threshold: " + std::to_string(avg_psnr) + " < " + std::to_string(min_psnr);
                return result;
            }
            
            if (avg_ssim < min_ssim) {
                result.error_message = "SSIM below threshold: " + std::to_string(avg_ssim) + " < " + std::to_string(min_ssim);
                return result;
            }
            
            result.passed = true;
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during round-trip test: " + std::string(e.what());
        }
        
        return result;
    }
    
    // Helper methods
    static std::string findTestFile(const std::string& codec_name, const std::string& test_dir);
    static bool validateDecodedFrame(const core::VideoFrame* frame, const std::string& codec_name);
    static bool validateEncodedFile(const std::string& file_path, const std::string& codec_name);
    static std::unique_ptr<core::VideoFrame> generateTestFrame(uint32_t width, uint32_t height, uint32_t frame_number);
    static std::unique_ptr<core::VideoFrame> generateTestPattern(uint32_t width, uint32_t height, uint32_t frame_number);
    static double calculatePSNR(const core::VideoFrame* original, const core::VideoFrame* decoded);
    static double calculateSSIM(const core::VideoFrame* original, const core::VideoFrame* decoded);
    static double getMinimumPSNR(const std::string& codec_name);
    static double getMinimumSSIM(const std::string& codec_name);
};

} // namespace ve::tests::integration
```

### **Container Format Tests**
```cpp
// File: tests/integration/container_format_tests.cpp
#include "integration_test_framework.hpp"
#include "media_io/format_detector.hpp"
#include "media_io/container_parser.hpp"

namespace ve::tests::integration {

class ContainerFormatTests {
public:
    static std::vector<TestResult> runAllContainerTests(const TestConfiguration& config) {
        std::vector<TestResult> results;
        
        // Test container formats
        auto containers = {
            "MP4", "MOV", "MKV", "AVI", "MTS", "M2TS", "TS", "MXF", 
            "WebM", "OGV", "FLV", "3GP", "WMV", "ASF"
        };
        
        for (const auto& container : containers) {
            results.push_back(testContainerParsing(container, config));
            results.push_back(testContainerWriting(container, config));
            results.push_back(testContainerMetadata(container, config));
        }
        
        // Test format detection
        results.push_back(testFormatDetection(config));
        results.push_back(testMixedContainerSupport(config));
        
        return results;
    }
    
private:
    static TestResult testContainerParsing(const std::string& container_format,
                                         const TestConfiguration& config) {
        TestResult result;
        result.test_name = "ContainerParsing_" + container_format;
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            // Find test files for container format
            auto test_files = findContainerTestFiles(container_format, config.test_data_directory);
            if (test_files.empty()) {
                result.error_message = "No test files found for container: " + container_format;
                return result;
            }
            
            uint32_t files_processed = 0;
            uint64_t total_parse_time = 0;
            
            for (const auto& test_file : test_files) {
                auto parse_start = std::chrono::steady_clock::now();
                
                // Parse container
                auto parser = media_io::ContainerParserFactory::createParser(container_format);
                if (!parser) {
                    result.error_message = "Failed to create parser for: " + container_format;
                    return result;
                }
                
                if (!parser->openContainer(test_file)) {
                    result.error_message = "Failed to open container: " + test_file;
                    return result;
                }
                
                // Validate container structure
                uint32_t stream_count = parser->getStreamCount();
                if (stream_count == 0) {
                    result.error_message = "No streams found in: " + test_file;
                    return result;
                }
                
                // Read and validate streams
                for (uint32_t i = 0; i < stream_count; ++i) {
                    auto stream_info = parser->getStreamInfo(i);
                    if (!validateStreamInfo(stream_info, container_format)) {
                        result.error_message = "Invalid stream info for stream " + std::to_string(i);
                        return result;
                    }
                }
                
                // Test packet reading
                uint32_t packets_read = 0;
                while (auto packet = parser->readNextPacket()) {
                    if (!validatePacket(packet.get())) {
                        result.error_message = "Invalid packet at index " + std::to_string(packets_read);
                        return result;
                    }
                    packets_read++;
                    
                    // Limit packet reading for performance
                    if (packets_read >= 100) break;
                }
                
                parser->closeContainer();
                
                auto parse_end = std::chrono::steady_clock::now();
                total_parse_time += std::chrono::duration_cast<std::chrono::microseconds>(
                    parse_end - parse_start).count();
                files_processed++;
            }
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["files_processed"] = files_processed;
            result.metrics["avg_parse_time_us"] = static_cast<double>(total_parse_time) / files_processed;
            
            result.passed = true;
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during container parsing: " + std::string(e.what());
        }
        
        return result;
    }
    
    static TestResult testFormatDetection(const TestConfiguration& config) {
        TestResult result;
        result.test_name = "FormatDetection";
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            auto detector = media_io::FormatDetector::getInstance();
            
            // Test files with known formats
            struct TestCase {
                std::string file_path;
                std::string expected_format;
                float min_confidence;
            };
            
            std::vector<TestCase> test_cases = {
                {config.test_data_directory + "/test.mp4", "MP4", 0.95f},
                {config.test_data_directory + "/test.mov", "MOV", 0.95f},
                {config.test_data_directory + "/test.mkv", "MKV", 0.95f},
                {config.test_data_directory + "/test.avi", "AVI", 0.90f},
                {config.test_data_directory + "/test.mts", "MTS", 0.90f}
            };
            
            uint32_t correct_detections = 0;
            uint32_t total_detections = 0;
            
            for (const auto& test_case : test_cases) {
                if (!std::filesystem::exists(test_case.file_path)) {
                    continue; // Skip if test file doesn't exist
                }
                
                auto format_info = detector->analyzeFormat(test_case.file_path);
                float confidence = detector->getDetectionConfidence(test_case.file_path);
                
                total_detections++;
                
                if (format_info.container_format == test_case.expected_format &&
                    confidence >= test_case.min_confidence) {
                    correct_detections++;
                } else {
                    result.warnings.push_back(
                        "Incorrect detection for " + test_case.file_path + 
                        ": expected " + test_case.expected_format + 
                        ", got " + format_info.container_format +
                        " (confidence: " + std::to_string(confidence) + ")"
                    );
                }
            }
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["total_detections"] = total_detections;
            result.metrics["correct_detections"] = correct_detections;
            result.metrics["accuracy"] = total_detections > 0 ? 
                static_cast<double>(correct_detections) / total_detections : 0.0;
            
            // Require at least 90% accuracy
            result.passed = (result.metrics["accuracy"] >= 0.90);
            if (!result.passed) {
                result.error_message = "Format detection accuracy below threshold: " + 
                    std::to_string(result.metrics["accuracy"]);
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during format detection: " + std::string(e.what());
        }
        
        return result;
    }
    
    // Helper methods
    static std::vector<std::string> findContainerTestFiles(const std::string& container_format, 
                                                          const std::string& test_dir);
    static bool validateStreamInfo(const media_io::StreamInfo& stream_info, 
                                 const std::string& container_format);
    static bool validatePacket(const media_io::MediaPacket* packet);
};

} // namespace ve::tests::integration
```

---

## 📊 **Quality Validation Tests**

### **Professional Standards Compliance Tests**
```cpp
// File: tests/integration/quality_validation_tests.cpp
#include "integration_test_framework.hpp"
#include "quality/format_validator.hpp"
#include "standards/compliance_engine.hpp"

namespace ve::tests::integration {

class QualityValidationTests {
public:
    static std::vector<TestResult> runAllQualityTests(const TestConfiguration& config) {
        std::vector<TestResult> results;
        
        // Test quality validation for different content types
        results.push_back(testBroadcastQuality(config));
        results.push_back(testStreamingQuality(config));
        results.push_back(testCinemaQuality(config));
        results.push_back(testArchivalQuality(config));
        
        // Test standards compliance
        results.push_back(testITUCompliance(config));
        results.push_back(testSMPTECompliance(config));
        results.push_back(testEBUCompliance(config));
        results.push_back(testATSCCompliance(config));
        
        // Test quality metrics
        results.push_back(testQualityMetrics(config));
        results.push_back(testQualityThresholds(config));
        
        return results;
    }
    
private:
    static TestResult testBroadcastQuality(const TestConfiguration& config) {
        TestResult result;
        result.test_name = "BroadcastQualityValidation";
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            auto validator = quality::FormatValidator::getInstance();
            
            // Test broadcast-compliant content
            std::vector<std::string> broadcast_files = {
                config.test_data_directory + "/broadcast_1080i25.ts",
                config.test_data_directory + "/broadcast_720p50.ts",
                config.test_data_directory + "/broadcast_4k2160p25.ts"
            };
            
            uint32_t files_validated = 0;
            uint32_t passed_validation = 0;
            
            for (const auto& file_path : broadcast_files) {
                if (!std::filesystem::exists(file_path)) {
                    continue;
                }
                
                auto validation_result = validator->validateForBroadcast(
                    file_path, 
                    quality::ValidationLevel::PROFESSIONAL
                );
                
                files_validated++;
                
                // Check broadcast-specific requirements
                bool meets_broadcast_standards = true;
                
                // Resolution check
                if (!isBroadcastResolution(validation_result.format_info.width,
                                         validation_result.format_info.height)) {
                    meets_broadcast_standards = false;
                    result.warnings.push_back("Non-broadcast resolution: " + file_path);
                }
                
                // Frame rate check
                if (!isBroadcastFrameRate(validation_result.format_info.frame_rate)) {
                    meets_broadcast_standards = false;
                    result.warnings.push_back("Non-broadcast frame rate: " + file_path);
                }
                
                // Color space check
                if (!isBroadcastColorSpace(validation_result.format_info.color_space)) {
                    meets_broadcast_standards = false;
                    result.warnings.push_back("Non-broadcast color space: " + file_path);
                }
                
                // Audio levels check
                if (!areBroadcastAudioLevels(validation_result.audio_levels)) {
                    meets_broadcast_standards = false;
                    result.warnings.push_back("Non-compliant audio levels: " + file_path);
                }
                
                if (meets_broadcast_standards && validation_result.overall_score >= 0.95) {
                    passed_validation++;
                }
            }
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["files_validated"] = files_validated;
            result.metrics["passed_validation"] = passed_validation;
            result.metrics["pass_rate"] = files_validated > 0 ? 
                static_cast<double>(passed_validation) / files_validated : 0.0;
            
            result.passed = (result.metrics["pass_rate"] >= 0.90);
            if (!result.passed) {
                result.error_message = "Broadcast validation pass rate below threshold";
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during broadcast quality validation: " + std::string(e.what());
        }
        
        return result;
    }
    
    static TestResult testQualityMetrics(const TestConfiguration& config) {
        TestResult result;
        result.test_name = "QualityMetricsValidation";
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            // Create reference and test content with known quality differences
            std::string reference_file = config.reference_directory + "/reference_1080p.mp4";
            std::string test_file = config.test_data_directory + "/compressed_1080p.mp4";
            
            if (!std::filesystem::exists(reference_file) || !std::filesystem::exists(test_file)) {
                result.error_message = "Required test files not found";
                return result;
            }
            
            auto quality_analyzer = quality::QualityAnalyzer::getInstance();
            
            // Calculate objective quality metrics
            auto metrics = quality_analyzer->compareFiles(reference_file, test_file);
            
            result.metrics["psnr"] = metrics.psnr;
            result.metrics["ssim"] = metrics.ssim;
            result.metrics["vmaf"] = metrics.vmaf;
            result.metrics["psnr_hvs"] = metrics.psnr_hvs;
            result.metrics["msssim"] = metrics.ms_ssim;
            
            // Validate metrics are within expected ranges
            bool metrics_valid = true;
            
            if (metrics.psnr < 30.0 || metrics.psnr > 60.0) {
                result.warnings.push_back("PSNR out of expected range: " + std::to_string(metrics.psnr));
                metrics_valid = false;
            }
            
            if (metrics.ssim < 0.8 || metrics.ssim > 1.0) {
                result.warnings.push_back("SSIM out of expected range: " + std::to_string(metrics.ssim));
                metrics_valid = false;
            }
            
            if (metrics.vmaf < 50.0 || metrics.vmaf > 100.0) {
                result.warnings.push_back("VMAF out of expected range: " + std::to_string(metrics.vmaf));
                metrics_valid = false;
            }
            
            // Test perceptual quality assessment
            auto perceptual_score = quality_analyzer->assessPerceptualQuality(test_file);
            result.metrics["perceptual_score"] = perceptual_score;
            
            if (perceptual_score < 0.0 || perceptual_score > 1.0) {
                result.warnings.push_back("Perceptual score out of range: " + std::to_string(perceptual_score));
                metrics_valid = false;
            }
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            result.passed = metrics_valid;
            if (!result.passed) {
                result.error_message = "Quality metrics validation failed";
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during quality metrics validation: " + std::string(e.what());
        }
        
        return result;
    }
    
    static TestResult testITUCompliance(const TestConfiguration& config) {
        TestResult result;
        result.test_name = "ITU_Compliance";
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            auto compliance_engine = standards::ComplianceEngine::getInstance();
            
            // Test ITU-R BT.709 compliance
            std::vector<std::string> itu_test_files = {
                config.test_data_directory + "/itu_bt709_test.mp4",
                config.test_data_directory + "/itu_bt2020_test.mp4"
            };
            
            uint32_t files_tested = 0;
            uint32_t compliant_files = 0;
            
            for (const auto& file_path : itu_test_files) {
                if (!std::filesystem::exists(file_path)) {
                    continue;
                }
                
                auto compliance_result = compliance_engine->checkCompliance(
                    file_path, 
                    "ITU-R_BT.709"
                );
                
                files_tested++;
                
                if (compliance_result.is_compliant && compliance_result.compliance_score >= 0.95) {
                    compliant_files++;
                } else {
                    result.warnings.push_back(
                        "ITU compliance failed for: " + file_path + 
                        " (score: " + std::to_string(compliance_result.compliance_score) + ")"
                    );
                }
            }
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["files_tested"] = files_tested;
            result.metrics["compliant_files"] = compliant_files;
            result.metrics["compliance_rate"] = files_tested > 0 ? 
                static_cast<double>(compliant_files) / files_tested : 0.0;
            
            result.passed = (result.metrics["compliance_rate"] >= 0.90);
            if (!result.passed) {
                result.error_message = "ITU compliance rate below threshold";
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during ITU compliance testing: " + std::string(e.what());
        }
        
        return result;
    }
    
    // Helper methods
    static bool isBroadcastResolution(uint32_t width, uint32_t height);
    static bool isBroadcastFrameRate(double frame_rate);
    static bool isBroadcastColorSpace(const std::string& color_space);
    static bool areBroadcastAudioLevels(const std::vector<double>& audio_levels);
};

} // namespace ve::tests::integration
```

---

## ⚡ **Performance Integration Tests**

### **Performance Benchmarking Tests**
```cpp
// File: tests/integration/performance_tests.cpp
#include "integration_test_framework.hpp"
#include "core/performance_profiler.hpp"

namespace ve::tests::integration {

class PerformanceTests {
public:
    static std::vector<TestResult> runAllPerformanceTests(const TestConfiguration& config) {
        std::vector<TestResult> results;
        
        // Encoding performance tests
        results.push_back(testEncodingPerformance(config));
        results.push_back(testDecodingPerformance(config));
        results.push_back(testTranscodingPerformance(config));
        
        // Stress tests
        results.push_back(testConcurrentOperations(config));
        results.push_back(testMemoryUsage(config));
        results.push_back(testLongRunningOperations(config));
        
        // Hardware acceleration tests
        results.push_back(testHardwareAcceleration(config));
        results.push_back(testMultiGPUPerformance(config));
        
        return results;
    }
    
private:
    static TestResult testEncodingPerformance(const TestConfiguration& config) {
        TestResult result;
        result.test_name = "EncodingPerformanceBenchmark";
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            auto profiler = core::PerformanceProfiler::getInstance();
            profiler->startProfiling("encoding_benchmark");
            
            // Test encoding performance for different resolutions and codecs
            struct EncodingTest {
                std::string codec;
                uint32_t width;
                uint32_t height;
                double expected_fps;
            };
            
            std::vector<EncodingTest> encoding_tests = {
                {"H.264", 1920, 1080, 30.0},
                {"H.264", 3840, 2160, 10.0},
                {"HEVC", 1920, 1080, 25.0},
                {"HEVC", 3840, 2160, 8.0},
                {"AV1", 1920, 1080, 5.0}
            };
            
            for (const auto& test : encoding_tests) {
                auto encoder = render::EncoderFactory::createEncoder(test.codec);
                if (!encoder) {
                    result.warnings.push_back("Encoder not available: " + test.codec);
                    continue;
                }
                
                render::EncoderConfig enc_config;
                enc_config.width = test.width;
                enc_config.height = test.height;
                enc_config.frame_rate = 30.0;
                enc_config.bitrate_kbps = calculateTargetBitrate(test.width, test.height);
                enc_config.enable_hardware_acceleration = config.enable_hardware_acceleration;
                
                if (!encoder->initialize(enc_config)) {
                    result.warnings.push_back("Failed to initialize encoder: " + test.codec);
                    continue;
                }
                
                // Encode test sequence
                uint32_t frame_count = 300; // 10 seconds at 30fps
                auto encode_start = std::chrono::steady_clock::now();
                
                for (uint32_t i = 0; i < frame_count; ++i) {
                    auto test_frame = generatePerformanceTestFrame(test.width, test.height, i);
                    encoder->encodeFrame(*test_frame);
                }
                
                encoder->finalize();
                auto encode_end = std::chrono::steady_clock::now();
                
                // Calculate performance metrics
                auto encode_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    encode_end - encode_start);
                double achieved_fps = (frame_count * 1000.0) / encode_duration.count();
                
                std::string test_key = test.codec + "_" + std::to_string(test.width) + "x" + std::to_string(test.height);
                result.metrics[test_key + "_fps"] = achieved_fps;
                result.metrics[test_key + "_expected_fps"] = test.expected_fps;
                result.metrics[test_key + "_performance_ratio"] = achieved_fps / test.expected_fps;
                
                // Performance should be at least 80% of expected
                if (achieved_fps < test.expected_fps * 0.8) {
                    result.warnings.push_back(
                        "Low performance for " + test_key + ": " + 
                        std::to_string(achieved_fps) + " fps (expected: " + 
                        std::to_string(test.expected_fps) + ")"
                    );
                }
            }
            
            profiler->stopProfiling("encoding_benchmark");
            auto profile_data = profiler->getProfilingData("encoding_benchmark");
            
            result.metrics["cpu_usage_avg"] = profile_data.average_cpu_usage;
            result.metrics["memory_usage_peak_mb"] = profile_data.peak_memory_usage_mb;
            result.metrics["gpu_usage_avg"] = profile_data.average_gpu_usage;
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            result.passed = result.warnings.size() <= encoding_tests.size() * 0.2; // Allow 20% failures
            if (!result.passed) {
                result.error_message = "Too many encoding performance issues";
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during encoding performance test: " + std::string(e.what());
        }
        
        return result;
    }
    
    static TestResult testConcurrentOperations(const TestConfiguration& config) {
        TestResult result;
        result.test_name = "ConcurrentOperationsStress";
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            auto profiler = core::PerformanceProfiler::getInstance();
            profiler->startProfiling("concurrent_operations");
            
            // Launch multiple concurrent operations
            uint32_t num_threads = std::min(config.max_concurrent_tests, 
                                          std::thread::hardware_concurrency());
            std::vector<std::future<bool>> futures;
            std::vector<std::string> operation_results;
            std::mutex results_mutex;
            
            for (uint32_t i = 0; i < num_threads; ++i) {
                futures.push_back(std::async(std::launch::async, [&, i]() {
                    try {
                        // Each thread performs encoding/decoding operations
                        auto encoder = render::EncoderFactory::createEncoder("H.264");
                        auto decoder = decode::DecoderFactory::createDecoder("H.264");
                        
                        if (!encoder || !decoder) {
                            std::lock_guard<std::mutex> lock(results_mutex);
                            operation_results.push_back("Thread " + std::to_string(i) + ": Failed to create codec");
                            return false;
                        }
                        
                        // Configure encoder
                        render::EncoderConfig enc_config;
                        enc_config.width = 1280;
                        enc_config.height = 720;
                        enc_config.frame_rate = 25.0;
                        enc_config.bitrate_kbps = 2000;
                        enc_config.output_file = config.output_directory + "/concurrent_" + std::to_string(i) + ".mp4";
                        
                        if (!encoder->initialize(enc_config)) {
                            std::lock_guard<std::mutex> lock(results_mutex);
                            operation_results.push_back("Thread " + std::to_string(i) + ": Encoder init failed");
                            return false;
                        }
                        
                        // Encode frames
                        for (uint32_t frame = 0; frame < 100; ++frame) {
                            auto test_frame = generatePerformanceTestFrame(1280, 720, frame);
                            encoder->encodeFrame(*test_frame);
                        }
                        
                        encoder->finalize();
                        
                        // Now decode the file
                        decode::DecoderConfig dec_config;
                        dec_config.input_file = enc_config.output_file;
                        
                        if (!decoder->initialize(dec_config)) {
                            std::lock_guard<std::mutex> lock(results_mutex);
                            operation_results.push_back("Thread " + std::to_string(i) + ": Decoder init failed");
                            return false;
                        }
                        
                        uint32_t decoded_frames = 0;
                        while (decoder->hasMoreFrames()) {
                            auto frame = decoder->decodeNextFrame();
                            if (!frame) break;
                            decoded_frames++;
                        }
                        
                        if (decoded_frames < 95) { // Allow some frame loss
                            std::lock_guard<std::mutex> lock(results_mutex);
                            operation_results.push_back("Thread " + std::to_string(i) + ": Too few frames decoded");
                            return false;
                        }
                        
                        std::lock_guard<std::mutex> lock(results_mutex);
                        operation_results.push_back("Thread " + std::to_string(i) + ": Success");
                        return true;
                        
                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lock(results_mutex);
                        operation_results.push_back("Thread " + std::to_string(i) + ": Exception: " + std::string(e.what()));
                        return false;
                    }
                }));
            }
            
            // Wait for all operations to complete
            uint32_t successful_operations = 0;
            for (auto& future : futures) {
                if (future.get()) {
                    successful_operations++;
                }
            }
            
            profiler->stopProfiling("concurrent_operations");
            auto profile_data = profiler->getProfilingData("concurrent_operations");
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.metrics["concurrent_threads"] = num_threads;
            result.metrics["successful_operations"] = successful_operations;
            result.metrics["success_rate"] = static_cast<double>(successful_operations) / num_threads;
            result.metrics["peak_memory_mb"] = profile_data.peak_memory_usage_mb;
            result.metrics["avg_cpu_usage"] = profile_data.average_cpu_usage;
            
            // Add operation results as warnings for debugging
            for (const auto& op_result : operation_results) {
                if (op_result.find("Success") == std::string::npos) {
                    result.warnings.push_back(op_result);
                }
            }
            
            // Require at least 90% success rate
            result.passed = (result.metrics["success_rate"] >= 0.90);
            if (!result.passed) {
                result.error_message = "Concurrent operations success rate below threshold";
            }
            
        } catch (const std::exception& e) {
            result.error_message = "Exception during concurrent operations test: " + std::string(e.what());
        }
        
        return result;
    }
    
    // Helper methods
    static uint32_t calculateTargetBitrate(uint32_t width, uint32_t height);
    static std::unique_ptr<core::VideoFrame> generatePerformanceTestFrame(uint32_t width, uint32_t height, uint32_t frame_number);
};

} // namespace ve::tests::integration
```

---

## 🚀 **Test Execution and Reporting**

### **Automated Test Runner**
```python
#!/usr/bin/env python3
# File: scripts/run_integration_tests.py

import os
import sys
import json
import subprocess
import argparse
import datetime
from pathlib import Path

class IntegrationTestRunner:
    def __init__(self, config_file="test_config.json"):
        self.config = self.load_config(config_file)
        self.results = []
        
    def load_config(self, config_file):
        """Load test configuration"""
        with open(config_file, 'r') as f:
            return json.load(f)
    
    def run_all_tests(self):
        """Run all integration tests"""
        print("🧪 Starting Comprehensive Integration Test Suite")
        print("=" * 60)
        
        # Test categories
        test_categories = [
            "codec_compatibility",
            "container_formats", 
            "quality_validation",
            "performance_benchmarks",
            "stress_tests",
            "standards_compliance"
        ]
        
        for category in test_categories:
            print(f"\n📋 Running {category} tests...")
            category_results = self.run_test_category(category)
            self.results.extend(category_results)
            
            # Print summary for category
            passed = sum(1 for r in category_results if r.get('passed', False))
            total = len(category_results)
            print(f"   ✅ {passed}/{total} tests passed")
        
        # Generate reports
        self.generate_reports()
        return self.get_overall_result()
    
    def run_test_category(self, category):
        """Run tests for specific category"""
        try:
            cmd = [
                self.config['test_executable'],
                '--category', category,
                '--config', self.config['test_config_file'],
                '--output-format', 'json'
            ]
            
            if self.config.get('enable_hardware_acceleration', True):
                cmd.append('--enable-hardware')
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=1800)
            
            if result.returncode == 0:
                return json.loads(result.stdout)
            else:
                print(f"❌ Error running {category}: {result.stderr}")
                return []
                
        except subprocess.TimeoutExpired:
            print(f"⏰ Timeout running {category} tests")
            return []
        except Exception as e:
            print(f"❌ Exception running {category}: {e}")
            return []
    
    def generate_reports(self):
        """Generate test reports"""
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # JSON Report
        json_report = {
            "timestamp": timestamp,
            "total_tests": len(self.results),
            "passed_tests": sum(1 for r in self.results if r.get('passed', False)),
            "failed_tests": sum(1 for r in self.results if not r.get('passed', False)),
            "test_results": self.results
        }
        
        json_file = f"test_results_{timestamp}.json"
        with open(json_file, 'w') as f:
            json.dump(json_report, f, indent=2)
        
        # HTML Report
        self.generate_html_report(json_report, f"test_results_{timestamp}.html")
        
        # Console Summary
        self.print_summary(json_report)
        
        print(f"\n📊 Reports generated:")
        print(f"   📄 JSON: {json_file}")
        print(f"   🌐 HTML: test_results_{timestamp}.html")
    
    def generate_html_report(self, report_data, filename):
        """Generate HTML test report"""
        html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Video Editor Integration Test Results</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        .header {{ background: #f0f0f0; padding: 20px; border-radius: 5px; }}
        .summary {{ display: flex; justify-content: space-around; margin: 20px 0; }}
        .metric {{ text-align: center; padding: 10px; background: #e8f4f8; border-radius: 5px; }}
        .passed {{ background: #d4edda; }}
        .failed {{ background: #f8d7da; }}
        .test-result {{ margin: 10px 0; padding: 10px; border-left: 4px solid #ddd; }}
        .test-passed {{ border-left-color: #28a745; }}
        .test-failed {{ border-left-color: #dc3545; }}
        .metrics {{ font-size: 0.9em; color: #666; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🧪 Video Editor Integration Test Results</h1>
        <p>Generated: {report_data['timestamp']}</p>
    </div>
    
    <div class="summary">
        <div class="metric">
            <h3>Total Tests</h3>
            <div style="font-size: 2em;">{report_data['total_tests']}</div>
        </div>
        <div class="metric passed">
            <h3>Passed</h3>
            <div style="font-size: 2em;">{report_data['passed_tests']}</div>
        </div>
        <div class="metric failed">
            <h3>Failed</h3>
            <div style="font-size: 2em;">{report_data['failed_tests']}</div>
        </div>
        <div class="metric">
            <h3>Pass Rate</h3>
            <div style="font-size: 2em;">{report_data['passed_tests']/report_data['total_tests']*100:.1f}%</div>
        </div>
    </div>
    
    <h2>Test Results</h2>
        """
        
        for test in report_data['test_results']:
            status_class = "test-passed" if test.get('passed', False) else "test-failed"
            status_icon = "✅" if test.get('passed', False) else "❌"
            
            html_content += f"""
    <div class="test-result {status_class}">
        <h3>{status_icon} {test.get('test_name', 'Unknown Test')}</h3>
        <p><strong>Execution Time:</strong> {test.get('execution_time_ms', 0)}ms</p>
            """
            
            if test.get('error_message'):
                html_content += f"<p><strong>Error:</strong> {test['error_message']}</p>"
            
            if test.get('metrics'):
                html_content += "<div class='metrics'><strong>Metrics:</strong><ul>"
                for key, value in test['metrics'].items():
                    html_content += f"<li>{key}: {value}</li>"
                html_content += "</ul></div>"
            
            html_content += "</div>"
        
        html_content += """
</body>
</html>
        """
        
        with open(filename, 'w') as f:
            f.write(html_content)
    
    def print_summary(self, report_data):
        """Print console summary"""
        print("\n" + "="*60)
        print("📊 TEST SUMMARY")
        print("="*60)
        print(f"Total Tests:  {report_data['total_tests']}")
        print(f"✅ Passed:    {report_data['passed_tests']}")
        print(f"❌ Failed:    {report_data['failed_tests']}")
        print(f"📈 Pass Rate: {report_data['passed_tests']/report_data['total_tests']*100:.1f}%")
        
        if report_data['failed_tests'] > 0:
            print("\n❌ FAILED TESTS:")
            for test in report_data['test_results']:
                if not test.get('passed', False):
                    print(f"   • {test.get('test_name', 'Unknown')}: {test.get('error_message', 'No error message')}")
    
    def get_overall_result(self):
        """Get overall test result"""
        total_tests = len(self.results)
        passed_tests = sum(1 for r in self.results if r.get('passed', False))
        
        if total_tests == 0:
            return False
        
        pass_rate = passed_tests / total_tests
        return pass_rate >= 0.95  # Require 95% pass rate for overall success

def main():
    parser = argparse.ArgumentParser(description='Run Video Editor Integration Tests')
    parser.add_argument('--config', default='test_config.json', help='Test configuration file')
    parser.add_argument('--category', help='Run specific test category only')
    parser.add_argument('--ci', action='store_true', help='Run in CI mode (exit with error code)')
    
    args = parser.parse_args()
    
    runner = IntegrationTestRunner(args.config)
    
    if args.category:
        results = runner.run_test_category(args.category)
        success = all(r.get('passed', False) for r in results)
    else:
        success = runner.run_all_tests()
    
    if args.ci and not success:
        sys.exit(1)
    
    print(f"\n🎯 Overall Result: {'✅ SUCCESS' if success else '❌ FAILURE'}")

if __name__ == "__main__":
    main()
```

### **Test Configuration**
```json
{
  "test_executable": "./build/qt-debug/tests/video_editor_integration_tests",
  "test_config_file": "tests/integration_test_config.json",
  "test_data_directory": "tests/data",
  "output_directory": "tests/output", 
  "reference_directory": "tests/reference",
  "enable_hardware_acceleration": true,
  "generate_detailed_reports": true,
  "test_timeout_seconds": 300,
  "max_concurrent_tests": 4,
  "quality_thresholds": {
    "min_psnr": 35.0,
    "min_ssim": 0.90,
    "min_vmaf": 80.0
  },
  "performance_thresholds": {
    "h264_1080p_fps": 30.0,
    "hevc_1080p_fps": 25.0,
    "h264_4k_fps": 10.0,
    "hevc_4k_fps": 8.0
  },
  "test_categories": {
    "codec_compatibility": {
      "enabled": true,
      "timeout_seconds": 600
    },
    "container_formats": {
      "enabled": true,
      "timeout_seconds": 300
    },
    "quality_validation": {
      "enabled": true,
      "timeout_seconds": 900
    },
    "performance_benchmarks": {
      "enabled": true,
      "timeout_seconds": 1200
    },
    "stress_tests": {
      "enabled": true,
      "timeout_seconds": 1800
    },
    "standards_compliance": {
      "enabled": true,
      "timeout_seconds": 600
    }
  }
}
```

---

This comprehensive integration test suite provides thorough validation of all aspects of the professional video format support system. The tests ensure that the system meets professional standards for quality, performance, and compatibility across all supported formats and use cases.

To run the complete test suite:

```bash
# Run all tests
python scripts/run_integration_tests.py

# Run specific category
python scripts/run_integration_tests.py --category codec_compatibility

# Run in CI mode (returns error code on failure)
python scripts/run_integration_tests.py --ci
```

The test suite generates comprehensive reports in both JSON and HTML formats, providing detailed analysis of test results, performance metrics, and quality validation outcomes.
