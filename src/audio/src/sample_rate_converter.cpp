#include "audio/sample_rate_converter.hpp"
#include <core/log.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include <cstring>
#include <sstream>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ve::audio {

/**
 * @brief Internal resampler state
 */
struct ResamplerState {
    // Conversion parameters
    double ratio = 1.0;
    double position = 0.0;
    
    // Filter parameters
    int filter_length = 0;
    std::vector<double> filter_coeffs;
    
    // Input buffer for streaming conversion
    std::vector<float> input_buffer;
    uint32_t buffer_used = 0;
    
    // Quality settings
    int sinc_table_size = 0;
    std::vector<double> sinc_table;
    
    // Channel configuration
    uint16_t channels = 2;
    
    ResamplerState() = default;
};

// Sinc function implementation
static double sinc(double x) {
    if (std::abs(x) < 1e-10) {
        return 1.0;
    }
    return std::sin(M_PI * x) / (M_PI * x);
}

// Windowed sinc function (Blackman window)
static double windowed_sinc(double x, int half_length) {
    if (std::abs(x) >= half_length) {
        return 0.0;
    }
    
    // Blackman window
    double window = 0.42 - 0.5 * std::cos(2.0 * M_PI * (x + half_length) / (2 * half_length))
                         + 0.08 * std::cos(4.0 * M_PI * (x + half_length) / (2 * half_length));
    
    return sinc(x) * window;
}

std::unique_ptr<SampleRateConverter> SampleRateConverter::create(const ResampleConfig& config) {
    auto converter = std::make_unique<SampleRateConverter>(config);
    if (converter->initialize() != AudioError::None) {
        return nullptr;
    }
    return converter;
}

SampleRateConverter::SampleRateConverter(const ResampleConfig& config)
    : config_(config), state_(std::make_unique<ResamplerState>()) {
}

SampleRateConverter::~SampleRateConverter() = default;

SampleRateConverter::SampleRateConverter(SampleRateConverter&&) noexcept = default;
SampleRateConverter& SampleRateConverter::operator=(SampleRateConverter&&) noexcept = default;

AudioError SampleRateConverter::initialize() {
    if (initialized_) {
        return AudioError::None;
    }
    
    // Validate configuration
    if (config_.input_sample_rate == 0 || config_.output_sample_rate == 0) {
        last_error_ = AudioError::ConfigurationError;
        ve::log::error("SampleRateConverter: Invalid sample rates");
        return last_error_;
    }
    
    if (config_.input_channels == 0 || config_.input_channels > 32) {
        last_error_ = AudioError::ConfigurationError;
        ve::log::error("SampleRateConverter: Invalid channel count");
        return last_error_;
    }
    
    // Initialize resampler state
    state_->ratio = static_cast<double>(config_.output_sample_rate) / 
                   static_cast<double>(config_.input_sample_rate);
    state_->channels = config_.input_channels;
    state_->position = 0.0;
    
    // Calculate filter parameters based on quality
    int base_filter_length = 0;
    switch (config_.quality) {
        case ResampleQuality::Fastest:
            base_filter_length = 8;  // Short filter for speed
            break;
        case ResampleQuality::Medium:
            base_filter_length = 32; // Balanced filter
            break;
        case ResampleQuality::Highest:
            base_filter_length = 128; // Long filter for quality
            break;
    }
    
    // Adjust filter length based on conversion ratio
    double min_ratio = std::min(1.0, state_->ratio);
    state_->filter_length = static_cast<int>(base_filter_length / min_ratio);
    if (state_->filter_length < 8) state_->filter_length = 8;
    if (state_->filter_length > 512) state_->filter_length = 512;
    
    // Ensure filter length is even
    if (state_->filter_length % 2 != 0) {
        state_->filter_length++;
    }
    
    // Pre-calculate sinc table for interpolation
    state_->sinc_table_size = 1024;
    state_->sinc_table.resize(state_->sinc_table_size * state_->filter_length);
    
    int half_length = state_->filter_length / 2;
    for (int i = 0; i < state_->sinc_table_size; ++i) {
        double fraction = static_cast<double>(i) / state_->sinc_table_size;
        
        for (int j = 0; j < state_->filter_length; ++j) {
            double x = (j - half_length) - fraction;
            if (state_->ratio < 1.0) {
                // Downsampling: apply anti-aliasing
                x *= state_->ratio;
                state_->sinc_table[i * state_->filter_length + j] = 
                    windowed_sinc(x, half_length) * state_->ratio;
            } else {
                // Upsampling: standard sinc
                state_->sinc_table[i * state_->filter_length + j] = 
                    windowed_sinc(x, half_length);
            }
        }
    }
    
    // Initialize input buffer for streaming conversion
    int buffer_samples = config_.buffer_size * 2 + state_->filter_length;
    state_->input_buffer.resize(buffer_samples * state_->channels);
    state_->buffer_used = 0;
    
    initialized_ = true;
    last_error_ = AudioError::None;
    
    std::ostringstream oss;
    oss << "SampleRateConverter initialized: " << config_.input_sample_rate 
        << "Hz -> " << config_.output_sample_rate << "Hz, " << config_.input_channels 
        << " channels, filter_length=" << state_->filter_length;
    ve::log::info(oss.str());
    
    return AudioError::None;
}

