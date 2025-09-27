/**
 * Professional Audio Monitoring - Phase 2 Implementation
 * 
 * Comprehensive professional audio monitoring system implementation
 * providing broadcast-quality monitoring for professional video editing.
 */

#include "audio/professional_monitoring.hpp"
#include "core/log.hpp"
#include <numeric>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstring>

namespace ve::audio {

// ============================================================================
// Enhanced EBU R128 Monitor Implementation
// ============================================================================

EnhancedEBUR128Monitor::EnhancedEBUR128Monitor(double sample_rate, uint16_t channels)
    : core_monitor_(std::make_unique<RealTimeLoudnessMonitor>(sample_rate, channels))
    , start_time_(std::chrono::steady_clock::now())
{
    history_.momentary_values.reserve(history_.max_history_size);
    history_.short_term_values.reserve(history_.max_history_size);
    history_.timestamps.reserve(history_.max_history_size);
}

void EnhancedEBUR128Monitor::initialize() {
    core_monitor_->initialize();
    
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    history_.momentary_values.clear();
    history_.short_term_values.clear();
    history_.timestamps.clear();
    samples_processed_.store(0);
    start_time_ = std::chrono::steady_clock::now();
    
    ve::log::info("Enhanced EBU R128 monitor initialized for " + target_platform_ + " compliance");
}

void EnhancedEBUR128Monitor::reset() {
    core_monitor_->reset();
    
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    history_.momentary_values.clear();
    history_.short_term_values.clear();
    history_.timestamps.clear();
    current_compliance_ = ComplianceStatus{};
    samples_processed_.store(0);
    start_time_ = std::chrono::steady_clock::now();
}

void EnhancedEBUR128Monitor::process_samples(const AudioFrame& frame) {
    // PHASE B: Test compliance checking operations while RealTimeLoudnessMonitor is disabled
    
    // This should be safe since RealTimeLoudnessMonitor::process_samples just returns early
    core_monitor_->process_samples(frame);
    
    // PHASE A SUCCESS: These operations work fine
    samples_processed_.fetch_add(frame.sample_count() * frame.channel_count());
    
    // PHASE B: Re-enable compliance checking and history operations
    static auto last_update = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count() >= 100) {
        // PHASE B: Re-enable these operations to test if they cause the crash
        update_compliance_status();
        update_history();
        last_update = now;
    }
}

double EnhancedEBUR128Monitor::get_momentary_lufs() const {
    auto measurement = core_monitor_->get_current_measurement();
    return measurement.momentary_lufs;
}

double EnhancedEBUR128Monitor::get_short_term_lufs() const {
    auto measurement = core_monitor_->get_current_measurement();
    return measurement.short_term_lufs;
}

double EnhancedEBUR128Monitor::get_integrated_lufs() const {
    return core_monitor_->get_integrated_lufs();
}

double EnhancedEBUR128Monitor::get_loudness_range() const {
    auto measurement = core_monitor_->get_current_measurement();
    // Calculate loudness range from available data (simplified)
    return 5.0; // Placeholder - would need proper LRA calculation
}

double EnhancedEBUR128Monitor::get_peak_level_dbfs() const {
    auto measurement = core_monitor_->get_current_measurement();
    return std::max(measurement.peak_left_dbfs, measurement.peak_right_dbfs);
}

EnhancedEBUR128Monitor::ComplianceStatus EnhancedEBUR128Monitor::get_compliance_status() const {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    return current_compliance_;
}

bool EnhancedEBUR128Monitor::is_broadcast_compliant() const {
    auto status = get_compliance_status();
    return status.integrated_compliant && status.range_compliant && status.peak_compliant;
}

std::vector<std::string> EnhancedEBUR128Monitor::get_compliance_warnings() const {
    std::vector<std::string> warnings;
    auto status = get_compliance_status();
    
    if (!status.integrated_compliant) {
        warnings.push_back("Integrated loudness outside target range: " + 
                          std::to_string(status.integrated_lufs) + " LUFS (target: " +
                          std::to_string(target_lufs_) + " ±" + std::to_string(tolerance_db_) + " LU)");
    }
    
    if (!status.range_compliant) {
        warnings.push_back("Loudness range excessive: " + 
                          std::to_string(status.loudness_range) + " LU (max recommended: 20 LU)");
    }
    
    if (!status.peak_compliant) {
        warnings.push_back("Peak level exceeds ceiling: " + 
                          std::to_string(status.peak_level_dbfs) + " dBFS (max: -1.0 dBFS)");
    }
    
    return warnings;
}

