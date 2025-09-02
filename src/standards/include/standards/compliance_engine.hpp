#pragma once

#include "quality/format_validator.hpp"
#include "quality/quality_metrics.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace ve::standards {

/**
 * Standards Compliance Engine
 * Comprehensive validation against industry standards and specifications
 */

enum class StandardsOrganization {
    SMPTE,          // Society of Motion Picture and Television Engineers
    EBU,            // European Broadcasting Union
    ITU,            // International Telecommunication Union
    ITU_R,          // ITU Radiocommunication Sector
    ITU_T,          // ITU Telecommunication Standardization Sector
    ISO,            // International Organization for Standardization
    IEC,            // International Electrotechnical Commission
    ANSI,           // American National Standards Institute
    CTA,            // Consumer Technology Association
    NABA,           // North American Broadcasters Association
    ARIB,           // Association of Radio Industries and Businesses (Japan)
    DVB,            // Digital Video Broadcasting
    ATSC,           // Advanced Television Systems Committee
    SCTE,           // Society of Cable Telecommunications Engineers
    ALLIANCE_FOR_IP_MEDIA_SOLUTIONS, // AIMS
    DCI,            // Digital Cinema Initiative
    FIAF,           // International Federation of Film Archives
    CUSTOM          // Custom/proprietary standards
};

enum class ComplianceLevel {
    NOT_COMPLIANT = 0,      // Does not meet standard requirements
    PARTIALLY_COMPLIANT,    // Meets some requirements
    SUBSTANTIALLY_COMPLIANT, // Meets most requirements with minor issues
    FULLY_COMPLIANT,        // Meets all mandatory requirements
    EXCEEDS_STANDARD        // Exceeds standard requirements
};

struct StandardDefinition {
    std::string standard_id;                    // e.g., "SMPTE ST 2067-2"
    std::string standard_name;                  // Human-readable name
    std::string version;                        // Standard version
    StandardsOrganization organization;
    std::string publication_date;
    std::string description;
    std::string scope;                          // What the standard covers
    
    // Requirements
    struct Requirement {
        std::string requirement_id;
        std::string description;
        std::string category;                   // "mandatory", "recommended", "optional"
        std::string test_method;                // How to test compliance
        std::map<std::string, std::string> parameters; // Requirement parameters
        std::vector<std::string> dependencies; // Other requirements this depends on
    };
    std::vector<Requirement> requirements;
    
    // References
    std::vector<std::string> referenced_standards;
    std::vector<std::string> superseded_standards;
    std::vector<std::string> related_standards;
    
    // Applicability
    std::vector<std::string> applicable_formats;
    std::vector<std::string> applicable_workflows;
    std::map<std::string, std::string> applicability_conditions;
};

struct ComplianceTestResult {
    std::string requirement_id;
    std::string requirement_description;
    ComplianceLevel compliance_level = ComplianceLevel::NOT_COMPLIANT;
    bool test_executed = false;
    std::string test_result_details;
    std::vector<std::string> issues;
    std::vector<std::string> recommendations;
    std::map<std::string, std::string> measured_values;
    std::map<std::string, std::string> expected_values;
    double confidence_score = 0.0;             // 0.0-1.0
};

struct StandardsComplianceReport {
    std::string content_identifier;
    std::string standard_id;
    std::string standard_name;
    ComplianceLevel overall_compliance = ComplianceLevel::NOT_COMPLIANT;
    double compliance_score = 0.0;             // 0.0-100.0
    
    std::vector<ComplianceTestResult> test_results;
    
    // Summary statistics
    uint32_t mandatory_requirements_total = 0;
    uint32_t mandatory_requirements_passed = 0;
    uint32_t recommended_requirements_total = 0;
    uint32_t recommended_requirements_passed = 0;
    uint32_t optional_requirements_total = 0;
    uint32_t optional_requirements_passed = 0;
    
    // Issues and recommendations
    std::vector<std::string> critical_issues;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    
    // Certification information
    bool certification_eligible = false;
    std::string certification_level;
    std::vector<std::string> certification_requirements_missing;
    
    // Report metadata
    std::chrono::system_clock::time_point test_date;
    std::string test_version;
    std::string tester_information;
};

class StandardsComplianceEngine {
public:
    StandardsComplianceEngine();
    ~StandardsComplianceEngine();
    
    // Main compliance testing interface
    StandardsComplianceReport testCompliance(const std::string& file_path,
                                           const std::string& standard_id);
    
