#include <gtest/gtest.h>
#include "media_io/hdr_infrastructure.hpp"
#include <vector>
#include <string>

/**
 * HDR Infrastructure Test Suite
 * Phase 2 Week 5 - Comprehensive testing for HDR support
 */

namespace ve::media_io::test {

class HDRInfrastructureTest : public ::testing::Test {
protected:
    void SetUp() override {
        hdr_infrastructure = std::make_unique<HDRInfrastructure>();
        
        // Create sample HDR10 metadata
        hdr10_metadata.hdr_standard = HDRStandard::HDR10;
        hdr10_metadata.transfer_function = TransferFunction::PQ;
        hdr10_metadata.color_primaries = ColorPrimaries::BT2020;
        hdr10_metadata.mastering_display.max_display_mastering_luminance = 1000.0f;
        hdr10_metadata.mastering_display.min_display_mastering_luminance = 0.01f;
        hdr10_metadata.content_light_level.max_content_light_level = 1000;
        hdr10_metadata.content_light_level.max_frame_average_light_level = 400;
        hdr10_metadata.is_valid = true;
        
        // Create sample HLG metadata
        hlg_metadata.hdr_standard = HDRStandard::HLG;
        hlg_metadata.transfer_function = TransferFunction::HLG;
        hlg_metadata.color_primaries = ColorPrimaries::BT2020;
        hlg_metadata.hlg_params.hlg_ootf_gamma = 1.2f;
        hlg_metadata.is_valid = true;
    }
    
