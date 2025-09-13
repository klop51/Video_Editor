#pragma once

#include "unified_format_api.hpp"
#include "../../../decode/include/decode/decoder_interface.hpp"
#include "../../../render/include/render/encoder_interface.hpp"
#include "../../../quality/include/quality/format_validator.hpp"
#include "../../../standards/include/standards/compliance_engine.hpp"
#include "../../../monitoring/include/monitoring/quality_system_monitoring.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <atomic>
#include <mutex>

namespace ve::integration {

/**
 * System Integration Framework
 * Coordinates all video format support components for seamless operation
 */

/**
 * Component Integration Manager
 * Manages integration between all major video editor components
 */
class ComponentIntegrationManager {
public:
    enum class ComponentType {
        MEDIA_IO,               // Format detection and I/O
        DECODER_SYSTEM,         // Video/audio decoding
        ENCODER_SYSTEM,         // Video/audio encoding  
        QUALITY_VALIDATION,     // Format and quality validation
        STANDARDS_COMPLIANCE,   // Industry standards compliance
        MONITORING_SYSTEM,      // System health monitoring
        CACHE_SYSTEM,          // Frame and metadata caching
        THREADING_SYSTEM,      // Multi-threading coordination
        MEMORY_MANAGEMENT,     // Memory allocation and cleanup
        ERROR_HANDLING,        // Error tracking and recovery
        USER_INTERFACE,        // UI integration points
        PLUGIN_SYSTEM         // Plugin and extension support
    };
    
    struct ComponentStatus {
        ComponentType type;
        std::string component_name;
        std::string version;
        bool initialized = false;
        bool operational = false;
        
        // Integration status
        std::vector<ComponentType> depends_on;
        std::vector<ComponentType> integrates_with;
        std::map<ComponentType, bool> integration_status;
        
        // Performance metrics
        std::chrono::milliseconds last_response_time{0};
        uint64_t operations_completed = 0;
        uint64_t operations_failed = 0;
        double success_rate = 0.0;
        
        // Resource usage
        size_t memory_usage_mb = 0;
        double cpu_usage_percent = 0.0;
        uint32_t active_threads = 0;
        
        // Issues and alerts
        std::vector<std::string> active_issues;
        std::vector<std::string> performance_warnings;
        std::string last_error;
        std::chrono::steady_clock::time_point last_error_time;
        
        std::string status_summary;
    };
    
    struct SystemIntegrationReport {
        std::chrono::steady_clock::time_point report_time;
        bool overall_integration_healthy = false;
        
        std::map<ComponentType, ComponentStatus> component_status;
        
        // Integration health
        uint32_t total_components = 0;
        uint32_t operational_components = 0;
        uint32_t failed_integrations = 0;
        double overall_integration_score = 0.0;
        
        // Performance summary
        std::chrono::milliseconds average_response_time{0};
        double overall_success_rate = 0.0;
        size_t total_memory_usage_mb = 0;
        double total_cpu_usage = 0.0;
        
        // Critical issues
        std::vector<std::string> critical_integration_issues;
        std::vector<std::string> performance_bottlenecks;
        std::vector<std::string> failed_component_communications;
        
        // Recommendations
        std::vector<std::string> integration_improvements;
        std::vector<std::string> performance_optimizations;
        std::vector<std::string> reliability_enhancements;
        
        std::string executive_summary;
    };
    
    // System integration management
    static bool initializeSystemIntegration();
    static void shutdownSystemIntegration();
    static bool validateSystemIntegration();
    static SystemIntegrationReport getIntegrationStatus();
    
    // Component management
    static bool registerComponent(ComponentType type, const std::string& component_name);
    static bool initializeComponent(ComponentType type);
    static void shutdownComponent(ComponentType type);
    static ComponentStatus getComponentStatus(ComponentType type);
    
    // Integration validation
    static bool validateComponentIntegration(ComponentType component1, ComponentType component2);
    static std::vector<std::string> getIntegrationIssues();
    static bool repairIntegration(ComponentType component);
    
    // Performance monitoring
    static void recordComponentOperation(ComponentType component, bool success,
                                       std::chrono::milliseconds response_time);
    static std::map<ComponentType, double> getComponentPerformanceMetrics();
    static std::vector<ComponentType> identifyPerformanceBottlenecks();
    
    // Error handling and recovery
    static void reportComponentError(ComponentType component, const std::string& error_message);
    static bool attemptComponentRecovery(ComponentType component);
    static std::vector<std::string> getRecoveryRecommendations(ComponentType component);
    
    // Configuration management
    static void setComponentConfiguration(ComponentType component,
                                        const std::map<std::string, std::string>& config);
    static std::map<std::string, std::string> getComponentConfiguration(ComponentType component);
    static bool validateComponentConfiguration(ComponentType component);

private:
    static std::mutex integration_mutex_;
    static std::map<ComponentType, ComponentStatus> component_registry_;
    static std::map<std::pair<ComponentType, ComponentType>, bool> integration_matrix_;
    static bool system_initialized_;
    
