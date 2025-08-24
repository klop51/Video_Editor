#pragma once
#include "decode/frame.hpp"
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <optional>

namespace ve::decode {

struct OpenParams {
    std::string filepath;
    bool video = true;
    bool audio = true;
    bool hw_accel = true; // future use
};

struct StreamSelection {
    int video_stream_index = 0;
    int audio_stream_index = 0;
};

struct DecoderStats {
    int64_t video_frames_decoded = 0;
    int64_t audio_frames_decoded = 0;
};

class IDecoder {
public:
    virtual ~IDecoder() = default;
    virtual bool open(const OpenParams& params) = 0;
    virtual bool seek_microseconds(int64_t pts_us) = 0;
    virtual std::optional<VideoFrame> read_video() = 0;
    virtual std::optional<AudioFrame> read_audio() = 0;
    virtual const DecoderStats& stats() const = 0;
};

std::unique_ptr<IDecoder> create_decoder();

} // namespace ve::decode
