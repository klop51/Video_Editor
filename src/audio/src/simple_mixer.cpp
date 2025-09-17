/**
 * @file simple_mixer.cpp
 * @brief Simple Audio Mixer Core Implementation
 *
 * Professional multi-track audio mixing with:
 * - Per-track gain control and stereo panning
 * - Master volume and mute controls
 * - Solo/mute functionality
 * - Soft clipping protection
 * - Real-time statistics and monitoring
 */

#include "audio/simple_mixer.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace ve::audio {

// Static utility functions
float SimpleMixer::db_to_linear(float db) {
    if (db <= -60.0f) return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

float SimpleMixer::linear_to_db(float linear) {
    if (linear <= 0.0f) return -60.0f;
    return 20.0f * std::log10(linear);
}

float SimpleMixer::soft_clip(float sample, float threshold) {
    if (std::abs(sample) <= threshold) {
        return sample;
    }

    // Soft clipping using tanh function
    return threshold * std::tanh(sample / threshold);
}

// SimpleMixer implementation

std::unique_ptr<SimpleMixer> SimpleMixer::create(const SimpleMixerConfig& config) {
    return std::unique_ptr<SimpleMixer>(new SimpleMixer(config));
}

SimpleMixer::SimpleMixer(const SimpleMixerConfig& config)
    : config_(config) {
    // Initialize accumulator buffer (stereo)
    accumulator_.resize(config_.channel_count, 0.0f);
}

SimpleMixer::~SimpleMixer() {
    shutdown();
}

MixerError SimpleMixer::initialize() {
    if (initialized_) {
        return MixerError::Success;
    }

    // Validate configuration
    if (config_.sample_rate == 0 || config_.channel_count == 0) {
        set_error(MixerError::InvalidConfiguration, "Invalid sample rate or channel count");
        return MixerError::InvalidConfiguration;
    }

    if (config_.max_channels == 0) {
        set_error(MixerError::InvalidConfiguration, "Max channels must be greater than 0");
        return MixerError::InvalidConfiguration;
    }

    // Initialize accumulator
    std::lock_guard<std::mutex> lock(accumulator_mutex_);
    accumulator_.assign(config_.channel_count, 0.0f);

    initialized_ = true;
    return MixerError::Success;
}

void SimpleMixer::shutdown() {
    if (!initialized_) {
        return;
    }

    // Clear all channels
    {
        std::lock_guard<std::mutex> lock(channels_mutex_);
        channels_.clear();
    }

    // Clear accumulator
    {
        std::lock_guard<std::mutex> lock(accumulator_mutex_);
        accumulator_.assign(accumulator_.size(), 0.0f);
    }

    initialized_ = false;
}

uint32_t SimpleMixer::add_channel(const std::string& name,
                                 float initial_gain_db,
                                 float initial_pan) {
    if (!initialized_) {
        set_error(MixerError::NotInitialized, "Mixer not initialized");
        return 0;
    }

    std::lock_guard<std::mutex> lock(channels_mutex_);

    if (channels_.size() >= config_.max_channels) {
        set_error(MixerError::TooManyChannels, "Maximum number of channels reached");
        return 0;
    }

    // Clamp gain and pan to valid ranges
    initial_gain_db = std::clamp(initial_gain_db, -60.0f, 12.0f);
    initial_pan = std::clamp(initial_pan, -1.0f, 1.0f);

    MixerChannel channel;
    channel.id = next_channel_id_.fetch_add(1);
    channel.name = name;
    channel.gain_db = initial_gain_db;
    channel.pan = initial_pan;
    channel.muted = false;
    channel.solo = false;
    channel.samples_processed = 0;

    channels_.push_back(channel);
    return channel.id;
}

bool SimpleMixer::remove_channel(uint32_t channel_id) {
    if (!initialized_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(channels_mutex_);

    auto it = std::find_if(channels_.begin(), channels_.end(),
                          [channel_id](const MixerChannel& ch) {
                              return ch.id == channel_id;
                          });

    if (it != channels_.end()) {
        channels_.erase(it);
        return true;
    }

    return false;
}

MixerChannel SimpleMixer::get_channel(uint32_t channel_id) const {
    if (!initialized_) {
        return MixerChannel{};
    }

    std::lock_guard<std::mutex> lock(channels_mutex_);

    auto it = std::find_if(channels_.begin(), channels_.end(),
                          [channel_id](const MixerChannel& ch) {
                              return ch.id == channel_id;
                          });

    if (it != channels_.end()) {
        return *it;
    }

    return MixerChannel{};
}

MixerError SimpleMixer::update_channel(const MixerChannel& channel) {
    if (!initialized_) {
        set_error(MixerError::NotInitialized, "Mixer not initialized");
        return MixerError::NotInitialized;
    }

    if (!channel.is_valid()) {
        set_error(MixerError::InvalidChannel, "Invalid channel configuration");
        return MixerError::InvalidChannel;
    }

    std::lock_guard<std::mutex> lock(channels_mutex_);

    auto it = std::find_if(channels_.begin(), channels_.end(),
                          [channel](const MixerChannel& ch) {
                              return ch.id == channel.id;
                          });

    if (it != channels_.end()) {
        // Clamp values to valid ranges
        MixerChannel updated_channel = channel;
        updated_channel.gain_db = std::clamp(updated_channel.gain_db, -60.0f, 12.0f);
        updated_channel.pan = std::clamp(updated_channel.pan, -1.0f, 1.0f);

        *it = updated_channel;
        return MixerError::Success;
    }

    set_error(MixerError::ChannelNotFound, "Channel not found");
    return MixerError::ChannelNotFound;
}

std::vector<MixerChannel> SimpleMixer::get_all_channels() const {
    if (!initialized_) {
        return {};
    }

    std::lock_guard<std::mutex> lock(channels_mutex_);
    return channels_;
}

uint32_t SimpleMixer::get_channel_count() const {
    if (!initialized_) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(channels_mutex_);
    return static_cast<uint32_t>(channels_.size());
}

MixerError SimpleMixer::process_channel(uint32_t channel_id, std::shared_ptr<AudioFrame> input) {
    if (!initialized_) {
        set_error(MixerError::NotInitialized, "Mixer not initialized");
        return MixerError::NotInitialized;
    }

    if (!input || input->sample_count() == 0) {
        set_error(MixerError::InvalidChannel, "Invalid input audio frame");
        return MixerError::InvalidChannel;
    }

    // Get channel information and solo status in one lock
    MixerChannel channel_info;
    bool has_solo = false;
    bool channel_found = false;

    {
        std::lock_guard<std::mutex> lock(channels_mutex_);

        // Find the channel and get its info
        auto it = std::find_if(channels_.begin(), channels_.end(),
                              [channel_id](const MixerChannel& ch) {
                                  return ch.id == channel_id;
                              });

        if (it != channels_.end()) {
            channel_info = *it;
            channel_found = true;
        }

        // Check if any channels are solo
        has_solo = std::any_of(channels_.begin(), channels_.end(),
                              [](const MixerChannel& ch) {
                                  return ch.solo;
                              });
    }

    if (!channel_found) {
        set_error(MixerError::ChannelNotFound, "Channel not found");
        return MixerError::ChannelNotFound;
    }

    // Check if channel should be processed (solo/mute logic)
    bool should_process = true;

    if (has_solo && !channel_info.solo) {
        should_process = false; // Other channels muted when solo active
    }

    if (channel_info.muted) {
        should_process = false; // Channel is muted
    }

    if (!should_process) {
        // Update statistics even for muted channels
        // Note: We can't update channel.samples_processed here since we don't have a pointer
        // This is a limitation of the current design - we'll handle this differently
        return MixerError::Success;
    }

    // Get channel settings
    float linear_gain = channel_info.get_linear_gain();
    float pan = channel_info.pan;

    // Process audio and mix to accumulator
    {
        std::lock_guard<std::mutex> lock(accumulator_mutex_);

        // Ensure accumulator is large enough
        if (accumulator_.size() < config_.channel_count) {
            accumulator_.resize(config_.channel_count, 0.0f);
        }

        // Process samples (assuming Float32 format for simplicity)
        if (input->format() == SampleFormat::Float32 && config_.channel_count >= 2) {
            const float* input_data = static_cast<const float*>(input->data());
            uint32_t input_channels = input->channel_count();
            uint32_t sample_count = input->sample_count();

            for (uint32_t sample = 0; sample < sample_count; ++sample) {
                float left_input = 0.0f;
                float right_input = 0.0f;

                // Extract left/right from input (handle mono->stereo conversion)
                if (input_channels == 1) {
                    // Mono to stereo
                    left_input = right_input = input_data[sample];
                } else if (input_channels >= 2) {
                    // Stereo or multi-channel
                    left_input = input_data[sample * input_channels];
                    right_input = input_data[sample * input_channels + 1];
                }

                // Apply gain
                left_input *= linear_gain;
                right_input *= linear_gain;

                // Apply panning
                float left_output, right_output;
                apply_panning(left_input, right_input, pan, left_output, right_output);

                // Add to accumulator with soft clipping protection
                if (config_.enable_clipping_protection) {
                    accumulator_[0] += soft_clip(left_output);
                    accumulator_[1] += soft_clip(right_output);
                } else {
                    accumulator_[0] += left_output;
                    accumulator_[1] += right_output;
                }
            }

            channel_info.samples_processed += sample_count;

            // Update the channel in the vector
            {
                std::lock_guard<std::mutex> lock(channels_mutex_);
                auto it = std::find_if(channels_.begin(), channels_.end(),
                                      [channel_id](const MixerChannel& ch) {
                                          return ch.id == channel_id;
                                      });
                if (it != channels_.end()) {
                    it->samples_processed = channel_info.samples_processed;
                }
            }
        }
    }

    return MixerError::Success;
}

MixerError SimpleMixer::mix_to_output(std::shared_ptr<AudioFrame>& output, bool clear_accumulator) {
    if (!initialized_) {
        set_error(MixerError::NotInitialized, "Mixer not initialized");
        return MixerError::NotInitialized;
    }

    if (!output) {
        set_error(MixerError::InvalidChannel, "Invalid output frame");
        return MixerError::InvalidChannel;
    }

    // Temporarily disable mutex to test if deadlock is caused by mutexes
    std::lock_guard<std::mutex> lock(accumulator_mutex_);

    if (accumulator_.size() < config_.channel_count) {
        set_error(MixerError::InvalidConfiguration, "Accumulator not properly initialized");
        return MixerError::InvalidConfiguration;
    }

    // Apply master controls
    float master_linear_gain = get_master_linear_gain();
    bool master_muted = config_.master_mute;

    // Copy accumulator to output with master processing
    if (output->format() == SampleFormat::Float32) {
        float* output_data = static_cast<float*>(output->data());
        uint32_t sample_count = output->sample_count();
        uint32_t output_channels = output->channel_count();

        for (uint32_t sample = 0; sample < sample_count; ++sample) {
            for (uint32_t ch = 0; ch < output_channels && ch < config_.channel_count; ++ch) {
                float sample_value = accumulator_[ch];

                // Apply master gain
                if (!master_muted) {
                    sample_value *= master_linear_gain;
                } else {
                    sample_value = 0.0f;
                }

                // Final soft clipping
                if (config_.enable_clipping_protection) {
                    sample_value = soft_clip(sample_value);
                }

                output_data[sample * output_channels + ch] = sample_value;

                // Clear accumulator if requested
                if (clear_accumulator) {
                    accumulator_[ch] = 0.0f;
                }
            }
        }
    }

    // Update statistics
    if (output->format() == SampleFormat::Float32 && config_.channel_count >= 2) {
        const float* output_data = static_cast<const float*>(output->data());
        uint32_t sample_count = output->sample_count();
        std::vector<float> mixed_buffer(output_data, output_data + sample_count * config_.channel_count);
        update_stats(mixed_buffer);
    }

    return MixerError::Success;
}

std::shared_ptr<AudioFrame> SimpleMixer::mix_channels(uint32_t frame_count,
                                                    const TimePoint& timestamp) {
    if (!initialized_) {
        set_error(MixerError::NotInitialized, "Mixer not initialized");
        return nullptr;
    }

    // Create output frame
    auto output = AudioFrame::create(
        config_.sample_rate,
        config_.channel_count,
        frame_count,
        config_.format,
        timestamp
    );

    if (!output) {
        set_error(MixerError::Unknown, "Failed to create output frame");
        return nullptr;
    }

    // Mix to output
    MixerError result = mix_to_output(output, true);
    if (result != MixerError::Success) {
        return nullptr;
    }

    return output;
}

void SimpleMixer::clear_accumulator() {
    std::lock_guard<std::mutex> lock(accumulator_mutex_);
    std::fill(accumulator_.begin(), accumulator_.end(), 0.0f);
}

MixerError SimpleMixer::set_master_volume(float volume_db) {
    if (!initialized_) {
        set_error(MixerError::NotInitialized, "Mixer not initialized");
        return MixerError::NotInitialized;
    }

    config_.master_volume_db = std::clamp(volume_db, -60.0f, 12.0f);
    return MixerError::Success;
}

MixerError SimpleMixer::set_master_mute(bool muted) {
    if (!initialized_) {
        set_error(MixerError::NotInitialized, "Mixer not initialized");
        return MixerError::NotInitialized;
    }

    config_.master_mute = muted;
    return MixerError::Success;
}

float SimpleMixer::get_master_linear_gain() const {
    return db_to_linear(config_.master_volume_db);
}

MixerError SimpleMixer::set_channel_solo(uint32_t channel_id, bool solo) {
    MixerChannel channel = get_channel(channel_id);
    if (channel.id == 0) {
        set_error(MixerError::ChannelNotFound, "Channel not found");
        return MixerError::ChannelNotFound;
    }

    channel.solo = solo;
    return update_channel(channel);
}

MixerError SimpleMixer::set_channel_mute(uint32_t channel_id, bool muted) {
    MixerChannel channel = get_channel(channel_id);
    if (channel.id == 0) {
        set_error(MixerError::ChannelNotFound, "Channel not found");
        return MixerError::ChannelNotFound;
    }

    channel.muted = muted;
    return update_channel(channel);
}

MixerError SimpleMixer::set_channel_gain(uint32_t channel_id, float gain_db) {
    MixerChannel channel = get_channel(channel_id);
    if (channel.id == 0) {
        set_error(MixerError::ChannelNotFound, "Channel not found");
        return MixerError::ChannelNotFound;
    }

    channel.gain_db = std::clamp(gain_db, -60.0f, 12.0f);
    return update_channel(channel);
}

MixerError SimpleMixer::set_channel_pan(uint32_t channel_id, float pan) {
    MixerChannel channel = get_channel(channel_id);
    if (channel.id == 0) {
        set_error(MixerError::ChannelNotFound, "Channel not found");
        return MixerError::ChannelNotFound;
    }

    channel.pan = std::clamp(pan, -1.0f, 1.0f);
    return update_channel(channel);
}

MixerStats SimpleMixer::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void SimpleMixer::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = MixerStats{};
}

std::string SimpleMixer::get_last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_message_;
}