void EnhancedEBUR128Monitor::set_target_platform(const std::string& platform) {
    target_platform_ = platform;
    
    // Set platform-specific targets
    if (platform == "EBU") {
        target_lufs_ = -23.0;
        tolerance_db_ = 1.0;
    } else if (platform == "ATSC") {
        target_lufs_ = -24.0;
        tolerance_db_ = 2.0;
    } else if (platform == "Spotify") {
        target_lufs_ = -14.0;
        tolerance_db_ = 1.0;
    } else if (platform == "YouTube") {
        target_lufs_ = -14.0;
        tolerance_db_ = 1.0;
    } else {
        target_lufs_ = -23.0; // Default to EBU
        tolerance_db_ = 1.0;
    }
    
    ve::log::info("Target platform set to " + platform + " (target: " + 
                  std::to_string(target_lufs_) + " LUFS)");
}

double EnhancedEBUR128Monitor::get_measurement_duration_seconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start_time_).count();
}

void EnhancedEBUR128Monitor::update_compliance_status() {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    
    current_compliance_.integrated_lufs = get_integrated_lufs();
    current_compliance_.loudness_range = get_loudness_range();
    current_compliance_.peak_level_dbfs = get_peak_level_dbfs();
    
    // Check compliance
    current_compliance_.integrated_compliant = 
        std::abs(current_compliance_.integrated_lufs - target_lufs_) <= tolerance_db_;
    
    current_compliance_.range_compliant = 
        current_compliance_.loudness_range <= 20.0; // Typical broadcast recommendation
    
    current_compliance_.peak_compliant = 
        current_compliance_.peak_level_dbfs <= ebu_r128::PEAK_CEILING_DBFS;
    
    current_compliance_.compliance_text = generate_compliance_text();
}

void EnhancedEBUR128Monitor::update_history() {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    
    double momentary = get_momentary_lufs();
    double short_term = get_short_term_lufs();
    auto timestamp = ve::TimePoint(0); // Use default TimePoint constructor
    
    history_.momentary_values.push_back(momentary);
    history_.short_term_values.push_back(short_term);
    history_.timestamps.push_back(timestamp);
    
    // Limit history size
    if (history_.momentary_values.size() > history_.max_history_size) {
        history_.momentary_values.erase(history_.momentary_values.begin());
        history_.short_term_values.erase(history_.short_term_values.begin());
        history_.timestamps.erase(history_.timestamps.begin());
    }
}

std::string EnhancedEBUR128Monitor::generate_compliance_text() const {
    if (is_broadcast_compliant()) {
        return target_platform_ + " compliant ✓";
    } else {
        return target_platform_ + " non-compliant ⚠";
    }
}

// ============================================================================
// Professional Meter System Implementation
// ============================================================================

ProfessionalMeterSystem::ProfessionalMeterSystem(uint16_t channels, double sample_rate)
    : channel_count_(channels)
    , sample_rate_(sample_rate)
{
    meters_.reserve(channel_count_);
    configs_.resize(channel_count_);
    
    // Initialize meters with default digital peak configuration
    for (uint16_t ch = 0; ch < channel_count_; ++ch) {
        MeterConfig default_config;
        configure_meter(ch, default_config);
    }
    
    visual_cache_.channel_levels_db.resize(channel_count_, -std::numeric_limits<double>::infinity());
    visual_cache_.peak_holds_db.resize(channel_count_, -std::numeric_limits<double>::infinity());
    visual_cache_.overload_indicators.resize(channel_count_, false);
}

