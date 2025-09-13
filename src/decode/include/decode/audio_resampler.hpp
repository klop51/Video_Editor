#pragma once
#include "frame.hpp"
#include <optional>
#include <cstdint>

namespace ve::decode {

struct ResampleParams {
    int in_rate = 0;
    int in_channels = 0;
    SampleFormat in_format = SampleFormat::Unknown;
    int out_rate = 48000;
    int out_channels = 2;
    SampleFormat out_format = SampleFormat::FLTP;
};

class AudioResampler {
public:
    AudioResampler();
    ~AudioResampler();
    bool init(const ResampleParams& params);
    std::optional<AudioFrame> resample(const AudioFrame& input);
private:
    struct Impl; Impl* impl_ = nullptr; // PIMPL to hide FFmpeg headers
    ResampleParams params_{};
};

} // namespace ve::decode
