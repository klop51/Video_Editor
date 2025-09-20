/**
 * @file ffmpeg_codec_validation_test.cpp
 * @brief FFmpeg Codec Enhancement Validation Test
 * 
 * Tests the enhanced FFmpeg codec detection with comprehensive fallback support.
 * Validates that MP3 and OGG codecs are properly detected and that diagnostic
 * information is available for troubleshooting.
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>

// Include audio system headers
#include "audio/ffmpeg_audio_encoder.hpp"
#include "audio/audio_types.hpp"
#include "core/log.hpp"

using namespace ve::audio;

void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
}

void print_success(const std::string& message) {
    std::cout << "✓ " << message << std::endl;
}

void print_warning(const std::string& message) {
    std::cout << "⚠ " << message << std::endl;
}

void print_error(const std::string& message) {
    std::cout << "✗ " << message << std::endl;
}

int main() {
    std::cout << "=== FFmpeg Codec Enhancement Validation Test ===" << std::endl;
    
    // Test 1: Basic codec detection
    print_section("Testing Enhanced Codec Detection");
    
    std::vector<std::pair<AudioExportFormat, std::string>> test_formats = {
        {AudioExportFormat::MP3, "MP3"},
        {AudioExportFormat::AAC, "AAC"},
        {AudioExportFormat::FLAC, "FLAC"},
        {AudioExportFormat::OGG, "OGG"}
    };
    
    int supported_count = 0;
    for (const auto& [format, name] : test_formats) {
        bool supported = FFmpegAudioEncoder::is_format_supported(format);
        std::string codec_name = FFmpegAudioEncoder::get_codec_name(format);
        
        if (supported) {
            print_success(name + " format supported (codec: " + codec_name + ")");
            supported_count++;
        } else {
            print_warning(name + " format not supported");
        }
    }
    
    std::cout << "Supported formats: " << supported_count << "/" << test_formats.size() << std::endl;
    
    // Test 2: Encoder creation with fallbacks
    print_section("Testing Encoder Creation with Fallbacks");
    
    for (const auto& [format, name] : test_formats) {
        AudioEncoderConfig config{48000, 2, 16, 320000, true, 5, false, 0, true, 0, 4096};
        auto encoder = FFmpegAudioEncoder::create(format, config);
        
        if (encoder) {
            print_success(name + " encoder created successfully");
        } else {
            print_warning(name + " encoder creation failed");
        }
    }
    
    // Test 3: Supported formats list
    print_section("Testing Supported Formats List");
    
    auto supported_formats = FFmpegAudioEncoder::get_supported_formats();
    std::cout << "Detected supported formats (" << supported_formats.size() << " total):" << std::endl;
    for (const auto& format : supported_formats) {
        std::cout << "  - " << format << std::endl;
    }
    
    if (supported_formats.empty()) {
        print_error("No supported formats detected!");
    }
    
    // Test 4: Available encoders
    print_section("Testing Available Encoders Detection");
    
    auto available_encoders = FFmpegAudioEncoder::get_available_encoders();
    std::cout << "Total available audio encoders: " << available_encoders.size() << std::endl;
    
    // Count specific codec types
    int mp3_count = 0, aac_count = 0, vorbis_count = 0, flac_count = 0;
    
    for (const auto& encoder : available_encoders) {
        if (encoder.find("mp3") != std::string::npos) mp3_count++;
        if (encoder.find("aac") != std::string::npos) aac_count++;
        if (encoder.find("vorbis") != std::string::npos) vorbis_count++;
        if (encoder.find("flac") != std::string::npos) flac_count++;
    }
    
    std::cout << "Codec family counts:" << std::endl;
    std::cout << "  MP3-related: " << mp3_count << std::endl;
    std::cout << "  AAC-related: " << aac_count << std::endl;
    std::cout << "  Vorbis-related: " << vorbis_count << std::endl;
    std::cout << "  FLAC-related: " << flac_count << std::endl;
    
    // Test 5: Comprehensive diagnostics
    print_section("Testing Comprehensive Codec Diagnostics");
    
    std::string diagnostics = FFmpegAudioEncoder::get_codec_diagnostics();
    std::cout << diagnostics << std::endl;
    
    // Test 6: FFmpeg version info
    print_section("Testing FFmpeg Version Information");
    
    std::string version = FFmpegAudioEncoder::get_version_info();
    std::cout << "FFmpeg version: " << version << std::endl;
    
    if (version.empty()) {
        print_warning("FFmpeg version information not available");
    } else {
        print_success("FFmpeg version information available");
    }
    
    // Final summary
    print_section("Validation Summary");
    
    bool all_tests_passed = true;
    
    if (supported_count == 0) {
        print_error("CRITICAL: No audio formats supported!");
        all_tests_passed = false;
    } else if (supported_count < 2) {
        print_warning("Limited format support - only " + std::to_string(supported_count) + " format(s) supported");
    } else {
        print_success("Good format support - " + std::to_string(supported_count) + " format(s) supported");
    }
    
    if (available_encoders.size() < 10) {
        print_warning("Limited encoder availability - only " + std::to_string(available_encoders.size()) + " encoders detected");
    } else {
        print_success("Good encoder availability - " + std::to_string(available_encoders.size()) + " encoders detected");
    }
    
    if (mp3_count == 0) {
        print_warning("No MP3 encoders detected");
    } else {
        print_success("MP3 encoders available: " + std::to_string(mp3_count));
    }
    
    if (vorbis_count == 0) {
        print_warning("No Vorbis/OGG encoders detected");
    } else {
        print_success("Vorbis/OGG encoders available: " + std::to_string(vorbis_count));
    }
    
    if (all_tests_passed && supported_count >= 2) {
        print_section("✓ FFmpeg Codec Enhancement Validation - SUCCESS");
        std::cout << "Enhanced codec detection is working properly with fallback support." << std::endl;
        return 0;
    } else {
        print_section("⚠ FFmpeg Codec Enhancement Validation - PARTIAL SUCCESS");
        std::cout << "Enhanced codec detection is functional but some codecs may be missing." << std::endl;
        return 1;
    }
}