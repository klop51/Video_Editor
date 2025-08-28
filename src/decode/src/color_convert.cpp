#include "decode/color_convert.hpp"
#include "decode/frame.hpp"
#include "core/log.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <optional>
#include <memory>

namespace ve::decode {

namespace {
    inline uint8_t clamp8(int v) { return (v < 0) ? 0 : (v > 255 ? 255 : (uint8_t)v); }

    const char* get_color_space_name(ColorSpace cs) {
        switch(cs) {
            case ColorSpace::BT709: return "BT.709";
            case ColorSpace::BT601: return "BT.601";
            case ColorSpace::BT2020: return "BT.2020";
            case ColorSpace::SMPTE240M: return "SMPTE-240M";
            case ColorSpace::SMPTE428: return "SMPTE-428";
            case ColorSpace::FILM: return "Film";
            default: return "Unknown";
        }
    }

    // YUV to RGB conversion with proper color space handling
    void yuv_to_rgb(int Y, int U, int V, ColorSpace color_space, ColorRange color_range, uint8_t* rgba_out) {
        int C, D, E;
        
        if (color_range == ColorRange::Full) {
            C = Y;
            D = U - 128;
            E = V - 128;
        } else {
            C = Y - 16;
            D = U - 128;
            E = V - 128;
        }
        
        int R, G, B;
        
        switch(color_space) {
            case ColorSpace::BT709:
                if (color_range == ColorRange::Full) {
                    R = (256 * C + 403 * E + 128) >> 8;
                    G = (256 * C - 48 * D - 120 * E + 128) >> 8;
                    B = (256 * C + 475 * D + 128) >> 8;
                } else {
                    R = (298 * C + 460 * E + 128) >> 8;
                    G = (298 * C - 55 * D - 137 * E + 128) >> 8;
                    B = (298 * C + 543 * D + 128) >> 8;
                }
                break;
            case ColorSpace::BT2020:
                if (color_range == ColorRange::Full) {
                    R = (256 * C + 360 * E + 128) >> 8;
                    G = (256 * C - 41 * D - 107 * E + 128) >> 8;
                    B = (256 * C + 512 * D + 128) >> 8;
                } else {
                    R = (298 * C + 410 * E + 128) >> 8;
                    G = (298 * C - 47 * D - 122 * E + 128) >> 8;
                    B = (298 * C + 584 * D + 128) >> 8;
                }
                break;
            default: // BT.601
                if (color_range == ColorRange::Full) {
                    R = (256 * C + 359 * E + 128) >> 8;
                    G = (256 * C - 88 * D - 183 * E + 128) >> 8;
                    B = (256 * C + 454 * D + 128) >> 8;
                } else {
                    R = (298 * C + 409 * E + 128) >> 8;
                    G = (298 * C - 100 * D - 208 * E + 128) >> 8;
                    B = (298 * C + 516 * D + 128) >> 8;
                }
                break;
        }
        
        rgba_out[0] = clamp8(R);
        rgba_out[1] = clamp8(G);
        rgba_out[2] = clamp8(B);
        rgba_out[3] = 255;
    }

    // RGB24 to RGBA conversion
    std::optional<VideoFrame> convert_rgb24_to_rgba(const VideoFrame& src, VideoFrame& out) {
        const uint8_t* src_data = src.data.data();
        uint8_t* out_data = out.data.data();
        const size_t pixel_count = static_cast<size_t>(out.width) * out.height;
        
        for(size_t i = 0; i < pixel_count; ++i) {
            out_data[i*4 + 0] = src_data[i*3 + 0]; // R
            out_data[i*4 + 1] = src_data[i*3 + 1]; // G
            out_data[i*4 + 2] = src_data[i*3 + 2]; // B
            out_data[i*4 + 3] = 255;               // A
        }
        return out;
    }

    // BGR24 to RGBA conversion
    std::optional<VideoFrame> convert_bgr24_to_rgba(const VideoFrame& src, VideoFrame& out) {
        const uint8_t* src_data = src.data.data();
        uint8_t* out_data = out.data.data();
        const size_t pixel_count = static_cast<size_t>(out.width) * out.height;
        
        for(size_t i = 0; i < pixel_count; ++i) {
            out_data[i*4 + 0] = src_data[i*3 + 2]; // R
            out_data[i*4 + 1] = src_data[i*3 + 1]; // G
            out_data[i*4 + 2] = src_data[i*3 + 0]; // B
            out_data[i*4 + 3] = 255;               // A
        }
        return out;
    }

