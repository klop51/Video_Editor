#pragma once

#include "core/frame.hpp"
#include "core/math_types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ve::media_io {

/**
 * Immersive Video Format Support
 * 360°/VR video formats with spatial metadata and rendering
 */

enum class ProjectionType {
    UNKNOWN = 0,
    EQUIRECTANGULAR,        // Spherical projection (most common)
    CUBEMAP,                // Cube map projection
    CYLINDRICAL,            // Cylindrical projection
    FISHEYE,                // Fisheye projection
    MESH,                   // Custom mesh-based projection
    EAC,                    // Equi-Angular Cubemap (YouTube/Facebook)
    OHP,                    // Octahedral Projection
    PERSPECTIVE,            // Standard perspective (non-360)
    STEREOGRAPHIC,          // Stereographic projection
    MERCATOR,               // Mercator projection
    HAMMER_AITOFF          // Hammer-Aitoff projection
};

enum class StereoMode {
    MONO = 0,               // Monoscopic (single view)
    TOP_BOTTOM,             // Stereoscopic top-bottom
    LEFT_RIGHT,             // Stereoscopic left-right
    TOP_BOTTOM_LR,          // Top-bottom with left-right encoding
    LEFT_RIGHT_TB,          // Left-right with top-bottom encoding
    FRAME_SEQUENTIAL,       // Frame sequential stereo
    SEPARATE_STREAMS        // Separate video streams for each eye
};

enum class ViewingMode {
    IMMERSIVE_360,          // Full 360-degree immersive
    IMMERSIVE_180,          // 180-degree front-facing
    DOME,                   // Dome/planetarium projection
    WINDOW,                 // Traditional windowed view
    VR_HEADSET,             // VR headset optimized
    AR_OVERLAY,             // Augmented reality overlay
    HOLOGRAPHIC             // Holographic display
};

struct SpatialAudioInfo {
    bool has_spatial_audio = false;
    uint32_t audio_channels = 0;
    std::string audio_format;           // "ambisonics", "binaural", "5.1", etc.
    uint32_t ambisonic_order = 0;       // First-order, second-order, etc.
    bool head_tracking_supported = false;
    double listener_position[3] = {0.0, 0.0, 0.0};  // X, Y, Z
    double listener_orientation[4] = {0.0, 0.0, 0.0, 1.0}; // Quaternion
};

struct ViewportInfo {
    // Field of view
    double horizontal_fov_degrees = 360.0;
    double vertical_fov_degrees = 180.0;
    
    // Initial viewing direction
    double yaw_degrees = 0.0;           // Horizontal rotation
    double pitch_degrees = 0.0;         // Vertical rotation
    double roll_degrees = 0.0;          // Roll rotation
    
    // Viewing bounds (for partial spheres)
    double min_yaw = -180.0;
    double max_yaw = 180.0;
    double min_pitch = -90.0;
    double max_pitch = 90.0;
    
    // Viewport optimization
    bool variable_resolution = false;    // Higher res in viewing direction
    double center_quality_factor = 1.0;  // Quality boost for center region
    uint32_t tiles_horizontal = 1;       // Tiled encoding
    uint32_t tiles_vertical = 1;
};

struct ImmersiveMetadata {
    ProjectionType projection = ProjectionType::EQUIRECTANGULAR;
    StereoMode stereo_mode = StereoMode::MONO;
    ViewingMode viewing_mode = ViewingMode::IMMERSIVE_360;
    
    ViewportInfo viewport;
    SpatialAudioInfo spatial_audio;
    
    // Projection-specific parameters
    struct {
        // Equirectangular
        double sphere_radius = 1.0;
        bool full_sphere = true;
        
        // Cubemap
        uint32_t cube_face_size = 0;
        std::string face_order = "RLUDFB";  // Right, Left, Up, Down, Front, Back
        
        // Fisheye
        double fisheye_fov = 180.0;
        double fisheye_center_x = 0.5;
        double fisheye_center_y = 0.5;
        double fisheye_radius = 0.5;
        
        // EAC (Equi-Angular Cubemap)
        uint32_t eac_face_width = 0;
        uint32_t eac_face_height = 0;
        
        // Custom mesh
        std::string mesh_file_path;
        std::vector<float> mesh_vertices;
        std::vector<uint32_t> mesh_indices;
        std::vector<float> mesh_uvs;
    } projection_params;
    
    // Quality and performance hints
    bool supports_foveated_rendering = false;
    bool supports_timewarp = false;
    uint32_t recommended_eye_resolution_width = 0;
    uint32_t recommended_eye_resolution_height = 0;
    uint32_t min_framerate = 60;         // Minimum for comfortable VR
    
    // Content classification
    std::string content_type;            // "live", "cgi", "mixed", "capture"
    std::string comfort_rating;          // "comfortable", "moderate", "intense"
    std::vector<std::string> content_warnings; // Motion sickness warnings
    
