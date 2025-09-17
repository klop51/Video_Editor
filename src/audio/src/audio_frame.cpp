#include "audio/audio_frame.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace ve::audio {

// Static methods for sample format handling
size_t AudioFrame::bytes_per_sample(SampleFormat format) {
    switch (format) {
        case SampleFormat::Int16:   return 2;
        case SampleFormat::Int32:   return 4;
        case SampleFormat::Float32: return 4;
        case SampleFormat::Unknown: return 0;
    }
    return 0;
}

ChannelLayout AudioFrame::deduce_channel_layout(uint16_t channel_count) {
    switch (channel_count) {
        case 1: return ChannelLayout::Mono;
        case 2: return ChannelLayout::Stereo;
        case 3: return ChannelLayout::Stereo21;
        case 6: return ChannelLayout::Surround51;
        case 8: return ChannelLayout::Surround71;
        default: return ChannelLayout::Unknown;
    }
}

const char* AudioFrame::format_string(SampleFormat format) {
    switch (format) {
        case SampleFormat::Int16:   return "int16";
        case SampleFormat::Int32:   return "int32";
        case SampleFormat::Float32: return "float32";
        case SampleFormat::Unknown: return "unknown";
    }
    return "unknown";
}

// Factory methods
std::shared_ptr<AudioFrame> AudioFrame::create(
    uint32_t sample_rate,
    uint16_t channel_count,
    uint32_t sample_count,
    SampleFormat format,
    const TimePoint& timestamp
) {
    if (sample_rate == 0 || channel_count == 0 || sample_count == 0 || format == SampleFormat::Unknown) {
        return nullptr;
    }

    auto frame = std::shared_ptr<AudioFrame>(new AudioFrame(
        sample_rate, channel_count, sample_count, format, timestamp
    ));

    // Allocate data buffer
    size_t total_samples = static_cast<size_t>(channel_count) * sample_count;
    size_t data_size = total_samples * bytes_per_sample(format);
    frame->data_.resize(data_size, 0); // Initialize to silence

    return frame;
}

std::shared_ptr<AudioFrame> AudioFrame::create_from_data(
    uint32_t sample_rate,
    uint16_t channel_count,
    uint32_t sample_count,
    SampleFormat format,
    const TimePoint& timestamp,
    const void* data,
    size_t data_size
) {
    auto frame = create(sample_rate, channel_count, sample_count, format, timestamp);
    if (!frame || !data) {
        return nullptr;
    }

    // Copy data, ensuring we don't exceed buffer bounds
    size_t copy_size = std::min(data_size, frame->data_.size());
    std::memcpy(frame->data_.data(), data, copy_size);

    return frame;
}

// Private constructor
AudioFrame::AudioFrame(uint32_t sample_rate, uint16_t channel_count, uint32_t sample_count,
                       SampleFormat format, const TimePoint& timestamp)
    : sample_rate_(sample_rate)
    , channel_count_(channel_count)
    , sample_count_(sample_count)
    , format_(format)
    , timestamp_(timestamp) {
}

// Clone method
std::shared_ptr<AudioFrame> AudioFrame::clone(SampleFormat new_format) const {
    SampleFormat target_format = (new_format == SampleFormat::Unknown) ? format_ : new_format;
    
    if (target_format == format_) {
        // Simple copy
        return create_from_data(
            sample_rate_, channel_count_, sample_count_, format_, 
            timestamp_, data_.data(), data_.size()
        );
    } else {
        // Format conversion required
        return frame_utils::convert_format(
            std::const_pointer_cast<AudioFrame>(shared_from_this()), 
            target_format
        );
    }
}

// Sample access methods
float AudioFrame::get_sample_as_float(uint16_t channel, uint32_t sample) const {
    if (channel >= channel_count_ || sample >= sample_count_) {
        return 0.0f;
    }

    size_t index = static_cast<size_t>(sample) * channel_count_ + channel;
    
    switch (format_) {
        case SampleFormat::Int16: {
            const int16_t* samples = reinterpret_cast<const int16_t*>(data_.data());
            return static_cast<float>(samples[index]) / 32768.0f;
        }
        case SampleFormat::Int32: {
            const int32_t* samples = reinterpret_cast<const int32_t*>(data_.data());
            return static_cast<float>(samples[index]) / 2147483648.0f;
        }
        case SampleFormat::Float32: {
            const float* samples = reinterpret_cast<const float*>(data_.data());
            return samples[index];
        }
        case SampleFormat::Unknown:
        default:
            return 0.0f;
    }
}