void ProfessionalMeterSystem::configure_meter(uint16_t channel, const MeterConfig& config) {
    if (channel >= channel_count_) return;
    
    configs_[channel] = config;
    
    // Create appropriate meter configuration using existing API
    MeterConfig meter_config = config; // Use the provided config directly
    
    switch (config.standard) {
        case MeterStandard::DIGITAL_PEAK:
            // Already set in provided config
            break;
        case MeterStandard::BBC_PPM:
            // Use BBC ballistics
            break;
        case MeterStandard::EBU_PPM:
            // Use EBU ballistics
            break;
        case MeterStandard::VU_METER:
            // Use VU meter ballistics
            break;
        case MeterStandard::K_SYSTEM:
            // Use K-System ballistics
            break;
    }
    
    // Configuration stored for use during processing
}

void ProfessionalMeterSystem::set_global_reference_level(double ref_db) {
    global_reference_db_ = ref_db;
    
    // Update all meter configurations
    for (uint16_t ch = 0; ch < channel_count_; ++ch) {
        auto config = configs_[ch];
        config.reference_level_db = ref_db;
        configure_meter(ch, config);
    }
}

void ProfessionalMeterSystem::process_samples(const AudioFrame& frame) {
    if (frame.channel_count() != channel_count_) return;
    
    // Process each channel
    for (uint16_t ch = 0; ch < channel_count_ && ch < meters_.size(); ++ch) {
        if (!meters_[ch]) continue;
        
        // Extract channel samples
        std::vector<float> channel_samples;
        channel_samples.reserve(frame.sample_count());
        
        for (size_t i = 0; i < frame.sample_count(); ++i) {
            float sample = frame.get_sample_as_float(ch, static_cast<uint32_t>(i));
            channel_samples.push_back(sample);
        }
        
        meters_[ch]->update_with_samples(channel_samples.data(), channel_samples.size());
    }
    
    update_visual_cache();
}

void ProfessionalMeterSystem::reset_meters() {
    for (auto& meter : meters_) {
        if (meter) {
            meter->reset();
        }
    }
    update_visual_cache();
}

void ProfessionalMeterSystem::reset_peak_holds() {
    // Reset peak hold values (simplified implementation)
    // In a full implementation, this would reset internal peak hold states
    update_visual_cache();
}

ProfessionalMeterSystem::MeterReading ProfessionalMeterSystem::get_meter_reading(uint16_t channel) const {
    if (channel >= meters_.size() || !meters_[channel]) {
        return MeterReading{};
    }
    
    auto visual_data = meters_[channel]->get_visual_data();
    
    MeterReading reading;
    reading.current_level_db = visual_data.current_level;
    reading.peak_hold_db = visual_data.peak_hold_level;
    reading.rms_level_db = visual_data.current_level; // Use current level as RMS approximation
    reading.overload = visual_data.over_ceiling; // Use over_ceiling as overload indicator
    reading.valid = visual_data.valid;
    reading.timestamp = std::chrono::steady_clock::now();
    
    return reading;
}

std::vector<ProfessionalMeterSystem::MeterReading> ProfessionalMeterSystem::get_all_readings() const {
    std::vector<MeterReading> readings;
    readings.reserve(channel_count_);
    
    for (uint16_t ch = 0; ch < channel_count_; ++ch) {
        readings.push_back(get_meter_reading(ch));
    }
    
    return readings;
}

bool ProfessionalMeterSystem::any_channel_overload() const {
    std::lock_guard<std::mutex> lock(readings_mutex_);
    return visual_cache_.any_overload;
}

ProfessionalMeterSystem::VisualMeterData ProfessionalMeterSystem::get_visual_data() const {
    std::lock_guard<std::mutex> lock(readings_mutex_);
    return visual_cache_;
}

void ProfessionalMeterSystem::update_visual_cache() {
    std::lock_guard<std::mutex> lock(readings_mutex_);
    
    visual_cache_.max_level_db = -std::numeric_limits<double>::infinity();
    visual_cache_.any_overload = false;
    
    for (uint16_t ch = 0; ch < channel_count_ && ch < meters_.size(); ++ch) {
        if (!meters_[ch]) continue;
        
        auto visual_data = meters_[ch]->get_visual_data();
        
        if (ch < visual_cache_.channel_levels_db.size()) {
            visual_cache_.channel_levels_db[ch] = visual_data.current_level;
            visual_cache_.peak_holds_db[ch] = visual_data.peak_hold_level;
            visual_cache_.overload_indicators[ch] = visual_data.over_ceiling;
            
            if (visual_data.current_level > visual_cache_.max_level_db) {
                visual_cache_.max_level_db = visual_data.current_level;
            }
            
            if (visual_data.over_ceiling) {
                visual_cache_.any_overload = true;
            }
        }
    }
}