    StandardsComplianceReport testCompliance(const quality::FormatValidationReport& format_report,
                                           const quality::QualityAnalysisReport& quality_report,
                                           const std::string& standard_id);
    
    // Batch compliance testing
    std::vector<StandardsComplianceReport> testMultipleStandards(
        const std::string& file_path,
        const std::vector<std::string>& standard_ids);
    
    std::vector<StandardsComplianceReport> testDirectory(
        const std::string& directory_path,
        const std::string& standard_id,
        bool recursive = false);
    
    // Standard management
    void loadStandard(const StandardDefinition& standard);
    void loadStandardsFromFile(const std::string& standards_file);
    StandardDefinition getStandard(const std::string& standard_id) const;
    std::vector<std::string> getAvailableStandards() const;
    std::vector<std::string> getApplicableStandards(const std::string& format_name) const;
    
    // Configuration
    void setStrictMode(bool strict);                    // Fail on any non-compliance
    void setRequirementCategory(const std::string& category, bool enabled);
    void setTestTimeout(uint32_t timeout_seconds);
    void enableDetailedLogging(bool enabled);
    
    // Custom testing
    void addCustomTest(const std::string& standard_id,
                      const std::string& requirement_id,
                      std::function<ComplianceTestResult(const quality::FormatValidationReport&,
                                                       const quality::QualityAnalysisReport&)> test_function);
    
    // Reporting
    std::string generateComplianceReport(const StandardsComplianceReport& report,
                                       const std::string& format = "html");
    bool exportReport(const StandardsComplianceReport& report,
                     const std::string& output_path,
                     const std::string& format = "html");
    
    // Certification support
    bool isCertificationEligible(const StandardsComplianceReport& report);
    std::vector<std::string> getCertificationRequirements(const std::string& standard_id);
    std::string generateCertificationRequest(const StandardsComplianceReport& report);

private:
    struct ComplianceEngineImpl;
    std::unique_ptr<ComplianceEngineImpl> impl_;
    
    // Core compliance testing methods
    ComplianceTestResult testVideoCodecCompliance(const std::string& standard_id,
                                                 const std::string& requirement_id,
                                                 const quality::FormatValidationReport& format_report);
    
    ComplianceTestResult testAudioCodecCompliance(const std::string& standard_id,
                                                 const std::string& requirement_id,
                                                 const quality::FormatValidationReport& format_report);
    
    ComplianceTestResult testContainerCompliance(const std::string& standard_id,
                                                const std::string& requirement_id,
                                                const quality::FormatValidationReport& format_report);
    
    ComplianceTestResult testMetadataCompliance(const std::string& standard_id,
                                               const std::string& requirement_id,
                                               const quality::FormatValidationReport& format_report);
    
    ComplianceTestResult testQualityCompliance(const std::string& standard_id,
                                              const std::string& requirement_id,
                                              const quality::QualityAnalysisReport& quality_report);
    
    // Specific standard implementations
    StandardsComplianceReport testSMPTECompliance(const quality::FormatValidationReport& format_report,
                                                 const quality::QualityAnalysisReport& quality_report,
                                                 const std::string& smpte_standard);
    
    StandardsComplianceReport testEBUCompliance(const quality::FormatValidationReport& format_report,
                                               const quality::QualityAnalysisReport& quality_report,
                                               const std::string& ebu_standard);
    
    StandardsComplianceReport testITURCompliance(const quality::FormatValidationReport& format_report,
                                                const quality::QualityAnalysisReport& quality_report,
                                                const std::string& itur_standard);
    
    // Helper methods
    ComplianceLevel calculateOverallCompliance(const std::vector<ComplianceTestResult>& results);
    double calculateComplianceScore(const std::vector<ComplianceTestResult>& results);
    std::vector<std::string> extractCriticalIssues(const std::vector<ComplianceTestResult>& results);
    std::vector<std::string> generateRecommendations(const std::vector<ComplianceTestResult>& results);
};

/**
 * Broadcast Standards Compliance
 * Specialized compliance testing for broadcast delivery standards
 */