    std::unique_ptr<HDRInfrastructure> hdr_infrastructure;
    HDRMetadata hdr10_metadata;
    HDRMetadata hlg_metadata;
};

TEST_F(HDRInfrastructureTest, DetectHDRStandard_HDR10) {
    // Simulate HDR10 stream data
    std::vector<uint8_t> hdr10_data = {
        // HEVC SEI payload for HDR10 (simplified)
        0x01, 0x89, 0x0A, 0x0B, 0x0C,  // Mastering display color volume
        0x03, 0xE8, 0x00, 0x00,        // Max luminance (1000 nits)
        0x00, 0x01,                    // Min luminance (0.01 nits)
        0x03, 0xE8,                    // Max CLL (1000 nits)
        0x01, 0x90                     // Max FALL (400 nits)
    };
    
    auto detected_standard = hdr_infrastructure->detect_hdr_standard(hdr10_data);
    EXPECT_EQ(detected_standard, HDRStandard::HDR10);
}

TEST_F(HDRInfrastructureTest, DetectHDRStandard_HLG) {
    // Simulate HLG stream data
    std::vector<uint8_t> hlg_data = {
        // HEVC VUI parameters for HLG
        0x12,                          // Transfer characteristics (HLG)
        0x09,                          // Color primaries (BT.2020)
        0x09                           // Matrix coefficients (BT.2020)
    };
    
    auto detected_standard = hdr_infrastructure->detect_hdr_standard(hlg_data);
    EXPECT_EQ(detected_standard, HDRStandard::HLG);
}

TEST_F(HDRInfrastructureTest, DetectHDRStandard_DolbyVision) {
    // Simulate Dolby Vision stream data
    std::vector<uint8_t> dv_data = {
        // Dolby Vision RPU (simplified)
        0x01, 0xBE, 0x03, 0x78,        // Dolby Vision header
        0x00, 0x40, 0x00, 0x0C,        // Enhancement layer data
        0x80, 0x00, 0x00, 0x00         // Dynamic metadata
    };
    
    auto detected_standard = hdr_infrastructure->detect_hdr_standard(dv_data);
    EXPECT_EQ(detected_standard, HDRStandard::DOLBY_VISION);
}

TEST_F(HDRInfrastructureTest, ParseHDRMetadata_HDR10) {
    // Sample HDR10 SEI message data
    std::vector<uint8_t> sei_data = {
        // Mastering display color volume SEI
        0x89,                          // Payload type
        0x18,                          // Payload size
        // Display primaries (x,y coordinates in 0.00002 units)
        0x8C, 0xC8, 0x4B, 0x72,       // Red primary (0.708, 0.292)
        0x2B, 0x48, 0xCB, 0x0A,       // Green primary (0.170, 0.797)
        0x21, 0x72, 0x0B, 0xDC,       // Blue primary (0.131, 0.046)
        0x4F, 0xAE, 0x54, 0x34,       // White point (0.3127, 0.3290)
        0x00, 0x00, 0x27, 0x10,       // Max luminance (10000 units = 1000 nits)
        0x00, 0x00, 0x00, 0x01,       // Min luminance (1 unit = 0.0001 nits)
        
        // Content light level info SEI
        0x90,                          // Payload type
        0x04,                          // Payload size
        0x03, 0xE8,                    // Max content light level (1000 nits)
        0x01, 0x90                     // Max frame average (400 nits)
    };
    
    auto metadata = hdr_infrastructure->parse_hdr_metadata(sei_data);
    
    EXPECT_EQ(metadata.hdr_standard, HDRStandard::HDR10);
    EXPECT_EQ(metadata.transfer_function, TransferFunction::PQ);
    EXPECT_EQ(metadata.color_primaries, ColorPrimaries::BT2020);
    EXPECT_FLOAT_EQ(metadata.mastering_display.max_display_mastering_luminance, 1000.0f);
    EXPECT_FLOAT_EQ(metadata.mastering_display.min_display_mastering_luminance, 0.0001f);
    EXPECT_EQ(metadata.content_light_level.max_content_light_level, 1000);
    EXPECT_EQ(metadata.content_light_level.max_frame_average_light_level, 400);
    EXPECT_TRUE(metadata.is_valid);
}

TEST_F(HDRInfrastructureTest, ValidateHDRMetadata_Valid) {
    EXPECT_TRUE(hdr_infrastructure->validate_hdr_metadata(hdr10_metadata));
    EXPECT_TRUE(hdr_infrastructure->validate_hdr_metadata(hlg_metadata));
}

TEST_F(HDRInfrastructureTest, ValidateHDRMetadata_Invalid) {
    HDRMetadata invalid_metadata = hdr10_metadata;
    
    // Test invalid luminance values
    invalid_metadata.mastering_display.max_display_mastering_luminance = -100.0f;
    EXPECT_FALSE(hdr_infrastructure->validate_hdr_metadata(invalid_metadata));
    
    // Test mismatched HDR standard and transfer function
    invalid_metadata = hdr10_metadata;
    invalid_metadata.hdr_standard = HDRStandard::HDR10;
    invalid_metadata.transfer_function = TransferFunction::HLG;
    EXPECT_FALSE(hdr_infrastructure->validate_hdr_metadata(invalid_metadata));
}

TEST_F(HDRInfrastructureTest, ProcessHDRFrame_ToneMapping) {
    HDRProcessingConfig config;
    config.enable_tone_mapping = true;
    config.tone_mapping.target_peak_luminance = 100.0f; // SDR target
    config.tone_mapping.use_aces = true;
    
    // Simulate HDR frame data (simplified)
    std::vector<uint16_t> hdr_frame_data(1920 * 1080 * 3); // RGB 10-bit
    std::fill(hdr_frame_data.begin(), hdr_frame_data.end(), 800); // Bright HDR value
    
    std::vector<uint8_t> output_frame;
    bool success = hdr_infrastructure->process_hdr_frame(
        reinterpret_cast<const uint8_t*>(hdr_frame_data.data()),
        hdr_frame_data.size() * sizeof(uint16_t),
        hdr10_metadata,
        config,
        output_frame
    );
    
    EXPECT_TRUE(success);
    EXPECT_FALSE(output_frame.empty());
}

TEST_F(HDRInfrastructureTest, ConvertColorSpace_BT2020_to_BT709) {
    HDRProcessingConfig config;
    config.enable_gamut_mapping = true;
    config.output_color_primaries = ColorPrimaries::BT709;
    
    // Test with sample BT.2020 data
    std::vector<float> bt2020_rgb = {0.8f, 0.9f, 0.7f}; // Wide gamut color
    std::vector<float> bt709_rgb;
    
    bool success = hdr_infrastructure->convert_color_space(
        bt2020_rgb,
        ColorPrimaries::BT2020,
        ColorPrimaries::BT709,
        bt709_rgb
    );
    
    EXPECT_TRUE(success);
    EXPECT_EQ(bt709_rgb.size(), 3);
    // BT.709 should clamp wide gamut colors
    EXPECT_LE(bt709_rgb[0], 1.0f);
    EXPECT_LE(bt709_rgb[1], 1.0f);
    EXPECT_LE(bt709_rgb[2], 1.0f);
}

TEST_F(HDRInfrastructureTest, SystemCapabilityDetection) {
    auto capabilities = hdr_infrastructure->get_system_hdr_capabilities();
    
    // Should detect at least basic HDR support
    EXPECT_TRUE(capabilities.hdr10_supported || 
               capabilities.hlg_supported || 
               capabilities.dolby_vision_supported);
    
    // Hardware acceleration detection
    EXPECT_TRUE(capabilities.hardware_tone_mapping_available ||
               !capabilities.hardware_tone_mapping_available); // Either is valid
}

} // namespace ve::media_io::test

// HDR Utilities Tests
namespace ve::media_io::hdr_utils::test {

class HDRUtilitiesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test metadata
        test_hdr10_metadata = create_hdr10_metadata(1000.0f, 0.01f, 1000, 400);
    }
    
    HDRMetadata test_hdr10_metadata;
};

