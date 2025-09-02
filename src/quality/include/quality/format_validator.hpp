#pragma once

#include "core/frame.hpp"
#include "media_io/format_types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace ve::quality {

/**
 * Comprehensive Format Validation System
 * Industry-standard validation for professional video workflows
 */

enum class ValidationLevel {
    BASIC = 0,              // Basic format structure validation
    PROFESSIONAL = 1,       // Professional workflow compliance
    BROADCAST = 2,          // Broadcast delivery standards
    MASTERING = 3,          // Mastering and archival standards
    FORENSIC = 4           // Forensic-level validation
};

enum class ValidationResult {
    PASSED = 0,             // Validation passed
    WARNING = 1,            // Minor issues detected
    FAILED = 2,             // Validation failed
    ERROR = 3,              // Validation could not complete
    NOT_APPLICABLE = 4      // Validation not applicable to format
};

struct ValidationIssue {
    enum class Severity {
        INFO,               // Informational message
        WARNING,            // Warning - may cause issues
        ERROR,              // Error - will cause problems
        CRITICAL            // Critical - format unusable
    };
    
    Severity severity = Severity::INFO;
    std::string category;           // "codec", "container", "metadata", etc.
    std::string issue_code;         // Machine-readable issue identifier
    std::string description;        // Human-readable description
    std::string recommendation;     // How to fix the issue
    std::string standard_reference; // Reference to violated standard
    
    // Location information
    uint64_t byte_offset = 0;       // Byte offset where issue occurs
    uint32_t frame_number = 0;      // Frame number (if applicable)
    double timestamp_seconds = 0.0; // Timestamp (if applicable)
    
    // Additional context
    std::map<std::string, std::string> metadata;
};

struct FormatValidationReport {
    ValidationResult overall_result = ValidationResult::PASSED;
    ValidationLevel validation_level = ValidationLevel::BASIC;
    std::string format_name;
    std::string codec_name;
    std::string container_format;
    
    // File information
    uint64_t file_size_bytes = 0;
    double duration_seconds = 0.0;
    uint32_t total_frames = 0;
    
    // Format specifications
    uint32_t width = 0;
    uint32_t height = 0;
    double frame_rate = 0.0;
    uint32_t bit_depth = 0;
    std::string color_space;
    std::string pixel_format;
    
    // Audio information
    uint32_t audio_channels = 0;
    uint32_t audio_sample_rate = 0;
    std::string audio_codec;
    
    // Validation results
    std::vector<ValidationIssue> issues;
    std::map<std::string, ValidationResult> category_results;
    
    // Performance metrics
    double validation_time_seconds = 0.0;
    uint64_t memory_usage_bytes = 0;
    
    // Standards compliance
    std::map<std::string, ValidationResult> standards_compliance;
    std::vector<std::string> applicable_standards;
    
    // Quality metrics
    double overall_quality_score = 0.0;    // 0-100 quality score
    std::map<std::string, double> quality_metrics;
    
    // Statistics
    uint32_t warnings_count = 0;
    uint32_t errors_count = 0;
    uint32_t critical_issues_count = 0;
};

class FormatValidator {
public:
    FormatValidator();
    ~FormatValidator();
    
    // Main validation interface
    FormatValidationReport validateFile(const std::string& file_path, 
                                       ValidationLevel level = ValidationLevel::PROFESSIONAL);
    
    FormatValidationReport validateFrame(const Frame& frame,
                                       const std::string& format_context = "");
    
    FormatValidationReport validateStream(const uint8_t* data, size_t size,
                                        const std::string& format_hint = "");
    
    // Configuration
    void setValidationLevel(ValidationLevel level);
    void enableStandardValidation(const std::string& standard_name, bool enable = true);
    void setStrictMode(bool strict);
    void setValidationTimeout(uint32_t timeout_seconds);
    
    // Standard-specific validation
    ValidationResult validateBroadcastCompliance(const FormatValidationReport& report);
    ValidationResult validateWebDeliveryCompliance(const FormatValidationReport& report);
    ValidationResult validateCinemaCompliance(const FormatValidationReport& report);
    ValidationResult validateArchivalCompliance(const FormatValidationReport& report);
    