    static void buildIntegrationMatrix();
    static void validateComponentDependencies();
    static void monitorComponentHealth();
    static double calculateIntegrationScore();
};

/**
 * Cross-Component Communication Hub
 * Facilitates communication between different video editor components
 */
class ComponentCommunicationHub {
public:
    enum class MessageType {
        FORMAT_DETECTED,        // New format detected
        DECODE_STARTED,         // Decoding operation started
        DECODE_COMPLETED,       // Decoding operation completed
        ENCODE_STARTED,         // Encoding operation started
        ENCODE_COMPLETED,       // Encoding operation completed
        QUALITY_VALIDATED,      // Quality validation completed
        COMPLIANCE_CHECKED,     // Standards compliance checked
        ERROR_OCCURRED,         // Error occurred in component
        PERFORMANCE_ALERT,      // Performance issue detected
        MEMORY_WARNING,         // Memory usage warning
        CACHE_UPDATED,         // Cache state updated
        USER_ACTION,           // User interface action
        CUSTOM_MESSAGE         // Custom application message
    };
    
    struct ComponentMessage {
        MessageType type;
        ComponentIntegrationManager::ComponentType sender;
        ComponentIntegrationManager::ComponentType receiver;
        std::chrono::steady_clock::time_point timestamp;
        
        std::string message_id;
        std::map<std::string, std::string> data;
        std::string payload;
        
        bool requires_response = false;
        bool is_broadcast = false;
        uint32_t priority = 5;  // 1=highest, 10=lowest
        std::chrono::milliseconds timeout{5000};
        
        std::string correlation_id;
        std::string session_id;
    };
    
    struct MessageHandler {
        std::function<void(const ComponentMessage&)> handler_function;
        std::vector<MessageType> handled_message_types;
        ComponentIntegrationManager::ComponentType component;
        bool async_processing = true;
        uint32_t max_queue_size = 1000;
    };
    
    // Message handling
    static void registerMessageHandler(ComponentIntegrationManager::ComponentType component,
                                     const MessageHandler& handler);
    static void unregisterMessageHandler(ComponentIntegrationManager::ComponentType component);
    
    static bool sendMessage(const ComponentMessage& message);
    static bool broadcastMessage(const ComponentMessage& message);
    static ComponentMessage sendSynchronousMessage(const ComponentMessage& message);
    
    // Message routing
    static void setMessageRoute(MessageType type,
                              ComponentIntegrationManager::ComponentType from,
                              ComponentIntegrationManager::ComponentType to);
    static std::vector<ComponentIntegrationManager::ComponentType> getMessageRoute(MessageType type);
    
    // Queue management
    static uint32_t getQueueSize(ComponentIntegrationManager::ComponentType component);
    static void clearQueue(ComponentIntegrationManager::ComponentType component);
    static std::vector<ComponentMessage> getQueuedMessages(ComponentIntegrationManager::ComponentType component);
    
    // Monitoring and diagnostics
    static std::map<MessageType, uint32_t> getMessageStatistics();
    static std::vector<std::string> getMessageFlowIssues();
    static double getMessageProcessingEfficiency();
    
    // Message filtering and transformation
    static void setMessageFilter(ComponentIntegrationManager::ComponentType component,
                               std::function<bool(const ComponentMessage&)> filter);
    static void setMessageTransformer(MessageType type,
                                    std::function<ComponentMessage(const ComponentMessage&)> transformer);

private:
    static std::mutex communication_mutex_;
    static std::map<ComponentIntegrationManager::ComponentType, MessageHandler> message_handlers_;
    static std::map<ComponentIntegrationManager::ComponentType, std::queue<ComponentMessage>> message_queues_;
    static std::map<MessageType, std::vector<ComponentIntegrationManager::ComponentType>> message_routes_;
    static std::atomic<uint64_t> next_message_id_;
    static std::map<MessageType, uint32_t> message_statistics_;
    
    static void processMessage(const ComponentMessage& message);
    static void processMessageQueue(ComponentIntegrationManager::ComponentType component);
    static std::string generateMessageId();
};

/**
 * System Resource Coordinator
 * Coordinates resource usage across all video editor components
 */
class SystemResourceCoordinator {
public:
    enum class ResourceType {
        CPU_CORES,              // CPU core allocation
        MEMORY,                 // RAM memory allocation  
        GPU_MEMORY,             // GPU memory allocation
        DISK_IO,               // Disk I/O bandwidth
        NETWORK_IO,            // Network I/O bandwidth
        DECODER_INSTANCES,     // Active decoder instances
        ENCODER_INSTANCES,     // Active encoder instances
        CACHE_SPACE,          // Cache memory allocation
        THREAD_POOL,          // Thread pool allocation
        FILE_HANDLES          // File handle allocation
    };
    
