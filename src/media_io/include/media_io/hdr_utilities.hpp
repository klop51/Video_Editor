#pragma once

#include "hdr_infrastructure.hpp"

/**
 * HDR Utilities for Phase 2 Week 5
 * FORMAT_SUPPORT_ROADMAP.md - Streaming Platform Workflows
 * 
 * This header provides utility functions for HDR workflows
 * specifically configured for major streaming platforms.
 */

namespace ve::media_io {

/**
 * HDR workflow configuration for streaming platforms
 */
struct HDRWorkflowConfig {
    // Content Light Level limits
    uint16_t max_cll = 10000;        // Maximum Content Light Level in nits
    uint16_t max_fall = 4000;        // Maximum Frame Average Light Level in nits
    
    // Display mastering information
    uint16_t master_display_max_luminance = 10000;  // nits
    uint16_t master_display_min_luminance = 5;      // 0.0005 nits (stored as 5/10000)
    
    // Color primaries and transfer characteristics
    ColorPrimaries color_primaries = ColorPrimaries::BT2020;
    TransferFunction transfer_function = TransferFunction::PQ;
    HDRStandard preferred_standard = HDRStandard::HDR10;
    
    // Platform-specific settings
    bool require_dynamic_metadata = false;
    bool allow_tone_mapping = true;
    std::string platform_name;
};

/**
 * HDR Utilities class providing streaming platform configurations
 */
class HDRUtilities {
public:
    /**
     * Get YouTube HDR workflow configuration
     * Optimized for YouTube's HDR requirements and recommendations
     */
    static HDRWorkflowConfig get_youtube_hdr_config();
    
    /**
     * Get Netflix HDR workflow configuration  
     * Matches Netflix's technical delivery specifications
     */
    static HDRWorkflowConfig get_netflix_hdr_config();
    
    /**
     * Get Apple TV+ HDR workflow configuration
     * Aligned with Apple's HDR content guidelines
     */
    static HDRWorkflowConfig get_apple_tv_hdr_config();
    
    /**
     * Get broadcast HDR workflow configuration
     * Suitable for traditional broadcast delivery
     */
    static HDRWorkflowConfig get_broadcast_hdr_config();
    
    /**
     * Validate HDR workflow configuration
     * @param config Configuration to validate
     * @return True if configuration is valid and achievable
     */
    static bool validate_workflow_config(const HDRWorkflowConfig& config);
    
    /**
     * Convert HDR metadata to workflow configuration
     * @param metadata Source HDR metadata
     * @param platform_hint Target platform hint
     * @return Optimized workflow configuration
     */
    static HDRWorkflowConfig metadata_to_workflow(const HDRMetadata& metadata, 
                                                   const std::string& platform_hint = "");
    
    /**
     * Check if workflow requires tone mapping
     * @param source_config Source workflow configuration
     * @param target_config Target workflow configuration  
     * @return True if tone mapping is needed
     */
    static bool requires_tone_mapping(const HDRWorkflowConfig& source_config,
                                       const HDRWorkflowConfig& target_config);
};

} // namespace ve::media_io