// ============================================================================
// Professional Audio Scopes Implementation
// ============================================================================

ProfessionalAudioScopes::ProfessionalAudioScopes(double sample_rate, uint16_t channels)
    : sample_rate_(sample_rate)
    , channel_count_(channels)
    , correlation_meter_(std::make_unique<CorrelationMeter>())
{
    // Initialize vectorscope
    vectorscope_data_.points.reserve(vectorscope_data_.max_points);
    
    // Initialize correlation
    correlation_data_.history.reserve(correlation_data_.max_history);
    
    // Initialize spectrum analyzer
    set_fft_size(fft_size_);
    generate_window_function();
}

void ProfessionalAudioScopes::set_fft_size(size_t size) {
    std::lock_guard<std::mutex> lock(spectrum_mutex_);
    
    fft_size_ = size;
    fft_buffer_.resize(fft_size_);
    spectrum_data_.frequencies_hz.resize(fft_size_ / 2);
    spectrum_data_.magnitudes_db.resize(fft_size_ / 2);
    spectrum_data_.peak_hold_db.resize(fft_size_ / 2, -std::numeric_limits<double>::infinity());
    
    // Calculate frequency bins
    spectrum_data_.frequency_resolution_hz = sample_rate_ / fft_size_;
    for (size_t i = 0; i < fft_size_ / 2; ++i) {
        spectrum_data_.frequencies_hz[i] = i * spectrum_data_.frequency_resolution_hz;
    }
    
    generate_window_function();
}

void ProfessionalAudioScopes::set_vectorscope_persistence(size_t max_points) {
    std::lock_guard<std::mutex> lock(vectorscope_mutex_);
    vectorscope_data_.max_points = max_points;
    vectorscope_data_.points.reserve(max_points);
}

void ProfessionalAudioScopes::enable_log_frequency_scale(bool enable) {
    std::lock_guard<std::mutex> lock(spectrum_mutex_);
    spectrum_data_.log_frequency_scale = enable;
}

void ProfessionalAudioScopes::process_samples(const AudioFrame& frame) {
    if (frame.channel_count() < 2) return; // Need stereo for scopes
    
    for (size_t i = 0; i < frame.sample_count(); ++i) {
        float left = frame.get_sample_as_float(0, static_cast<uint32_t>(i));
        float right = frame.get_sample_as_float(1, static_cast<uint32_t>(i));
        
        // Update vectorscope
        update_vectorscope(left, right);
        
        // Update correlation
        correlation_meter_->process_samples(left, right);
    }
    
    // Update spectrum analyzer less frequently
    static size_t frame_counter = 0;
    if (++frame_counter % 10 == 0) { // Every 10th frame
        update_spectrum(frame);
    }
    
    // Update correlation data
    {
        std::lock_guard<std::mutex> lock(correlation_mutex_);
        auto correlation = correlation_meter_->get_correlation();
        correlation_data_.correlation = correlation;
        correlation_data_.decorrelation_db = 20.0 * std::log10(std::max(1.0 - std::abs(correlation), 1e-10));
        correlation_data_.mono_compatible = correlation > 0.5;
        
        correlation_data_.history.push_back(correlation_data_.correlation);
        if (correlation_data_.history.size() > correlation_data_.max_history) {
            correlation_data_.history.erase(correlation_data_.history.begin());
        }
    }
}

void ProfessionalAudioScopes::reset_scopes() {
    {
        std::lock_guard<std::mutex> lock(vectorscope_mutex_);
        vectorscope_data_.points.clear();
        vectorscope_buffer_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(correlation_mutex_);
        correlation_data_.history.clear();
        correlation_meter_->reset();
    }
    
    {
        std::lock_guard<std::mutex> lock(spectrum_mutex_);
        std::fill(spectrum_data_.peak_hold_db.begin(), spectrum_data_.peak_hold_db.end(), 
                  -std::numeric_limits<double>::infinity());
    }
}

