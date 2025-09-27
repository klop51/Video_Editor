/**
 * Professional Audio Monitoring - Phase 2 Implementation
 * 
 * Comprehensive professional audio monitoring system including:
 * - EBU R128 Loudness Monitoring with full compliance checking
 * - Peak/RMS Meters with professional ballistics and standards
 * - Professional Audio Scopes (vectorscope, phase correlation, spectrum)
 * - Real-time visualization and broadcast compliance validation
 * 
 * This system provides broadcast-quality monitoring for professional
 * video editing workflows with industry-standard visual feedback.
 */

#pragma once

#include "audio/audio_frame.hpp"
#include "audio/loudness_monitor.hpp"
#include "audio/safe_loudness_monitor.hpp"
#include "audio/audio_meters.hpp"
#include "core/time.hpp"
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <cmath>
#include <algorithm>
#include <complex>
#include <deque>

namespace ve::audio {

/**
 * Enhanced EBU R128 Loudness Monitor with full compliance checking
 */
class EnhancedEBUR128Monitor {
public:
    struct ComplianceStatus {
        bool integrated_compliant{false};
        bool range_compliant{false};
        bool peak_compliant{false};
        double integrated_lufs{-std::numeric_limits<double>::infinity()};
        double loudness_range{0.0};
        double peak_level_dbfs{-std::numeric_limits<double>::infinity()};
        std::string compliance_text;
    };

    struct LoudnessHistory {
        std::vector<double> momentary_values;
        std::vector<double> short_term_values;
        std::vector<ve::TimePoint> timestamps;
        size_t max_history_size{10000}; // ~3 hours at 3s intervals
    };

    explicit EnhancedEBUR128Monitor(double sample_rate = 48000.0, uint16_t channels = 2);
    ~EnhancedEBUR128Monitor() = default;

    // Core processing
    void initialize();
    void reset();
    void process_samples(const AudioFrame& frame);

    // EBU R128 measurements
    double get_momentary_lufs() const;
    double get_short_term_lufs() const;
    double get_integrated_lufs() const;
    double get_loudness_range() const;
    double get_peak_level_dbfs() const;

    // Compliance checking
    ComplianceStatus get_compliance_status() const;
    bool is_broadcast_compliant() const;
    std::vector<std::string> get_compliance_warnings() const;

    // History and analysis
    const LoudnessHistory& get_loudness_history() const { return history_; }
    void set_target_platform(const std::string& platform);
    
    // Statistics
    uint64_t get_samples_processed() const { return samples_processed_.load(); }
    double get_measurement_duration_seconds() const;

private:
    std::unique_ptr<SafeRealTimeLoudnessMonitor> core_monitor_;
    
    // Enhanced measurement tracking
    mutable std::mutex measurement_mutex_;
    LoudnessHistory history_;
    ComplianceStatus current_compliance_;
    
    // Platform-specific targets
    std::string target_platform_{"EBU"};
    double target_lufs_{ebu_r128::REFERENCE_LUFS};
    double tolerance_db_{1.0};
    
    // Processing state
    std::atomic<uint64_t> samples_processed_{0};
    std::chrono::steady_clock::time_point start_time_;
    
    // Internal methods
    void update_compliance_status();
    void update_history();
    std::string generate_compliance_text() const;
};

/**
 * Professional Peak/RMS Meter System
 */
class ProfessionalMeterSystem {
public:
    enum class MeterStandard {
        DIGITAL_PEAK,    // Digital peak with sample-accurate detection
        BBC_PPM,         // BBC PPM with Type I ballistics
        EBU_PPM,         // EBU PPM with Type IIa ballistics
        VU_METER,        // True VU meter with VU ballistics
        K_SYSTEM         // K-System metering
    };

    struct MeterReading {
        double current_level_db{-std::numeric_limits<double>::infinity()};
        double peak_hold_db{-std::numeric_limits<double>::infinity()};
        double rms_level_db{-std::numeric_limits<double>::infinity()};
        bool overload{false};
        bool valid{false};
        std::chrono::steady_clock::time_point timestamp;
    };

    struct MeterConfig {
        MeterStandard standard{MeterStandard::DIGITAL_PEAK};
        double reference_level_db{-20.0}; // K-20 by default
        double peak_hold_time_ms{1500.0};
        double integration_time_ms{300.0}; // For RMS
        bool enable_overload_detection{true};
        double overload_threshold_db{-0.1};
    };

    explicit ProfessionalMeterSystem(uint16_t channels = 2, double sample_rate = 48000.0);
    ~ProfessionalMeterSystem() = default;

    // Configuration
    void configure_meter(uint16_t channel, const MeterConfig& config);
    void set_global_reference_level(double ref_db);

    // Processing
    void process_samples(const AudioFrame& frame);
    void reset_meters();
    void reset_peak_holds();

    // Readings
    MeterReading get_meter_reading(uint16_t channel) const;
    std::vector<MeterReading> get_all_readings() const;
    bool any_channel_overload() const;

    // Visual data for UI
    struct VisualMeterData {
        std::vector<double> channel_levels_db;
        std::vector<double> peak_holds_db;
        std::vector<bool> overload_indicators;
        double max_level_db{-std::numeric_limits<double>::infinity()};
        bool any_overload{false};
    };
    VisualMeterData get_visual_data() const;

private:
    std::vector<std::unique_ptr<ProfessionalAudioMeter>> meters_;
    std::vector<MeterConfig> configs_;
    mutable std::mutex readings_mutex_;
    
