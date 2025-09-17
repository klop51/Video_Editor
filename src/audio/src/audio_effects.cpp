/**
 * @file audio_effects.cpp
 * @brief Professional Audio Effects Suite Implementation
 * 
 * Phase 2 Week 5: Core Audio Effects Implementation
 */

#define _USE_MATH_DEFINES
#include <cmath>

#include "audio/audio_effects.hpp"
#include "core/log.hpp"

#include <immintrin.h>  // For AVX/AVX2 intrinsics
#include <chrono>
#include <cstring>

namespace ve::audio {

//-----------------------------------------------------------------------------
// EffectNode Base Class Implementation
//-----------------------------------------------------------------------------

EffectNode::EffectNode(NodeID id, EffectType type, const std::string& name)
    : AudioNode(id, NodeType::Effect, name), effect_type_(type) {
    ve::log::info("Created effect node '" + name + "' of type " + std::to_string(static_cast<int>(type)));
}

bool EffectNode::process(
    const std::vector<std::shared_ptr<AudioFrame>>& inputs,
    std::vector<std::shared_ptr<AudioFrame>>& outputs,
    const TimePoint& timestamp) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Validate inputs
    if (inputs.empty() || !inputs[0]) {
        return false;
    }
    
    auto input = inputs[0];
    
    // Check if bypassed
    if (bypass_.load()) {
        outputs.push_back(input);  // Pass through unchanged
        return true;
    }
    
    // Create output frame
    auto output = AudioFrame::create(
        input->sample_rate(), input->channel_count(), 
        input->sample_count(), input->format(), timestamp
    );
    
    if (!output) {
        return false;
    }
    
    // Process effect
    bool success = process_effect(input, output);
    
    if (success) {
        outputs.push_back(output);
    }
    
    // Update performance statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    stats_.update_stats(input->sample_count(), duration.count());
    
    return success;
}

bool EffectNode::configure(const AudioProcessingParams& params) {
    if (!AudioNode::configure(params)) {
        return false;
    }
    
    ve::log::info("Configured effect node '" + get_name() + "' - " +
                  std::to_string(params.sample_rate) + "Hz, " +
                  std::to_string(params.channels) + " channels");
    return true;
}

void EffectNode::set_parameter(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    auto it = parameters_.find(name);
    if (it != parameters_.end()) {
        it->second->set_value(value);
    } else {
        ve::log::warn("Effect parameter '" + name + "' not found in " + get_name());
    }
}

float EffectNode::get_parameter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    auto it = parameters_.find(name);
    if (it != parameters_.end()) {
        return it->second->get_value();
    }
    return 0.0f;
}

std::vector<std::string> EffectNode::get_parameter_names() const {
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    std::vector<std::string> names;
    names.reserve(parameters_.size());
    for (const auto& pair : parameters_) {
        names.push_back(pair.first);
    }
    return names;
}

void EffectNode::register_parameter(const std::string& name, 
                                  float min_val, float max_val, float default_val,
                                  const std::string& unit) {
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    parameters_[name] = std::make_unique<EffectParameter>(min_val, max_val, default_val, name, unit);
}

EffectParameter* EffectNode::get_parameter_ptr(const std::string& name) {
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    auto it = parameters_.find(name);
    return (it != parameters_.end()) ? it->second.get() : nullptr;
}

const EffectParameter* EffectNode::get_parameter_ptr(const std::string& name) const {
    std::lock_guard<std::mutex> lock(parameters_mutex_);
    auto it = parameters_.find(name);
    return (it != parameters_.end()) ? it->second.get() : nullptr;
}

//-----------------------------------------------------------------------------
// EQNode Implementation  
//-----------------------------------------------------------------------------