    struct ResourcePool {
        ResourceType type;
        std::string resource_name;
        size_t total_capacity = 0;
        size_t available_capacity = 0;
        size_t allocated_capacity = 0;
        
        // Allocation tracking
        std::map<ComponentIntegrationManager::ComponentType, size_t> component_allocations;
        std::vector<std::string> allocation_requests;
        std::queue<std::string> pending_requests;
        
        // Resource limits
        size_t min_reserve = 0;
        size_t max_allocation_per_component = 0;
        bool allow_oversubscription = false;
        
        // Performance metrics
        double utilization_percent = 0.0;
        double efficiency_score = 0.0;
        uint32_t allocation_failures = 0;
        std::chrono::milliseconds average_allocation_time{0};
        
        std::string resource_status;
        std::vector<std::string> resource_warnings;
    };
    
    struct ResourceAllocationRequest {
        std::string request_id;
        ComponentIntegrationManager::ComponentType requesting_component;
        ResourceType resource_type;
        size_t requested_amount = 0;
        
        uint32_t priority = 5;  // 1=highest, 10=lowest
        std::chrono::milliseconds timeout{30000};
        bool exclusive_access = false;
        
        std::string allocation_reason;
        std::chrono::steady_clock::time_point request_time;
        std::function<void(bool, const std::string&)> completion_callback;
    };
    
    struct ResourceAllocationResult {
        std::string request_id;
        bool allocation_successful = false;
        size_t allocated_amount = 0;
        std::string allocation_id;
        
        std::chrono::milliseconds allocation_time{0};
        std::chrono::steady_clock::time_point allocation_timestamp;
        std::chrono::steady_clock::time_point expiration_time;
        
        std::string failure_reason;
        std::vector<std::string> alternative_suggestions;
    };
    
    // Resource pool management
    static bool initializeResourcePools();
    static void shutdownResourcePools();
    static ResourcePool getResourcePool(ResourceType type);
    static std::vector<ResourcePool> getAllResourcePools();
    
    // Resource allocation
    static ResourceAllocationResult allocateResource(const ResourceAllocationRequest& request);
    static bool releaseResource(const std::string& allocation_id);
    static bool reallocateResource(const std::string& allocation_id, size_t new_amount);
    
    // Batch resource operations
    static std::vector<ResourceAllocationResult> allocateResourceBundle(
        const std::vector<ResourceAllocationRequest>& requests);
    static bool releaseResourceBundle(const std::vector<std::string>& allocation_ids);
    
    // Resource monitoring
    static std::map<ResourceType, double> getResourceUtilization();
    static std::vector<ResourceType> getResourceBottlenecks();
    static std::vector<std::string> getResourceWarnings();
    
    // Resource optimization
    static std::vector<std::string> getOptimizationRecommendations();
    static bool optimizeResourceAllocation();
    static void defragmentResourcePools();
    
    // Resource limits and policies
    static void setResourceLimit(ResourceType type, size_t limit);
    static void setComponentResourceQuota(ComponentIntegrationManager::ComponentType component,
                                        ResourceType resource, size_t quota);
    static void setResourcePolicy(ResourceType type, const std::string& policy_name);
    
    // Emergency resource management
    static bool handleResourceEmergency(ResourceType type);
    static void triggerResourceCleanup();
    static std::vector<std::string> getEmergencyActions(ResourceType type);

private:
    static std::mutex resource_mutex_;
    static std::map<ResourceType, ResourcePool> resource_pools_;
    static std::map<std::string, ResourceAllocationResult> active_allocations_;
    static std::map<ComponentIntegrationManager::ComponentType, std::map<ResourceType, size_t>> component_quotas_;
    static std::queue<ResourceAllocationRequest> pending_requests_;
    
    static void initializeDefaultResourcePools();
    static bool canAllocateResource(ResourceType type, size_t amount);
    static void updateResourceMetrics();
    static void processResourceQueue();
    static std::string generateAllocationId();
};

/**
 * Integration Testing Framework
 * Automated testing of component integrations and system behavior
 */
class IntegrationTestFramework {
public:
    enum class TestType {
        COMPONENT_INTEGRATION,  // Test component-to-component integration
        DATA_FLOW,             // Test data flow through pipeline
        ERROR_HANDLING,        // Test error propagation and recovery
        PERFORMANCE,           // Test system performance under load
        RESOURCE_MANAGEMENT,   // Test resource allocation and cleanup
        CONCURRENT_OPERATIONS, // Test concurrent operation handling
        MEMORY_MANAGEMENT,     // Test memory allocation and cleanup
        QUALITY_PIPELINE,      // Test quality validation pipeline
        STANDARDS_COMPLIANCE,  // Test standards compliance integration
        USER_WORKFLOW         // Test end-to-end user workflows
    };
    