void SimpleMixer::clear_error() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = MixerError::Success;
    last_error_message_.clear();
}

MixerChannel* SimpleMixer::find_channel(uint32_t channel_id) {
    std::lock_guard<std::mutex> lock(channels_mutex_);

    auto it = std::find_if(channels_.begin(), channels_.end(),
                          [channel_id](MixerChannel& ch) {
                              return ch.id == channel_id;
                          });

    return (it != channels_.end()) ? &(*it) : nullptr;
}

const MixerChannel* SimpleMixer::find_channel(uint32_t channel_id) const {
    std::lock_guard<std::mutex> lock(channels_mutex_);

    auto it = std::find_if(channels_.begin(), channels_.end(),
                          [channel_id](const MixerChannel& ch) {
                              return ch.id == channel_id;
                          });

    return (it != channels_.end()) ? &(*it) : nullptr;
}

bool SimpleMixer::has_solo_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);

    return std::any_of(channels_.begin(), channels_.end(),
                      [](const MixerChannel& ch) {
                          return ch.solo;
                      });
}

void SimpleMixer::apply_panning(float left_input, float right_input, float pan,
                               float& left_output, float& right_output) {
    // Pan law: -3dB when centered, -6dB when fully panned
    if (pan == 0.0f) {
        // Center - no change
        left_output = left_input;
        right_output = right_input;
    } else if (pan > 0.0f) {
        // Pan right
        float pan_factor = pan;
        left_output = left_input * std::sqrt(1.0f - pan_factor);
        right_output = right_input * std::sqrt(1.0f + pan_factor);
    } else {
        // Pan left
        float pan_factor = -pan;
        left_output = left_input * std::sqrt(1.0f + pan_factor);
        right_output = right_input * std::sqrt(1.0f - pan_factor);
    }
}