    // Platform-specific metadata
    std::map<std::string, std::string> platform_metadata; // Platform-specific fields
};

/**
 * Immersive Format Processor
 * Handles projection conversions and spatial transformations
 */
class ImmersiveFormatProcessor {
public:
    ImmersiveFormatProcessor();
    ~ImmersiveFormatProcessor();
    
    // Format detection and parsing
    bool detectImmersiveFormat(const Frame& frame);
    ImmersiveMetadata extractMetadata(const Frame& frame);
    bool parseSphericalMetadata(const std::string& metadata_xml);
    
    // Projection conversions
    Frame convertProjection(const Frame& input_frame, 
                          ProjectionType from_projection,
                          ProjectionType to_projection,
                          const ImmersiveMetadata& metadata);
    
    // Viewport extraction
    Frame extractViewport(const Frame& immersive_frame,
                         const ViewportInfo& viewport,
                         uint32_t output_width, uint32_t output_height);
    
    // Stereo processing
    std::pair<Frame, Frame> extractStereoViews(const Frame& stereo_frame, StereoMode mode);
    Frame combineStereoViews(const Frame& left_eye, const Frame& right_eye, StereoMode mode);
    
    // Quality optimization
    Frame applyFoveatedRendering(const Frame& input_frame,
                                const ViewportInfo& current_viewport,
                                double gaze_x, double gaze_y);
    
    // Stabilization and motion
    Frame stabilizeImmersiveVideo(const Frame& input_frame,
                                 const ImmersiveMetadata& metadata,
                                 double* motion_vector = nullptr);
    
    // Metadata management
    void setImmersiveMetadata(Frame& frame, const ImmersiveMetadata& metadata);
    bool validateMetadata(const ImmersiveMetadata& metadata);
    
    // Platform-specific export
    Frame prepareForPlatform(const Frame& immersive_frame,
                           const std::string& target_platform,
                           const ImmersiveMetadata& metadata);

private:
    struct ProjectionContext;
    std::unique_ptr<ProjectionContext> context_;
    
    // Projection algorithms
    Frame equirectangularToCubemap(const Frame& eq_frame, uint32_t face_size);
    Frame cubemapToEquirectangular(const Frame& cubemap_frame);
    Frame equirectangularToFisheye(const Frame& eq_frame, double fov);
    Frame fisheyeToEquirectangular(const Frame& fisheye_frame, double fov);
    Frame convertToEAC(const Frame& eq_frame);
    Frame convertFromEAC(const Frame& eac_frame);
    
    // Coordinate transformations
    Vec3f sphericalToCartesian(double theta, double phi);
    std::pair<double, double> cartesianToSpherical(const Vec3f& point);
    Vec2f projectToPlane(const Vec3f& world_point, ProjectionType projection);
    Vec3f unprojectFromPlane(const Vec2f& screen_point, ProjectionType projection);
    
    // Sampling and interpolation
    uint8_t sampleBilinear(const Frame& frame, double x, double y, int channel);
    uint8_t sampleBicubic(const Frame& frame, double x, double y, int channel);
    
    // Quality optimization helpers
    double calculateViewportPriority(double x, double y, const ViewportInfo& viewport);
    void applyQualityScaling(Frame& frame, const std::function<double(double, double)>& quality_func);
};

/**
 * VR/AR Metadata Standards Support
 * Support for industry-standard spatial video metadata
 */
class SpatialMetadataManager {
public:
    // Google VR180/360 metadata
    static std::string generateGoogleVRMetadata(const ImmersiveMetadata& metadata);
    static ImmersiveMetadata parseGoogleVRMetadata(const std::string& xml_metadata);
    
    // Facebook 360 metadata
    static std::string generateFacebook360Metadata(const ImmersiveMetadata& metadata);
    static ImmersiveMetadata parseFacebook360Metadata(const std::string& json_metadata);
    
    // MPEG-OMAF (Omnidirectional Media Format)
    static std::string generateOMafMetadata(const ImmersiveMetadata& metadata);
    static ImmersiveMetadata parseOMafMetadata(const std::string& omaf_data);
    
    // WebXR metadata
    static std::string generateWebXRMetadata(const ImmersiveMetadata& metadata);
    static ImmersiveMetadata parseWebXRMetadata(const std::string& webxr_json);
    
    // Platform-specific metadata
    static std::string generateOculusMetadata(const ImmersiveMetadata& metadata);
    static std::string generateSteamVRMetadata(const ImmersiveMetadata& metadata);
    static std::string generateMagicLeapMetadata(const ImmersiveMetadata& metadata);
    
    // Validation and compliance
    static bool validateForPlatform(const ImmersiveMetadata& metadata, 
                                  const std::string& platform);
    static std::vector<std::string> getComplianceIssues(const ImmersiveMetadata& metadata,
                                                       const std::string& platform);

private:
    static std::string generateSphericalXML(const ImmersiveMetadata& metadata);
    static ImmersiveMetadata parseSphericalXML(const std::string& xml_content);
};

