#include "media_io/hdr_infrastructure.hpp"
#include <algorithm>

/**
 * HDR Workflow Utilities Implementation
 * Phase 2 Week 5 - HDR Infrastructure utilities for common workflows
 */

namespace ve::media_io {
namespace hdr_utils {

HDRMetadata create_hdr10_metadata(float max_luminance, float min_luminance,
                                 uint16_t max_cll, uint16_t max_fall) {
    HDRMetadata metadata;
    
    // Set HDR10 standard
    metadata.hdr_standard = HDRStandard::HDR10;
    metadata.transfer_function = TransferFunction::PQ;
    metadata.color_primaries = ColorPrimaries::BT2020;
    
    // Set mastering display info
    metadata.mastering_display.max_display_mastering_luminance = max_luminance;
    metadata.mastering_display.min_display_mastering_luminance = min_luminance;
    
    // Set BT.2020 primaries
    metadata.mastering_display.display_primaries = {{
        {{0.708f, 0.292f}},  // Red
        {{0.170f, 0.797f}},  // Green
        {{0.131f, 0.046f}}   // Blue
    }};
    metadata.mastering_display.white_point = {0.3127f, 0.3290f}; // D65
    
    // Set content light levels
    metadata.content_light_level.max_content_light_level = max_cll;
    metadata.content_light_level.max_frame_average_light_level = max_fall;
    
    // Set names for identification
    metadata.color_space_name = "BT.2020";
    metadata.transfer_characteristics_name = "PQ";
    metadata.matrix_coefficients_name = "BT.2020 Non-Constant Luminance";
    
    metadata.is_valid = true;
    return metadata;
}

HDRMetadata convert_sdr_to_hdr(const HDRMetadata& sdr_metadata, HDRStandard target_standard) {
    HDRMetadata hdr_metadata = sdr_metadata;
    
    switch (target_standard) {
        case HDRStandard::HDR10:
            hdr_metadata.hdr_standard = HDRStandard::HDR10;
            hdr_metadata.transfer_function = TransferFunction::PQ;
            hdr_metadata.color_primaries = ColorPrimaries::BT2020;
            
            // Set default HDR10 mastering display values
            hdr_metadata.mastering_display.max_display_mastering_luminance = 1000.0f;
            hdr_metadata.mastering_display.min_display_mastering_luminance = 0.01f;
            hdr_metadata.content_light_level.max_content_light_level = 1000;
            hdr_metadata.content_light_level.max_frame_average_light_level = 400;
            break;
            
        case HDRStandard::HLG:
            hdr_metadata.hdr_standard = HDRStandard::HLG;
            hdr_metadata.transfer_function = TransferFunction::HLG;
            hdr_metadata.color_primaries = ColorPrimaries::BT2020;
            hdr_metadata.hlg_params.hlg_ootf_gamma = 1.2f;
            break;
            
        default:
            // No conversion for unsupported standards
            break;
    }
    
    return hdr_metadata;
}

HDRCompatibilityInfo check_hdr_compatibility(const HDRMetadata& source_metadata,
                                            const HDRMetadata& target_metadata) {
    HDRCompatibilityInfo info;
    
    // Check if standards match
    if (source_metadata.hdr_standard == target_metadata.hdr_standard) {
        info.fully_compatible = true;
        info.compatibility_notes.push_back("HDR standards match perfectly");
    } else {
        info.requires_conversion = true;
        info.compatibility_notes.push_back("HDR standard conversion required");
        
        // Check for quality loss scenarios
        if (source_metadata.hdr_standard == HDRStandard::DOLBY_VISION && 
            target_metadata.hdr_standard == HDRStandard::HDR10) {
            info.quality_loss_expected = true;
            info.compatibility_notes.push_back("Dolby Vision to HDR10: some enhancement data will be lost");
        }
        
        if (source_metadata.hdr_standard != HDRStandard::NONE && 
            target_metadata.hdr_standard == HDRStandard::NONE) {
            info.quality_loss_expected = true;
            info.compatibility_notes.push_back("HDR to SDR conversion: dynamic range will be reduced");
        }
    }
    
    // Check transfer function compatibility
    if (source_metadata.transfer_function != target_metadata.transfer_function) {
        info.requires_conversion = true;
        info.compatibility_notes.push_back("Transfer function conversion required");
    }
    
    // Check color primaries compatibility
    if (source_metadata.color_primaries != target_metadata.color_primaries) {
        info.requires_conversion = true;
        info.compatibility_notes.push_back("Color gamut conversion required");
        
        // Check for gamut clipping
        if (source_metadata.color_primaries == ColorPrimaries::BT2020 && 
            target_metadata.color_primaries == ColorPrimaries::BT709) {
            info.quality_loss_expected = true;
            info.compatibility_notes.push_back("BT.2020 to BT.709: some colors will be clipped");
        }
    }
    
    return info;
}

HDRProcessingConfig get_youtube_hdr_config() {
    HDRProcessingConfig config;
    
    // YouTube HDR requirements
    config.output_hdr_standard = HDRStandard::HDR10;
    config.output_transfer_function = TransferFunction::PQ;
    config.output_color_primaries = ColorPrimaries::BT2020;
    
    // YouTube tone mapping preferences
    config.tone_mapping.target_peak_luminance = 1000.0f;
    config.tone_mapping.use_aces = true;
    
    // Enable all processing for YouTube compatibility
    config.enable_tone_mapping = true;
    config.enable_gamut_mapping = true;
    config.preserve_dynamic_metadata = false; // YouTube doesn't support HDR10+
    
    return config;
}

HDRProcessingConfig get_netflix_hdr_config() {
    HDRProcessingConfig config;
    
    // Netflix supports multiple HDR formats
    config.output_hdr_standard = HDRStandard::DOLBY_VISION; // Preferred
    config.output_transfer_function = TransferFunction::PQ;
    config.output_color_primaries = ColorPrimaries::BT2020;
    
    // Netflix tone mapping preferences
    config.tone_mapping.target_peak_luminance = 4000.0f; // High-end displays
    config.tone_mapping.use_aces = true;
    
    // Preserve all metadata for Netflix
    config.preserve_dynamic_metadata = true;
    config.enable_tone_mapping = true;
    config.enable_gamut_mapping = true;
    
    return config;
}

HDRProcessingConfig get_broadcast_hlg_config() {
    HDRProcessingConfig config;
    
    // Broadcast HLG requirements
    config.output_hdr_standard = HDRStandard::HLG;
    config.output_transfer_function = TransferFunction::HLG;
    config.output_color_primaries = ColorPrimaries::BT2020;
    
    // HLG tone mapping (different from PQ)
    config.tone_mapping.target_peak_luminance = 1000.0f;
    config.tone_mapping.use_aces = false; // HLG has its own tone mapping
    
    // HLG-specific settings
    config.enable_tone_mapping = true;
    config.enable_gamut_mapping = true;
    config.preserve_dynamic_metadata = false; // HLG doesn't use dynamic metadata
    
    return config;
}

HDRProcessingConfig get_cinema_dci_p3_config() {
    HDRProcessingConfig config;
    
    // Cinema DCI-P3 requirements
    config.output_hdr_standard = HDRStandard::NONE; // Cinema often uses custom formats
    config.output_transfer_function = TransferFunction::DCI_P3;
    config.output_color_primaries = ColorPrimaries::DCI_P3;
    
    // Cinema tone mapping
    config.tone_mapping.target_peak_luminance = 48.0f; // DCI standard
    config.tone_mapping.use_aces = true; // ACES is standard in cinema
    
    config.enable_tone_mapping = true;
    config.enable_gamut_mapping = true;
    
    return config;
}

HDRProcessingConfig get_apple_dolby_vision_config() {
    HDRProcessingConfig config;
    
    // Apple Dolby Vision requirements
    config.output_hdr_standard = HDRStandard::DOLBY_VISION;
    config.output_transfer_function = TransferFunction::PQ;
    config.output_color_primaries = ColorPrimaries::DISPLAY_P3; // Apple uses Display P3
    
    // Apple-specific tone mapping
    config.tone_mapping.target_peak_luminance = 1000.0f;
    config.tone_mapping.use_aces = true;
    
    // Preserve all Apple metadata
    config.preserve_dynamic_metadata = true;
    config.enable_tone_mapping = true;
    config.enable_gamut_mapping = true;
    
    return config;
}

StreamingValidationResult validate_for_streaming_platform(const HDRMetadata& metadata,
                                                         const std::string& platform_name) {
    StreamingValidationResult result;
    
    if (platform_name == "YouTube") {
        // YouTube HDR requirements
        if (metadata.hdr_standard == HDRStandard::HDR10) {
            result.requirements_met.push_back("HDR10 format supported");
        } else if (metadata.hdr_standard == HDRStandard::HLG) {
            result.requirements_met.push_back("HLG format supported");
        } else if (metadata.hdr_standard != HDRStandard::NONE) {
            result.requirements_failed.push_back("Unsupported HDR format for YouTube");
            result.recommendations.push_back("Convert to HDR10 or HLG");
        }
        
        if (metadata.color_primaries == ColorPrimaries::BT2020) {
            result.requirements_met.push_back("BT.2020 color primaries supported");
        } else {
            result.requirements_failed.push_back("Non-BT.2020 color primaries");
            result.recommendations.push_back("Convert to BT.2020 for optimal HDR");
        }
        
    } else if (platform_name == "Netflix") {
        // Netflix HDR requirements
        if (metadata.hdr_standard == HDRStandard::DOLBY_VISION ||
            metadata.hdr_standard == HDRStandard::HDR10 ||
            metadata.hdr_standard == HDRStandard::HDR10_PLUS) {
            result.requirements_met.push_back("Supported HDR format");
        } else if (metadata.hdr_standard != HDRStandard::NONE) {
            result.requirements_failed.push_back("Unsupported HDR format");
            result.recommendations.push_back("Convert to Dolby Vision or HDR10+");
        }
        
        // Netflix has strict quality requirements
        if (metadata.content_light_level.max_content_light_level > 0 &&
            metadata.content_light_level.max_content_light_level <= 4000) {
            result.requirements_met.push_back("Content light levels within limits");
        } else {
            result.requirements_failed.push_back("Invalid content light levels");
            result.recommendations.push_back("Ensure MaxCLL ≤ 4000 nits");
        }
        
    } else if (platform_name == "Apple TV+") {
        // Apple TV+ requirements
        if (metadata.hdr_standard == HDRStandard::DOLBY_VISION) {
            result.requirements_met.push_back("Dolby Vision supported");
        } else if (metadata.hdr_standard == HDRStandard::HDR10) {
            result.requirements_met.push_back("HDR10 supported");
            result.recommendations.push_back("Dolby Vision preferred for optimal quality");
        } else if (metadata.hdr_standard != HDRStandard::NONE) {
            result.requirements_failed.push_back("Unsupported HDR format");
            result.recommendations.push_back("Convert to Dolby Vision or HDR10");
        }
        
    } else {
        result.requirements_failed.push_back("Unknown streaming platform");
        result.recommendations.push_back("Check platform-specific HDR requirements");
    }
    
    // Overall assessment
    result.meets_requirements = result.requirements_failed.empty();
    
    if (!result.meets_requirements) {
        result.recommendations.push_back("Review HDR metadata and consider format conversion");
    }
    
    return result;
}

} // namespace hdr_utils
} // namespace ve::media_io