void SimpleMixer::update_stats(const std::vector<float>& mixed_buffer) {
    if (mixed_buffer.size() < 2) return;

    std::lock_guard<std::mutex> lock(stats_mutex_);

    float peak_left = 0.0f;
    float peak_right = 0.0f;
    float sum_squares_left = 0.0f;
    float sum_squares_right = 0.0f;

    uint32_t sample_count = static_cast<uint32_t>(mixed_buffer.size() / config_.channel_count);

    for (uint32_t i = 0; i < sample_count; ++i) {
        float left = mixed_buffer[i * config_.channel_count];
        float right = mixed_buffer[i * config_.channel_count + 1];

        peak_left = std::max(peak_left, std::abs(left));
        peak_right = std::max(peak_right, std::abs(right));

        sum_squares_left += left * left;
        sum_squares_right += right * right;

        // Check for clipping
        if (std::abs(left) > 0.99f || std::abs(right) > 0.99f) {
            stats_.clipping_events++;
        }
    }

    stats_.peak_level_left = peak_left;
    stats_.peak_level_right = peak_right;
    stats_.rms_level_left = std::sqrt(sum_squares_left / sample_count);
    stats_.rms_level_right = std::sqrt(sum_squares_right / sample_count);
    stats_.total_samples_processed += sample_count;

    // Note: active_channels will be updated separately to avoid deadlock
    // with channels_mutex_ when called from mix_to_output
}

void SimpleMixer::set_error(MixerError error, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    last_error_message_ = message;
}

} // namespace ve::audio