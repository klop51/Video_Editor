/**
 * @file audio_effects.hpp
 * @brief Professional Audio Effects Suite for Video Editor
 * 
 * Phase 2 Week 5: class EffectNode : public AudioNode {
public:
    EffectNode(NodeID id, EffectType type, const std::string& name);
    virtual ~EffectNode() = default; Audio Effects Implementation
 * - 4-band parametric EQ with Q control
 * - Professional compressor with attack/release/ratio
 * - Noise gate with hysteresis  
 * - Peak limiter for output protection
 * - SIMD-optimized processing for real-time performance
 */

#pragma once

#include <memory>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

// Core includes
#include "core/time.hpp"
#include "core/log.hpp"

// Audio includes
#include "audio/audio_frame.hpp"
#include "audio/mixing_graph.hpp"

namespace ve::audio {

// Forward declarations
class EffectNode;
class EQNode;
class CompressorNode;
class GateNode; 
class LimiterNode;

/**
 * @brief Audio effect types enumeration
 */
enum class EffectType : uint8_t {
    EQ_4BAND,           ///< 4-band parametric equalizer
    COMPRESSOR,         ///< Professional dynamics compressor
    NOISE_GATE,         ///< Noise gate with hysteresis
    PEAK_LIMITER,       ///< Peak limiter for output protection
    UNKNOWN
};

/**
 * @brief Effect parameter value with smooth interpolation
 */
struct EffectParameter {
    std::atomic<float> target_value{0.0f};
    std::atomic<float> current_value{0.0f};
    float min_value{0.0f};
    float max_value{1.0f};
    float default_value{0.0f};
    float smoothing_factor{0.99f};  // For parameter smoothing
    std::string name;
    std::string unit;
    
    EffectParameter() = default;
    
    EffectParameter(float min_val, float max_val, float default_val, 
                   const std::string& param_name, const std::string& param_unit = "")
        : min_value(min_val), max_value(max_val), default_value(default_val), 
          name(param_name), unit(param_unit) {
        target_value.store(default_val);
        current_value.store(default_val);
    }
    
    // Set target value with bounds checking
    void set_value(float value) {
        value = std::clamp(value, min_value, max_value);
        target_value.store(value);
    }
    
    // Get current smoothed value
    float get_value() {
        float current = current_value.load();
        float target = target_value.load();
        
        if (std::abs(current - target) > 1e-6f) {
            // Smooth interpolation to prevent zipper noise
            current = current * smoothing_factor + target * (1.0f - smoothing_factor);
            current_value.store(current);
        }
        
        return current;
    }
};

/**
 * @brief Effect processing statistics
 */
struct EffectStats {
    std::atomic<uint64_t> samples_processed{0};
    std::atomic<uint64_t> processing_time_ns{0};
    std::atomic<float> cpu_usage_percent{0.0f};
    std::atomic<bool> bypass_state{false};
    
    void update_stats(uint64_t samples, uint64_t time_ns) {
        samples_processed.fetch_add(samples);
        processing_time_ns.fetch_add(time_ns);
        
        // Calculate approximate CPU usage based on real-time constraint
        double sample_time_ns = (double)samples * 1000000000.0 / 48000.0; // Assume 48kHz
        cpu_usage_percent.store((float)(time_ns / sample_time_ns * 100.0));
    }
};

/**
 * @brief Base class for audio effects that integrate with mixing graph
 * 
 * EffectNode extends AudioNode to provide audio effects processing
 * capabilities within the node-based mixing architecture.
 */
class EffectNode : public AudioNode {
public:
    explicit EffectNode(NodeID id, EffectType type, const std::string& name);
    virtual ~EffectNode() = default;
    
    // AudioNode interface implementation
    bool process(
        const std::vector<std::shared_ptr<AudioFrame>>& inputs,
        std::vector<std::shared_ptr<AudioFrame>>& outputs,
        const TimePoint& timestamp
    ) override;
    
    uint16_t get_input_count() const override { return 1; }
    uint16_t get_output_count() const override { return 1; }
    bool configure(const AudioProcessingParams& params) override;
    
    // Effect-specific interface
    virtual bool process_effect(
        const std::shared_ptr<AudioFrame>& input,
        std::shared_ptr<AudioFrame>& output
    ) = 0;
    
    // Effect control
    virtual void set_parameter(const std::string& name, float value);
    virtual float get_parameter(const std::string& name) const;
    virtual std::vector<std::string> get_parameter_names() const;
    
    // Bypass control
    void set_bypass(bool bypass) { bypass_.store(bypass); }
    bool is_bypassed() const { return bypass_.load(); }
    
    // Effect information
    EffectType get_effect_type() const { return effect_type_; }
    const EffectStats& get_effect_stats() const { return stats_; }
    