EQNode::EQNode(NodeID id, const std::string& name) : EffectNode(id, EffectType::EQ_4BAND, name) {
    // Register parameters for 4 bands
    for (int band = 0; band < NUM_BANDS; ++band) {
        std::string band_str = std::to_string(band + 1);
        
        register_parameter("band" + band_str + "_gain", -20.0f, 20.0f, 0.0f, "dB");
        register_parameter("band" + band_str + "_freq", 20.0f, 20000.0f, 
                         (band == 0) ? 100.0f : (band == 1) ? 500.0f : (band == 2) ? 2000.0f : 8000.0f, "Hz");
        register_parameter("band" + band_str + "_q", 0.1f, 10.0f, 0.707f, "Q");
        register_parameter("band" + band_str + "_enabled", 0.0f, 1.0f, 1.0f, "");
    }
    
    // Initialize filter coefficients
    for (int band = 0; band < NUM_BANDS; ++band) {
        update_filter_coefficients(band);
    }
}

bool EQNode::process_effect(
    const std::shared_ptr<AudioFrame>& input,
    std::shared_ptr<AudioFrame>& output) {
    
    const float* input_data = static_cast<const float*>(input->data());
    float* output_data = static_cast<float*>(output->data());
    
    uint32_t sample_count = input->sample_count();
    uint16_t channel_count = input->channel_count();
    
    // Copy input to output first
    std::memcpy(output_data, input_data, sample_count * channel_count * sizeof(float));
    
    // Apply each enabled EQ band
    for (int band = 0; band < NUM_BANDS; ++band) {
        if (!band_enabled_[band]) continue;
        
        // Update filter coefficients if parameters changed
        update_filter_coefficients(band);
        
        // Process each channel
        for (uint16_t ch = 0; ch < channel_count; ++ch) {
            for (uint32_t sample = 0; sample < sample_count; ++sample) {
                size_t index = sample * channel_count + ch;
                output_data[index] = filters_[band].process_sample(output_data[index], ch);
            }
        }
    }
    
    return true;
}

float EQNode::BiquadFilter::process_sample(float input, int channel) {
    if (channel >= 2) return input;  // Safety check
    
    // Direct Form II Transposed implementation
    float output = a0 * input + x1[channel];
    x1[channel] = a1 * input - b1 * output + x2[channel];
    x2[channel] = a2 * input - b2 * output;
    
    return output;
}

void EQNode::update_filter_coefficients(int band) {
    if (band >= NUM_BANDS) return;
    
    std::string band_str = std::to_string(band + 1);
    float freq = get_parameter("band" + band_str + "_freq");
    float gain_db = get_parameter("band" + band_str + "_gain");
    float q = get_parameter("band" + band_str + "_q");
    
    band_enabled_[band] = (get_parameter("band" + band_str + "_enabled") > 0.5f);
    
    if (!band_enabled_[band]) return;
    
    calculate_biquad_coefficients(band, freq, gain_db, q,
                                filters_[band].a0, filters_[band].a1, filters_[band].a2,
                                filters_[band].b1, filters_[band].b2);
}

void EQNode::calculate_biquad_coefficients(int band, float freq, float gain_db, float q,
                                         float& a0, float& a1, float& a2, float& b1, float& b2) {
    // Get sample rate from current configuration
    float sample_rate = 48000.0f; // TODO: Get from actual configuration
    
    float A = std::powf(10.0f, gain_db / 40.0f);
    float omega = 2.0f * static_cast<float>(M_PI) * freq / sample_rate;
    float sin_omega = std::sinf(omega);
    float cos_omega = std::cosf(omega);
    float alpha = sin_omega / (2.0f * q);
    
    // Peaking EQ filter coefficients
    float b0 = 1.0f + alpha * A;
    float b1_coeff = -2.0f * cos_omega;
    float b2_coeff = 1.0f - alpha * A;
    float a0_coeff = 1.0f + alpha / A;
    float a1_coeff = -2.0f * cos_omega;
    float a2_coeff = 1.0f - alpha / A;
    
    // Normalize coefficients
    a0 = b0 / a0_coeff;
    a1 = b1_coeff / a0_coeff;
    a2 = b2_coeff / a0_coeff;
    b1 = a1_coeff / a0_coeff;
    b2 = a2_coeff / a0_coeff;
}

