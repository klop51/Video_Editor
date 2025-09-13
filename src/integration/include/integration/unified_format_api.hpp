#pragma once

#include "../../../media_io/include/media_io/format_detector.hpp"
#include "../../../decode/include/decode/decoder_interface.hpp" 
#include "../../../render/include/render/encoder_interface.hpp"
#include "../../../quality/include/quality/format_validator.hpp"
#include "../../../quality/include/quality/quality_metrics.hpp"
#include "../../../standards/include/standards/compliance_engine.hpp"
#include "../../../monitoring/include/monitoring/quality_system_monitoring.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace ve::integration {

/**
 * Unified Format Support API
 * Single entry point for all professional video format operations
 * Provides consistent interface across all codecs, containers, and quality systems
 */

/**
 * Format Support Capabilities Summary
 * Complete overview of supported formats and features
 */
struct FormatSupportMatrix {
    // Video codec support
    struct VideoCodecSupport {
        std::string codec_name;
        std::vector<std::string> supported_profiles;
        std::vector<std::string> supported_levels;
        std::vector<uint32_t> supported_bit_depths;
        std::vector<std::string> supported_chroma_formats;
        std::vector<std::pair<uint32_t, uint32_t>> supported_resolutions;
        std::vector<double> supported_frame_rates;
        bool hardware_decode_available = false;
        bool hardware_encode_available = false;
        bool hdr_support = false;
        bool professional_features = false;
        std::string implementation_notes;
    };
    
    // Audio codec support
    struct AudioCodecSupport {
        std::string codec_name;
        std::vector<uint32_t> supported_sample_rates;
        std::vector<uint32_t> supported_channel_counts;
        std::vector<uint32_t> supported_bit_depths;
        std::vector<uint32_t> supported_bitrates_kbps;
        bool lossless_support = false;
        bool multichannel_support = false;
        bool professional_features = false;
        std::string implementation_notes;
    };
    
    // Container format support
    struct ContainerSupport {
        std::string container_name;
        std::vector<std::string> file_extensions;
        std::vector<std::string> supported_video_codecs;
        std::vector<std::string> supported_audio_codecs;
        bool metadata_support = false;
        bool subtitle_support = false;
        bool chapter_support = false;
        bool professional_workflow_support = false;
        std::string implementation_notes;
    };
    
    std::vector<VideoCodecSupport> video_codecs;
    std::vector<AudioCodecSupport> audio_codecs;
    std::vector<ContainerSupport> containers;
    
    // Summary statistics
    uint32_t total_video_codecs = 0;
    uint32_t total_audio_codecs = 0;
    uint32_t total_container_formats = 0;
    uint32_t hardware_accelerated_codecs = 0;
    uint32_t professional_workflow_codecs = 0;
    uint32_t hdr_capable_codecs = 0;
    
    std::string implementation_version;
    std::string last_updated;
    std::string implementation_status;
};

/**
 * Unified Format Operations Interface
 * Single interface for all format operations with consistent error handling
 */
class UnifiedFormatAPI {
public:
    // System initialization and configuration
    static bool initialize(const std::map<std::string, std::string>& config = {});
    static void shutdown();
    static bool isInitialized();
    
    // Format detection and analysis
    static media_io::FormatInfo detectFormat(const std::string& file_path);
    static media_io::FormatInfo analyzeStream(const std::vector<uint8_t>& data);
    static bool isFormatSupported(const media_io::FormatInfo& format_info);
    static std::vector<std::string> getSupportedExtensions();
    
    // Decoding operations
    static std::unique_ptr<decode::DecoderInterface> createDecoder(const media_io::FormatInfo& format_info);
    static std::unique_ptr<decode::DecoderInterface> createOptimalDecoder(const std::string& file_path);
    static bool canDecode(const media_io::FormatInfo& format_info);
    static std::vector<std::string> getDecoderRecommendations(const media_io::FormatInfo& format_info);
    