class BroadcastStandardsCompliance {
public:
    // Common broadcast standards
    enum class BroadcastStandard {
        AS_11_DPP,              // AS-11 DPP (UK Digital Production Partnership)
        AS_11_UKDPP,            // AS-11 UK DPP
        AS_11_XDCAM,            // AS-11 XDCAM variant
        EBU_R128,               // EBU R128 Audio Loudness
        EBU_R103,               // EBU R103 Video Quality
        SMPTE_ST_2067_2_IMF,    // SMPTE ST 2067-2 IMF Core Constraints
        SMPTE_ST_2067_3_IMF,    // SMPTE ST 2067-3 IMF Audio
        SMPTE_ST_2067_5_IMF,    // SMPTE ST 2067-5 IMF Video
        NETFLIX_TECHNICAL_SPEC, // Netflix Technical Specification
        AMAZON_TECHNICAL_SPEC,  // Amazon Technical Specification
        DISNEY_TECHNICAL_SPEC,  // Disney Technical Specification
        BBC_TECHNICAL_SPEC,     // BBC Technical Specification
        ATSC_3_0,              // ATSC 3.0 Standard
        DVB_T2,                // DVB-T2 Standard
        ISDB_T                 // ISDB-T Standard
    };
    
    // Broadcast compliance testing
    static StandardsComplianceReport testBroadcastCompliance(
        const quality::FormatValidationReport& format_report,
        const quality::QualityAnalysisReport& quality_report,
        BroadcastStandard standard);
    
    // Delivery format validation
    static bool validateDeliveryFormat(const quality::FormatValidationReport& format_report,
                                     BroadcastStandard target_standard);
    
    // Technical specifications
    static std::map<std::string, std::string> getTechnicalSpecs(BroadcastStandard standard);
    static std::vector<std::string> getRequiredMetadata(BroadcastStandard standard);
    static std::vector<std::string> getSupportedCodecs(BroadcastStandard standard);
    
    // Quality requirements
    static std::map<std::string, double> getQualityThresholds(BroadcastStandard standard);
    static std::vector<std::string> getMandatoryQualityChecks(BroadcastStandard standard);

private:
    // Specific standard validators
    static StandardsComplianceReport validateAS11DPP(const quality::FormatValidationReport& format_report,
                                                    const quality::QualityAnalysisReport& quality_report);
    static StandardsComplianceReport validateEBUR128(const quality::QualityAnalysisReport& quality_report);
    static StandardsComplianceReport validateIMF(const quality::FormatValidationReport& format_report,
                                                const quality::QualityAnalysisReport& quality_report);
    static StandardsComplianceReport validateStreamingSpec(const quality::FormatValidationReport& format_report,
                                                          const quality::QualityAnalysisReport& quality_report,
                                                          const std::string& platform);
};

/**
 * Cinema Standards Compliance
 * Digital Cinema Initiative (DCI) and related cinema standards
 */
class CinemaStandardsCompliance {
public:
    enum class CinemaStandard {
        DCI_SPECIFICATION,      // DCI Digital Cinema System Specification
        SMPTE_ST_429_2_DCP,    // SMPTE ST 429-2 DCP Format
        SMPTE_ST_428_1_DCDM,   // SMPTE ST 428-1 DCDM Format
        ISDCF_NAMING,          // ISDCF Naming Convention
        INTEROP_DCP,           // Interop DCP Format
        SMPTE_DCP,             // SMPTE DCP Format
        HFR_CINEMA,            // High Frame Rate Cinema
        HDR_CINEMA,            // HDR Cinema Standards
        IMMERSIVE_AUDIO_CINEMA // Immersive Audio Cinema Standards
    };
    
    static StandardsComplianceReport testCinemaCompliance(
        const quality::FormatValidationReport& format_report,
        const quality::QualityAnalysisReport& quality_report,
        CinemaStandard standard);
    
    static bool validateDCPStructure(const std::string& dcp_directory_path);
    static bool validateColorSpace(const quality::FormatValidationReport& format_report,
                                 const std::string& target_color_space);
    static bool validateAudioConfiguration(const quality::FormatValidationReport& format_report,
                                          CinemaStandard standard);

private:
    static StandardsComplianceReport validateDCISpec(const quality::FormatValidationReport& format_report,
                                                    const quality::QualityAnalysisReport& quality_report);
    static bool validateSecurityRequirements(const std::string& content_path);
    static bool validatePackagingRequirements(const std::string& package_path);
};

/**
 * Streaming Standards Compliance
 * OTT and streaming service technical specifications
 */