    uint16_t channel_count_;
    double sample_rate_;
    double global_reference_db_{-20.0};

    void update_visual_cache();
    mutable VisualMeterData visual_cache_;
};

/**
 * Professional Audio Scopes System
 */
class ProfessionalAudioScopes {
public:
    // Vectorscope for stereo field analysis
    struct VectorscopeData {
        std::vector<std::complex<float>> points; // Complex representation of L+R vs L-R
        double correlation_coefficient{0.0};
        double stereo_width{0.0};
        bool mono_compatible{true};
        size_t max_points{1000};
    };

    // Phase correlation meter
    struct PhaseCorrelationData {
        double correlation{0.0};           // -1.0 to +1.0
        double decorrelation_db{0.0};      // Decorrelation in dB
        bool mono_compatible{true};
        std::vector<double> history;       // Recent correlation values
        size_t max_history{100};
    };

    // Spectrum analyzer
    struct SpectrumData {
        std::vector<double> frequencies_hz;
        std::vector<double> magnitudes_db;
        size_t fft_size{2048};
        double frequency_resolution_hz{0.0};
        std::vector<double> peak_hold_db;
        bool log_frequency_scale{true};
    };

    explicit ProfessionalAudioScopes(double sample_rate = 48000.0, uint16_t channels = 2);
    ~ProfessionalAudioScopes() = default;

    // Configuration
    void set_fft_size(size_t size);
    void set_vectorscope_persistence(size_t max_points);
    void enable_log_frequency_scale(bool enable);

    // Processing
    void process_samples(const AudioFrame& frame);
    void reset_scopes();

    // Data access
    VectorscopeData get_vectorscope_data() const;
    PhaseCorrelationData get_phase_correlation_data() const;
    SpectrumData get_spectrum_data() const;

    // Analysis
    bool detect_phase_issues() const;
    bool detect_mono_compatibility_issues() const;
    std::vector<std::string> get_scope_warnings() const;

private:
    double sample_rate_;
    uint16_t channel_count_;
    
    // Vectorscope
    mutable std::mutex vectorscope_mutex_;
    VectorscopeData vectorscope_data_;
    std::deque<std::complex<float>> vectorscope_buffer_;
    
    // Phase correlation
    mutable std::mutex correlation_mutex_;
    PhaseCorrelationData correlation_data_;
    std::unique_ptr<CorrelationMeter> correlation_meter_;
    
    // Spectrum analyzer
    mutable std::mutex spectrum_mutex_;
    SpectrumData spectrum_data_;
    std::vector<std::complex<float>> fft_buffer_;
    std::vector<float> window_function_;
    size_t fft_size_{2048};
    
    // Processing helpers
    void update_vectorscope(float left, float right);
    void update_spectrum(const AudioFrame& frame);
    void compute_fft();
    void apply_window_function();
    void generate_window_function();
};

/**
 * Unified Professional Audio Monitoring System
 */
class ProfessionalAudioMonitoringSystem {
public:
    struct MonitoringConfig {
        bool enable_loudness_monitoring{true};
        bool enable_peak_rms_meters{true};
        bool enable_audio_scopes{true};
        std::string target_platform{"EBU"};
        double reference_level_db{-20.0};
        size_t update_rate_hz{30}; // UI update rate
    };

    explicit ProfessionalAudioMonitoringSystem(const MonitoringConfig& config = {});
    ~ProfessionalAudioMonitoringSystem() = default;

    // Lifecycle
    bool initialize(double sample_rate = 48000.0, uint16_t channels = 2);
    void shutdown();
    void reset_all();

    // Configuration
    void configure(const MonitoringConfig& config);
    void set_target_platform(const std::string& platform);

    // Processing - main entry point
    void process_audio_frame(const AudioFrame& frame);

    // Component access
    const EnhancedEBUR128Monitor* get_loudness_monitor() const { return loudness_monitor_.get(); }
    const ProfessionalMeterSystem* get_meter_system() const { return meter_system_.get(); }
    const ProfessionalAudioScopes* get_scopes() const { return scopes_.get(); }

    // Unified status
    struct SystemStatus {
        bool broadcast_compliant{false};
        std::vector<std::string> warnings;
        std::vector<std::string> recommendations;
        double overall_quality_score{0.0}; // 0.0 to 1.0
    };
    SystemStatus get_system_status() const;

    // Performance monitoring
    struct PerformanceStats {
        double avg_processing_time_ms{0.0};
        double max_processing_time_ms{0.0};
        uint64_t frames_processed{0};
        double cpu_usage_percent{0.0};
    };
    PerformanceStats get_performance_stats() const;

private:
    MonitoringConfig config_;
    bool initialized_{false};
    
    // Core monitoring components
    std::unique_ptr<EnhancedEBUR128Monitor> loudness_monitor_;
    std::unique_ptr<ProfessionalMeterSystem> meter_system_;
    std::unique_ptr<ProfessionalAudioScopes> scopes_;
    
    // Performance tracking
    mutable std::mutex perf_mutex_;
    std::vector<double> processing_times_ms_;
    std::chrono::steady_clock::time_point last_update_;
    uint64_t frames_processed_{0};
    
    // Internal methods
    void update_performance_stats(double processing_time_ms);
    SystemStatus analyze_system_status() const;
};

} // namespace ve::audio