    // Encoding operations
    static std::unique_ptr<render::EncoderInterface> createEncoder(const std::string& codec_name);
    static std::unique_ptr<render::EncoderInterface> createOptimalEncoder(const media_io::FormatInfo& target_format);
    static bool canEncode(const std::string& codec_name);
    static std::vector<std::string> getEncoderRecommendations(const media_io::FormatInfo& target_format);
    
    // Quality validation and compliance
    static quality::FormatValidationReport validateFormat(const std::string& file_path,
                                                         quality::ValidationLevel level = quality::ValidationLevel::STANDARD);
    static quality::QualityAnalysisReport analyzeQuality(const std::string& file_path,
                                                        const std::string& reference_path = "");
    static standards::ComplianceReport checkCompliance(const std::string& file_path,
                                                      const std::string& standard_name);
    
    // Format conversion and transcoding
    static bool transcodeFile(const std::string& input_path,
                            const std::string& output_path,
                            const media_io::FormatInfo& target_format,
                            const std::map<std::string, std::string>& options = {});
    
    static bool convertFormat(const std::string& input_path,
                            const std::string& output_path,
                            const std::string& target_codec,
                            const std::map<std::string, std::string>& settings = {});
    
    // Batch operations
    static std::vector<std::string> processBatch(const std::vector<std::string>& input_files,
                                                const std::string& output_directory,
                                                const media_io::FormatInfo& target_format,
                                                const std::map<std::string, std::string>& options = {});
    
    // Format support matrix and capabilities
    static FormatSupportMatrix getFormatSupportMatrix();
    static VideoCodecSupport getVideoCodecSupport(const std::string& codec_name);
    static AudioCodecSupport getAudioCodecSupport(const std::string& codec_name);
    static ContainerSupport getContainerSupport(const std::string& container_name);
    
    // Professional workflow support
    static bool validateProfessionalWorkflow(const std::string& file_path,
                                           const std::string& workflow_type);
    static std::vector<std::string> getProfessionalFormatRecommendations(const std::string& use_case);
    static bool checkBroadcastCompliance(const std::string& file_path,
                                       const std::string& broadcast_standard);
    
    // System status and monitoring
    static monitoring::QualitySystemHealthMonitor::SystemHealthReport getSystemHealth();
    static std::vector<std::string> getSystemAlerts();
    static monitoring::PerformanceMetricsCollector::PerformanceReport getPerformanceMetrics();
    
    // Configuration and settings
    static void setHardwareAcceleration(bool enabled);
    static void setQualityLevel(quality::ValidationLevel level);
    static void setComplianceStandards(const std::vector<std::string>& standards);
    static void setPerformanceMode(const std::string& mode); // "quality", "balanced", "performance"
    
    // Error handling and diagnostics
    static std::vector<std::string> getLastErrors();
    static void clearErrors();
    static std::string getDiagnosticInfo();
    static bool runSystemDiagnostics();

private:
    static bool initialized_;
    static std::map<std::string, std::string> system_config_;
    static std::vector<std::string> error_log_;
    static FormatSupportMatrix support_matrix_;
    
    static void initializeFormatSupport();
    static void initializeQualitySystems();
    static void initializeMonitoring();
    static void buildSupportMatrix();
    static void validateSystemConfiguration();
    static void logError(const std::string& error_message);
};

/**
 * Format Workflow Manager
 * Manages complex format workflows and professional pipelines
 */
class FormatWorkflowManager {
public:
    enum class WorkflowType {
        BROADCAST_DELIVERY,     // Broadcast television delivery
        STREAMING_OPTIMIZATION, // Streaming service optimization
        CINEMA_MASTERING,       // Digital cinema mastering
        ARCHIVE_PRESERVATION,   // Long-term archive storage
        PROFESSIONAL_EDITING,   // Professional editing workflows
        WEB_DELIVERY,          // Web-optimized delivery
        MOBILE_OPTIMIZATION,   // Mobile device optimization
        CUSTOM_WORKFLOW        // User-defined custom workflow
    };
    
    struct WorkflowStep {
        std::string step_name;
        std::string description;
        std::function<bool(const std::string&, const std::string&)> operation;
        std::map<std::string, std::string> parameters;
        bool is_required = true;
        std::string failure_action = "abort";  // "abort", "skip", "retry"
    };
    
