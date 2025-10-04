#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace ve::core {
    class StageTimer;
}

namespace ve::decode {

enum class PixelFormat { 
    Unknown, 
    
    // Basic RGB formats
    RGB24, RGBA32, BGR24, BGRA32,
    
    // Professional 16-bit RGB formats (Phase 1 Week 1)
    RGB48LE, RGB48BE, RGBA64LE, RGBA64BE,
    
    // Basic YUV formats
    YUV420P, YUV422P, YUV444P, 
    YUV410P, YUV411P, YUV440P,
    YUYV422, UYVY422,
    
    // Professional 16-bit YUV formats (Phase 1 Week 1)
    YUV420P16LE, YUV422P16LE, YUV444P16LE,
    
    // Professional packed formats (Phase 1 Week 1)
    V210,           // 10-bit 4:2:2 YUV packed
    V410,           // 10-bit 4:4:4 YUV packed
    
    // Alpha variants for professional workflows (Phase 1 Week 1)
    YUVA420P, YUVA422P, YUVA444P,
    YUVA420P10LE, YUVA422P10LE, YUVA444P10LE,
    YUVA420P16LE, YUVA422P16LE, YUVA444P16LE,
    
    // Existing high bit depth
    YUV420P10LE, YUV422P10LE, YUV444P10LE,
    YUV420P12LE, YUV422P12LE, YUV444P12LE,
    
    // NV (semi-planar) formats
    NV12, NV21, NV16, NV24,
    NV20LE, NV20BE,    // 10-bit semi-planar
    
    // Grayscale formats
    GRAY8, GRAY16LE,
    
    // P010/P016 for HDR
    P010LE, P016LE,
    
    // Additional professional formats for broadcast/cinema
    UYVY422_10BIT,     // 10-bit packed 4:2:2
    YUV422P14LE,       // 14-bit planar (some cinema cameras)
    YUV444P14LE,       // 14-bit planar
    GBRP,              // Planar RGB
    GBRP10LE, GBRP12LE, GBRP16LE // High bit depth planar RGB
};

enum class ColorSpace { 
    Unknown, 
    
    // Standard broadcast color spaces
    BT601, BT709, BT2020, 
    SMPTE170M, SMPTE240M, 
    BT470BG, BT470M,
    FILM, SMPTE428, SMPTE431, SMPTE432,
    
    // Professional color spaces (Phase 1 Week 1)
    DCI_P3,           // Digital Cinema Initiative P3
    DISPLAY_P3,       // Apple Display P3
    BT2020_NCL,       // BT.2020 Non-Constant Luminance
    BT2020_CL,        // BT.2020 Constant Luminance
    SMPTE_C,          // SMPTE-C (legacy broadcast)
    ADOBE_RGB,        // Adobe RGB (1998)
    PROPHOTO_RGB,     // ProPhoto RGB (wide gamut)
    
    // Additional cinema and broadcast spaces
    ACES_CG,          // ACES working space
    ACES_CC,          // ACES color correction space
    ALEXA_WIDE_GAMUT, // ARRI Alexa Wide Gamut
    SONY_SGAMUT3,     // Sony S-Gamut3
    CANON_CINEMA_GAMUT, // Canon Cinema Gamut
    BLACKMAGIC_WIDE_GAMUT, // Blackmagic Wide Gamut
    DAVINCI_WIDE_GAMUT,    // DaVinci Wide Gamut
    
    // HDR transfer functions (related to color space)
    HDR10_ST2084,     // SMPTE ST 2084 (PQ)
    HLG_ARIB_STD_B67, // Hybrid Log-Gamma
    DOLBY_VISION      // Dolby Vision enhancement layer
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
    std::shared_ptr<ve::core::StageTimer> timing;
};

struct AudioFrame {
    int64_t pts = 0; // in microseconds
    int32_t sample_rate = 0;
    int32_t channels = 0;
    SampleFormat format = SampleFormat::Unknown;
    std::vector<uint8_t> data; // interleaved or planar (flag TBD)
};

} // namespace ve::decode