void AudioFrame::set_sample_from_float(uint16_t channel, uint32_t sample, float value) {
    if (channel >= channel_count_ || sample >= sample_count_) {
        return;
    }

    // Clamp value to valid range
    value = std::clamp(value, -1.0f, 1.0f);
    
    size_t index = static_cast<size_t>(sample) * channel_count_ + channel;
    
    switch (format_) {
        case SampleFormat::Int16: {
            int16_t* samples = reinterpret_cast<int16_t*>(data_.data());
            samples[index] = static_cast<int16_t>(value * 32767.0f);
            break;
        }
        case SampleFormat::Int32: {
            int32_t* samples = reinterpret_cast<int32_t*>(data_.data());
            samples[index] = static_cast<int32_t>(value * 2147483647.0f);
            break;
        }
        case SampleFormat::Float32: {
            float* samples = reinterpret_cast<float*>(data_.data());
            samples[index] = value;
            break;
        }
        case SampleFormat::Unknown:
        default:
            break;
    }
}

// Frame utilities implementation
namespace frame_utils {

    std::shared_ptr<AudioFrame> mix_frames(
        const std::shared_ptr<AudioFrame>& frame1,
        const std::shared_ptr<AudioFrame>& frame2,
        float gain1,
        float gain2
    ) {
        if (!frame1 || !frame2) return nullptr;
        
        // Frames must have compatible parameters
        if (frame1->sample_rate() != frame2->sample_rate() ||
            frame1->channel_count() != frame2->channel_count() ||
            frame1->format() != frame2->format()) {
            return nullptr;
        }

        uint32_t mix_sample_count = std::min(frame1->sample_count(), frame2->sample_count());
        
        auto result = AudioFrame::create(
            frame1->sample_rate(),
            frame1->channel_count(),
            mix_sample_count,
            frame1->format(),
            frame1->timestamp() // Use first frame's timestamp
        );

        if (!result) return nullptr;

        // Mix samples
        for (uint16_t ch = 0; ch < frame1->channel_count(); ++ch) {
            for (uint32_t s = 0; s < mix_sample_count; ++s) {
                float sample1 = frame1->get_sample_as_float(ch, s);
                float sample2 = frame2->get_sample_as_float(ch, s);
                float mixed = (sample1 * gain1) + (sample2 * gain2);
                
                // Soft clipping to prevent harsh clipping artifacts
                mixed = std::tanh(mixed);
                
                result->set_sample_from_float(ch, s, mixed);
            }
        }

        return result;
    }

    std::shared_ptr<AudioFrame> apply_gain(
        const std::shared_ptr<AudioFrame>& frame,
        float gain_db
    ) {
        if (!frame) return nullptr;

        float linear_gain = std::pow(10.0f, gain_db / 20.0f);
        
        auto result = frame->clone();
        if (!result) return nullptr;

        // Apply gain to all samples
        for (uint16_t ch = 0; ch < frame->channel_count(); ++ch) {
            for (uint32_t s = 0; s < frame->sample_count(); ++s) {
                float sample = frame->get_sample_as_float(ch, s);
                sample *= linear_gain;
                sample = std::clamp(sample, -1.0f, 1.0f);
                result->set_sample_from_float(ch, s, sample);
            }
        }

        return result;
    }

    std::shared_ptr<AudioFrame> convert_format(
        const std::shared_ptr<AudioFrame>& frame,
        SampleFormat target_format
    ) {
        if (!frame || target_format == SampleFormat::Unknown) return nullptr;
        if (frame->format() == target_format) return frame->clone();

        auto result = AudioFrame::create(
            frame->sample_rate(),
            frame->channel_count(),
            frame->sample_count(),
            target_format,
            frame->timestamp()
        );

        if (!result) return nullptr;

        // Convert all samples
        for (uint16_t ch = 0; ch < frame->channel_count(); ++ch) {
            for (uint32_t s = 0; s < frame->sample_count(); ++s) {
                float sample = frame->get_sample_as_float(ch, s);
                result->set_sample_from_float(ch, s, sample);
            }
        }

        return result;
    }

    std::shared_ptr<AudioFrame> extract_channels(
        const std::shared_ptr<AudioFrame>& frame,
        const std::vector<uint16_t>& channels
    ) {
        if (!frame || channels.empty()) return nullptr;

        // Validate channel indices
        for (uint16_t ch : channels) {
            if (ch >= frame->channel_count()) {
                return nullptr;
            }
        }

        auto result = AudioFrame::create(
            frame->sample_rate(),
            static_cast<uint16_t>(channels.size()),
            frame->sample_count(),
            frame->format(),
            frame->timestamp()
        );

        if (!result) return nullptr;

        // Extract specified channels
        for (uint16_t out_ch = 0; out_ch < static_cast<uint16_t>(channels.size()); ++out_ch) {
            uint16_t in_ch = channels[out_ch];
            for (uint32_t s = 0; s < frame->sample_count(); ++s) {
                float sample = frame->get_sample_as_float(in_ch, s);
                result->set_sample_from_float(out_ch, s, sample);
            }
        }

        return result;
    }

} // namespace frame_utils

} // namespace ve::audio