void EQNode::set_band_gain(int band, float gain_db) {
    if (band >= 0 && band < NUM_BANDS) {
        set_parameter("band" + std::to_string(band + 1) + "_gain", gain_db);
    }
}

void EQNode::set_band_frequency(int band, float freq_hz) {
    if (band >= 0 && band < NUM_BANDS) {
        set_parameter("band" + std::to_string(band + 1) + "_freq", freq_hz);
    }
}

void EQNode::set_band_q_factor(int band, float q) {
    if (band >= 0 && band < NUM_BANDS) {
        set_parameter("band" + std::to_string(band + 1) + "_q", q);
    }
}

void EQNode::set_band_enabled(int band, bool enabled) {
    if (band >= 0 && band < NUM_BANDS) {
        set_parameter("band" + std::to_string(band + 1) + "_enabled", enabled ? 1.0f : 0.0f);
        band_enabled_[band] = enabled;
    }
}

float EQNode::get_band_gain(int band) const {
    if (band >= 0 && band < NUM_BANDS) {
        return get_parameter("band" + std::to_string(band + 1) + "_gain");
    }
    return 0.0f;
}

float EQNode::get_band_frequency(int band) const {
    if (band >= 0 && band < NUM_BANDS) {
        return get_parameter("band" + std::to_string(band + 1) + "_freq");
    }
    return 0.0f;
}

float EQNode::get_band_q_factor(int band) const {
    if (band >= 0 && band < NUM_BANDS) {
        return get_parameter("band" + std::to_string(band + 1) + "_q");
    }
    return 0.0f;
}

bool EQNode::is_band_enabled(int band) const {
    if (band >= 0 && band < NUM_BANDS) {
        return get_parameter("band" + std::to_string(band + 1) + "_enabled") > 0.5f;
    }
    return false;
}

bool EQNode::save_preset(const std::string& name) {
    // TODO: Implement preset saving
    ve::log::info("Saving EQ preset: " + name);
    return true;
}

bool EQNode::load_preset(const std::string& name) {
    // TODO: Implement preset loading
    ve::log::info("Loading EQ preset: " + name);
    return true;
}

//-----------------------------------------------------------------------------
// CompressorNode Implementation
//-----------------------------------------------------------------------------

CompressorNode::CompressorNode(NodeID id, const std::string& name) : EffectNode(id, EffectType::COMPRESSOR, name) {
    register_parameter("threshold", -60.0f, 0.0f, -12.0f, "dB");
    register_parameter("ratio", 1.0f, 20.0f, 4.0f, ":1");
    register_parameter("attack", 0.1f, 100.0f, 2.0f, "ms");
    register_parameter("release", 10.0f, 1000.0f, 100.0f, "ms");
    register_parameter("knee", 0.0f, 10.0f, 2.0f, "dB");
    register_parameter("makeup", 0.0f, 20.0f, 0.0f, "dB");
    
    update_time_constants();
}

bool CompressorNode::process_effect(
    const std::shared_ptr<AudioFrame>& input,
    std::shared_ptr<AudioFrame>& output) {
    
    const float* input_data = static_cast<const float*>(input->data());
    float* output_data = static_cast<float*>(output->data());
    
    uint32_t sample_count = input->sample_count();
    uint16_t channel_count = input->channel_count();
    
    // Update time constants if changed
    update_time_constants();
    
    float makeup_gain = effects_utils::db_to_linear(get_parameter("makeup"));
    
    for (uint32_t sample = 0; sample < sample_count; ++sample) {
        // Calculate RMS level for this sample (simplified)
        float sum_squares = 0.0f;
        for (uint16_t ch = 0; ch < channel_count; ++ch) {
            float input_sample = input_data[sample * channel_count + ch];
            sum_squares += input_sample * input_sample;
        }
        
        float rms_level = std::sqrt(sum_squares / channel_count);
        float level_db = effects_utils::linear_to_db(rms_level);
        
        // Compute gain reduction
        float gr_db = compute_gain_reduction(level_db);
        
        // Smooth gain reduction with envelope follower
        float target_gr = std::abs(gr_db);
        if (target_gr > gain_reduction_) {
            // Attack
            gain_reduction_ = target_gr * (1.0f - attack_coeff_) + gain_reduction_ * attack_coeff_;
        } else {
            // Release
            gain_reduction_ = target_gr * (1.0f - release_coeff_) + gain_reduction_ * release_coeff_;
        }
        
        float gain_linear = effects_utils::db_to_linear(-gain_reduction_) * makeup_gain;
        
        // Apply gain to all channels
        for (uint16_t ch = 0; ch < channel_count; ++ch) {
            size_t index = sample * channel_count + ch;
            output_data[index] = input_data[index] * gain_linear;
        }
    }
    
    return true;
}