bool SampleRateConverter::is_initialized() const {
    return initialized_;
}

std::shared_ptr<AudioFrame> SampleRateConverter::convert_frame(const std::shared_ptr<AudioFrame>& input_frame) {
    if (!initialized_ || !input_frame) {
        last_error_ = AudioError::ConfigurationError;
        return nullptr;
    }
    
    // Calculate output sample count
    uint32_t input_samples = input_frame->sample_count();
    uint32_t output_samples = calculate_output_samples(input_samples);
    
    // Create output frame
    auto output_frame = AudioFrame::create(
        config_.output_sample_rate,
        config_.output_channels,
        output_samples,
        config_.output_format,
        input_frame->timestamp()
    );
    
    if (!output_frame) {
        last_error_ = AudioError::MemoryError;
        return nullptr;
    }
    
    // Convert sample format to float32 for processing
    std::vector<float> input_float;
    if (input_frame->format() == SampleFormat::Float32) {
        const float* input_data = static_cast<const float*>(input_frame->data());
        input_float.assign(input_data, input_data + input_samples * input_frame->channel_count());
    } else {
        input_float.resize(input_samples * input_frame->channel_count());
        convert_to_float32(input_frame->data(), input_frame->format(), 
                          input_float.data(), input_samples * input_frame->channel_count());
    }
    
    // Perform conversion
    std::vector<float> output_float(output_samples * config_.output_channels);
    uint32_t actual_output_samples = 0;
    
    AudioError error = convert_streaming(
        input_float.data(),
        input_samples,
        output_float.data(),
        output_samples,
        actual_output_samples
    );
    
    if (error != AudioError::None) {
        last_error_ = error;
        return nullptr;
    }
    
    // Convert back to target format if needed
    if (config_.output_format == SampleFormat::Float32) {
        std::memcpy(output_frame->data(), output_float.data(), 
                   actual_output_samples * config_.output_channels * sizeof(float));
    } else {
        convert_from_float32(output_float.data(), output_frame->data(), 
                           config_.output_format, actual_output_samples * config_.output_channels);
    }
    
    last_error_ = AudioError::None;
    return output_frame;
}

AudioError SampleRateConverter::convert_streaming(
    const float* input_data,
    uint32_t input_sample_count,
    float* output_data,
    uint32_t max_output_samples,
    uint32_t& output_sample_count) {
    
    if (!initialized_) {
        return AudioError::ConfigurationError;
    }
    
    return convert_internal(input_data, input_sample_count, 
                           output_data, max_output_samples, 
                           output_sample_count, false);
}

AudioError SampleRateConverter::flush(
    float* output_data,
    uint32_t max_output_samples,
    uint32_t& output_sample_count) {
    
    if (!initialized_) {
        return AudioError::ConfigurationError;
    }
    
    return convert_internal(nullptr, 0, 
                           output_data, max_output_samples, 
                           output_sample_count, true);
}

void SampleRateConverter::reset() {
    if (!initialized_) return;
    
    state_->position = 0.0;
    state_->buffer_used = 0;
    std::fill(state_->input_buffer.begin(), state_->input_buffer.end(), 0.0f);
}

double SampleRateConverter::conversion_ratio() const {
    return state_->ratio;
}

uint32_t SampleRateConverter::calculate_output_samples(uint32_t input_samples) const {
    if (!initialized_) return 0;
    
    // Add a small margin for rounding
    double exact_output = input_samples * state_->ratio + 2.0;
    return static_cast<uint32_t>(std::ceil(exact_output));
}

uint32_t SampleRateConverter::get_latency_samples() const {
    if (!initialized_) return 0;
    
    // Latency is approximately half the filter length
    return state_->filter_length / 2;
}

TimePoint SampleRateConverter::get_latency_time() const {
    if (!initialized_) return TimePoint();
    
    uint32_t latency_samples = get_latency_samples();
    TimeRational latency_time(latency_samples, config_.output_sample_rate);
    return TimePoint(latency_time);
}

