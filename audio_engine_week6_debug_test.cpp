#include <iostream>
#include <chrono>
#include <thread>

// Include your audio engine headers
#include "audio/master_clock.h"
#include "audio/sync_validator.h"
#include "audio/latency_compensator.h"
#include "core/log.hpp"

using namespace ve::audio;

int main() {
    try {
        ve::log::info("=== Week 6 A/V Sync Debug Test ===");
        
        // Test 1: Basic component creation
        ve::log::info("Testing component creation...");
        
        MasterClockConfig clock_config;
        clock_config.sample_rate = 48000;
        clock_config.buffer_size = 256;
        clock_config.drift_tolerance_ms = 1.0;
        clock_config.correction_speed = 0.1;
        
        auto master_clock = MasterClock::create(clock_config);
        if (!master_clock) {
            ve::log::error("Failed to create master clock");
            return 1;
        }
        ve::log::info("✅ Master clock created");
        
        SyncValidatorConfig validator_config;
        validator_config.sync_tolerance_ms = 10.0;
        validator_config.measurement_interval_ms = 100.0;
        validator_config.enable_quality_monitoring = true;
        
        auto sync_validator = SyncValidator::create(validator_config);
        if (!sync_validator) {
            ve::log::error("Failed to create sync validator");
            return 1;
        }
        ve::log::info("✅ Sync validator created");
        
        LatencyCompensatorConfig compensator_config;
        compensator_config.max_compensation_ms = 100.0;
        compensator_config.enable_pdc = true;
        compensator_config.adaptation_speed = 0.1;
        
        auto latency_compensator = LatencyCompensator::create(compensator_config);
        if (!latency_compensator) {
            ve::log::error("Failed to create latency compensator");
            return 1;
        }
        ve::log::info("✅ Latency compensator created");
        
        // Test 2: Component startup
        ve::log::info("Testing component startup...");
        
        if (!master_clock->start()) {
            ve::log::error("Failed to start master clock");
            return 1;
        }
        ve::log::info("✅ Master clock started");
        
        if (!sync_validator->start()) {
            ve::log::error("Failed to start sync validator");
            return 1;
        }
        ve::log::info("✅ Sync validator started");
        
        if (!latency_compensator->start()) {
            ve::log::error("Failed to start latency compensator");
            return 1;
        }
        ve::log::info("✅ Latency compensator started");
        
        // Keep components alive and don't let destructors run until we're done
        ve::log::info("=== Components are running, proceeding with test operations ===");
        
        // Test 3: Basic operations
        ve::log::info("Testing basic operations...");
        
        ve::log::info("Step 1: Creating timestamp...");
        auto timestamp = std::chrono::high_resolution_clock::now();
        ve::log::info("✅ Timestamp created");
        
        ve::log::info("Step 2: Calling update_audio_position...");
        try {
            master_clock->update_audio_position(4800, timestamp); // 100ms worth at 48kHz
            ve::log::info("✅ Master clock update successful");
        } catch (const std::exception& e) {
            ve::log::error("Exception in update_audio_position: " + std::string(e.what()));
            latency_compensator->stop();
            sync_validator->stop();
            master_clock->stop();
            return 1;
        } catch (...) {
            ve::log::error("Unknown exception in update_audio_position");
            latency_compensator->stop();
            sync_validator->stop();
            master_clock->stop();
            return 1;
        }
        
        ve::log::info("Step 3: Creating TimePoint objects...");
        ve::TimePoint audio_pos(4800, 48000);
        ve::TimePoint video_pos(4800, 48000);
        ve::log::info("✅ TimePoint objects created");
        
        ve::log::info("Step 4: Calling record_measurement...");
        try {
            auto measurement = sync_validator->record_measurement(audio_pos, video_pos, timestamp);
            ve::log::info("✅ Sync measurement successful");
        } catch (const std::exception& e) {
            ve::log::error("Exception in record_measurement: " + std::string(e.what()));
            latency_compensator->stop();
            sync_validator->stop();
            master_clock->stop();
            return 1;
        } catch (...) {
            ve::log::error("Unknown exception in record_measurement");
            latency_compensator->stop();
            sync_validator->stop();
            master_clock->stop();
            return 1;
        }
        
        ve::log::info("Step 5: Calling measure_total_latency...");
        try {
            latency_compensator->measure_total_latency();
            ve::log::info("✅ Latency measurement successful");
        } catch (const std::exception& e) {
            ve::log::error("Exception in measure_total_latency: " + std::string(e.what()));
            latency_compensator->stop();
            sync_validator->stop();
            master_clock->stop();
            return 1;
        } catch (...) {
            ve::log::error("Unknown exception in measure_total_latency");
            latency_compensator->stop();
            sync_validator->stop();
            master_clock->stop();
            return 1;
        }
        
        // Test 4: Cleanup (explicit stop to avoid destructor issues)
        ve::log::info("Testing component shutdown...");
        
        latency_compensator->stop();
        ve::log::info("✅ Latency compensator stopped explicitly");
        
        sync_validator->stop();
        ve::log::info("✅ Sync validator stopped");
        
        master_clock->stop();
        ve::log::info("✅ Master clock stopped");
        
        // Give components time to clean up before destructors
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        ve::log::info("=== All debug tests passed! ===");
        return 0;
        
    } catch (const std::exception& e) {
        ve::log::error("Exception caught: " + std::string(e.what()));
        return 1;
    } catch (...) {
        ve::log::error("Unknown exception caught");
        return 1;
    }
}