void CompressorNode::update_time_constants() {
    float sample_rate = 48000.0f; // TODO: Get from configuration
    float attack_ms = get_parameter("attack");
    float release_ms = get_parameter("release");
    
    attack_coeff_ = std::exp(-1.0f / (attack_ms * sample_rate * 0.001f));
    release_coeff_ = std::exp(-1.0f / (release_ms * sample_rate * 0.001f));
}

float CompressorNode::compute_gain_reduction(float input_level_db) {
    float threshold_db = get_parameter("threshold");
    float ratio = get_parameter("ratio");
    float knee_db = get_parameter("knee");
    
    if (input_level_db <= threshold_db) {
        return 0.0f; // No compression
    }
    
    // Apply knee
    float input_with_knee = apply_knee(input_level_db, threshold_db, knee_db);
    
    // Calculate gain reduction
    float over_threshold = input_with_knee - threshold_db;
    float compressed_over = over_threshold / ratio;
    float gain_reduction = over_threshold - compressed_over;
    
    return gain_reduction;
}

float CompressorNode::apply_knee(float input_db, float threshold_db, float knee_db) {
    if (knee_db <= 0.0f) {
        return input_db; // Hard knee
    }
    
    float knee_start = threshold_db - knee_db * 0.5f;
    float knee_end = threshold_db + knee_db * 0.5f;
    
    if (input_db < knee_start) {
        return input_db;
    } else if (input_db > knee_end) {
        return input_db;
    } else {
        // Soft knee interpolation
        float knee_ratio = (input_db - knee_start) / knee_db;
        float knee_curve = knee_ratio * knee_ratio;
        return input_db + knee_curve * (threshold_db - input_db);
    }
}

float CompressorNode::get_gain_reduction() const {
    return gain_reduction_;
}

bool CompressorNode::save_preset(const std::string& name) {
    ve::log::info("Saving compressor preset: " + name);
    return true;
}

bool CompressorNode::load_preset(const std::string& name) {
    ve::log::info("Loading compressor preset: " + name);
    return true;
}

//-----------------------------------------------------------------------------
// GateNode Implementation
//-----------------------------------------------------------------------------

GateNode::GateNode(NodeID id, const std::string& name) : EffectNode(id, EffectType::NOISE_GATE, name) {
    register_parameter("threshold", -80.0f, -10.0f, -40.0f, "dB");
    register_parameter("ratio", 2.0f, 100.0f, 10.0f, ":1");
    register_parameter("attack", 0.1f, 10.0f, 1.0f, "ms");
    register_parameter("hold", 0.0f, 1000.0f, 10.0f, "ms");
    register_parameter("release", 10.0f, 5000.0f, 100.0f, "ms");
    register_parameter("hysteresis", 1.0f, 10.0f, 3.0f, "dB");
    
    update_time_constants();
}