    // Batch validation
    std::vector<FormatValidationReport> validateDirectory(const std::string& directory_path,
                                                         bool recursive = false);
    FormatValidationReport validatePlaylist(const std::vector<std::string>& file_paths);
    
    // Custom validation rules
    void addCustomRule(const std::string& rule_name, 
                      std::function<ValidationIssue(const FormatValidationReport&)> validator);
    void removeCustomRule(const std::string& rule_name);
    
    // Reporting
    std::string generateHTMLReport(const FormatValidationReport& report);
    std::string generateJSONReport(const FormatValidationReport& report);
    std::string generateXMLReport(const FormatValidationReport& report);
    bool exportReport(const FormatValidationReport& report, 
                     const std::string& output_path,
                     const std::string& format = "html");
    
    // Statistics and analytics
    struct ValidationStatistics {
        uint32_t total_files_validated = 0;
        uint32_t passed_files = 0;
        uint32_t warning_files = 0;
        uint32_t failed_files = 0;
        uint32_t error_files = 0;
        
        std::map<std::string, uint32_t> format_counts;
        std::map<std::string, uint32_t> issue_counts;
        std::map<std::string, double> average_quality_scores;
        
        double total_validation_time = 0.0;
        uint64_t total_data_processed = 0;
    };
    
    ValidationStatistics getValidationStatistics() const;
    void resetStatistics();

private:
    struct ValidatorImpl;
    std::unique_ptr<ValidatorImpl> impl_;
    
    // Core validation methods
    ValidationResult validateCodecCompliance(const std::string& codec_name,
                                           const std::map<std::string, std::string>& properties);
    ValidationResult validateContainerCompliance(const std::string& container_format,
                                                const std::map<std::string, std::string>& properties);
    ValidationResult validateMetadataCompliance(const std::map<std::string, std::string>& metadata);
    