class StreamingStandardsCompliance {
public:
    enum class StreamingPlatform {
        NETFLIX,
        AMAZON_PRIME,
        DISNEY_PLUS,
        APPLE_TV_PLUS,
        HBO_MAX,
        PARAMOUNT_PLUS,
        PEACOCK,
        HULU,
        YOUTUBE,
        TIKTOK,
        INSTAGRAM,
        FACEBOOK,
        TWITTER,
        GENERIC_OTT
    };
    
    struct StreamingRequirements {
        std::vector<std::string> supported_codecs;
        std::vector<std::string> supported_containers;
        std::map<std::string, std::string> resolution_requirements;
        std::map<std::string, std::string> bitrate_requirements;
        std::map<std::string, std::string> audio_requirements;
        std::vector<std::string> subtitle_requirements;
        std::map<std::string, std::string> metadata_requirements;
        std::vector<std::string> quality_requirements;
    };
    
    static StandardsComplianceReport testStreamingCompliance(
        const quality::FormatValidationReport& format_report,
        const quality::QualityAnalysisReport& quality_report,
        StreamingPlatform platform);
    
    static StreamingRequirements getStreamingRequirements(StreamingPlatform platform);
    static bool validateStreamingFormat(const quality::FormatValidationReport& format_report,
                                      StreamingPlatform platform);
    static std::vector<std::string> getRecommendedEncodingSettings(StreamingPlatform platform);

private:
    static StreamingRequirements getNetflixRequirements();
    static StreamingRequirements getAmazonRequirements();
    static StreamingRequirements getDisneyRequirements();
    static StreamingRequirements getYouTubeRequirements();
    static StreamingRequirements getGenericOTTRequirements();
};

/**
 * Archive Standards Compliance
 * Long-term preservation and archival standards
 */
class ArchiveStandardsCompliance {
public:
    enum class ArchiveStandard {
        OAIS,                   // Open Archival Information System
        PREMIS,                 // Preservation Metadata Implementation Strategies
        METS,                   // Metadata Encoding and Transmission Standard
        FEDORA,                 // Fedora Digital Repository Architecture
        DUBLIN_CORE,            // Dublin Core Metadata Initiative
        FIAF_TECHNICAL_SPEC,    // FIAF Technical Guidelines
        IASA_TC03,             // IASA-TC 03 Audio Preservation
        IASA_TC04,             // IASA-TC 04 Video Preservation
        NDSA_LEVELS,           // NDSA Levels of Digital Preservation
        ISO_14721_OAIS         // ISO 14721 OAIS Reference Model
    };
    
    static StandardsComplianceReport testArchiveCompliance(
        const quality::FormatValidationReport& format_report,
        const quality::QualityAnalysisReport& quality_report,
        ArchiveStandard standard);
    
    static bool validatePreservationFormat(const quality::FormatValidationReport& format_report);
    static std::vector<std::string> getRecommendedPreservationFormats();
    static std::map<std::string, std::string> generatePreservationMetadata(
        const quality::FormatValidationReport& format_report);

private:
    static bool validateLongTermViability(const std::string& format_name);
    static bool validateMetadataCompleteness(const quality::FormatValidationReport& format_report);
    static double calculatePreservationRisk(const quality::FormatValidationReport& format_report);
};

/**
 * Standards Database Manager
 * Centralized management of standards definitions and updates
 */
class StandardsDatabaseManager {
public:
    // Database management
    void loadStandardsDatabase(const std::string& database_path);
    void saveStandardsDatabase(const std::string& database_path);
    void updateStandardsDatabase(const std::string& update_source);
    
    // Standard queries
    std::vector<StandardDefinition> searchStandards(const std::string& query);
    StandardDefinition getLatestVersion(const std::string& standard_id);
    std::vector<std::string> getRelatedStandards(const std::string& standard_id);
    
    // Versioning
    std::string getCurrentDatabaseVersion() const;
    bool isDatabaseUpToDate() const;
    std::vector<std::string> getAvailableUpdates() const;
    
    // Statistics
    uint32_t getTotalStandardsCount() const;
    std::map<StandardsOrganization, uint32_t> getStandardsByOrganization() const;
    std::vector<std::string> getRecentlyUpdatedStandards() const;

private:
    std::map<std::string, StandardDefinition> standards_database_;
    std::string database_version_;
    std::chrono::system_clock::time_point last_update_;
    
    void parseStandardDefinition(const std::string& definition_xml, StandardDefinition& standard);
    bool validateStandardDefinition(const StandardDefinition& standard);
    void indexStandards();
};

} // namespace ve::standards
