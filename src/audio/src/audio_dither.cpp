/**
 * @file audio_dither.cpp
 * @brief High-quality audio dithering implementation
 */

#include "audio/audio_dither.hpp"
#include <algorithm>
#include <numeric>
#include <random>

namespace ve::audio {

std::unique_ptr<AudioDither> AudioDither::create(const DitherConfig& config) {
    return std::make_unique<AudioDither>(config);
}

AudioDither::AudioDither(const DitherConfig& config) : config_(config) {
    // Initialize random number generators
    rngs_.resize(config.channels);
    uniform_dist_.resize(config.channels);
    normal_dist_.resize(config.channels);

    // Seed RNGs with random device
    std::random_device rd;
    for (uint16_t ch = 0; ch < config.channels; ++ch) {
        rngs_[ch].seed(rd());
        uniform_dist_[ch] = std::uniform_real_distribution<float>(-0.5f, 0.5f);
        normal_dist_[ch] = std::normal_distribution<float>(0.0f, 1.0f);
    }

    // Initialize triangular dither history
    triangular_history_.resize(config.channels, 0.0f);

    // Initialize noise shaping
    initialize_noise_shaping();

    // Initialize DC blocking
    if (config.dc_block) {
        dc_x1_.resize(config.channels, 0.0f);
        dc_y1_.resize(config.channels, 0.0f);
    }

    reset();
}

AudioDither::~AudioDither() = default;

void AudioDither::initialize_noise_shaping() {
    noise_shape_history_.resize(config_.channels);
    noise_shape_coeffs_.resize(config_.channels);

    for (uint16_t ch = 0; ch < config_.channels; ++ch) {
        switch (config_.noise_filter) {
            case NoiseShapeFilter::Simple: {
                // Simple 1st order high-pass: H(z) = 1 - z^(-1)
                noise_shape_history_[ch].resize(1, 0.0f);
                noise_shape_coeffs_[ch] = {1.0f, -1.0f};
                break;
            }
            case NoiseShapeFilter::FWeighted: {
                // Fletcher-Munson weighted (psychoacoustic)
                // 3rd order filter optimized for human hearing
                noise_shape_history_[ch].resize(3, 0.0f);
                noise_shape_coeffs_[ch] = {2.033f, -2.165f, 1.959f, -1.590f};
                break;
            }
            case NoiseShapeFilter::UltraSonic: {
                // Ultra-sonic noise shaping (moves noise above 20kHz)
                // 5th order filter
                noise_shape_history_[ch].resize(5, 0.0f);
                noise_shape_coeffs_[ch] = {3.033f, -4.165f, 3.959f, -2.590f, 1.201f, -0.590f};
                break;
            }
        }
    }
}

void AudioDither::process(const float* input, void* output, uint32_t sample_count,
                         uint8_t target_bits, uint16_t channels) {
    if (channels != config_.channels) {
        // Reconfigure for different channel count
        config_.channels = channels;
        reset();
    }

    // Process in blocks for efficiency
    const uint32_t block_size = 1024;
    uint32_t processed = 0;

    while (processed < sample_count) {
        uint32_t block_samples = std::min(block_size, sample_count - processed);

        // Temporary buffer for float processing
        std::vector<float> temp_input(block_samples * channels);
        std::vector<float> temp_output(block_samples * channels);

        // Copy input block
        std::memcpy(temp_input.data(), input + processed * channels,
                   block_samples * channels * sizeof(float));

        // Process each sample
        for (uint32_t i = 0; i < block_samples * channels; ++i) {
            uint16_t ch = i % channels;
            temp_output[i] = process_sample(temp_input[i], target_bits, ch);
        }

        // Convert to target format and copy to output
        convert_to_target_format(temp_output.data(),
                               static_cast<char*>(output) + processed * channels * (target_bits / 8),
                               block_samples, target_bits);

        processed += block_samples;
    }
}

float AudioDither::process_sample(float input, uint8_t target_bits, uint16_t channel) {
    // Apply dithering based on type
    float dithered = input;

    switch (config_.type) {
        case DitherType::None:
            // No dithering - just quantize
            break;

        case DitherType::Rectangular: {
            // Uniform random dithering
            float dither = uniform_dist_[channel](rngs_[channel]) * config_.amplitude;
            dithered = input + dither;
            break;
        }

        case DitherType::Triangular: {
            // Triangular PDF dithering (RPDF)
            float r1 = uniform_dist_[channel](rngs_[channel]);
            float r2 = uniform_dist_[channel](rngs_[channel]);
            float dither = (r1 + r2 - 1.0f) * config_.amplitude;
            dithered = input + dither;
            break;
        }

        case DitherType::Shaped: {
            // Noise-shaped dithering
            float quantized = quantize_to_bits(input, target_bits);
            float error = input - quantized;
            float shaped_error = apply_noise_shaping(error, channel);
            dithered = input + shaped_error;
            break;
        }
    }

    // Quantize to target bit depth
    float quantized = quantize_to_bits(dithered, target_bits);

    // Apply DC blocking if enabled
    if (config_.dc_block) {
        apply_dc_block(quantized, channel);
    }

    return quantized;
}

float AudioDither::apply_noise_shaping(float error, uint16_t channel) {
    auto& history = noise_shape_history_[channel];
    auto& coeffs = noise_shape_coeffs_[channel];

    // Apply noise shaping filter
    float shaped_error = error * coeffs[0];

    for (size_t i = 1; i < coeffs.size() && i <= history.size(); ++i) {
        shaped_error -= history[i-1] * coeffs[i];
    }

    // Update history
    for (size_t i = history.size() - 1; i > 0; --i) {
        history[i] = history[i-1];
    }
    if (!history.empty()) {
        history[0] = shaped_error;
    }

    return shaped_error;
}

float AudioDither::triangular_dither(uint16_t channel) {
    // Generate triangular distributed dither
    float r1 = uniform_dist_[channel](rngs_[channel]);
    float r2 = uniform_dist_[channel](rngs_[channel]);

    // Triangular distribution: sum of two uniform - 1
    float dither = (r1 + r2 - 1.0f) * config_.amplitude;

    // Store for potential use in shaped dithering
    triangular_history_[channel] = dither;

    return dither;
}

float AudioDither::shaped_dither(float input, float quantized, uint16_t channel) {
    // Calculate quantization error
    float error = input - quantized;

    // Apply triangular dither to error
    float dither = triangular_dither(channel);

    // Add shaped error
    return error + apply_noise_shaping(error, channel) + dither;
}

void AudioDither::apply_dc_block(float& sample, uint16_t channel) {
    // Simple 1st order DC blocking filter: H(z) = (1 - z^(-1)) / (1 - 0.995*z^(-1))
    // This removes DC while preserving low frequencies
    const float pole = 0.995f;  // Close to 1 for gentle filtering

    float x0 = sample;
    float y0 = x0 - dc_x1_[channel] + pole * dc_y1_[channel];

    // Update state
    dc_x1_[channel] = x0;
    dc_y1_[channel] = y0;

    sample = y0;
}

float AudioDither::quantize_to_bits(float sample, uint8_t bits) {
    // Clamp to valid range
    sample = std::clamp(sample, -1.0f, 1.0f);

    // Calculate quantization levels
    int32_t levels = 1 << bits;
    float scale = static_cast<float>(levels) / 2.0f;  // For signed representation

    // Quantize
    float quantized = std::round(sample * scale) / scale;

    return std::clamp(quantized, -1.0f, 1.0f);
}

void AudioDither::convert_to_target_format(const float* input, void* output,
                                         uint32_t sample_count, uint8_t target_bits) {
    char* out_ptr = static_cast<char*>(output);

    for (uint32_t i = 0; i < sample_count; ++i) {
        float sample = input[i];

        switch (target_bits) {
            case 8: {
                // 8-bit signed
                int8_t quantized = static_cast<int8_t>(sample * 127.0f);
                *out_ptr++ = quantized;
                break;
            }
            case 16: {
                // 16-bit signed
                int16_t quantized = static_cast<int16_t>(sample * 32767.0f);
                *reinterpret_cast<int16_t*>(out_ptr) = quantized;
                out_ptr += 2;
                break;
            }
            case 24: {
                // 24-bit signed (stored in 32-bit for alignment)
                int32_t quantized = static_cast<int32_t>(sample * 8388607.0f);
                *reinterpret_cast<int32_t*>(out_ptr) = quantized;
                out_ptr += 4;
                break;
            }
            case 32: {
                // 32-bit float
                *reinterpret_cast<float*>(out_ptr) = sample;
                out_ptr += 4;
                break;
            }
        }
    }
}

void AudioDither::reset() {
    // Reset triangular history
    std::fill(triangular_history_.begin(), triangular_history_.end(), 0.0f);

    // Reset noise shaping history
    for (auto& history : noise_shape_history_) {
        std::fill(history.begin(), history.end(), 0.0f);
    }

    // Reset DC blocking
    if (config_.dc_block) {
        std::fill(dc_x1_.begin(), dc_x1_.end(), 0.0f);
        std::fill(dc_y1_.begin(), dc_y1_.end(), 0.0f);
    }
}

} // namespace ve::audio