bool GateNode::process_effect(
    const std::shared_ptr<AudioFrame>& input,
    std::shared_ptr<AudioFrame>& output) {
    
    const float* input_data = static_cast<const float*>(input->data());
    float* output_data = static_cast<float*>(output->data());
    
    uint32_t sample_count = input->sample_count();
    uint16_t channel_count = input->channel_count();
    
    update_time_constants();
    
    for (uint32_t sample = 0; sample < sample_count; ++sample) {
        // Calculate RMS level
        float sum_squares = 0.0f;
        for (uint16_t ch = 0; ch < channel_count; ++ch) {
            float input_sample = input_data[sample * channel_count + ch];
            sum_squares += input_sample * input_sample;
        }
        
        float rms_level = std::sqrt(sum_squares / channel_count);
        float level_db = effects_utils::linear_to_db(rms_level);
        
        // Update gate state
        update_gate_state(level_db);
        
        // Update gate gain based on state
        float target_gain = 1.0f;
        switch (gate_state_) {
            case GateState::CLOSED:
                target_gain = 0.0f;
                break;
            case GateState::OPENING:
                gate_gain_ = gate_gain_ * attack_coeff_ + target_gain * (1.0f - attack_coeff_);
                break;
            case GateState::OPEN:
            case GateState::HOLDING:
                target_gain = 1.0f;
                gate_gain_ = target_gain;
                break;
            case GateState::CLOSING:
                target_gain = 0.0f;
                gate_gain_ = gate_gain_ * release_coeff_ + target_gain * (1.0f - release_coeff_);
                break;
        }
        
        // Apply gate gain to all channels
        for (uint16_t ch = 0; ch < channel_count; ++ch) {
            size_t index = sample * channel_count + ch;
            output_data[index] = input_data[index] * gate_gain_;
        }
    }
    
    return true;
}

void GateNode::update_time_constants() {
    float sample_rate = 48000.0f; // TODO: Get from configuration
    float attack_ms = get_parameter("attack");
    float release_ms = get_parameter("release");
    float hold_ms = get_parameter("hold");
    
    attack_coeff_ = std::exp(-1.0f / (attack_ms * sample_rate * 0.001f));
    release_coeff_ = std::exp(-1.0f / (release_ms * sample_rate * 0.001f));
    hold_time_samples_ = static_cast<uint32_t>(hold_ms * sample_rate * 0.001f);
}

void GateNode::update_gate_state(float input_level_db) {
    float threshold_db = get_parameter("threshold");
    float hysteresis_db = get_parameter("hysteresis");
    
    float open_threshold = threshold_db;
    float close_threshold = threshold_db - hysteresis_db;
    
    switch (gate_state_) {
        case GateState::CLOSED:
            if (input_level_db > open_threshold) {
                gate_state_ = GateState::OPENING;
            }
            break;
            
        case GateState::OPENING:
            if (input_level_db <= close_threshold) {
                gate_state_ = GateState::CLOSING;
            } else if (gate_gain_ >= 0.99f) {
                gate_state_ = GateState::OPEN;
                hold_counter_ = 0;
            }
            break;
            
        case GateState::OPEN:
            if (input_level_db <= close_threshold) {
                gate_state_ = GateState::HOLDING;
                hold_counter_ = 0;
            }
            break;
            
        case GateState::HOLDING:
            hold_counter_++;
            if (input_level_db > open_threshold) {
                gate_state_ = GateState::OPEN;
            } else if (hold_counter_ >= hold_time_samples_) {
                gate_state_ = GateState::CLOSING;
            }
            break;
            
        case GateState::CLOSING:
            if (input_level_db > open_threshold) {
                gate_state_ = GateState::OPENING;
            } else if (gate_gain_ <= 0.01f) {
                gate_state_ = GateState::CLOSED;
            }
            break;
    }
}

bool GateNode::save_preset(const std::string& name) {
    ve::log::info("Saving gate preset: " + name);
    return true;
}

bool GateNode::load_preset(const std::string& name) {
    ve::log::info("Loading gate preset: " + name);
    return true;
}

//-----------------------------------------------------------------------------
// LimiterNode Implementation 
//-----------------------------------------------------------------------------

LimiterNode::LimiterNode(NodeID id, const std::string& name) : EffectNode(id, EffectType::PEAK_LIMITER, name) {
    register_parameter("threshold", -20.0f, 0.0f, -0.1f, "dB");
    register_parameter("release", 1.0f, 100.0f, 10.0f, "ms");
    register_parameter("lookahead", 0.0f, 10.0f, 5.0f, "ms");
    
    // Initialize delay buffer
    delay_buffer_.fill({});
    
    update_lookahead_delay();
    update_release_coeff();
}

