# Professional Video Format Support API Documentation

> Complete API reference for the professional video format support system - comprehensive guide for developers integrating with the video editor's format capabilities.

## 📋 **Table of Contents**

1. [API Overview](#api-overview)
2. [Getting Started](#getting-started)
3. [Format Detection & Analysis](#format-detection--analysis)
4. [Decoding Operations](#decoding-operations)
5. [Encoding Operations](#encoding-operations)
6. [Quality Validation](#quality-validation)
7. [Standards Compliance](#standards-compliance)
8. [Performance Monitoring](#performance-monitoring)
9. [Error Handling](#error-handling)
10. [Advanced Features](#advanced-features)
11. [Code Examples](#code-examples)
12. [API Reference](#api-reference)

---

## 🎯 **API Overview**

The Professional Video Format Support API provides a unified interface for handling all professional video formats, codecs, and quality standards. Built for production environments, it supports broadcast, streaming, cinema, and archive workflows.

### **Key Features**
- **Universal Format Support**: 50+ video codecs, 30+ audio codecs, 25+ container formats
- **Professional Quality**: SMPTE, EBU, ITU-R standards compliance
- **Hardware Acceleration**: D3D11VA, NVENC, Quick Sync Video support
- **Real-time Monitoring**: Performance metrics and health monitoring
- **Production Ready**: Enterprise-grade error handling and recovery

### **Architecture Overview**
```cpp
namespace ve {
    namespace integration {     // Unified API and system integration
        class UnifiedFormatAPI;
        class FormatWorkflowManager;
        class ComponentIntegrationManager;
    }
    namespace media_io {        // Format detection and I/O
        class FormatDetector;
        class ContainerParser;
    }
    namespace decode {          // Video/audio decoding
        class DecoderInterface;
        class HardwareDecoder;
    }
    namespace render {          // Video/audio encoding
        class EncoderInterface;
        class HardwareEncoder;
    }
    namespace quality {         // Quality validation and metrics
        class FormatValidator;
        class QualityMetricsEngine;
    }
    namespace standards {       // Industry standards compliance
        class ComplianceEngine;
        class BroadcastStandards;
    }
    namespace monitoring {      // System monitoring and health
        class QualitySystemHealthMonitor;
        class PerformanceMetricsCollector;
    }
}
```

---

## 🚀 **Getting Started**

### **System Requirements**
- **Windows 10/11** (x64) with latest updates
- **Visual Studio 2022** or compatible C++ compiler  
- **DirectX 11** compatible graphics card
- **8GB RAM minimum** (16GB+ recommended for 4K+ content)
- **FFmpeg 6.0+** libraries
- **CMake 3.20+** for building

### **Installation & Setup**

#### 1. Initialize the Format Support System
```cpp
#include "integration/unified_format_api.hpp"

using namespace ve::integration;

// Initialize the system with default configuration
bool success = UnifiedFormatAPI::initialize();
if (!success) {
    // Handle initialization failure
    auto errors = UnifiedFormatAPI::getLastErrors();
    for (const auto& error : errors) {
        std::cerr << "Initialization error: " << error << std::endl;
    }
    return false;
}

// Verify system is ready
if (!UnifiedFormatAPI::isInitialized()) {
    std::cerr << "System not properly initialized" << std::endl;
    return false;
}
```

#### 2. Configure System Settings
```cpp
// Enable hardware acceleration
UnifiedFormatAPI::setHardwareAcceleration(true);

// Set quality validation level
UnifiedFormatAPI::setQualityLevel(quality::ValidationLevel::PROFESSIONAL);

// Configure compliance standards
std::vector<std::string> standards = {"SMPTE", "EBU_R128", "ITU_R_BT709"};
UnifiedFormatAPI::setComplianceStandards(standards);

// Set performance mode
UnifiedFormatAPI::setPerformanceMode("balanced"); // "quality", "balanced", "performance"
```

#### 3. Check System Capabilities
```cpp
// Get format support matrix
auto support_matrix = UnifiedFormatAPI::getFormatSupportMatrix();

std::cout << "Video Codecs: " << support_matrix.total_video_codecs << std::endl;
std::cout << "Audio Codecs: " << support_matrix.total_audio_codecs << std::endl;
std::cout << "Container Formats: " << support_matrix.total_container_formats << std::endl;
std::cout << "Hardware Accelerated: " << support_matrix.hardware_accelerated_codecs << std::endl;

// Check specific codec support
auto h264_support = UnifiedFormatAPI::getVideoCodecSupport("H.264");
if (!h264_support.codec_name.empty()) {
    std::cout << "H.264 Support:" << std::endl;
    std::cout << "  Hardware Decode: " << h264_support.hardware_decode_available << std::endl;
    std::cout << "  Hardware Encode: " << h264_support.hardware_encode_available << std::endl;
    std::cout << "  HDR Support: " << h264_support.hdr_support << std::endl;
}
```

---

## 🔍 **Format Detection & Analysis**

### **Auto-Detection**
The system automatically detects formats using advanced heuristics and metadata analysis.

```cpp
#include "media_io/format_detector.hpp"

// Detect format from file
auto format_info = UnifiedFormatAPI::detectFormat("input_video.mp4");

std::cout << "Container: " << format_info.container_format << std::endl;
std::cout << "Video Codec: " << format_info.video_codec << std::endl;
std::cout << "Audio Codec: " << format_info.audio_codec << std::endl;
std::cout << "Resolution: " << format_info.width << "x" << format_info.height << std::endl;
std::cout << "Duration: " << format_info.duration_seconds << " seconds" << std::endl;

// Check if format is supported
bool supported = UnifiedFormatAPI::isFormatSupported(format_info);
if (!supported) {
    auto recommendations = UnifiedFormatAPI::getDecoderRecommendations(format_info);
    std::cout << "Format not supported. Recommendations:" << std::endl;
    for (const auto& rec : recommendations) {
        std::cout << "  - " << rec << std::endl;
    }
}
```

### **Stream Analysis**
```cpp
// Analyze from memory buffer
std::vector<uint8_t> file_data = readFileToBuffer("video.mkv");
auto stream_format = UnifiedFormatAPI::analyzeStream(file_data);

// Get detailed format information
if (stream_format.has_video) {
    std::cout << "Video Stream:" << std::endl;
    std::cout << "  Codec: " << stream_format.video_codec << std::endl;
    std::cout << "  Profile: " << stream_format.video_profile << std::endl;
    std::cout << "  Level: " << stream_format.video_level << std::endl;
    std::cout << "  Bit Depth: " << stream_format.video_bit_depth << std::endl;
    std::cout << "  Color Space: " << stream_format.color_space << std::endl;
    std::cout << "  HDR: " << (stream_format.is_hdr ? "Yes" : "No") << std::endl;
}

if (stream_format.has_audio) {
    std::cout << "Audio Stream:" << std::endl;
    std::cout << "  Codec: " << stream_format.audio_codec << std::endl;
    std::cout << "  Sample Rate: " << stream_format.audio_sample_rate << " Hz" << std::endl;
    std::cout << "  Channels: " << stream_format.audio_channels << std::endl;
    std::cout << "  Bit Depth: " << stream_format.audio_bit_depth << " bits" << std::endl;
}
```

### **Professional Metadata Extraction**
```cpp
// Extract comprehensive metadata
auto metadata = format_info.metadata;

// Timecode information
if (metadata.has_timecode) {
    std::cout << "Start Timecode: " << metadata.start_timecode << std::endl;
    std::cout << "Timecode Rate: " << metadata.timecode_frame_rate << std::endl;
}

// Color information
if (metadata.has_color_metadata) {
    std::cout << "Color Primaries: " << metadata.color_primaries << std::endl;
    std::cout << "Transfer Function: " << metadata.transfer_function << std::endl;
    std::cout << "Matrix Coefficients: " << metadata.matrix_coefficients << std::endl;
}

// HDR metadata
if (metadata.has_hdr_metadata) {
    std::cout << "Master Display:" << std::endl;
    std::cout << "  White Point: (" << metadata.hdr_white_point_x << ", " 
              << metadata.hdr_white_point_y << ")" << std::endl;
    std::cout << "  Max Luminance: " << metadata.hdr_max_luminance << " nits" << std::endl;
    std::cout << "  Min Luminance: " << metadata.hdr_min_luminance << " nits" << std::endl;
}
```

---

## 🎬 **Decoding Operations**

### **Basic Decoding**
```cpp
// Create optimal decoder for the format
auto decoder = UnifiedFormatAPI::createOptimalDecoder("input_video.mov");
if (!decoder) {
    std::cerr << "Failed to create decoder" << std::endl;
    return false;
}

// Initialize decoder
decode::DecoderConfig config;
config.enable_hardware_acceleration = true;
config.output_pixel_format = "YUV420P";
config.max_frame_cache = 10;

bool init_success = decoder->initialize(config);
if (!init_success) {
    std::cerr << "Failed to initialize decoder" << std::endl;
    return false;
}

// Decode frames
while (decoder->hasMoreFrames()) {
    auto frame = decoder->decodeNextFrame();
    if (frame) {
        // Process the decoded frame
        processFrame(*frame);
    }
}
```

### **Advanced Decoding with Quality Control**
```cpp
// Create decoder with quality validation
auto decoder = UnifiedFormatAPI::createDecoder(format_info);

// Configure advanced decoder settings
decode::AdvancedDecoderConfig advanced_config;
advanced_config.enable_error_concealment = true;
advanced_config.enable_quality_monitoring = true;
advanced_config.quality_threshold = 0.95;
advanced_config.enable_frame_validation = true;
advanced_config.validation_level = quality::ValidationLevel::PROFESSIONAL;

decoder->setAdvancedConfiguration(advanced_config);

// Decode with quality monitoring
while (decoder->hasMoreFrames()) {
    auto decode_result = decoder->decodeNextFrameWithQuality();
    
    if (decode_result.success) {
        auto frame = decode_result.frame;
        auto quality_info = decode_result.quality_metrics;
        
        // Check frame quality
        if (quality_info.overall_score < 0.9) {
            std::cout << "Warning: Frame quality below threshold: " 
                      << quality_info.overall_score << std::endl;
            
            // Get quality details
            std::cout << "  PSNR: " << quality_info.psnr_db << " dB" << std::endl;
            std::cout << "  SSIM: " << quality_info.ssim << std::endl;
            std::cout << "  Artifacts detected: " << quality_info.artifacts_detected << std::endl;
        }
        
        processFrame(*frame);
    } else {
        std::cerr << "Decode error: " << decode_result.error_message << std::endl;
        
        // Attempt error recovery
        if (decoder->canRecover()) {
            decoder->recoverFromError();
        }
    }
}
```

### **Hardware-Accelerated Decoding**
```cpp
// Check hardware acceleration availability
bool hw_available = decoder->isHardwareAccelerationAvailable();
if (hw_available) {
    auto hw_info = decoder->getHardwareInfo();
    std::cout << "Hardware Decoder: " << hw_info.device_name << std::endl;
    std::cout << "Driver Version: " << hw_info.driver_version << std::endl;
    std::cout << "Memory: " << hw_info.memory_mb << " MB" << std::endl;
}

// Configure hardware acceleration
decode::HardwareConfig hw_config;
hw_config.prefer_hardware = true;
hw_config.fallback_to_software = true;
hw_config.gpu_memory_limit_mb = 2048;

decoder->setHardwareConfiguration(hw_config);

// Monitor hardware performance
auto hw_stats = decoder->getHardwareStatistics();
std::cout << "GPU Usage: " << hw_stats.gpu_usage_percent << "%" << std::endl;
std::cout << "GPU Memory: " << hw_stats.gpu_memory_used_mb << " MB" << std::endl;
std::cout << "Decode Speed: " << hw_stats.frames_per_second << " FPS" << std::endl;
```

---

## 🎥 **Encoding Operations**

### **Basic Encoding**
```cpp
// Create encoder for target format
auto encoder = UnifiedFormatAPI::createEncoder("H.264");
if (!encoder) {
    std::cerr << "Failed to create H.264 encoder" << std::endl;
    return false;
}

// Configure encoder
render::EncoderConfig config;
config.width = 1920;
config.height = 1080;
config.frame_rate = 30.0;
config.bitrate_kbps = 5000;
config.profile = "High";
config.level = "4.1";
config.pixel_format = "YUV420P";

// Advanced quality settings
config.rate_control_mode = "VBR";
config.quality_preset = "medium";
config.enable_b_frames = true;
config.gop_size = 60;

bool init_success = encoder->initialize(config);
if (!init_success) {
    std::cerr << "Failed to initialize encoder" << std::endl;
    return false;
}

// Encode frames
for (const auto& frame : input_frames) {
    auto encoded_data = encoder->encodeFrame(frame);
    if (encoded_data.success) {
        writeToFile(encoded_data.data, encoded_data.size);
    } else {
        std::cerr << "Encoding error: " << encoded_data.error_message << std::endl;
    }
}

// Finalize encoding
encoder->finalize();
```

### **Professional Encoding with Quality Control**
```cpp
// Create encoder with quality validation
media_io::FormatInfo target_format;
target_format.video_codec = "HEVC";
target_format.width = 3840;
target_format.height = 2160;
target_format.frame_rate = 60.0;
target_format.color_space = "BT2020";
target_format.hdr_metadata.enabled = true;

auto encoder = UnifiedFormatAPI::createOptimalEncoder(target_format);

// Configure professional encoding settings
render::ProfessionalEncoderConfig pro_config;
pro_config.enable_quality_monitoring = true;
pro_config.target_quality_score = 0.98;
pro_config.enable_compliance_checking = true;
pro_config.compliance_standards = {"SMPTE_ST2084", "ITU_R_BT2020"};
pro_config.enable_real_time_analysis = true;

encoder->setProfessionalConfiguration(pro_config);

// Encode with quality validation
for (const auto& frame : input_frames) {
    auto encode_result = encoder->encodeFrameWithQuality(frame);
    
    if (encode_result.success) {
        // Check encoding quality
        auto quality_metrics = encode_result.quality_metrics;
        if (quality_metrics.quality_score < pro_config.target_quality_score) {
            std::cout << "Quality warning - Score: " << quality_metrics.quality_score << std::endl;
            
            // Adjust encoder settings dynamically
            auto current_settings = encoder->getCurrentSettings();
            current_settings.bitrate_kbps *= 1.2;  // Increase bitrate
            encoder->updateSettings(current_settings);
        }
        
        // Check compliance
        if (encode_result.compliance_report.has_violations) {
            std::cout << "Compliance violations detected:" << std::endl;
            for (const auto& violation : encode_result.compliance_report.violations) {
                std::cout << "  - " << violation << std::endl;
            }
        }
        
        writeToFile(encode_result.data, encode_result.size);
    }
}
```

### **Hardware-Accelerated Encoding**
```cpp
// Check hardware encoder availability
auto hw_encoders = UnifiedFormatAPI::getAvailableHardwareEncoders();
for (const auto& hw_encoder : hw_encoders) {
    std::cout << "Available: " << hw_encoder.name << " (" << hw_encoder.type << ")" << std::endl;
}

// Create hardware encoder
auto encoder = UnifiedFormatAPI::createEncoder("H.264");

// Configure hardware acceleration
render::HardwareEncoderConfig hw_config;
hw_config.prefer_hardware = true;
hw_config.encoder_type = "NVENC";  // "NVENC", "QuickSync", "AMF"
hw_config.quality_preset = "P6";   // NVENC quality presets
hw_config.rate_control = "VBR";
hw_config.enable_b_frames = true;
hw_config.gpu_memory_limit_mb = 1024;

encoder->setHardwareConfiguration(hw_config);

// Monitor encoding performance
auto performance_monitor = [&encoder]() {
    while (encoder->isEncoding()) {
        auto stats = encoder->getEncodingStatistics();
        std::cout << "Encoding Speed: " << stats.frames_per_second << " FPS" << std::endl;
        std::cout << "Queue Length: " << stats.input_queue_length << std::endl;
        std::cout << "GPU Usage: " << stats.gpu_usage_percent << "%" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
};

std::thread monitor_thread(performance_monitor);
```

---

## ✅ **Quality Validation**

### **Format Validation**
```cpp
// Validate file format compliance
auto validation_report = UnifiedFormatAPI::validateFormat(
    "broadcast_content.mxf", 
    quality::ValidationLevel::PROFESSIONAL
);

std::cout << "Validation Result: " << (validation_report.is_valid ? "PASS" : "FAIL") << std::endl;
std::cout << "Validation Level: " << validation_report.validation_level_name << std::endl;
std::cout << "Overall Score: " << validation_report.overall_score << std::endl;

// Check validation details
if (!validation_report.is_valid) {
    std::cout << "Validation Issues:" << std::endl;
    for (const auto& issue : validation_report.issues) {
        std::cout << "  [" << issue.severity << "] " << issue.description << std::endl;
        if (!issue.recommendation.empty()) {
            std::cout << "    Recommendation: " << issue.recommendation << std::endl;
        }
    }
}

// Professional workflow validation
bool workflow_compliant = UnifiedFormatAPI::validateProfessionalWorkflow(
    "content.mov", 
    "broadcast_delivery"
);

if (!workflow_compliant) {
    auto recommendations = UnifiedFormatAPI::getProfessionalFormatRecommendations(
        "broadcast_delivery"
    );
    std::cout << "Workflow Recommendations:" << std::endl;
    for (const auto& rec : recommendations) {
        std::cout << "  - " << rec << std::endl;
    }
}
```

### **Quality Analysis**
```cpp
// Analyze video quality
auto quality_report = UnifiedFormatAPI::analyzeQuality(
    "encoded_video.mp4",
    "reference_video.mov"  // Optional reference for comparison
);

std::cout << "Quality Analysis Results:" << std::endl;
std::cout << "  Overall Score: " << quality_report.overall_quality_score << std::endl;

// Objective metrics
auto& objective = quality_report.objective_metrics;
std::cout << "  PSNR: " << objective.psnr_db << " dB" << std::endl;
std::cout << "  SSIM: " << objective.ssim << std::endl;
std::cout << "  VMAF: " << objective.vmaf << std::endl;

// Perceptual metrics
auto& perceptual = quality_report.perceptual_metrics;
std::cout << "  BUTTERAUGLI: " << perceptual.butteraugli_score << std::endl;
std::cout << "  MS-SSIM: " << perceptual.ms_ssim << std::endl;

// Temporal analysis
auto& temporal = quality_report.temporal_metrics;
std::cout << "  Motion Smoothness: " << temporal.motion_smoothness << std::endl;
std::cout << "  Flicker Detection: " << temporal.flicker_score << std::endl;
std::cout << "  Scene Changes: " << temporal.scene_change_count << std::endl;

// Quality issues
if (!quality_report.quality_issues.empty()) {
    std::cout << "Quality Issues Detected:" << std::endl;
    for (const auto& issue : quality_report.quality_issues) {
        std::cout << "  - " << issue.description << " (Frame: " << issue.frame_number << ")" << std::endl;
    }
}
```

### **Real-time Quality Monitoring**
```cpp
// Start real-time quality monitoring
quality::RealTimeQualityMonitor monitor;

quality::MonitoringConfig monitor_config;
monitor_config.enable_objective_metrics = true;
monitor_config.enable_perceptual_metrics = true;
monitor_config.quality_threshold = 0.90;
monitor_config.alert_on_quality_drop = true;
monitor_config.monitoring_interval_ms = 100;

monitor.initialize(monitor_config);

// Set quality alert handler
monitor.setQualityAlertHandler([](const quality::QualityAlert& alert) {
    std::cout << "Quality Alert: " << alert.message << std::endl;
    std::cout << "  Severity: " << alert.severity << std::endl;
    std::cout << "  Current Score: " << alert.current_quality_score << std::endl;
    std::cout << "  Threshold: " << alert.threshold << std::endl;
});

// Monitor during processing
monitor.startMonitoring();

// Process video with monitoring
for (const auto& frame : video_frames) {
    // Submit frame for quality analysis
    monitor.submitFrame(frame);
    
    // Get current quality status
    auto status = monitor.getCurrentQualityStatus();
    if (status.quality_declining) {
        std::cout << "Warning: Quality declining trend detected" << std::endl;
        
        // Take corrective action
        adjustProcessingParameters();
    }
    
    processFrame(frame);
}

monitor.stopMonitoring();
```

---

## 📏 **Standards Compliance**

### **Broadcast Standards Compliance**
```cpp
// Check broadcast compliance
auto compliance_report = UnifiedFormatAPI::checkCompliance(
    "broadcast_master.mxf",
    "AS11_DPP_HD"
);

std::cout << "Compliance Check: " << compliance_report.standard_name << std::endl;
std::cout << "Compliant: " << (compliance_report.is_compliant ? "YES" : "NO") << std::endl;
std::cout << "Compliance Score: " << compliance_report.compliance_score << std::endl;

// Check specific requirements
for (const auto& requirement : compliance_report.requirements) {
    std::cout << "  " << requirement.requirement_name << ": ";
    std::cout << (requirement.met ? "PASS" : "FAIL") << std::endl;
    
    if (!requirement.met) {
        std::cout << "    Issue: " << requirement.issue_description << std::endl;
        std::cout << "    Action: " << requirement.corrective_action << std::endl;
    }
}

// Audio loudness compliance (EBU R128)
auto loudness_compliance = standards::AudioLoudnessStandards::testLoudnessCompliance(
    loudness_measurement,
    standards::AudioLoudnessStandards::LoudnessStandard::EBU_R128
);

if (!loudness_compliance) {
    auto issues = standards::AudioLoudnessStandards::getLoudnessIssues(
        loudness_measurement,
        standards::AudioLoudnessStandards::LoudnessStandard::EBU_R128
    );
    
    std::cout << "Audio Loudness Issues:" << std::endl;
    for (const auto& issue : issues) {
        std::cout << "  - " << issue << std::endl;
    }
    
    // Get correction recommendations
    auto corrections = standards::AudioLoudnessStandards::getLoudnessCorrectionRecommendations(
        loudness_measurement,
        standards::AudioLoudnessStandards::LoudnessStandard::EBU_R128
    );
    
    std::cout << "Correction Recommendations:" << std::endl;
    for (const auto& correction : corrections) {
        std::cout << "  - " << correction << std::endl;
    }
}
```

### **Multiple Standards Validation**
```cpp
// Check multiple standards simultaneously
std::vector<std::string> standards_to_check = {
    "SMPTE_ST377",      // MXF format standard
    "EBU_R128",         // Audio loudness standard
    "ITU_R_BT709",      // HD video standard
    "AS11_DPP_HD"       // UK DPP delivery standard
};

standards::ComplianceEngine compliance_engine;
auto multi_compliance_report = compliance_engine.checkMultipleStandards(
    "professional_content.mxf",
    standards_to_check
);

std::cout << "Multi-Standards Compliance Report:" << std::endl;
std::cout << "Overall Compliance: " << (multi_compliance_report.overall_compliant ? "PASS" : "FAIL") << std::endl;

for (const auto& standard_result : multi_compliance_report.standard_results) {
    std::cout << "  " << standard_result.standard_name << ": ";
    std::cout << (standard_result.compliant ? "PASS" : "FAIL");
    std::cout << " (Score: " << standard_result.compliance_score << ")" << std::endl;
    
    if (!standard_result.compliant) {
        for (const auto& violation : standard_result.violations) {
            std::cout << "    Violation: " << violation.description << std::endl;
            std::cout << "    Severity: " << violation.severity << std::endl;
        }
    }
}

// Get remediation plan
if (!multi_compliance_report.overall_compliant) {
    auto remediation_plan = compliance_engine.generateRemediationPlan(multi_compliance_report);
    
    std::cout << "Remediation Plan:" << std::endl;
    for (const auto& step : remediation_plan.remediation_steps) {
        std::cout << "  " << step.step_number << ". " << step.description << std::endl;
        std::cout << "     Estimated Time: " << step.estimated_time_minutes << " minutes" << std::endl;
        std::cout << "     Priority: " << step.priority << std::endl;
    }
}
```

---

## 📊 **Performance Monitoring**

### **System Health Monitoring**
```cpp
// Get current system health
auto health_report = UnifiedFormatAPI::getSystemHealth();

std::cout << "System Health Report:" << std::endl;
std::cout << "Overall Status: " << health_report.overall_status << std::endl;
std::cout << "CPU Usage: " << health_report.overall_cpu_usage << "%" << std::endl;
std::cout << "Memory Usage: " << health_report.overall_memory_usage_mb << " MB" << std::endl;
std::cout << "Active Operations: " << health_report.queued_operations << std::endl;

// Check component health
for (const auto& [component, health] : health_report.component_health) {
    std::cout << "Component: " << health.component_name << std::endl;
    std::cout << "  Status: " << health.status_message << std::endl;
    std::cout << "  Success Rate: " << health.success_rate << "%" << std::endl;
    std::cout << "  Response Time: " << health.average_response_time.count() << " ms" << std::endl;
    
    if (!health.recent_errors.empty()) {
        std::cout << "  Recent Errors:" << std::endl;
        for (const auto& error : health.recent_errors) {
            std::cout << "    - " << error << std::endl;
        }
    }
}

// Get active alerts
auto alerts = UnifiedFormatAPI::getSystemAlerts();
if (!alerts.empty()) {
    std::cout << "Active Alerts:" << std::endl;
    for (const auto& alert : alerts) {
        std::cout << "  - " << alert << std::endl;
    }
}
```

### **Performance Metrics**
```cpp
// Get performance metrics
auto performance_report = UnifiedFormatAPI::getPerformanceMetrics();

std::cout << "Performance Report:" << std::endl;
std::cout << "Overall Latency: " << performance_report.overall_latency_ms << " ms" << std::endl;
std::cout << "Throughput: " << performance_report.overall_throughput_ops_sec << " ops/sec" << std::endl;
std::cout << "Error Rate: " << performance_report.overall_error_rate << "%" << std::endl;

// Performance trends
if (!performance_report.degrading_metrics.empty()) {
    std::cout << "Degrading Performance Metrics:" << std::endl;
    for (const auto& metric : performance_report.degrading_metrics) {
        std::cout << "  - " << metric << std::endl;
    }
}

if (!performance_report.optimization_recommendations.empty()) {
    std::cout << "Optimization Recommendations:" << std::endl;
    for (const auto& recommendation : performance_report.optimization_recommendations) {
        std::cout << "  - " << recommendation << std::endl;
    }
}

// Real-time performance monitoring
monitoring::PerformanceMetricsCollector::recordLatency("decode_frame", std::chrono::milliseconds(25));
monitoring::PerformanceMetricsCollector::recordThroughput("encode_operations", 30.5);
monitoring::PerformanceMetricsCollector::recordResourceUsage("cpu_usage", 75.2);
```

---

## ⚠️ **Error Handling**

### **Error Management**
```cpp
// Get last errors
auto errors = UnifiedFormatAPI::getLastErrors();
for (const auto& error : errors) {
    std::cout << "Error: " << error << std::endl;
}

// Clear error log
UnifiedFormatAPI::clearErrors();

// Set up error callback
UnifiedFormatAPI::setErrorCallback([](const std::string& error_message, 
                                     const std::string& component_name,
                                     int severity_level) {
    std::cout << "[" << component_name << "] ";
    
    switch (severity_level) {
        case 1: std::cout << "CRITICAL: "; break;
        case 2: std::cout << "ERROR: "; break;
        case 3: std::cout << "WARNING: "; break;
        default: std::cout << "INFO: "; break;
    }
    
    std::cout << error_message << std::endl;
    
    // Log to file or external system
    logErrorToFile(error_message, component_name, severity_level);
});
```

### **Recovery and Diagnostics**
```cpp
// Run system diagnostics
bool diagnostic_success = UnifiedFormatAPI::runSystemDiagnostics();
if (!diagnostic_success) {
    auto diagnostic_info = UnifiedFormatAPI::getDiagnosticInfo();
    std::cout << "System Diagnostic Information:" << std::endl;
    std::cout << diagnostic_info << std::endl;
}

// Automatic recovery
if (!UnifiedFormatAPI::isInitialized()) {
    std::cout << "Attempting system recovery..." << std::endl;
    
    // Try to reinitialize
    bool recovery_success = UnifiedFormatAPI::initialize();
    if (recovery_success) {
        std::cout << "System recovery successful" << std::endl;
    } else {
        std::cout << "System recovery failed" << std::endl;
        // Implement fallback or emergency procedures
    }
}
```

---

## 🔧 **Advanced Features**

### **Format Workflow Management**
```cpp
// Create broadcast delivery workflow
auto workflow = FormatWorkflowManager::createBroadcastDeliveryWorkflow("AS11_DPP_HD");

// Execute workflow
auto workflow_result = FormatWorkflowManager::executeWorkflow(
    workflow,
    "source_content.mov",
    "broadcast_delivery.mxf"
);

std::cout << "Workflow Result: " << (workflow_result.success ? "SUCCESS" : "FAILED") << std::endl;
std::cout << "Duration: " << workflow_result.total_duration.count() << " ms" << std::endl;

if (!workflow_result.success) {
    std::cout << "Failed Steps:" << std::endl;
    for (const auto& failed_step : workflow_result.failed_steps) {
        std::cout << "  - " << failed_step << std::endl;
    }
    
    std::cout << "Recommendations:" << std::endl;
    for (const auto& recommendation : workflow_result.recommendations) {
        std::cout << "  - " << recommendation << std::endl;
    }
}
```

### **Format Migration**
```cpp
// Analyze migration options
media_io::FormatInfo current_format = UnifiedFormatAPI::detectFormat("legacy_content.avi");
auto migration_options = FormatMigrationAssistant::analyzeMigrationOptions(
    current_format,
    "streaming_delivery"
);

std::cout << "Migration Options:" << std::endl;
for (const auto& option : migration_options) {
    std::cout << "  " << option.plan_name << ":" << std::endl;
    std::cout << "    Quality Retention: " << option.expected_quality_retention * 100 << "%" << std::endl;
    std::cout << "    Estimated Time: " << option.estimated_time_per_file.count() << " minutes" << std::endl;
    std::cout << "    Complexity: " << option.complexity_level << std::endl;
}

// Execute migration
if (!migration_options.empty()) {
    auto migration_result = FormatMigrationAssistant::executeMigration(
        migration_options[0],  // Use first option
        "legacy_content.avi",
        "modern_content.mp4"
    );
    
    std::cout << "Migration Result: " << (migration_result.success ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Quality Retention: " << migration_result.quality_retention_score * 100 << "%" << std::endl;
    std::cout << "Size Change: " << migration_result.size_change_percent << "%" << std::endl;
}
```

---

## 📚 **API Reference**

### **Core Classes**

#### `UnifiedFormatAPI`
Main entry point for all format operations.

**Static Methods:**
- `bool initialize(const std::map<std::string, std::string>& config = {})`
- `void shutdown()`
- `bool isInitialized()`
- `FormatInfo detectFormat(const std::string& file_path)`
- `std::unique_ptr<DecoderInterface> createDecoder(const FormatInfo& format_info)`
- `std::unique_ptr<EncoderInterface> createEncoder(const std::string& codec_name)`
- `FormatValidationReport validateFormat(const std::string& file_path, ValidationLevel level)`
- `QualityAnalysisReport analyzeQuality(const std::string& file_path, const std::string& reference_path)`
- `ComplianceReport checkCompliance(const std::string& file_path, const std::string& standard_name)`

#### `FormatWorkflowManager`
Manages complex format workflows and professional pipelines.

**Static Methods:**
- `WorkflowResult executeWorkflow(const WorkflowConfiguration& workflow, const std::string& input_file, const std::string& output_file)`
- `std::vector<WorkflowConfiguration> getAvailableWorkflows()`
- `WorkflowConfiguration createBroadcastDeliveryWorkflow(const std::string& broadcast_standard)`
- `WorkflowConfiguration createStreamingOptimizationWorkflow(const std::string& platform)`

#### `FormatMigrationAssistant`
Helps migrate between different format standards and versions.

**Static Methods:**
- `std::vector<MigrationPlan> analyzeMigrationOptions(const FormatInfo& current_format, const std::string& target_use_case)`
- `MigrationResult executeMigration(const MigrationPlan& plan, const std::string& source_file, const std::string& target_file)`
- `bool validateMigrationPlan(const MigrationPlan& plan)`

### **Data Structures**

#### `FormatInfo`
```cpp
struct FormatInfo {
    std::string container_format;
    std::string video_codec;
    std::string audio_codec;
    uint32_t width = 0;
    uint32_t height = 0;
    double frame_rate = 0.0;
    uint32_t video_bit_depth = 8;
    std::string color_space;
    bool is_hdr = false;
    uint32_t audio_sample_rate = 0;
    uint32_t audio_channels = 0;
    uint32_t audio_bit_depth = 16;
    double duration_seconds = 0.0;
    FormatMetadata metadata;
};
```

#### `ValidationReport`
```cpp
struct FormatValidationReport {
    bool is_valid = false;
    ValidationLevel validation_level;
    std::string validation_level_name;
    double overall_score = 0.0;
    std::vector<ValidationIssue> issues;
    std::vector<std::string> recommendations;
    std::chrono::milliseconds validation_time{0};
};
```

#### `QualityAnalysisReport`
```cpp
struct QualityAnalysisReport {
    double overall_quality_score = 0.0;
    ObjectiveMetrics objective_metrics;
    PerceptualMetrics perceptual_metrics;
    TemporalMetrics temporal_metrics;
    std::vector<QualityIssue> quality_issues;
    std::vector<std::string> recommendations;
};
```

---

## 🔗 **Related Documentation**

- **[Professional Formats User Guide](PROFESSIONAL_FORMATS_GUIDE.md)** - User-friendly guide for format capabilities
- **[Developer Extension Guide](EXTENDING_FORMAT_SUPPORT.md)** - How to extend format support
- **[Production Deployment Guide](PRODUCTION_DEPLOYMENT.md)** - Production deployment instructions
- **[Troubleshooting Guide](TROUBLESHOOTING.md)** - Common issues and solutions
- **[Performance Optimization Guide](PERFORMANCE_OPTIMIZATION.md)** - Performance tuning recommendations

---

*This API documentation covers the complete Professional Video Format Support system. For specific implementation details, examples, or troubleshooting, refer to the related documentation or contact the development team.*
