/**
 * @file audio_dither.hpp
 * @brief High-quality audio dithering for bit depth reduction
 *
 * Implements professional dithering algorithms to prevent quantization noise
 * during bit depth reduction, including noise shaping for improved quality.
 */

#pragma once

#include <memory>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>

namespace ve::audio {

/**
 * @brief Dithering algorithm types
 */
enum class DitherType {
    None,           // No dithering (truncation)
    Rectangular,    // Uniform random dithering
    Triangular,     // Triangular PDF dithering (high quality)
    Shaped          // Noise-shaped dithering (highest quality)
};

/**
 * @brief Noise shaping filter types for shaped dithering
 */
enum class NoiseShapeFilter {
    Simple,     // Simple 1st order high-pass
    FWeighted,  // Fletcher-Munson weighted (psychoacoustic)
    UltraSonic  // Ultra-sonic noise shaping (inaudible)
};

/**
 * @brief Dithering configuration
 */
struct DitherConfig {
    DitherType type = DitherType::Triangular;
    NoiseShapeFilter noise_filter = NoiseShapeFilter::FWeighted;
    double amplitude = 1.0;  // Dither amplitude (0.5-2.0 LSB recommended)
    bool dc_block = true;    // Remove DC offset from dither
    uint32_t channels = 2;
    uint32_t sample_rate = 48000;
};

/**
 * @brief High-quality audio dithering processor
 *
 * Provides professional dithering algorithms for bit depth reduction:
 * - Triangular PDF dithering for optimal SNR
 * - Noise shaping to move quantization noise to inaudible frequencies
 * - DC blocking to prevent low-frequency artifacts
 * - Per-channel processing for multi-channel audio
 */
class AudioDither {
public:
    /**
     * @brief Create dither processor
     */
    static std::unique_ptr<AudioDither> create(const DitherConfig& config);

    /**
     * @brief Constructor
     */
    explicit AudioDither(const DitherConfig& config);

    /**
     * @brief Destructor
     */
    ~AudioDither();

    // Prevent copying
    AudioDither(const AudioDither&) = delete;
    AudioDither& operator=(const AudioDither&) = delete;

    /**
     * @brief Process audio samples with dithering
     *
     * @param input Input samples (float, full scale -1.0 to +1.0)
     * @param output Output samples (quantized to target bit depth)
     * @param sample_count Number of samples per channel
     * @param target_bits Target bit depth (8, 16, 24)
     * @param channels Number of channels
     */
    void process(const float* input, void* output, uint32_t sample_count,
                uint8_t target_bits, uint16_t channels);

    /**
     * @brief Process single sample with dithering
     */
    float process_sample(float input, uint8_t target_bits, uint16_t channel);

    /**
     * @brief Reset dither state (clears noise shaping history)
     */
    void reset();

    /**
     * @brief Get current configuration
     */
    const DitherConfig& get_config() const { return config_; }

private:
    DitherConfig config_;

    // Random number generators (one per channel)
    std::vector<std::mt19937> rngs_;
    std::vector<std::uniform_real_distribution<float>> uniform_dist_;
    std::vector<std::normal_distribution<float>> normal_dist_;

    // Triangular dither state (per channel)
    std::vector<float> triangular_history_;

    // Noise shaping state (per channel)
    std::vector<std::vector<float>> noise_shape_history_;
    std::vector<std::vector<float>> noise_shape_coeffs_;

    // DC blocking filter state
    std::vector<float> dc_x1_, dc_y1_;  // Per channel

    void initialize_noise_shaping();
    float apply_noise_shaping(float error, uint16_t channel);
    float triangular_dither(uint16_t channel);
    float shaped_dither(float input, float quantized, uint16_t channel);
    void apply_dc_block(float& sample, uint16_t channel);

    // Quantization helpers
    float quantize_to_bits(float sample, uint8_t bits);
    void convert_to_target_format(const float* input, void* output,
                                uint32_t sample_count, uint8_t target_bits);
};

/**
 * @brief Utility functions for dithering
 */
namespace dither_utils {

    /**
     * @brief Calculate optimal dither amplitude for given bit depth
     */
    inline float optimal_dither_amplitude(uint8_t target_bits) {
        // Optimal dither is typically 0.5-1.0 LSB
        float lsb = 1.0f / (1 << (target_bits - 1));
        return 0.8f * lsb;  // 0.8 LSB is a good compromise
    }

    /**
     * @brief Check if dithering is recommended for bit depth reduction
     */
    inline bool dithering_recommended(uint8_t source_bits, uint8_t target_bits) {
        return (source_bits - target_bits) >= 4;  // Recommend for 4+ bit reduction
    }

    /**
     * @brief Get recommended dither type for quality level
     */
    inline DitherType recommended_type(uint8_t source_bits, uint8_t target_bits) {
        int reduction = source_bits - target_bits;
        if (reduction >= 8) return DitherType::Shaped;
        if (reduction >= 4) return DitherType::Triangular;
        if (reduction >= 2) return DitherType::Rectangular;
        return DitherType::None;
    }

} // namespace dither_utils

} // namespace ve::audio