AudioError SampleRateConverter::convert_internal(
    const float* input,
    uint32_t input_frames,
    float* output,
    uint32_t max_output_frames,
    uint32_t& output_frames,
    bool is_final) {
    
    output_frames = 0;
    
    // Copy new input data to buffer
    if (input && input_frames > 0) {
        uint32_t input_samples = input_frames * state_->channels;
        uint32_t buffer_capacity = static_cast<uint32_t>(state_->input_buffer.size());
        uint32_t available_space = buffer_capacity - state_->buffer_used;
        
        if (input_samples > available_space) {
            // Shift existing data to make room
            uint32_t filter_samples = static_cast<uint32_t>(state_->filter_length * state_->channels);
            uint32_t samples_to_keep = std::min(state_->buffer_used, filter_samples);
            std::memmove(state_->input_buffer.data(), 
                        state_->input_buffer.data() + state_->buffer_used - samples_to_keep,
                        samples_to_keep * sizeof(float));
            state_->buffer_used = samples_to_keep;
            available_space = buffer_capacity - state_->buffer_used;
        }
        
        uint32_t samples_to_copy = std::min(input_samples, available_space);
        std::memcpy(state_->input_buffer.data() + state_->buffer_used,
                   input, samples_to_copy * sizeof(float));
        state_->buffer_used += samples_to_copy;
    }
    
    // Convert samples using sinc interpolation
    uint32_t available_input_frames = state_->buffer_used / state_->channels;
    int half_filter = state_->filter_length / 2;
    
    while (output_frames < max_output_frames) {
        int input_index = static_cast<int>(state_->position);
        double fraction = state_->position - input_index;
        
        // Check if we have enough input samples for this output sample
        if (input_index + half_filter >= static_cast<int>(available_input_frames)) {
            if (!is_final) break;
            // Pad with zeros for final flush
        }
        
        // Calculate sinc interpolation
        int table_index = static_cast<int>(fraction * state_->sinc_table_size);
        table_index = std::clamp(table_index, 0, state_->sinc_table_size - 1);
        
        for (uint16_t ch = 0; ch < state_->channels; ++ch) {
            double sample = 0.0;
            
            for (int i = 0; i < state_->filter_length; ++i) {
                int src_index = input_index - half_filter + i;
                
                if (src_index >= 0 && src_index < static_cast<int>(available_input_frames)) {
                    double coeff = state_->sinc_table[table_index * state_->filter_length + i];
                    sample += state_->input_buffer[src_index * state_->channels + ch] * coeff;
                }
            }
            
            output[output_frames * state_->channels + ch] = static_cast<float>(sample);
        }
        
        output_frames++;
        state_->position += 1.0 / state_->ratio;
    }
    
    // Update buffer state
    int consumed_frames = static_cast<int>(state_->position);
    if (consumed_frames > 0) {
        uint32_t consumed_samples = consumed_frames * state_->channels;
        if (consumed_samples < state_->buffer_used) {
            std::memmove(state_->input_buffer.data(),
                        state_->input_buffer.data() + consumed_samples,
                        (state_->buffer_used - consumed_samples) * sizeof(float));
            state_->buffer_used -= consumed_samples;
        } else {
            state_->buffer_used = 0;
        }
        state_->position -= consumed_frames;
    }
    
    return AudioError::None;
}

void SampleRateConverter::convert_to_float32(const void* input, SampleFormat input_format, 
                                           float* output, uint32_t sample_count) {
    switch (input_format) {
        case SampleFormat::Int16: {
            const int16_t* src = static_cast<const int16_t*>(input);
            for (uint32_t i = 0; i < sample_count; ++i) {
                output[i] = static_cast<float>(src[i]) / 32768.0f;
            }
            break;
        }
        case SampleFormat::Int32: {
            const int32_t* src = static_cast<const int32_t*>(input);
            for (uint32_t i = 0; i < sample_count; ++i) {
                output[i] = static_cast<float>(src[i]) / 2147483648.0f;
            }
            break;
        }
        case SampleFormat::Float32: {
            const float* src = static_cast<const float*>(input);
            std::memcpy(output, src, sample_count * sizeof(float));
            break;
        }
    }
}

void SampleRateConverter::convert_from_float32(const float* input, void* output, 
                                             SampleFormat output_format, uint32_t sample_count) {
    switch (output_format) {
        case SampleFormat::Int16: {
            int16_t* dst = static_cast<int16_t*>(output);
            for (uint32_t i = 0; i < sample_count; ++i) {
                float sample = std::clamp(input[i], -1.0f, 1.0f);
                dst[i] = static_cast<int16_t>(sample * 32767.0f);
            }
            break;
        }
        case SampleFormat::Int32: {
            int32_t* dst = static_cast<int32_t*>(output);
            for (uint32_t i = 0; i < sample_count; ++i) {
                float sample = std::clamp(input[i], -1.0f, 1.0f);
                dst[i] = static_cast<int32_t>(sample * 2147483647.0f);
            }
            break;
        }
        case SampleFormat::Float32: {
            float* dst = static_cast<float*>(output);
            std::memcpy(dst, input, sample_count * sizeof(float));
            break;
        }
    }
}

// Utility functions
namespace resample_utils {

uint32_t recommend_buffer_size(uint32_t input_rate, uint32_t output_rate, ResampleQuality quality) {
    // Base buffer size on sample rates and quality
    uint32_t base_size = 1024;
    
    double ratio = static_cast<double>(output_rate) / input_rate;
    if (ratio > 2.0 || ratio < 0.5) {
        base_size *= 2; // Larger buffer for extreme ratios
    }
    
    switch (quality) {
        case ResampleQuality::Fastest:
            return base_size / 2;
        case ResampleQuality::Medium:
            return base_size;
        case ResampleQuality::Highest:
            return base_size * 2;
    }
    
    return base_size;
}

const char* quality_to_string(ResampleQuality quality) {
    switch (quality) {
        case ResampleQuality::Fastest: return "Fastest";
        case ResampleQuality::Medium: return "Medium";
        case ResampleQuality::Highest: return "Highest";
        default: return "Unknown";
    }
}

} // namespace resample_utils

} // namespace ve::audio