    struct WorkflowConfiguration {
        WorkflowType type;
        std::string workflow_name;
        std::string description;
        std::vector<WorkflowStep> steps;
        
        // Input requirements
        std::vector<std::string> supported_input_formats;
        std::vector<std::string> required_input_properties;
        
        // Output specifications
        media_io::FormatInfo target_format;
        quality::ValidationLevel validation_level;
        std::vector<std::string> compliance_standards;
        
        // Quality requirements
        std::map<std::string, double> quality_thresholds;
        bool enable_quality_validation = true;
        bool enable_compliance_checking = true;
        
        // Performance settings
        bool enable_hardware_acceleration = true;
        uint32_t max_parallel_operations = 4;
        std::chrono::minutes timeout_per_step{30};
        
        std::string workflow_version;
        std::string created_by;
        std::chrono::system_clock::time_point created_date;
    };
    
    struct WorkflowResult {
        WorkflowType workflow_type;
        std::string workflow_name;
        bool success = false;
        std::chrono::milliseconds total_duration{0};
        
        // Step results
        std::vector<std::string> completed_steps;
        std::vector<std::string> failed_steps;
        std::map<std::string, std::chrono::milliseconds> step_durations;
        
        // Quality results
        quality::FormatValidationReport validation_report;
        quality::QualityAnalysisReport quality_report;
        standards::ComplianceReport compliance_report;
        
        // Output information
        std::string output_file_path;
        media_io::FormatInfo output_format;
        size_t output_file_size_bytes = 0;
        
        // Issues and recommendations
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        std::vector<std::string> recommendations;
        
        std::string summary;
    };
    
    // Workflow management
    static WorkflowResult executeWorkflow(const WorkflowConfiguration& workflow,
                                        const std::string& input_file,
                                        const std::string& output_file);
    
    static std::vector<WorkflowConfiguration> getAvailableWorkflows();
    static WorkflowConfiguration getWorkflowByType(WorkflowType type);
    static WorkflowConfiguration createCustomWorkflow(const std::string& workflow_name,
                                                     const std::vector<WorkflowStep>& steps);
    
    // Predefined workflows
    static WorkflowConfiguration createBroadcastDeliveryWorkflow(const std::string& broadcast_standard);
    static WorkflowConfiguration createStreamingOptimizationWorkflow(const std::string& platform);
    static WorkflowConfiguration createCinemaMasteringWorkflow(const std::string& cinema_standard);
    static WorkflowConfiguration createArchivePreservationWorkflow();
    static WorkflowConfiguration createProfessionalEditingWorkflow();
    
    // Workflow validation
    static bool validateWorkflow(const WorkflowConfiguration& workflow);
    static std::vector<std::string> getWorkflowIssues(const WorkflowConfiguration& workflow);
    static std::vector<std::string> getWorkflowRecommendations(const WorkflowConfiguration& workflow);
    
    // Batch workflow processing
    static std::vector<WorkflowResult> executeBatchWorkflow(const WorkflowConfiguration& workflow,
                                                          const std::vector<std::string>& input_files,
                                                          const std::string& output_directory);
    
    // Workflow reporting
    static std::string generateWorkflowReport(const WorkflowResult& result);
    static void exportWorkflowResults(const std::vector<WorkflowResult>& results,
                                    const std::string& output_path);

private:
    static std::map<WorkflowType, WorkflowConfiguration> predefined_workflows_;
    static std::vector<WorkflowConfiguration> custom_workflows_;
    
    static void initializePredefinedWorkflows();
    static bool executeWorkflowStep(const WorkflowStep& step,
                                  const std::string& input_file,
                                  const std::string& output_file);
    static void validateWorkflowResult(WorkflowResult& result);
};

/**
 * Format Migration Assistant
 * Helps migrate between different format standards and versions
 */
