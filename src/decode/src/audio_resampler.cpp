// Full implementation only when FFmpeg is enabled
#include "decode/audio_resampler.hpp"
#if VE_HAVE_FFMPEG
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/samplefmt.h>
}
#include <vector>
#include <cstring>

namespace ve::decode {

struct AudioResampler::Impl {
    SwrContext* swr = nullptr;
};

AudioResampler::AudioResampler() : impl_(new Impl) {}
AudioResampler::~AudioResampler() {
    if(impl_->swr) swr_free(&impl_->swr);
    delete impl_;
}

static AVSampleFormat to_av(SampleFormat f) {
    switch(f) {
        case SampleFormat::S16: return AV_SAMPLE_FMT_S16;
        case SampleFormat::FLTP: return AV_SAMPLE_FMT_FLTP;
        case SampleFormat::FLT: return AV_SAMPLE_FMT_FLT;
        default: return AV_SAMPLE_FMT_NONE;
    }
}

bool AudioResampler::init(const ResampleParams& params) {
    params_ = params;
    if(impl_->swr) swr_free(&impl_->swr);
    
    // Set up channel layouts for new FFmpeg API
    AVChannelLayout in_ch_layout, out_ch_layout;
    av_channel_layout_default(&in_ch_layout, params.in_channels);
    av_channel_layout_default(&out_ch_layout, params.out_channels);
    
    int ret = swr_alloc_set_opts2(&impl_->swr,
        &out_ch_layout, to_av(params.out_format), params.out_rate,
        &in_ch_layout, to_av(params.in_format), params.in_rate,
        0, nullptr);
    
    av_channel_layout_uninit(&in_ch_layout);
    av_channel_layout_uninit(&out_ch_layout);
    
    if(ret < 0) return false;
    if(swr_init(impl_->swr) < 0) return false;
    return true;
}

std::optional<AudioFrame> AudioResampler::resample(const AudioFrame& input) {
    if(!impl_->swr) return std::nullopt;
    int in_channels = params_.in_channels;
    int out_channels = params_.out_channels;
    int in_samples = 0;
    AVSampleFormat in_fmt = to_av(input.format);
    AVSampleFormat out_fmt = to_av(params_.out_format);
    if(in_fmt == AV_SAMPLE_FMT_NONE || out_fmt == AV_SAMPLE_FMT_NONE) return std::nullopt;

    if(input.format == SampleFormat::FLTP) {
        in_samples = static_cast<int>(input.data.size() / (sizeof(float) * static_cast<size_t>(in_channels)));
    } else if(input.format == SampleFormat::S16) {
        in_samples = static_cast<int>(input.data.size() / (2 * static_cast<size_t>(in_channels)));
    } else if(input.format == SampleFormat::FLT) {
        in_samples = static_cast<int>(input.data.size() / (sizeof(float) * static_cast<size_t>(in_channels)));
    } else {
        return std::nullopt;
    }

    int max_out_samples = static_cast<int>(av_rescale_rnd(swr_get_delay(impl_->swr, params_.in_rate) + in_samples, params_.out_rate, params_.in_rate, AV_ROUND_UP));
    int out_line_size = 0;
    uint8_t** out_data = nullptr;
    if(av_samples_alloc_array_and_samples(&out_data, &out_line_size, out_channels, max_out_samples, out_fmt, 0) < 0) {
        return std::nullopt;
    }

    const uint8_t* in_data[AV_NUM_DATA_POINTERS] = {0};
    in_data[0] = input.data.data();

    int converted = swr_convert(impl_->swr, out_data, max_out_samples, in_data, in_samples);
    if(converted < 0) {
        if(out_data) {
            av_freep(&out_data[0]);
            av_freep(&out_data);
        }
        return std::nullopt;
    }

    AudioFrame out;
    out.pts = input.pts;
    out.sample_rate = params_.out_rate;
    out.channels = out_channels;
    out.format = params_.out_format;
    size_t sample_size = (out.format == SampleFormat::S16) ? 2 : sizeof(float);
    size_t total_bytes = static_cast<size_t>(converted) * static_cast<size_t>(out.channels) * sample_size;
    out.data.resize(total_bytes);
    if(out.format == SampleFormat::FLTP) {
        float* dst = reinterpret_cast<float*>(out.data.data());
        for(int s=0; s<converted; ++s) {
            for(int c=0; c<out.channels; ++c) {
                float* plane = reinterpret_cast<float*>(out_data[c]);
                dst[s * out.channels + c] = plane[s];
            }
        }
        out.format = SampleFormat::FLT;
    } else if(out.format == SampleFormat::S16) {
        std::memcpy(out.data.data(), out_data[0], total_bytes);
    }

    if(out_data) {
        av_freep(&out_data[0]);
        av_freep(&out_data);
    }
    return out;
}

} // namespace ve::decode
#else // !VE_HAVE_FFMPEG

#include <optional>
namespace ve::decode {
struct AudioResampler::Impl {};
AudioResampler::AudioResampler() : impl_(nullptr) {}
AudioResampler::~AudioResampler() = default;
bool AudioResampler::init(const ResampleParams& params) { (void)params; return false; }
std::optional<AudioFrame> AudioResampler::resample(const AudioFrame& input) { (void)input; return std::nullopt; }
} // namespace ve::decode

#endif // VE_HAVE_FFMPEG