    // Preset management
    virtual bool save_preset(const std::string& name) = 0;
    virtual bool load_preset(const std::string& name) = 0;

protected:
    // Helper for parameter management
    void register_parameter(const std::string& name, 
                          float min_val, float max_val, float default_val,
                          const std::string& unit = "");
    
    EffectParameter* get_parameter_ptr(const std::string& name);
    const EffectParameter* get_parameter_ptr(const std::string& name) const;

private:
    EffectType effect_type_;
    std::atomic<bool> bypass_{false};
    std::unordered_map<std::string, std::unique_ptr<EffectParameter>> parameters_;
    mutable std::mutex parameters_mutex_;
    EffectStats stats_;
};

/**
 * @brief 4-Band Parametric Equalizer
 * 
 * Professional quality EQ with independent control over:
 * - Low shelf (20-200 Hz)
 * - Low mid (200-2000 Hz) 
 * - High mid (2000-8000 Hz)
 * - High shelf (8000-20000 Hz)
 */
class EQNode : public EffectNode {
public:
    explicit EQNode(NodeID id, const std::string& name);
    ~EQNode() override = default;
    
    bool process_effect(
        const std::shared_ptr<AudioFrame>& input,
        std::shared_ptr<AudioFrame>& output
    ) override;
    
    bool save_preset(const std::string& name) override;
    bool load_preset(const std::string& name) override;
    
    // EQ-specific controls
    void set_band_gain(int band, float gain_db);         // -20 to +20 dB
    void set_band_frequency(int band, float freq_hz);    // Band center frequency
    void set_band_q_factor(int band, float q);           // 0.1 to 10.0
    void set_band_enabled(int band, bool enabled);
    
    float get_band_gain(int band) const;
    float get_band_frequency(int band) const;
    float get_band_q_factor(int band) const;
    bool is_band_enabled(int band) const;

private:
    static constexpr int NUM_BANDS = 4;
    
    // Biquad filter state for each band and channel
    struct BiquadFilter {
        // Filter coefficients
        float a0, a1, a2, b1, b2;
        // Filter state (per channel)
        std::array<float, 2> x1{0.0f, 0.0f};  // Previous input samples
        std::array<float, 2> x2{0.0f, 0.0f};
        std::array<float, 2> y1{0.0f, 0.0f};  // Previous output samples  
        std::array<float, 2> y2{0.0f, 0.0f};
        
        BiquadFilter() : a0(1.0f), a1(0.0f), a2(0.0f), b1(0.0f), b2(0.0f) {}
        
        // Process single sample for given channel
        float process_sample(float input, int channel);
    };
    
    std::array<BiquadFilter, NUM_BANDS> filters_;
    std::array<bool, NUM_BANDS> band_enabled_{true, true, true, true};
    
    // Update filter coefficients when parameters change
    void update_filter_coefficients(int band);
    void calculate_biquad_coefficients(int band, float freq, float gain_db, float q, 
                                     float& a0, float& a1, float& a2, float& b1, float& b2);
};

/**
 * @brief Professional Audio Compressor
 * 
 * High-quality dynamics processor with:
 * - Variable threshold, ratio, attack, release
 * - Knee control (hard/soft)
 * - Make-up gain
 * - Side-chain input support
 */
class CompressorNode : public EffectNode {
public:
    explicit CompressorNode(NodeID id, const std::string& name);
    ~CompressorNode() override = default;
    
    bool process_effect(
        const std::shared_ptr<AudioFrame>& input,
        std::shared_ptr<AudioFrame>& output
    ) override;
    
    bool save_preset(const std::string& name) override;
    bool load_preset(const std::string& name) override;
    
    // Compressor-specific controls
    void set_threshold(float threshold_db);      // -60 to 0 dB
    void set_ratio(float ratio);                 // 1:1 to 20:1
    void set_attack_time(float attack_ms);       // 0.1 to 100 ms
    void set_release_time(float release_ms);     // 10 to 1000 ms
    void set_knee_width(float knee_db);          // 0 to 10 dB
    void set_makeup_gain(float gain_db);         // 0 to 20 dB
    
    float get_gain_reduction() const;            // Current GR in dB

private:
    // Envelope detector state
    float envelope_follower_{0.0f};
    float gain_reduction_{0.0f};
    
    // Attack/release coefficients (updated when times change)
    float attack_coeff_{0.0f};
    float release_coeff_{0.0f};
    
    void update_time_constants();
    float compute_gain_reduction(float input_level_db);
    float apply_knee(float input_db, float threshold_db, float knee_db);
};

/**
 * @brief Noise Gate with Hysteresis
 * 
 * Intelligent noise gate that eliminates background noise while
 * preserving natural attack and decay of wanted signals.
 */
class GateNode : public EffectNode {
public:
    explicit GateNode(NodeID id, const std::string& name);
    ~GateNode() override = default;
    