class FormatMigrationAssistant {
public:
    enum class MigrationType {
        CODEC_UPGRADE,          // Upgrade to newer codec version
        STANDARD_MIGRATION,     // Migrate to new industry standard
        QUALITY_IMPROVEMENT,    // Improve quality while maintaining compatibility
        SIZE_OPTIMIZATION,      // Optimize file size while maintaining quality
        COMPATIBILITY_UPDATE,   // Update for broader compatibility
        FUTURE_PROOFING        // Migrate to future-proof formats
    };
    
    struct MigrationPlan {
        MigrationType type;
        std::string plan_name;
        std::string description;
        
        // Source and target
        media_io::FormatInfo source_format;
        media_io::FormatInfo target_format;
        
        // Migration strategy
        std::string migration_strategy;
        std::vector<std::string> required_steps;
        std::map<std::string, std::string> conversion_parameters;
        
        // Quality preservation
        double expected_quality_retention = 0.95;
        std::vector<std::string> quality_preservation_techniques;
        
        // Compatibility information
        std::vector<std::string> compatibility_gains;
        std::vector<std::string> compatibility_losses;
        std::vector<std::string> alternative_options;
        
        // Timeline and effort
        std::chrono::minutes estimated_time_per_file{5};
        std::string complexity_level = "medium";
        std::vector<std::string> potential_issues;
        
        bool reversible = false;
        std::string rollback_strategy;
    };
    
    struct MigrationResult {
        MigrationType migration_type;
        std::string plan_name;
        bool success = false;
        
        // File information
        std::string source_file;
        std::string target_file;
        size_t source_size_bytes = 0;
        size_t target_size_bytes = 0;
        double size_change_percent = 0.0;
        
        // Quality metrics
        double quality_retention_score = 0.0;
        std::map<std::string, double> quality_metrics;
        bool quality_acceptable = false;
        
        // Performance metrics
        std::chrono::milliseconds conversion_time{0};
        double conversion_speed_fps = 0.0;
        
        // Validation results
        quality::FormatValidationReport validation_report;
        standards::ComplianceReport compliance_report;
        
        // Issues and recommendations
        std::vector<std::string> migration_issues;
        std::vector<std::string> quality_issues;
        std::vector<std::string> recommendations;
        
        std::string migration_summary;
    };
    
    // Migration planning
    static std::vector<MigrationPlan> analyzeMigrationOptions(const media_io::FormatInfo& current_format,
                                                             const std::string& target_use_case);
    
    static MigrationPlan createMigrationPlan(const media_io::FormatInfo& source_format,
                                           const media_io::FormatInfo& target_format,
                                           MigrationType type);
    
    static std::vector<std::string> getMigrationRecommendations(const media_io::FormatInfo& current_format);
    
    // Migration execution
    static MigrationResult executeMigration(const MigrationPlan& plan,
                                          const std::string& source_file,
                                          const std::string& target_file);
    
    static std::vector<MigrationResult> executeBatchMigration(const MigrationPlan& plan,
                                                            const std::vector<std::string>& source_files,
                                                            const std::string& target_directory);
    
    // Migration validation
    static bool validateMigrationPlan(const MigrationPlan& plan);
    static std::vector<std::string> getMigrationRisks(const MigrationPlan& plan);
    static double estimateMigrationQuality(const MigrationPlan& plan);
    
    // Rollback support
    static bool canRollback(const MigrationResult& result);
    static bool rollbackMigration(const MigrationResult& result);
    static std::string createRollbackPlan(const MigrationPlan& plan);
    
    // Migration reporting
    static std::string generateMigrationReport(const std::vector<MigrationResult>& results);
    static void exportMigrationAnalysis(const std::vector<MigrationPlan>& plans,
                                       const std::string& output_path);

private:
    static std::vector<MigrationPlan> predefined_migration_plans_;
    
    static void initializeMigrationPlans();
    static double calculateQualityRetention(const media_io::FormatInfo& source,
                                          const media_io::FormatInfo& target);
    static std::vector<std::string> analyzeCompatibilityImpact(const media_io::FormatInfo& source,
                                                             const media_io::FormatInfo& target);
    static std::chrono::minutes estimateConversionTime(const MigrationPlan& plan, size_t file_size_mb);
};

} // namespace ve::integration