ProfessionalAudioScopes::VectorscopeData ProfessionalAudioScopes::get_vectorscope_data() const {
    std::lock_guard<std::mutex> lock(vectorscope_mutex_);
    return vectorscope_data_;
}

ProfessionalAudioScopes::PhaseCorrelationData ProfessionalAudioScopes::get_phase_correlation_data() const {
    std::lock_guard<std::mutex> lock(correlation_mutex_);
    return correlation_data_;
}

ProfessionalAudioScopes::SpectrumData ProfessionalAudioScopes::get_spectrum_data() const {
    std::lock_guard<std::mutex> lock(spectrum_mutex_);
    return spectrum_data_;
}

bool ProfessionalAudioScopes::detect_phase_issues() const {
    auto correlation_data = get_phase_correlation_data();
    return correlation_data.correlation < -0.3; // Significant phase issues
}

bool ProfessionalAudioScopes::detect_mono_compatibility_issues() const {
    auto correlation_data = get_phase_correlation_data();
    return !correlation_data.mono_compatible;
}

std::vector<std::string> ProfessionalAudioScopes::get_scope_warnings() const {
    std::vector<std::string> warnings;
    
    if (detect_phase_issues()) {
        warnings.push_back("Phase correlation issues detected (correlation < -0.3)");
    }
    
    if (detect_mono_compatibility_issues()) {
        warnings.push_back("Mono compatibility issues detected");
    }
    
    auto vectorscope = get_vectorscope_data();
    if (vectorscope.stereo_width < 0.1) {
        warnings.push_back("Narrow stereo image detected");
    }
    
    return warnings;
}

void ProfessionalAudioScopes::update_vectorscope(float left, float right) {
    std::lock_guard<std::mutex> lock(vectorscope_mutex_);
    
    // Convert to L+R vs L-R representation
    float sum = (left + right) * 0.5f;
    float diff = (left - right) * 0.5f;
    std::complex<float> point(sum, diff);
    
    vectorscope_buffer_.push_back(point);
    
    // Decimation - only keep every Nth sample for display
    static size_t decimation_counter = 0;
    if (++decimation_counter % 16 == 0) { // Keep every 16th sample
        vectorscope_data_.points.push_back(point);
        
        if (vectorscope_data_.points.size() > vectorscope_data_.max_points) {
            vectorscope_data_.points.erase(vectorscope_data_.points.begin());
        }
    }
    
    // Calculate stereo width from recent samples
    if (vectorscope_buffer_.size() >= 1024) {
        double width_accumulator = 0.0;
        for (const auto& p : vectorscope_buffer_) {
            width_accumulator += std::abs(p.imag()); // L-R component
        }
        vectorscope_data_.stereo_width = width_accumulator / vectorscope_buffer_.size();
        
        // Keep only recent samples
        while (vectorscope_buffer_.size() > 1024) {
            vectorscope_buffer_.pop_front();
        }
    }
}

void ProfessionalAudioScopes::update_spectrum(const AudioFrame& frame) {
    if (frame.sample_count() < fft_size_) return;
    
    std::lock_guard<std::mutex> lock(spectrum_mutex_);
    
    // Fill FFT buffer with mono sum
    for (size_t i = 0; i < fft_size_ && i < frame.sample_count(); ++i) {
        float left = frame.get_sample_as_float(0, static_cast<uint32_t>(i));
        float right = frame.channel_count() > 1 ? 
                     frame.get_sample_as_float(1, static_cast<uint32_t>(i)) : left;
        
        fft_buffer_[i] = std::complex<float>((left + right) * 0.5f, 0.0f);
    }
    
    apply_window_function();
    compute_fft();
}