    bool process_effect(
        const std::shared_ptr<AudioFrame>& input,
        std::shared_ptr<AudioFrame>& output
    ) override;
    
    bool save_preset(const std::string& name) override;
    bool load_preset(const std::string& name) override;
    
    // Gate-specific controls
    void set_threshold(float threshold_db);      // -80 to -10 dB
    void set_ratio(float ratio);                 // 1:2 to 1:100 (downward expansion)
    void set_attack_time(float attack_ms);       // 0.1 to 10 ms
    void set_hold_time(float hold_ms);           // 0 to 1000 ms
    void set_release_time(float release_ms);     // 10 to 5000 ms
    void set_hysteresis(float hysteresis_db);    // 1 to 10 dB

private:
    enum class GateState {
        CLOSED,
        OPENING, 
        OPEN,
        HOLDING,
        CLOSING
    };
    
    GateState gate_state_{GateState::CLOSED};
    float envelope_follower_{0.0f};
    float gate_gain_{0.0f};
    uint32_t hold_counter_{0};
    uint32_t hold_time_samples_{0};
    
    // Time constant coefficients
    float attack_coeff_{0.0f};
    float release_coeff_{0.0f};
    
    void update_time_constants();
    void update_gate_state(float input_level_db);
};

/**
 * @brief Peak Limiter for Output Protection
 * 
 * Transparent limiting to prevent digital clipping while
 * maintaining audio quality and dynamics.
 */
class LimiterNode : public EffectNode {
public:
    explicit LimiterNode(NodeID id, const std::string& name);
    ~LimiterNode() override = default;
    
    bool process_effect(
        const std::shared_ptr<AudioFrame>& input,
        std::shared_ptr<AudioFrame>& output
    ) override;
    
    bool save_preset(const std::string& name) override;
    bool load_preset(const std::string& name) override;
    
    // Limiter-specific controls
    void set_threshold(float threshold_db);      // -20 to 0 dB
    void set_release_time(float release_ms);     // 1 to 100 ms
    void set_lookahead_time(float lookahead_ms); // 0 to 10 ms
    
    float get_gain_reduction() const;            // Current GR in dB

private:
    static constexpr size_t MAX_LOOKAHEAD_SAMPLES = 1024;
    
    // Lookahead delay buffer
    std::array<std::array<float, MAX_LOOKAHEAD_SAMPLES>, 2> delay_buffer_;
    size_t delay_write_pos_{0};
    size_t lookahead_samples_{0};
    
    // Envelope follower
    float envelope_peak_{0.0f};
    float gain_reduction_{0.0f};
    float release_coeff_{0.0f};
    
    void update_lookahead_delay();
    void update_release_coeff();
    float detect_peak_ahead(size_t channel, size_t samples_ahead);
};

/**
 * @brief Effect factory for creating audio effects
 */
class EffectFactory {
public:
    static std::unique_ptr<EQNode> create_eq_node(NodeID id, const std::string& name);
    static std::unique_ptr<CompressorNode> create_compressor_node(NodeID id, const std::string& name);
    static std::unique_ptr<GateNode> create_gate_node(NodeID id, const std::string& name);
    static std::unique_ptr<LimiterNode> create_limiter_node(NodeID id, const std::string& name);
    
    // Create effect chain (effects in series)
    static std::vector<std::unique_ptr<EffectNode>> create_standard_chain(NodeID start_id, const std::string& base_name);
    static std::vector<std::unique_ptr<EffectNode>> create_vocal_chain(NodeID start_id, const std::string& base_name);
    static std::vector<std::unique_ptr<EffectNode>> create_instrument_chain(NodeID start_id, const std::string& base_name);
};

/**
 * @brief Audio effects utilities for SIMD optimization and DSP
 */
namespace effects_utils {

// SIMD-optimized audio processing functions
void apply_gain_simd(float* audio_data, uint32_t sample_count, float gain);
void mix_buffers_simd(const float* input1, const float* input2, float* output, uint32_t sample_count);
void copy_with_gain_simd(const float* input, float* output, uint32_t sample_count, float gain);

// DSP utility functions
float db_to_linear(float db);
float linear_to_db(float linear);
float ms_to_samples(float ms, uint32_t sample_rate);
float samples_to_ms(uint32_t samples, uint32_t sample_rate);

// RMS and peak detection
float calculate_rms(const float* audio_data, uint32_t sample_count);
float calculate_peak(const float* audio_data, uint32_t sample_count);
float calculate_rms_simd(const float* audio_data, uint32_t sample_count);
float calculate_peak_simd(const float* audio_data, uint32_t sample_count);

} // namespace effects_utils

} // namespace ve::audio