    // BGRA32 to RGBA conversion
    std::optional<VideoFrame> convert_bgra32_to_rgba(const VideoFrame& src, VideoFrame& out) {
        const uint8_t* src_data = src.data.data();
        uint8_t* out_data = out.data.data();
        const size_t pixel_count = static_cast<size_t>(out.width) * out.height;
        
        for(size_t i = 0; i < pixel_count; ++i) {
            out_data[i*4 + 0] = src_data[i*4 + 2]; // R
            out_data[i*4 + 1] = src_data[i*4 + 1]; // G
            out_data[i*4 + 2] = src_data[i*4 + 0]; // B
            out_data[i*4 + 3] = src_data[i*4 + 3]; // A
        }
        return out;
    }

    // YUV420P to RGBA conversion
    std::optional<VideoFrame> convert_yuv420p_to_rgba(const VideoFrame& src, VideoFrame& out) {
        const int W = src.width;
        const int H = src.height;
        const uint8_t* Y_plane = src.data.data();
        const uint8_t* U_plane = Y_plane + W * H;
        const uint8_t* V_plane = U_plane + (W * H) / 4;
        uint8_t* rgba_data = out.data.data();

        for(int y = 0; y < H; ++y) {
            for(int x = 0; x < W; ++x) {
                const int Y = Y_plane[y * W + x];
                const int U = U_plane[(y/2) * (W/2) + (x/2)];
                const int V = V_plane[(y/2) * (W/2) + (x/2)];
                
                yuv_to_rgb(Y, U, V, src.color_space, src.color_range, &rgba_data[(y * W + x) * 4]);
            }
        }
        return out;
    }

    // Simple implementations for other formats (can be expanded)
    std::optional<VideoFrame> convert_yuv422p_to_rgba(const VideoFrame& src, VideoFrame& out) {
        // For now, use YUV420P logic (simplified)
        return convert_yuv420p_to_rgba(src, out);
    }

    std::optional<VideoFrame> convert_yuv444p_to_rgba(const VideoFrame& src, VideoFrame& out) {
        // For now, use YUV420P logic (simplified)
        return convert_yuv420p_to_rgba(src, out);
    }

