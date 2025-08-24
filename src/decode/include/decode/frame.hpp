#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace ve::decode {

enum class PixelFormat { 
    Unknown, 
    RGB24, RGBA32, BGR24, BGRA32,
    YUV420P, YUV422P, YUV444P, 
    YUV410P, YUV411P, YUV440P,
    YUYV422, UYVY422,
    NV12, NV21, NV16, NV24,
    YUV420P10LE, YUV422P10LE, YUV444P10LE,
    YUV420P12LE, YUV422P12LE, YUV444P12LE,
    GRAY8, GRAY16LE,
    P010LE, P016LE
};

enum class ColorSpace { 
    Unknown, 
    BT601, BT709, BT2020, 
    SMPTE170M, SMPTE240M, 
    BT470BG, BT470M,
    FILM, SMPTE428, SMPTE431, SMPTE432
};

enum class ColorRange { Unknown, Limited, Full };

enum class SampleFormat { Unknown, S16, FLT, FLTP }; // simplified

struct VideoFrame {
    int64_t pts = 0; // in microseconds
    int32_t width = 0;
    int32_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    ColorSpace color_space = ColorSpace::Unknown;
    ColorRange color_range = ColorRange::Unknown;
    std::vector<uint8_t> data; // packed or planar (future: separate planes)
};

struct AudioFrame {
    int64_t pts = 0; // in microseconds
    int32_t sample_rate = 0;
    int32_t channels = 0;
    SampleFormat format = SampleFormat::Unknown;
    std::vector<uint8_t> data; // interleaved or planar (flag TBD)
};

} // namespace ve::decode