    struct IntegrationTest {
        std::string test_name;
        TestType test_type;
        std::string description;
        
        std::vector<ComponentIntegrationManager::ComponentType> components_under_test;
        std::vector<std::string> test_prerequisites;
        std::vector<std::string> test_steps;
        
        // Test data
        std::vector<std::string> test_files;
        std::map<std::string, std::string> test_parameters;
        std::string expected_outcome;
        
        // Test configuration
        std::chrono::minutes timeout{10};
        bool requires_hardware_acceleration = false;
        size_t min_memory_mb = 512;
        uint32_t max_concurrent_operations = 1;
        
        // Success criteria
        std::vector<std::string> success_conditions;
        std::vector<std::string> failure_conditions;
        std::map<std::string, double> performance_thresholds;
        
        std::string test_category;
        uint32_t test_priority = 5;
    };
    
    struct IntegrationTestResult {
        std::string test_name;
        TestType test_type;
        bool test_passed = false;
        std::chrono::milliseconds execution_time{0};
        
        // Test execution details
        std::vector<std::string> completed_steps;
        std::vector<std::string> failed_steps;
        std::string failure_point;
        std::string failure_reason;
        
        // Component performance
        std::map<ComponentIntegrationManager::ComponentType, bool> component_status;
        std::map<ComponentIntegrationManager::ComponentType, std::chrono::milliseconds> component_response_times;
        
        // Integration validation
        bool data_flow_correct = false;
        bool error_handling_correct = false;
        bool resource_cleanup_successful = false;
        bool memory_leaks_detected = false;
        
        // Performance metrics
        std::map<std::string, double> performance_metrics;
        bool performance_acceptable = false;
        
        // Quality and compliance
        quality::FormatValidationReport validation_results;
        standards::ComplianceReport compliance_results;
        
        std::vector<std::string> test_warnings;
        std::vector<std::string> test_errors;
        std::vector<std::string> improvement_suggestions;
        
        std::string test_summary;
    };
    
    struct IntegrationTestSuite {
        std::string suite_name;
        std::string suite_description;
        std::vector<IntegrationTest> tests;
        
        // Suite configuration
        bool run_tests_in_parallel = false;
        bool stop_on_first_failure = false;
        uint32_t max_concurrent_tests = 4;
        std::chrono::hours suite_timeout{2};
        
        // Test selection
        std::vector<TestType> test_types_to_run;
        std::vector<std::string> test_categories_to_run;
        uint32_t min_priority_level = 1;
        
        std::string suite_version;
        std::string created_by;
    };
    
    // Test execution
    static std::vector<IntegrationTestResult> runIntegrationTestSuite(const IntegrationTestSuite& suite);
    static IntegrationTestResult runIntegrationTest(const IntegrationTest& test);
    static std::vector<IntegrationTestResult> runTestsByType(TestType type);
    
    // Test management
    static IntegrationTestSuite loadTestSuite(const std::string& suite_file_path);
    static void saveTestSuite(const IntegrationTestSuite& suite, const std::string& file_path);
    static std::vector<IntegrationTest> getAvailableTests();
    static std::vector<IntegrationTest> getTestsByComponent(ComponentIntegrationManager::ComponentType component);
    
    // Predefined test suites
    static IntegrationTestSuite createBasicIntegrationTestSuite();
    static IntegrationTestSuite createPerformanceTestSuite();
    static IntegrationTestSuite createQualityValidationTestSuite();
    static IntegrationTestSuite createStandardsComplianceTestSuite();
    static IntegrationTestSuite createStressTestSuite();
    
    // Test reporting
    static std::string generateTestReport(const std::vector<IntegrationTestResult>& results);
    static void exportTestResults(const std::vector<IntegrationTestResult>& results,
                                 const std::string& output_path);
    static std::vector<std::string> identifyIntegrationIssues(const std::vector<IntegrationTestResult>& results);
    
    // Test automation
    static void scheduleAutomaticTesting(std::chrono::hours interval);
    static void runContinuousIntegrationTests();
    static bool validateSystemAfterChanges();

private:
    static std::vector<IntegrationTest> available_tests_;
    static std::map<TestType, std::vector<IntegrationTest>> tests_by_type_;
    
    static void initializePredefinedTests();
    static bool validateTestPrerequisites(const IntegrationTest& test);
    static void setupTestEnvironment(const IntegrationTest& test);
    static void cleanupTestEnvironment(const IntegrationTest& test);
    static bool evaluateTestSuccess(const IntegrationTest& test, const IntegrationTestResult& result);
};

} // namespace ve::integration