bool LimiterNode::process_effect(
    const std::shared_ptr<AudioFrame>& input,
    std::shared_ptr<AudioFrame>& output) {
    
    const float* input_data = static_cast<const float*>(input->data());
    float* output_data = static_cast<float*>(output->data());
    
    uint32_t sample_count = input->sample_count();
    uint16_t channel_count = input->channel_count();
    
    update_lookahead_delay();
    update_release_coeff();
    
    float threshold_linear = effects_utils::db_to_linear(get_parameter("threshold"));
    
    for (uint32_t sample = 0; sample < sample_count; ++sample) {
        // Store input in delay buffer
        for (uint16_t ch = 0; ch < std::min<uint16_t>(channel_count, 2); ++ch) {
            delay_buffer_[ch][delay_write_pos_] = input_data[sample * channel_count + ch];
        }
        
        // Detect peak ahead
        float peak_ahead = 0.0f;
        for (uint16_t ch = 0; ch < std::min<uint16_t>(channel_count, 2); ++ch) {
            float channel_peak = detect_peak_ahead(ch, lookahead_samples_);
            peak_ahead = std::max(peak_ahead, channel_peak);
        }
        
        // Calculate required gain reduction
        float target_gr = 0.0f;
        if (peak_ahead > threshold_linear) {
            target_gr = effects_utils::linear_to_db(threshold_linear / peak_ahead);
        }
        
        // Smooth gain reduction
        if (target_gr < gain_reduction_) {
            gain_reduction_ = target_gr; // Instant attack for limiting
        } else {
            gain_reduction_ = gain_reduction_ * release_coeff_ + target_gr * (1.0f - release_coeff_);
        }
        
        float gain_linear = effects_utils::db_to_linear(gain_reduction_);
        
        // Get delayed samples and apply limiting
        size_t read_pos = (delay_write_pos_ + MAX_LOOKAHEAD_SAMPLES - lookahead_samples_) % MAX_LOOKAHEAD_SAMPLES;
        
        for (uint16_t ch = 0; ch < channel_count; ++ch) {
            size_t index = sample * channel_count + ch;
            if (ch < 2) {
                output_data[index] = delay_buffer_[ch][read_pos] * gain_linear;
            } else {
                output_data[index] = input_data[index] * gain_linear; // For >2 channels
            }
        }
        
        // Advance write position
        delay_write_pos_ = (delay_write_pos_ + 1) % MAX_LOOKAHEAD_SAMPLES;
    }
    
    return true;
}

void LimiterNode::update_lookahead_delay() {
    float lookahead_ms = get_parameter("lookahead");
    float sample_rate = 48000.0f; // TODO: Get from configuration
    lookahead_samples_ = static_cast<size_t>(lookahead_ms * sample_rate * 0.001f);
    lookahead_samples_ = std::min(lookahead_samples_, MAX_LOOKAHEAD_SAMPLES - 1);
}

void LimiterNode::update_release_coeff() {
    float release_ms = get_parameter("release");
    float sample_rate = 48000.0f; // TODO: Get from configuration
    release_coeff_ = std::exp(-1.0f / (release_ms * sample_rate * 0.001f));
}

float LimiterNode::detect_peak_ahead(size_t channel, size_t samples_ahead) {
    if (channel >= 2) return 0.0f;
    
    float peak = 0.0f;
    for (size_t i = 0; i < samples_ahead; ++i) {
        size_t pos = (delay_write_pos_ + i) % MAX_LOOKAHEAD_SAMPLES;
        peak = std::max(peak, std::abs(delay_buffer_[channel][pos]));
    }
    return peak;
}

float LimiterNode::get_gain_reduction() const {
    return gain_reduction_;
}

bool LimiterNode::save_preset(const std::string& name) {
    ve::log::info("Saving limiter preset: " + name);
    return true;
}