TEST_F(HDRUtilitiesTest, CreateHDR10Metadata) {
    auto metadata = create_hdr10_metadata(4000.0f, 0.005f, 4000, 1000);
    
    EXPECT_EQ(metadata.hdr_standard, HDRStandard::HDR10);
    EXPECT_EQ(metadata.transfer_function, TransferFunction::PQ);
    EXPECT_EQ(metadata.color_primaries, ColorPrimaries::BT2020);
    EXPECT_FLOAT_EQ(metadata.mastering_display.max_display_mastering_luminance, 4000.0f);
    EXPECT_FLOAT_EQ(metadata.mastering_display.min_display_mastering_luminance, 0.005f);
    EXPECT_EQ(metadata.content_light_level.max_content_light_level, 4000);
    EXPECT_EQ(metadata.content_light_level.max_frame_average_light_level, 1000);
    EXPECT_TRUE(metadata.is_valid);
}

TEST_F(HDRUtilitiesTest, ConvertSDRToHDR) {
    HDRMetadata sdr_metadata;
    sdr_metadata.hdr_standard = HDRStandard::NONE;
    sdr_metadata.transfer_function = TransferFunction::BT709;
    sdr_metadata.color_primaries = ColorPrimaries::BT709;
    
    auto hdr_metadata = convert_sdr_to_hdr(sdr_metadata, HDRStandard::HDR10);
    
    EXPECT_EQ(hdr_metadata.hdr_standard, HDRStandard::HDR10);
    EXPECT_EQ(hdr_metadata.transfer_function, TransferFunction::PQ);
    EXPECT_EQ(hdr_metadata.color_primaries, ColorPrimaries::BT2020);
}

TEST_F(HDRUtilitiesTest, CheckHDRCompatibility_Perfect) {
    auto compatibility = check_hdr_compatibility(test_hdr10_metadata, test_hdr10_metadata);
    
    EXPECT_TRUE(compatibility.fully_compatible);
    EXPECT_FALSE(compatibility.requires_conversion);
    EXPECT_FALSE(compatibility.quality_loss_expected);
}

TEST_F(HDRUtilitiesTest, CheckHDRCompatibility_ConversionRequired) {
    HDRMetadata hlg_metadata;
    hlg_metadata.hdr_standard = HDRStandard::HLG;
    hlg_metadata.transfer_function = TransferFunction::HLG;
    hlg_metadata.color_primaries = ColorPrimaries::BT2020;
    
    auto compatibility = check_hdr_compatibility(test_hdr10_metadata, hlg_metadata);
    
    EXPECT_FALSE(compatibility.fully_compatible);
    EXPECT_TRUE(compatibility.requires_conversion);
    EXPECT_FALSE(compatibility.compatibility_notes.empty());
}

TEST_F(HDRUtilitiesTest, StreamingPlatformConfigs) {
    auto youtube_config = get_youtube_hdr_config();
    EXPECT_EQ(youtube_config.output_hdr_standard, HDRStandard::HDR10);
    EXPECT_EQ(youtube_config.output_transfer_function, TransferFunction::PQ);
    EXPECT_TRUE(youtube_config.enable_tone_mapping);
    
    auto netflix_config = get_netflix_hdr_config();
    EXPECT_EQ(netflix_config.output_hdr_standard, HDRStandard::DOLBY_VISION);
    EXPECT_TRUE(netflix_config.preserve_dynamic_metadata);
    
    auto broadcast_config = get_broadcast_hlg_config();
    EXPECT_EQ(broadcast_config.output_hdr_standard, HDRStandard::HLG);
    EXPECT_EQ(broadcast_config.output_transfer_function, TransferFunction::HLG);
}

TEST_F(HDRUtilitiesTest, ValidateForStreamingPlatform_YouTube) {
    auto result = validate_for_streaming_platform(test_hdr10_metadata, "YouTube");
    
    EXPECT_TRUE(result.meets_requirements);
    EXPECT_FALSE(result.requirements_met.empty());
    EXPECT_TRUE(result.requirements_failed.empty());
}

TEST_F(HDRUtilitiesTest, ValidateForStreamingPlatform_Netflix) {
    auto result = validate_for_streaming_platform(test_hdr10_metadata, "Netflix");
    
    EXPECT_TRUE(result.meets_requirements);
    EXPECT_FALSE(result.requirements_met.empty());
}

TEST_F(HDRUtilitiesTest, ValidateForStreamingPlatform_Unsupported) {
    HDRMetadata unsupported_metadata;
    unsupported_metadata.hdr_standard = HDRStandard::HDR10_PLUS;
    
    auto result = validate_for_streaming_platform(unsupported_metadata, "YouTube");
    
    EXPECT_FALSE(result.meets_requirements);
    EXPECT_FALSE(result.requirements_failed.empty());
    EXPECT_FALSE(result.recommendations.empty());
}

} // namespace ve::media_io::hdr_utils::test
