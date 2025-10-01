/**
 * @file sync_validator_test.cpp
 * @brief Test the A/V synchronization validation framework
 */

#include "audio/sync_validator.h"
#include "core/time.hpp"
#include "core/log.hpp"
#include <thread>
#include <chrono>
#include <iostream>

using namespace ve;
using namespace ve::audio;

int main() {
    ve::log::info("Starting A/V Sync Validator Test");
    
    try {
        // Create validator with test configuration
        SyncValidatorConfig config;
        config.sync_tolerance_ms = 20.0;
        config.measurement_interval_ms = 100;
        config.enable_lip_sync_detection = true;
        config.lip_sync_threshold_ms = 40.0;
        
        auto validator = SyncValidator::create(config);
        
        // Set up event callback
        validator->set_event_callback([](const SyncEvent& event) {
            std::string event_type;
            switch (event.type) {
                case SyncEventType::InSync: event_type = "IN_SYNC"; break;
                case SyncEventType::OutOfSync: event_type = "OUT_OF_SYNC"; break;
                case SyncEventType::LipSyncIssue: event_type = "LIP_SYNC_ISSUE"; break;
                case SyncEventType::DriftDetected: event_type = "DRIFT_DETECTED"; break;
            }
            
            ve::log::info("Sync Event: " + std::string(event_type) + 
                         " - Offset: " + std::to_string(event.offset_ms) + "ms - " + event.description);
        });
        
        // Start validation
        if (!validator->start()) {
            ve::log::error("Failed to start sync validator");
            return 1;
        }
        
        ve::log::info("Validator started successfully");
        
        // Simulate measurements with different sync scenarios
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Test 1: Perfect sync
        ve::log::info("Test 1: Perfect sync scenario");
        for (int i = 0; i < 10; ++i) {
            auto timestamp = std::chrono::high_resolution_clock::now();
            double time_sec = i * 0.1; // 100ms intervals
            
            ve::TimePoint audio_pos(time_sec * 48000, 48000);
            ve::TimePoint video_pos(time_sec * 30, 30);
            
            validator->record_measurement(audio_pos, video_pos, timestamp);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Test 2: Audio lag (video ahead)
        ve::log::info("Test 2: Audio lag scenario (video ahead by 30ms)");
        for (int i = 0; i < 10; ++i) {
            auto timestamp = std::chrono::high_resolution_clock::now();
            double time_sec = (10 + i) * 0.1;
            
            ve::TimePoint audio_pos(time_sec * 48000, 48000);
            ve::TimePoint video_pos((time_sec + 0.03) * 30, 30); // 30ms ahead
            
            validator->record_measurement(audio_pos, video_pos, timestamp);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Test 3: Severe lip-sync issue
        ve::log::info("Test 3: Severe lip-sync issue (50ms offset)");
        for (int i = 0; i < 5; ++i) {
            auto timestamp = std::chrono::high_resolution_clock::now();
            double time_sec = (20 + i) * 0.1;
            
            ve::TimePoint audio_pos(time_sec * 48000, 48000);
            ve::TimePoint video_pos((time_sec + 0.05) * 30, 30); // 50ms ahead
            
            validator->record_measurement(audio_pos, video_pos, timestamp);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Test 4: Return to sync
        ve::log::info("Test 4: Return to sync");
        for (int i = 0; i < 5; ++i) {
            auto timestamp = std::chrono::high_resolution_clock::now();
            double time_sec = (25 + i) * 0.1;
            
            ve::TimePoint audio_pos(time_sec * 48000, 48000);
            ve::TimePoint video_pos(time_sec * 30, 30); // Back in sync
            
            validator->record_measurement(audio_pos, video_pos, timestamp);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Get current state
        auto current_offset = validator->get_current_offset_ms();
        bool in_sync = validator->is_in_sync();
        auto correction = validator->calculate_correction_recommendation();
        
        std::ostringstream log_oss;
        log_oss << "Current offset: " << std::fixed << std::setprecision(2) << current_offset << "ms";
        ve::log::info(log_oss.str());
        log_oss.str(""); log_oss.clear();
        
        log_oss << "In sync: " << (in_sync ? "YES" : "NO");
        ve::log::info(log_oss.str());
        log_oss.str(""); log_oss.clear();
        
        log_oss << "Correction recommendation: " << std::fixed << std::setprecision(2) << correction << "ms";
        ve::log::info(log_oss.str());
        
        // Get quality metrics
        auto metrics = validator->get_quality_metrics();
        ve::log::info("Quality Metrics:");
        std::ostringstream metrics_oss;
        metrics_oss << "  Measurement count: " << metrics.measurement_count;
        ve::log::info(metrics_oss.str());
        metrics_oss.str(""); metrics_oss.clear();
        
        metrics_oss << "  Sync percentage: " << std::fixed << std::setprecision(1) << metrics.sync_percentage << "%";
        ve::log::info(metrics_oss.str());
        std::ostringstream oss;
        oss << "  Mean offset: " << std::fixed << std::setprecision(2) << metrics.mean_offset_ms << "ms";
        ve::log::info(oss.str());
        
        oss.str(""); oss.clear();
        oss << "  Std deviation: " << std::fixed << std::setprecision(2) << metrics.std_deviation_ms << "ms";
        ve::log::info(oss.str());
        
        oss.str(""); oss.clear();
        oss << "  Overall quality: " << std::fixed << std::setprecision(2) << metrics.overall_quality_score;
        ve::log::info(oss.str());
        
        // Generate quality report
        auto report = validator->generate_quality_report();
        ve::log::info("Quality Report:\n" + report);
        
        // Export measurements
        if (validator->export_measurements("sync_test_measurements.csv")) {
            ve::log::info("Measurements exported to sync_test_measurements.csv");
        }
        
        // Test lip-sync validation
        double lip_sync_quality = validator->validate_lip_sync(nullptr, nullptr);
        std::ostringstream quality_oss;
        quality_oss << "Lip-sync quality score: " << std::fixed << std::setprecision(2) << lip_sync_quality;
        ve::log::info(quality_oss.str());
        
        // Stop validator
        validator->stop();
        ve::log::info("Sync validator test completed successfully");
        
        return 0;
        
    } catch (const std::exception& e) {
        ve::log::error("Sync validator test failed: " + std::string(e.what()));
        return 1;
    }
}