void ProfessionalAudioScopes::compute_fft() {
    // Simple DFT implementation (for production, use FFTW or similar)
    for (size_t k = 0; k < fft_size_ / 2; ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        
        for (size_t n = 0; n < fft_size_; ++n) {
            float angle = -2.0f * static_cast<float>(M_PI) * k * n / fft_size_;
            std::complex<float> w(std::cos(angle), std::sin(angle));
            sum += fft_buffer_[n] * w;
        }
        
        double magnitude_db = 20.0 * std::log10(std::max(static_cast<double>(std::abs(sum)) / fft_size_, 1e-10));
        spectrum_data_.magnitudes_db[k] = magnitude_db;
        
        // Update peak hold
        if (magnitude_db > spectrum_data_.peak_hold_db[k]) {
            spectrum_data_.peak_hold_db[k] = magnitude_db;
        }
    }
}

void ProfessionalAudioScopes::apply_window_function() {
    for (size_t i = 0; i < fft_size_; ++i) {
        fft_buffer_[i] *= window_function_[i];
    }
}

void ProfessionalAudioScopes::generate_window_function() {
    window_function_.resize(fft_size_);
    
    // Hann window
    for (size_t i = 0; i < fft_size_; ++i) {
        window_function_[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (fft_size_ - 1)));
    }
}

// ============================================================================
// Unified Professional Audio Monitoring System Implementation
// ============================================================================

ProfessionalAudioMonitoringSystem::ProfessionalAudioMonitoringSystem(const MonitoringConfig& config)
    : config_(config)
{
    processing_times_ms_.reserve(1000); // Keep last 1000 measurements
}