/**
 * Spatial Audio Integration
 * Links spatial audio with immersive video
 */
class SpatialAudioProcessor {
public:
    // Audio format detection
    static SpatialAudioInfo analyzeSpatialAudio(const std::vector<uint8_t>& audio_data);
    static bool isAmbisonicAudio(const std::vector<uint8_t>& audio_data);
    static uint32_t detectAmbisonicOrder(const std::vector<uint8_t>& audio_data);
    
    // Spatial audio transformation
    static std::vector<uint8_t> transformSpatialAudio(
        const std::vector<uint8_t>& input_audio,
        const SpatialAudioInfo& input_info,
        const SpatialAudioInfo& output_info);
    
    // Head tracking integration
    static std::vector<uint8_t> applySpatialTransform(
        const std::vector<uint8_t>& ambisonic_audio,
        double listener_yaw, double listener_pitch, double listener_roll);
    
    // Binaural rendering
    static std::vector<uint8_t> renderBinaural(
        const std::vector<uint8_t>& spatial_audio,
        const SpatialAudioInfo& audio_info,
        double head_yaw, double head_pitch);
    
    // Platform-specific audio processing
    static std::vector<uint8_t> prepareForVRPlatform(
        const std::vector<uint8_t>& spatial_audio,
        const std::string& platform);

private:
    static Matrix4f calculateRotationMatrix(double yaw, double pitch, double roll);
    static std::vector<float> applyHRTF(const std::vector<float>& audio_channels,
                                       double azimuth, double elevation);
};

/**
 * Immersive Content Analytics
 * Analytics and optimization for immersive content
 */
class ImmersiveAnalytics {
public:
    struct ViewingStats {
        // Viewport usage
        std::vector<std::pair<double, double>> viewport_heatmap; // (yaw, pitch) viewing frequency
        double average_viewing_time_seconds = 0.0;
        double total_head_movement_degrees = 0.0;
        
        // Quality metrics
        double motion_sickness_risk = 0.0;    // 0.0 = comfortable, 1.0 = high risk
        double visual_comfort_score = 0.0;    // Visual comfort assessment
        double immersion_quality = 0.0;       // Overall immersion quality
        
        // Performance metrics
        uint32_t dropped_frames = 0;
        double average_framerate = 0.0;
        uint32_t tracking_lost_count = 0;
    };
    
    // Content analysis
    static ViewingStats analyzeImmersiveContent(const Frame& immersive_frame,
                                               const ImmersiveMetadata& metadata);
    
    // Motion sickness assessment
    static double assessMotionSicknessRisk(const std::vector<Frame>& frame_sequence,
                                         const ImmersiveMetadata& metadata);
    
    // Quality recommendations
    static std::vector<std::string> getQualityRecommendations(const ViewingStats& stats);
    static ImmersiveMetadata optimizeForComfort(const ImmersiveMetadata& original_metadata,
                                               const ViewingStats& viewing_data);
    
    // Platform optimization recommendations
    static std::vector<std::string> getPlatformOptimizations(const ViewingStats& stats,
                                                            const std::string& target_platform);

private:
    static double calculateOpticalFlow(const Frame& frame1, const Frame& frame2);
    static double calculateVisualComplexity(const Frame& frame);
    static std::vector<std::pair<double, double>> detectSalientRegions(const Frame& frame);
};

/**
 * Immersive Format Converter
 * Batch conversion and optimization tools
 */
class ImmersiveFormatConverter {
public:
    struct ConversionParams {
        ProjectionType target_projection = ProjectionType::EQUIRECTANGULAR;
        StereoMode target_stereo_mode = StereoMode::MONO;
        uint32_t target_width = 0;
        uint32_t target_height = 0;
        std::string target_platform;
        bool optimize_for_streaming = false;
        bool enable_foveated_encoding = false;
        double quality_factor = 1.0;
    };
    
    // Batch conversion
    static std::vector<Frame> convertImmersiveSequence(
        const std::vector<Frame>& input_frames,
        const ImmersiveMetadata& input_metadata,
        const ConversionParams& conversion_params);
    
    // Platform-specific optimization
    static ConversionParams getOptimalParamsForPlatform(const std::string& platform,
                                                       const ImmersiveMetadata& source_metadata);
    
    // Quality vs size optimization
    static ConversionParams optimizeForFileSize(const ImmersiveMetadata& metadata,
                                              uint64_t target_file_size_bytes);
    static ConversionParams optimizeForQuality(const ImmersiveMetadata& metadata,
                                             double target_quality_score);
    
    // Streaming optimization
    static std::vector<ConversionParams> generateAdaptiveStreamingLevels(
        const ImmersiveMetadata& metadata);

private:
    static uint64_t estimateOutputSize(const ConversionParams& params,
                                     const ImmersiveMetadata& metadata);
    static double estimateQualityScore(const ConversionParams& params,
                                     const ImmersiveMetadata& metadata);
};

} // namespace ve::media_io