bool LimiterNode::load_preset(const std::string& name) {
    ve::log::info("Loading limiter preset: " + name);
    return true;
}

//-----------------------------------------------------------------------------
// EffectFactory Implementation
//-----------------------------------------------------------------------------

std::unique_ptr<EQNode> EffectFactory::create_eq_node(NodeID id, const std::string& name) {
    return std::make_unique<EQNode>(id, name);
}

std::unique_ptr<CompressorNode> EffectFactory::create_compressor_node(NodeID id, const std::string& name) {
    return std::make_unique<CompressorNode>(id, name);
}

std::unique_ptr<GateNode> EffectFactory::create_gate_node(NodeID id, const std::string& name) {
    return std::make_unique<GateNode>(id, name);
}

std::unique_ptr<LimiterNode> EffectFactory::create_limiter_node(NodeID id, const std::string& name) {
    return std::make_unique<LimiterNode>(id, name);
}

std::vector<std::unique_ptr<EffectNode>> EffectFactory::create_standard_chain(NodeID start_id, const std::string& base_name) {
    std::vector<std::unique_ptr<EffectNode>> chain;
    chain.push_back(create_gate_node(start_id, base_name + "_Gate"));
    chain.push_back(create_eq_node(start_id + 1, base_name + "_EQ"));
    chain.push_back(create_compressor_node(start_id + 2, base_name + "_Compressor"));
    chain.push_back(create_limiter_node(start_id + 3, base_name + "_Limiter"));
    return chain;
}

std::vector<std::unique_ptr<EffectNode>> EffectFactory::create_vocal_chain(NodeID start_id, const std::string& base_name) {
    std::vector<std::unique_ptr<EffectNode>> chain;
    auto gate = create_gate_node(start_id, base_name + "_Gate");
    gate->set_parameter("threshold", -45.0f);
    chain.push_back(std::move(gate));
    
    auto eq = create_eq_node(start_id + 1, base_name + "_EQ");
    eq->set_band_frequency(0, 80.0f);   // High-pass for vocal clarity
    eq->set_band_gain(0, -6.0f);
    chain.push_back(std::move(eq));
    
    auto compressor = create_compressor_node(start_id + 2, base_name + "_Compressor");
    compressor->set_parameter("ratio", 3.0f);
    compressor->set_parameter("attack", 1.0f);
    chain.push_back(std::move(compressor));
    
    chain.push_back(create_limiter_node(start_id + 3, base_name + "_Limiter"));
    return chain;
}

std::vector<std::unique_ptr<EffectNode>> EffectFactory::create_instrument_chain(NodeID start_id, const std::string& base_name) {
    std::vector<std::unique_ptr<EffectNode>> chain;
    chain.push_back(create_eq_node(start_id, base_name + "_EQ"));
    
    auto compressor = create_compressor_node(start_id + 1, base_name + "_Compressor");
    compressor->set_parameter("ratio", 2.0f);
    compressor->set_parameter("attack", 5.0f);
    chain.push_back(std::move(compressor));
    
    chain.push_back(create_limiter_node(start_id + 2, base_name + "_Limiter"));
    return chain;
}

//-----------------------------------------------------------------------------
// Effects Utilities Implementation
//-----------------------------------------------------------------------------