    // Specific format validators
    ValidationResult validateH264(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    ValidationResult validateHEVC(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    ValidationResult validateAV1(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    ValidationResult validateProRes(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    ValidationResult validateDNxHD(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    
    // Container validators
    ValidationResult validateMP4(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    ValidationResult validateMOV(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    ValidationResult validateMXF(const uint8_t* data, size_t size, std::vector<ValidationIssue>& issues);
    
    // Quality analysis
    double calculateOverallQuality(const FormatValidationReport& report);
    void analyzeVideoQuality(const Frame& frame, std::map<std::string, double>& quality_metrics);
    void analyzeAudioQuality(const uint8_t* audio_data, size_t size, 
                           std::map<std::string, double>& quality_metrics);
    
    // Standards compliance checkers
    bool checkSMPTECompliance(const FormatValidationReport& report, const std::string& standard);
    bool checkEBUCompliance(const FormatValidationReport& report, const std::string& standard);
    bool checkITURCompliance(const FormatValidationReport& report, const std::string& standard);
    
    // Issue management
    void addIssue(std::vector<ValidationIssue>& issues, 
                 ValidationIssue::Severity severity,
                 const std::string& category,
                 const std::string& code,
                 const std::string& description,
                 const std::string& recommendation = "");
    
    bool isIssueFiltered(const ValidationIssue& issue);
    void deduplicateIssues(std::vector<ValidationIssue>& issues);
};

/**
 * Professional Format Compliance Checker
 * Specialized validation for professional video workflows
 */
class ProfessionalComplianceChecker {
public:
    // Professional workflow validation
    static ValidationResult validateEditorialWorkflow(const FormatValidationReport& report);
    static ValidationResult validateColorGradingWorkflow(const FormatValidationReport& report);
    static ValidationResult validateVFXWorkflow(const FormatValidationReport& report);
    static ValidationResult validateAudioPostWorkflow(const FormatValidationReport& report);
    
    // Camera format validation
    static ValidationResult validateCameraCompliance(const FormatValidationReport& report,
                                                   const std::string& camera_model);
    static std::vector<std::string> getSupportedCameraFormats(const std::string& camera_model);
    
    // NLE compatibility
    static ValidationResult validateNLECompatibility(const FormatValidationReport& report,
                                                   const std::string& nle_name);
    static std::vector<std::string> getRecommendedFormats(const std::string& nle_name);
    
    // Delivery format validation
    static ValidationResult validateDeliveryFormat(const FormatValidationReport& report,
                                                  const std::string& delivery_spec);
    static std::vector<std::string> getDeliveryRequirements(const std::string& delivery_spec);
    
    // Archive format validation
    static ValidationResult validateArchiveFormat(const FormatValidationReport& report);
    static std::vector<std::string> getArchiveRecommendations(const FormatValidationReport& report);

private:
    static bool checkResolutionCompliance(uint32_t width, uint32_t height, 
                                        const std::string& workflow_type);
    static bool checkFrameRateCompliance(double frame_rate, const std::string& workflow_type);
    static bool checkColorSpaceCompliance(const std::string& color_space, 
                                        const std::string& workflow_type);
    static bool checkCodecCompliance(const std::string& codec, const std::string& workflow_type);
};

/**
 * Format Compatibility Matrix
 * Comprehensive compatibility information for professional workflows
 */
class FormatCompatibilityMatrix {
public:
    struct CompatibilityInfo {
        bool is_supported = false;
        std::string support_level;      // "native", "transcode", "proxy", "unsupported"
        std::string performance_level;  // "excellent", "good", "fair", "poor"
        std::vector<std::string> limitations;
        std::vector<std::string> recommendations;
        std::string workflow_impact;    // Description of workflow impact
    };
    
    // Application compatibility
    static CompatibilityInfo checkNLECompatibility(const std::string& format_name,
                                                  const std::string& nle_name,
                                                  const std::string& nle_version = "");
    
    static CompatibilityInfo checkDAWCompatibility(const std::string& audio_format,
                                                  const std::string& daw_name,
                                                  const std::string& daw_version = "");
    
    // Hardware compatibility
    static CompatibilityInfo checkHardwareCompatibility(const std::string& format_name,
                                                       const std::string& hardware_spec);
    
    // Platform compatibility
    static CompatibilityInfo checkPlatformCompatibility(const std::string& format_name,
                                                       const std::string& platform);
    
    // Workflow compatibility
    static std::map<std::string, CompatibilityInfo> getWorkflowCompatibility(
        const std::string& format_name);
    
    // Matrix queries
    static std::vector<std::string> getRecommendedFormats(const std::string& use_case);
    static std::vector<std::string> getAlternativeFormats(const std::string& current_format,
                                                         const std::string& target_workflow);
    
    // Compatibility scoring
    static double calculateCompatibilityScore(const std::string& format_name,
                                            const std::vector<std::string>& requirements);

private:
    static std::map<std::string, std::map<std::string, CompatibilityInfo>> compatibility_matrix_;
    static void initializeCompatibilityMatrix();
    
    static CompatibilityInfo getDefaultCompatibility();
    static void loadCompatibilityData(const std::string& data_file);
};

/**
 * Format Validation Database
 * Centralized knowledge base for format validation rules
 */
class FormatValidationDatabase {
public:
    // Rule management
    void loadValidationRules(const std::string& rules_file);
    void saveValidationRules(const std::string& rules_file);
    void addValidationRule(const std::string& format_name,
                          const std::string& rule_name,
                          const std::function<ValidationIssue(const FormatValidationReport&)>& validator);
    
    // Standard definitions
    void loadStandardDefinitions(const std::string& standards_file);
    std::vector<std::string> getApplicableStandards(const std::string& format_name);
    std::map<std::string, std::string> getStandardRequirements(const std::string& standard_name);
    
    // Knowledge base queries
    std::vector<ValidationIssue> getKnownIssues(const std::string& format_name);
    std::vector<std::string> getRecommendedSettings(const std::string& format_name,
                                                   const std::string& use_case);
    
    // Update management
    void updateDatabase(const std::string& update_source);
    std::string getDatabaseVersion() const;
    bool isDatabaseOutdated() const;

private:
    std::map<std::string, std::vector<std::function<ValidationIssue(const FormatValidationReport&)>>> validation_rules_;
    std::map<std::string, std::map<std::string, std::string>> standard_definitions_;
    std::map<std::string, std::vector<ValidationIssue>> known_issues_;
    std::string database_version_;
    std::chrono::system_clock::time_point last_update_;
};

} // namespace ve::quality
