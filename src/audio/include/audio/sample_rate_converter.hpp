#pragma once

#include "audio_frame.hpp"
#include "decoder.hpp"  // for AudioError enum
#include <core/time.hpp>
#include <memory>
#include <vector>

namespace ve::audio {

/**
 * @brief Sample rate conversion quality settings
 */
enum class ResampleQuality {
    Fastest,      // Low quality but fast (for preview/scrubbing)
    Medium,       // Balanced quality/performance for real-time
    Highest       // Highest quality for export (may be slower)
};

/**
 * @brief Configuration for sample rate conversion
 */
struct ResampleConfig {
    uint32_t input_sample_rate = 44100;
    uint32_t output_sample_rate = 48000;
    uint16_t input_channels = 2;
    uint16_t output_channels = 2;
    ResampleQuality quality = ResampleQuality::Medium;
    SampleFormat input_format = SampleFormat::Float32;
    SampleFormat output_format = SampleFormat::Float32;
    
    // Buffer size for streaming conversion (samples per channel)
    uint32_t buffer_size = 1024;
    
    // Enable real-time processing optimizations
    bool realtime_mode = true;
};

/**
 * @brief Resampler state for streaming conversion
 */
struct ResamplerState;

/**
 * @brief High-quality sample rate converter for professional audio
 * 
 * This converter provides:
 * - Arbitrary sample rate conversion (44.1kHz ↔ 48kHz ↔ 96kHz)
 * - Latency-optimized streaming conversion
 * - High quality resampling (<0.1dB THD+N target)
 * - Lock-free operation for real-time audio threads
 * 
 * Audio Engine Phase 1 Week 2 deliverable
 */
class SampleRateConverter {
public:
    /**
     * @brief Create a sample rate converter with specified configuration
     * @param config Conversion configuration
     * @return Unique pointer to converter, or nullptr on failure
     */
    static std::unique_ptr<SampleRateConverter> create(const ResampleConfig& config);
    
    /**
     * @brief Constructor
     * @param config Conversion configuration
     */
    explicit SampleRateConverter(const ResampleConfig& config);
    
    /**
     * @brief Destructor
     */
    ~SampleRateConverter();
    
    // Non-copyable but movable
    SampleRateConverter(const SampleRateConverter&) = delete;
    SampleRateConverter& operator=(const SampleRateConverter&) = delete;
    SampleRateConverter(SampleRateConverter&&) noexcept;
    SampleRateConverter& operator=(SampleRateConverter&&) noexcept;
    
    /**
     * @brief Initialize the converter
     * @return AudioError::None on success
     */
    AudioError initialize();
    
    /**
     * @brief Check if converter is initialized and ready
     */
    bool is_initialized() const;
    
    /**
     * @brief Convert sample rate of an audio frame
     * @param input_frame Input audio frame
     * @return Converted audio frame, or nullptr on error
     */
    std::shared_ptr<AudioFrame> convert_frame(const std::shared_ptr<AudioFrame>& input_frame);
    
    /**
     * @brief Convert sample rate with streaming (for real-time processing)
     * @param input_data Input audio samples (interleaved)
     * @param input_sample_count Number of samples per channel in input
     * @param output_data Output buffer (must be pre-allocated)
     * @param max_output_samples Maximum samples per channel in output buffer
     * @param output_sample_count [out] Actual samples per channel written to output
     * @return AudioError::None on success
     */
    AudioError convert_streaming(
        const float* input_data,
        uint32_t input_sample_count,
        float* output_data,
        uint32_t max_output_samples,
        uint32_t& output_sample_count
    );
    
    /**
     * @brief Flush remaining samples from internal buffers
     * @param output_data Output buffer
     * @param max_output_samples Maximum samples per channel in output buffer
     * @param output_sample_count [out] Actual samples per channel written
     * @return AudioError::None on success
     */
    AudioError flush(
        float* output_data,
        uint32_t max_output_samples,
        uint32_t& output_sample_count
    );
    
    /**
     * @brief Reset converter state (clear internal buffers)
     */
    void reset();
    
    /**
     * @brief Get input sample rate
     */
    uint32_t input_sample_rate() const { return config_.input_sample_rate; }
    
    /**
     * @brief Get output sample rate
     */
    uint32_t output_sample_rate() const { return config_.output_sample_rate; }
    
    /**
     * @brief Get conversion ratio (output_rate / input_rate)
     */
    double conversion_ratio() const;
    
    /**
     * @brief Calculate output sample count for given input count
     * @param input_samples Input sample count per channel
     * @return Estimated output sample count per channel
     */
    uint32_t calculate_output_samples(uint32_t input_samples) const;
    
    /**
     * @brief Get converter latency in samples (output rate)
     */
    uint32_t get_latency_samples() const;
    
    /**
     * @brief Get converter latency as TimePoint
     */
    TimePoint get_latency_time() const;
    
    /**
     * @brief Get last error
     */
    AudioError last_error() const { return last_error_; }

private:
    ResampleConfig config_;
    std::unique_ptr<ResamplerState> state_;
    AudioError last_error_ = AudioError::None;
    bool initialized_ = false;
    
    // Internal conversion implementation
    AudioError convert_internal(
        const float* input,
        uint32_t input_frames,
        float* output,
        uint32_t max_output_frames,
        uint32_t& output_frames,
        bool is_final
    );
    
    // Format conversion helpers
    void convert_to_float32(const void* input, SampleFormat input_format, 
                           float* output, uint32_t sample_count);
    void convert_from_float32(const float* input, void* output, 
                             SampleFormat output_format, uint32_t sample_count);
};

/**
 * @brief Utility functions for sample rate conversion
 */
namespace resample_utils {
    /**
     * @brief Check if sample rate conversion is needed
     */
    inline bool conversion_needed(uint32_t input_rate, uint32_t output_rate) {
        return input_rate != output_rate;
    }
    
    /**
     * @brief Get recommended buffer size for given sample rates
     */
    uint32_t recommend_buffer_size(uint32_t input_rate, uint32_t output_rate, 
                                  ResampleQuality quality = ResampleQuality::Medium);
    
    /**
     * @brief Calculate conversion ratio
     */
    inline double calculate_ratio(uint32_t input_rate, uint32_t output_rate) {
        return static_cast<double>(output_rate) / static_cast<double>(input_rate);
    }
    
    /**
     * @brief Get quality description string
     */
    const char* quality_to_string(ResampleQuality quality);
}

} // namespace ve::audio