    std::optional<VideoFrame> convert_yuv420p10_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        // For now, return null (needs 10-bit handling)
        ve::log::warn("YUV420P10 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_yuv422p10_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("YUV422P10 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_yuv444p10_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("YUV444P10 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_nv12_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("NV12 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_nv21_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("NV21 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_p010_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("P010 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_yuyv422_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("YUYV422 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_uyvy422_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("UYVY422 conversion not yet implemented");
        return std::nullopt;
    }

    std::optional<VideoFrame> convert_gray8_to_rgba(const VideoFrame& src, VideoFrame& out) {
        const uint8_t* src_data = src.data.data();
        uint8_t* out_data = out.data.data();
        const size_t pixel_count = static_cast<size_t>(out.width) * out.height;
        
        for(size_t i = 0; i < pixel_count; ++i) {
            const uint8_t gray = src_data[i];
            out_data[i*4 + 0] = gray; // R
            out_data[i*4 + 1] = gray; // G
            out_data[i*4 + 2] = gray; // B
            out_data[i*4 + 3] = 255;  // A
        }
        return out;
    }

    std::optional<VideoFrame> convert_gray16_to_rgba([[maybe_unused]] const VideoFrame& src, [[maybe_unused]] VideoFrame& out) {
        ve::log::warn("GRAY16 conversion not yet implemented");
        return std::nullopt;
    }
}

// FFmpeg swscale fast path (guarded)
#if VE_HAVE_FFMPEG
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
}

static AVPixelFormat to_avpf(PixelFormat pf) {
    switch(pf) {
        case PixelFormat::RGB24:   return AV_PIX_FMT_RGB24;
        case PixelFormat::RGBA32:  return AV_PIX_FMT_RGBA;
        case PixelFormat::BGR24:   return AV_PIX_FMT_BGR24;
        case PixelFormat::BGRA32:  return AV_PIX_FMT_BGRA;
        case PixelFormat::YUV420P: return AV_PIX_FMT_YUV420P;
        case PixelFormat::YUV422P: return AV_PIX_FMT_YUV422P;
        case PixelFormat::YUV444P: return AV_PIX_FMT_YUV444P;
        case PixelFormat::NV12:    return AV_PIX_FMT_NV12;
        case PixelFormat::NV21:    return AV_PIX_FMT_NV21;
        case PixelFormat::YUYV422: return AV_PIX_FMT_YUYV422;
        case PixelFormat::UYVY422: return AV_PIX_FMT_UYVY422;
        case PixelFormat::GRAY8:   return AV_PIX_FMT_GRAY8;
        case PixelFormat::P010LE:  return AV_PIX_FMT_P010LE;
        default: return AV_PIX_FMT_NONE;
    }
}

static bool fill_src_planes(const VideoFrame& src, AVPixelFormat avpf,
                            uint8_t* data[4], int linesize[4]) {
    const int w = src.width;
    const int h = src.height;
    const uint8_t* base = src.data.data();
    memset(data, 0, sizeof(uint8_t*)*4);
    memset(linesize, 0, sizeof(int)*4);

    switch(avpf) {
        case AV_PIX_FMT_YUV420P: {
            const int y_size = w * h;
            const int c_w = (w+1)/2, c_h = (h+1)/2;
            const int c_size = c_w * c_h;
            data[0] = const_cast<uint8_t*>(base);
            data[1] = const_cast<uint8_t*>(base + y_size);
            data[2] = const_cast<uint8_t*>(base + y_size + c_size);
            linesize[0] = w;
            linesize[1] = c_w;
            linesize[2] = c_w;
            return true;
        }
        case AV_PIX_FMT_YUV422P: {
            const int y_size = w*h;
            const int c_w = (w+1)/2, c_h = h;
            const int u_size = c_w * c_h;
            data[0] = const_cast<uint8_t*>(base);
            data[1] = const_cast<uint8_t*>(base + y_size);
            data[2] = const_cast<uint8_t*>(base + y_size + u_size);
            linesize[0] = w; linesize[1] = c_w; linesize[2] = c_w;
            return true;
        }
        case AV_PIX_FMT_YUV444P: {
            const int y_size = w*h;
            const int c_size = y_size;
            data[0] = const_cast<uint8_t*>(base);
            data[1] = const_cast<uint8_t*>(base + y_size);
            data[2] = const_cast<uint8_t*>(base + y_size + c_size);
            linesize[0] = w; linesize[1] = w; linesize[2] = w;
            return true;
        }
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21: {
            const int y_size = w*h;
            data[0] = const_cast<uint8_t*>(base);
            data[1] = const_cast<uint8_t*>(base + y_size);
            linesize[0] = w; linesize[1] = w;
            return true;
        }
        case AV_PIX_FMT_YUYV422:
        case AV_PIX_FMT_UYVY422: {
            data[0] = const_cast<uint8_t*>(base);
            linesize[0] = w * 2;
            return true;
        }
        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_BGR24: {
            data[0] = const_cast<uint8_t*>(base);
            linesize[0] = w * 3;
            return true;
        }
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_RGBA: {
            data[0] = const_cast<uint8_t*>(base);
            linesize[0] = w * 4;
            return true;
        }
        case AV_PIX_FMT_GRAY8: {
            data[0] = const_cast<uint8_t*>(base);
            linesize[0] = w;
            return true;
        }
        case AV_PIX_FMT_P010LE: {
            data[0] = const_cast<uint8_t*>(base);
            linesize[0] = w * 2;
            // second plane interleaved UV 10-bit
            const int y_bytes = w * 2 * h;
            data[1] = const_cast<uint8_t*>(base + y_bytes);
            linesize[1] = w * 2;
            return true;
        }
        default:
            return false;
    }
}

static std::optional<VideoFrame> to_rgba_scaled_ffmpeg(const VideoFrame& src, int target_w, int target_h) noexcept {
    if (src.data.empty() || src.width <= 0 || src.height <= 0) return std::nullopt;
    if (target_w <= 0) target_w = src.width;
    if (target_h <= 0) target_h = src.height;

    const AVPixelFormat src_fmt = to_avpf(src.format);
    if (src_fmt == AV_PIX_FMT_NONE) return std::nullopt;

    SwsContext* ctx = sws_getContext(src.width, src.height, src_fmt,
                                     target_w, target_h, AV_PIX_FMT_RGBA,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!ctx) return std::nullopt;

    uint8_t* src_data[4];
    int src_linesize[4];
    if (!fill_src_planes(src, src_fmt, src_data, src_linesize)) {
        sws_freeContext(ctx);
        return std::nullopt;
    }

    VideoFrame out;
    out.width = target_w;
    out.height = target_h;
    out.pts = src.pts;
    out.format = PixelFormat::RGBA32;
    out.color_space = src.color_space;
    out.color_range = src.color_range;
    out.data.resize(static_cast<size_t>(target_w) * target_h * 4);

    uint8_t* dst_data[4] = { out.data.data(), nullptr, nullptr, nullptr };
    int dst_linesize[4] = { target_w * 4, 0, 0, 0 };

    int res = sws_scale(ctx, src_data, src_linesize, 0, src.height, dst_data, dst_linesize);
    sws_freeContext(ctx);
    if (res <= 0) return std::nullopt;
    return out;
}
#endif // VE_HAVE_FFMPEG

std::optional<VideoFrame> to_rgba_scaled(const VideoFrame& src, int target_w, int target_h) noexcept {
    if(src.format == PixelFormat::RGBA32 && (target_w <=0 || target_h <=0 || (target_w==src.width && target_h==src.height))) return src;

#if VE_HAVE_FFMPEG
    if (auto out = to_rgba_scaled_ffmpeg(src, target_w, target_h)) {
        return out;
    }
#endif

    // Fallback to existing converters (no scaling here); caller may scale QImage/QPixmap
    // by display size. If scaling requested, we'll return source-size RGBA and let UI scale it.
    VideoFrame out;
    out.width = src.width;
    out.height = src.height;
    out.pts = src.pts;
    out.format = PixelFormat::RGBA32;
    out.color_space = src.color_space;
    out.color_range = src.color_range;
    out.data.resize(static_cast<size_t>(out.width) * out.height * 4);

    // Log color space info once per conversion
    static bool logged = false;
    if (!logged) {
        const char* cs_name = get_color_space_name(src.color_space);
        const char* range_name = (src.color_range == ColorRange::Full) ? "Full" : 
                                (src.color_range == ColorRange::Limited) ? "Limited" : "Unknown";
        ve::log::info("Color conversion: " + std::string(cs_name) + " " + std::string(range_name) + " range");
        logged = true;
    }

    // Dispatch to format-specific conversion
    switch(src.format) {
        case PixelFormat::RGB24: return convert_rgb24_to_rgba(src, out);
        case PixelFormat::BGR24: return convert_bgr24_to_rgba(src, out);
        case PixelFormat::BGRA32: return convert_bgra32_to_rgba(src, out);
        case PixelFormat::YUV420P: return convert_yuv420p_to_rgba(src, out);
        case PixelFormat::YUV422P: return convert_yuv422p_to_rgba(src, out);
        case PixelFormat::YUV444P: return convert_yuv444p_to_rgba(src, out);
        case PixelFormat::YUV420P10LE: return convert_yuv420p10_to_rgba(src, out);
        case PixelFormat::YUV422P10LE: return convert_yuv422p10_to_rgba(src, out);
        case PixelFormat::YUV444P10LE: return convert_yuv444p10_to_rgba(src, out);
        case PixelFormat::NV12: return convert_nv12_to_rgba(src, out);
        case PixelFormat::NV21: return convert_nv21_to_rgba(src, out);
        case PixelFormat::P010LE: return convert_p010_to_rgba(src, out);
        case PixelFormat::YUYV422: return convert_yuyv422_to_rgba(src, out);
        case PixelFormat::UYVY422: return convert_uyvy422_to_rgba(src, out);
        case PixelFormat::GRAY8: return convert_gray8_to_rgba(src, out);
        case PixelFormat::GRAY16LE: return convert_gray16_to_rgba(src, out);
        default:
            ve::log::error("Unsupported pixel format for conversion: " + std::to_string(static_cast<int>(src.format)));
            return std::nullopt;
    }
}

std::optional<VideoFrame> to_rgba(const VideoFrame& src) noexcept {
    return to_rgba_scaled(src, src.width, src.height);
}

} // namespace ve::decode