bool ProfessionalAudioMonitoringSystem::initialize(double sample_rate, uint16_t channels) {
    try {
        if (config_.enable_loudness_monitoring) {
            loudness_monitor_ = std::make_unique<EnhancedEBUR128Monitor>(sample_rate, channels);
            loudness_monitor_->initialize();
            loudness_monitor_->set_target_platform(config_.target_platform);
        }
        
        if (config_.enable_peak_rms_meters) {
            meter_system_ = std::make_unique<ProfessionalMeterSystem>(channels, sample_rate);
            meter_system_->set_global_reference_level(config_.reference_level_db);
        }
        
        if (config_.enable_audio_scopes) {
            scopes_ = std::make_unique<ProfessionalAudioScopes>(sample_rate, channels);
        }
        
        initialized_ = true;
        last_update_ = std::chrono::steady_clock::now();
        
        ve::log::info("Professional Audio Monitoring System initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        ve::log::error("Failed to initialize Professional Audio Monitoring System: " + std::string(e.what()));
        return false;
    }
}

void ProfessionalAudioMonitoringSystem::shutdown() {
    loudness_monitor_.reset();
    meter_system_.reset();
    scopes_.reset();
    initialized_ = false;
    
    ve::log::info("Professional Audio Monitoring System shut down");
}

void ProfessionalAudioMonitoringSystem::reset_all() {
    if (loudness_monitor_) loudness_monitor_->reset();
    if (meter_system_) meter_system_->reset_meters();
    if (scopes_) scopes_->reset_scopes();
    
    std::lock_guard<std::mutex> lock(perf_mutex_);
    processing_times_ms_.clear();
    frames_processed_ = 0;
}

void ProfessionalAudioMonitoringSystem::configure(const MonitoringConfig& config) {
    config_ = config;
    
    if (loudness_monitor_) {
        loudness_monitor_->set_target_platform(config_.target_platform);
    }
    
    if (meter_system_) {
        meter_system_->set_global_reference_level(config_.reference_level_db);
    }
}

void ProfessionalAudioMonitoringSystem::set_target_platform(const std::string& platform) {
    config_.target_platform = platform;
    if (loudness_monitor_) {
        loudness_monitor_->set_target_platform(platform);
    }
}

void ProfessionalAudioMonitoringSystem::process_audio_frame(const AudioFrame& frame) {
    ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - ENTRY");
    
    if (!initialized_) {
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Not initialized, returning early");
        return;
    }
    ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Initialization check passed");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process through all enabled monitoring components
    ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - About to check loudness_monitor_");
    
    // PHASE G: Completely disable loudness monitor to test stack corruption theory
    // Skip ALL loudness monitor processing to isolate the crash source
    if (false && loudness_monitor_) {  // PHASE G: Force disable loudness monitor
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Loudness monitor enabled, processing samples");
        loudness_monitor_->process_samples(frame);
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Loudness monitor processing completed");
    } else {
        ve::log::info("PHASE G: Loudness monitor DISABLED - testing if this eliminates stack corruption crash");
    }
    
    ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - About to check meter_system_");
    if (meter_system_) {
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Meter system enabled, processing samples");
        meter_system_->process_samples(frame);
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Meter system processing completed");
    } else {
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Meter system disabled");
    }
    
    ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - About to check scopes_");
    if (scopes_) {
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Scopes enabled, processing samples");
        scopes_->process_samples(frame);
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Scopes processing completed");
    } else {
        ve::log::info("ProfessionalAudioMonitoringSystem::process_audio_frame - Scopes disabled");
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double processing_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    update_performance_stats(processing_time_ms);
    frames_processed_++;
}

ProfessionalAudioMonitoringSystem::SystemStatus ProfessionalAudioMonitoringSystem::get_system_status() const {
    return analyze_system_status();
}

ProfessionalAudioMonitoringSystem::PerformanceStats ProfessionalAudioMonitoringSystem::get_performance_stats() const {
    std::lock_guard<std::mutex> lock(perf_mutex_);
    
    PerformanceStats stats;
    stats.frames_processed = frames_processed_;
    
    if (!processing_times_ms_.empty()) {
        stats.avg_processing_time_ms = std::accumulate(processing_times_ms_.begin(), 
                                                      processing_times_ms_.end(), 0.0) / 
                                      processing_times_ms_.size();
        
        stats.max_processing_time_ms = *std::max_element(processing_times_ms_.begin(), 
                                                        processing_times_ms_.end());
        
        // Estimate CPU usage (very rough)
        double frame_rate = 30.0; // Assume 30fps for estimation
        double expected_frame_time_ms = 1000.0 / frame_rate;
        stats.cpu_usage_percent = (stats.avg_processing_time_ms / expected_frame_time_ms) * 100.0;
    }
    
    return stats;
}

void ProfessionalAudioMonitoringSystem::update_performance_stats(double processing_time_ms) {
    std::lock_guard<std::mutex> lock(perf_mutex_);
    
    processing_times_ms_.push_back(processing_time_ms);
    
    // Keep only recent measurements
    if (processing_times_ms_.size() > 1000) {
        processing_times_ms_.erase(processing_times_ms_.begin());
    }
}

ProfessionalAudioMonitoringSystem::SystemStatus ProfessionalAudioMonitoringSystem::analyze_system_status() const {
    SystemStatus status;
    
    // Check loudness compliance
    if (loudness_monitor_) {
        status.broadcast_compliant = loudness_monitor_->is_broadcast_compliant();
        auto warnings = loudness_monitor_->get_compliance_warnings();
        status.warnings.insert(status.warnings.end(), warnings.begin(), warnings.end());
    }
    
    // Check for meter overloads
    if (meter_system_) {
        if (meter_system_->any_channel_overload()) {
            status.warnings.push_back("Audio overload detected on one or more channels");
        }
    }
    
    // Check scope warnings
    if (scopes_) {
        auto scope_warnings = scopes_->get_scope_warnings();
        status.warnings.insert(status.warnings.end(), scope_warnings.begin(), scope_warnings.end());
    }
    
    // Generate recommendations
    if (!status.broadcast_compliant) {
        status.recommendations.push_back("Adjust audio levels to meet broadcast standards");
    }
    
    if (meter_system_ && meter_system_->any_channel_overload()) {
        status.recommendations.push_back("Reduce input gain to prevent clipping");
    }
    
    // Calculate overall quality score (0.0 to 1.0)
    status.overall_quality_score = 1.0;
    if (!status.broadcast_compliant) status.overall_quality_score -= 0.5;
    if (meter_system_ && meter_system_->any_channel_overload()) status.overall_quality_score -= 0.3;
    status.overall_quality_score -= status.warnings.size() * 0.1;
    status.overall_quality_score = std::max(0.0, status.overall_quality_score);
    
    return status;
}

} // namespace ve::audio