namespace effects_utils {

void apply_gain_simd(float* audio_data, uint32_t sample_count, float gain) {
    const uint32_t simd_count = sample_count & ~7; // Process 8 samples at a time
    const __m256 gain_vec = _mm256_set1_ps(gain);
    
    for (uint32_t i = 0; i < simd_count; i += 8) {
        __m256 samples = _mm256_loadu_ps(&audio_data[i]);
        samples = _mm256_mul_ps(samples, gain_vec);
        _mm256_storeu_ps(&audio_data[i], samples);
    }
    
    // Handle remaining samples
    for (uint32_t i = simd_count; i < sample_count; ++i) {
        audio_data[i] *= gain;
    }
}

void mix_buffers_simd(const float* input1, const float* input2, float* output, uint32_t sample_count) {
    const uint32_t simd_count = sample_count & ~7;
    
    for (uint32_t i = 0; i < simd_count; i += 8) {
        __m256 samples1 = _mm256_loadu_ps(&input1[i]);
        __m256 samples2 = _mm256_loadu_ps(&input2[i]);
        __m256 result = _mm256_add_ps(samples1, samples2);
        _mm256_storeu_ps(&output[i], result);
    }
    
    for (uint32_t i = simd_count; i < sample_count; ++i) {
        output[i] = input1[i] + input2[i];
    }
}

void copy_with_gain_simd(const float* input, float* output, uint32_t sample_count, float gain) {
    const uint32_t simd_count = sample_count & ~7;
    const __m256 gain_vec = _mm256_set1_ps(gain);
    
    for (uint32_t i = 0; i < simd_count; i += 8) {
        __m256 samples = _mm256_loadu_ps(&input[i]);
        samples = _mm256_mul_ps(samples, gain_vec);
        _mm256_storeu_ps(&output[i], samples);
    }
    
    for (uint32_t i = simd_count; i < sample_count; ++i) {
        output[i] = input[i] * gain;
    }
}

float db_to_linear(float db) {
    return std::powf(10.0f, db / 20.0f);
}

float linear_to_db(float linear) {
    return 20.0f * std::log10(std::max(linear, 1e-10f));
}

float ms_to_samples(float ms, uint32_t sample_rate) {
    return ms * sample_rate * 0.001f;
}

float samples_to_ms(uint32_t samples, uint32_t sample_rate) {
    return static_cast<float>(samples) / sample_rate * 1000.0f;
}

float calculate_rms(const float* audio_data, uint32_t sample_count) {
    float sum_squares = 0.0f;
    for (uint32_t i = 0; i < sample_count; ++i) {
        sum_squares += audio_data[i] * audio_data[i];
    }
    return std::sqrt(sum_squares / sample_count);
}

float calculate_peak(const float* audio_data, uint32_t sample_count) {
    float peak = 0.0f;
    for (uint32_t i = 0; i < sample_count; ++i) {
        peak = std::max(peak, std::abs(audio_data[i]));
    }
    return peak;
}

float calculate_rms_simd(const float* audio_data, uint32_t sample_count) {
    const uint32_t simd_count = sample_count & ~7;
    __m256 sum_vec = _mm256_setzero_ps();
    
    for (uint32_t i = 0; i < simd_count; i += 8) {
        __m256 samples = _mm256_loadu_ps(&audio_data[i]);
        sum_vec = _mm256_fmadd_ps(samples, samples, sum_vec);
    }
    
    // Horizontal sum
    float sum_array[8];
    _mm256_storeu_ps(sum_array, sum_vec);
    float sum_squares = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3] +
                       sum_array[4] + sum_array[5] + sum_array[6] + sum_array[7];
    
    // Handle remaining samples
    for (uint32_t i = simd_count; i < sample_count; ++i) {
        sum_squares += audio_data[i] * audio_data[i];
    }
    
    return std::sqrt(sum_squares / sample_count);
}

float calculate_peak_simd(const float* audio_data, uint32_t sample_count) {
    const uint32_t simd_count = sample_count & ~7;
    __m256 max_vec = _mm256_setzero_ps();
    
    for (uint32_t i = 0; i < simd_count; i += 8) {
        __m256 samples = _mm256_loadu_ps(&audio_data[i]);
        __m256 abs_samples = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), samples);
        max_vec = _mm256_max_ps(max_vec, abs_samples);
    }
    
    // Horizontal max
    float max_array[8];
    _mm256_storeu_ps(max_array, max_vec);
    float peak = std::max({max_array[0], max_array[1], max_array[2], max_array[3],
                          max_array[4], max_array[5], max_array[6], max_array[7]});
    
    // Handle remaining samples
    for (uint32_t i = simd_count; i < sample_count; ++i) {
        peak = std::max(peak, std::abs(audio_data[i]));
    }
    
    return peak;
}

} // namespace effects_utils

} // namespace ve::audio