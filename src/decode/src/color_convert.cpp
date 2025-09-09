#include "decode/color_convert.hpp"
#include "decode/frame.hpp"
#include "decode/rgba_view.hpp"
#include "core/log.hpp"
#include "core/profiling.hpp" // added for VE_PROFILE_SCOPE
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <optional>
#include <memory>
#include <thread>
#include <cstdlib>

#ifndef VE_HEAP_DEBUG
#define VE_HEAP_DEBUG 0
#endif

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
        const size_t pixel_count = static_cast<size_t>(out.width) * static_cast<size_t>(out.height);
        
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
        const size_t pixel_count = static_cast<size_t>(out.width) * static_cast<size_t>(out.height);
        
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
        const size_t pixel_count = static_cast<size_t>(out.width) * static_cast<size_t>(out.height);
        
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
        const size_t y_size = static_cast<size_t>(W) * static_cast<size_t>(H);
        const size_t uv_size = (static_cast<size_t>(W) * static_cast<size_t>(H)) / 4;
        const size_t needed = y_size + 2 * uv_size;
        if (src.data.size() < needed) {
            ve::log::error("convert_yuv420p_to_rgba: undersized buffer guard hit (have=" + std::to_string(src.data.size()) + ", need=" + std::to_string(needed) + ")");
            return std::nullopt;
        }
        const uint8_t* Y_plane = src.data.data();
        const uint8_t* U_plane = Y_plane + y_size;
        const uint8_t* V_plane = U_plane + uv_size;
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

    std::optional<VideoFrame> convert_nv12_to_rgba(const VideoFrame& src, VideoFrame& out) {
    // NV12 -> RGBA conversion (hot path). Avoid per-frame logging.
        
        const int W = src.width;
        const int H = src.height;
        const size_t y_size = static_cast<size_t>(W) * static_cast<size_t>(H);
        const size_t uv_size = (static_cast<size_t>(W) * static_cast<size_t>(H)) / 2; // NV12: UV plane is half size but interleaved
        const size_t needed = y_size + uv_size;
        
        if (src.data.size() < needed) {
            ve::log::error("convert_nv12_to_rgba: undersized buffer guard hit (have=" + std::to_string(src.data.size()) + ", need=" + std::to_string(needed) + ")");
            return std::nullopt;
        }
        
        const uint8_t* Y_plane = src.data.data();
        const uint8_t* UV_plane = Y_plane + y_size;
        uint8_t* rgba_data = out.data.data();

        for(int y = 0; y < H; ++y) {
            for(int x = 0; x < W; ++x) {
                const int Y = Y_plane[y * W + x];
                
                // NV12 format: UV samples are interleaved and subsampled 2x2
                const int uv_x = x / 2;
                const int uv_y = y / 2;
                const int uv_index = (uv_y * (W / 2) + uv_x) * 2; // 2 bytes per UV pair
                
                const int U = UV_plane[uv_index];     // U component
                const int V = UV_plane[uv_index + 1]; // V component
                
                yuv_to_rgb(Y, U, V, src.color_space, src.color_range, &rgba_data[(y * W + x) * 4]);
            }
        }
        
        return out;
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
        const size_t pixel_count = static_cast<size_t>(out.width) * static_cast<size_t>(out.height);
        
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
            const size_t needed = static_cast<size_t>(y_size) + 2ull * c_size;
            if (src.data.size() < needed) {
                ve::log::error("fill_src_planes: undersized YUV420P buffer (have=" + std::to_string(src.data.size()) + ", need=" + std::to_string(needed) + ")");
                return false;
            }
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
            const size_t needed = static_cast<size_t>(y_size) + 2ull * u_size;
            if (src.data.size() < needed) {
                ve::log::error("fill_src_planes: undersized YUV422P buffer (have=" + std::to_string(src.data.size()) + ", need=" + std::to_string(needed) + ")");
                return false;
            }
            data[0] = const_cast<uint8_t*>(base);
            data[1] = const_cast<uint8_t*>(base + y_size);
            data[2] = const_cast<uint8_t*>(base + y_size + u_size);
            linesize[0] = w; linesize[1] = c_w; linesize[2] = c_w;
            return true;
        }
        case AV_PIX_FMT_YUV444P: {
            const int y_size = w*h;
            const int c_size = y_size;
            const size_t needed = static_cast<size_t>(y_size) * 3ull;
            if (src.data.size() < needed) {
                ve::log::error("fill_src_planes: undersized YUV444P buffer (have=" + std::to_string(src.data.size()) + ", need=" + std::to_string(needed) + ")");
                return false;
            }
            data[0] = const_cast<uint8_t*>(base);
            data[1] = const_cast<uint8_t*>(base + y_size);
            data[2] = const_cast<uint8_t*>(base + y_size + c_size);
            linesize[0] = w; linesize[1] = w; linesize[2] = w;
            return true;
        }
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21: {
            const int y_size = w*h;
            const int uv_size = ((w+1)/2) * ((h+1)/2) * 2; // interleaved UV (one sample per 2x2 => 2 bytes)
            const size_t needed = static_cast<size_t>(y_size) + uv_size;
            if (src.data.size() < needed) {
                ve::log::error("fill_src_planes: undersized NV12/NV21 buffer (have=" + std::to_string(src.data.size()) + ", need=" + std::to_string(needed) + ")");
                return false;
            }
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

    // Use optimized conversion algorithms based on resolution
    int algorithm = SWS_FAST_BILINEAR;
    // For 4K content, prioritize speed
    if (src.width >= 3840 || src.height >= 2160) {
        algorithm = SWS_FAST_BILINEAR;
    }
    
    SwsContext* ctx = sws_getContext(src.width, src.height, src_fmt,
                                     target_w, target_h, AV_PIX_FMT_RGBA,
                                     algorithm, nullptr, nullptr, nullptr);
    if (!ctx) return std::nullopt;
    
    // Enable multi-threading for 4K content (simplified)
    // Note: swscale threading needs careful implementation
    // For now, rely on decoder threading optimization

    uint8_t* src_data[4];
    int src_linesize[4];
    if (!fill_src_planes(src, src_fmt, src_data, src_linesize)) {
        sws_freeContext(ctx);
        return std::nullopt;
    }

    const size_t row_bytes = static_cast<size_t>(target_w) * 4;
    const size_t needed = row_bytes * static_cast<size_t>(target_h);
#if VE_HEAP_DEBUG
    // Debug guard allocation to detect over/underruns without any timing code
    const size_t GUARD = 32;
    std::vector<uint8_t> guarded(needed + GUARD*2, 0xCD);
    uint8_t* dst_base = guarded.data() + GUARD;
    uint8_t* dst_data[4] = { dst_base, nullptr, nullptr, nullptr };
    int dst_linesize[4] = { static_cast<int>(row_bytes), 0, 0, 0 };
    int res = sws_scale(ctx, src_data, src_linesize, 0, src.height, dst_data, dst_linesize);
    sws_freeContext(ctx);
    if (res <= 0) return std::nullopt;
    for (size_t i=0;i<GUARD;++i) {
        if (guarded[i] != 0xCD || guarded[GUARD+needed+i] != 0xCD) {
            ve::log::error("sws_scale guard overrun detected");
            break;
        }
    }
    VideoFrame out; out.width=target_w; out.height=target_h; out.pts=src.pts; out.format=PixelFormat::RGBA32; out.color_space=src.color_space; out.color_range=src.color_range; out.data.assign(dst_base, dst_base+needed); return out;
#else
    VideoFrame out; out.width=target_w; out.height=target_h; out.pts=src.pts; out.format=PixelFormat::RGBA32; out.color_space=src.color_space; out.color_range=src.color_range; out.data.resize(needed);
    uint8_t* dst_data[4] = { out.data.data(), nullptr, nullptr, nullptr }; int dst_linesize[4] = { static_cast<int>(row_bytes), 0, 0, 0 };
    int res = sws_scale(ctx, src_data, src_linesize, 0, src.height, dst_data, dst_linesize); sws_freeContext(ctx); if (res <= 0) return std::nullopt; return out;
#endif
}
#endif // VE_HAVE_FFMPEG

std::optional<VideoFrame> to_rgba_scaled(const VideoFrame& src, int target_w, int target_h) noexcept {
    if(src.format == PixelFormat::RGBA32 && (target_w <=0 || target_h <=0 || (target_w==src.width && target_h==src.height))) return src;

    auto expected_src_size = [&](const VideoFrame& f)->size_t {
        int w=f.width, h=f.height; if (w<=0||h<=0) return 0; switch(f.format) {
            case PixelFormat::YUV420P: return static_cast<size_t>(w)*h + 2*static_cast<size_t>((w+1)/2)*((h+1)/2);
            case PixelFormat::YUV422P: return static_cast<size_t>(w)*h + 2*static_cast<size_t>((w+1)/2)*h;
            case PixelFormat::YUV444P: return 3ull*static_cast<size_t>(w)*h;
            case PixelFormat::NV12: return static_cast<size_t>(w)*h + static_cast<size_t>((w+1)/2)*((h+1)/2)*2; // Y plane + interleaved UV plane
            case PixelFormat::RGB24: return 3ull*static_cast<size_t>(w)*h;
            case PixelFormat::BGR24: return 3ull*static_cast<size_t>(w)*h;
            case PixelFormat::RGBA32: return 4ull*static_cast<size_t>(w)*h;
            case PixelFormat::BGRA32: return 4ull*static_cast<size_t>(w)*h;
            case PixelFormat::GRAY8: return static_cast<size_t>(w)*h;
            default: return 0; }
    };
    size_t expect = expected_src_size(src);
    if (expect && src.data.size() < expect) {
        return std::nullopt;
    }

#if VE_HAVE_FFMPEG
    static bool disable_ffmpeg = (std::getenv("VE_DISABLE_FFMPEG_CONVERT") != nullptr);
    // Heuristic: very small frames (<=2x2) route to manual path to avoid potential swscale edge-case crashes.
    bool tiny_frame = (src.width * src.height) <= 4;
    if (!disable_ffmpeg && !tiny_frame) {
#ifdef _MSC_VER
        if (_CrtCheckMemory() == 0) {
            ve::log::error("CRT heap check failed BEFORE ffmpeg swscale in to_rgba_scaled");
        }
#endif
        if (auto out = to_rgba_scaled_ffmpeg(src, target_w, target_h)) {
#ifdef _MSC_VER
            if (_CrtCheckMemory() == 0) {
                ve::log::error("CRT heap check failed AFTER ffmpeg swscale in to_rgba_scaled");
            }
#endif
            return out;
        }
    } else if (!tiny_frame) {
        ve::log::info("VE_DISABLE_FFMPEG_CONVERT set - using manual fallback converters");
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

std::optional<RgbaView> to_rgba_scaled_view(const VideoFrame& src, int target_w, int target_h) noexcept {
    VE_PROFILE_SCOPE_UNIQ("to_rgba_scaled_view");
    if (src.data.empty() || src.width <= 0 || src.height <= 0) return std::nullopt;
    if (target_w <= 0) target_w = src.width;
    if (target_h <= 0) target_h = src.height;
    const size_t row_bytes = static_cast<size_t>(target_w) * 4;
    const size_t needed = row_bytes * static_cast<size_t>(target_h);

    // Thread-local context + ring buffers for reuse.
    struct Scratch {
#if VE_HAVE_FFMPEG
        SwsContext* ctx = nullptr; int src_w=0, src_h=0, dst_w=0, dst_h=0; AVPixelFormat fmt = AV_PIX_FMT_NONE; int flags = 0;
        ~Scratch(){ if (ctx) sws_freeContext(ctx); }
#endif
        std::vector<uint8_t> buffers[2]; int index = 0;
    };
    static thread_local Scratch tls;
    auto& buf = tls.buffers[tls.index];
    if (buf.size() < needed) buf.resize(needed);
    uint8_t* dst = buf.data();

#if VE_HAVE_FFMPEG
    static bool disable_ffmpeg = (std::getenv("VE_DISABLE_FFMPEG_CONVERT") != nullptr);
    if (!disable_ffmpeg) {
    VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.ffmpeg");
        const AVPixelFormat src_fmt = to_avpf(src.format);
        if (src_fmt != AV_PIX_FMT_NONE) {
            // Recreate context only when parameters change.
            const bool no_scale = (src.width == target_w && src.height == target_h);
            const int new_flags = no_scale ? SWS_POINT : SWS_FAST_BILINEAR;
            if (!tls.ctx || tls.src_w!=src.width || tls.src_h!=src.height || tls.dst_w!=target_w || tls.dst_h!=target_h || tls.fmt!=src_fmt || tls.flags!=new_flags) {
                if (tls.ctx) sws_freeContext(tls.ctx);
                tls.ctx = sws_getContext(src.width, src.height, src_fmt,
                                         target_w, target_h, AV_PIX_FMT_RGBA,
                                         new_flags, nullptr, nullptr, nullptr);
                tls.src_w = src.width; tls.src_h = src.height; tls.dst_w = target_w; tls.dst_h = target_h; tls.fmt = src_fmt; tls.flags = new_flags;
            }
            if (tls.ctx) {
                VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.sws_scale");
                uint8_t* src_data[4]; int src_linesize[4];
                if (fill_src_planes(src, src_fmt, src_data, src_linesize)) {
                    uint8_t* dst_data[4] = { dst, nullptr, nullptr, nullptr }; int dst_linesize[4] = { static_cast<int>(row_bytes),0,0,0 };
                    int res = sws_scale(tls.ctx, src_data, src_linesize, 0, src.height, dst_data, dst_linesize);
                    if (res > 0) {
                        RgbaView view{ dst, target_w, target_h, static_cast<int>(row_bytes) };
                        tls.index = (tls.index + 1) & 1; // advance ring after successful fill
                        return view;
                    }
                }
            }
        }
        // fall through to manual converters if ffmpeg path failed
    }
#endif

    // Manual converters with optional nearest-neighbor scaling if requested.
    bool need_scale = (target_w != src.width || target_h != src.height);
    int convert_w = src.width, convert_h = src.height; // size we actually convert from source
    const size_t src_row_bytes = static_cast<size_t>(convert_w)*4;
    switch(src.format) {
        case PixelFormat::RGB24: {
            VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.rgb24");
            const uint8_t* s = src.data.data();
            for (int y=0;y<src.height;++y){
                for(int x=0;x<src.width;++x){
                    size_t si=(size_t(y)*src.width+x)*3;
                    size_t di=(size_t(y)*src.width+x)*4;
                    dst[di+0]=s[si+0]; dst[di+1]=s[si+1]; dst[di+2]=s[si+2]; dst[di+3]=255;
                }
            }
            break; }
        case PixelFormat::BGR24: {
            VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.bgr24");
            const uint8_t* s = src.data.data();
            for (int y=0;y<src.height;++y){
                for(int x=0;x<src.width;++x){
                    size_t si=(size_t(y)*src.width+x)*3;
                    size_t di=(size_t(y)*src.width+x)*4;
                    dst[di+0]=s[si+2]; dst[di+1]=s[si+1]; dst[di+2]=s[si+0]; dst[di+3]=255;
                }
            }
            break; }
        case PixelFormat::BGRA32: {
            VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.bgra32");
            const uint8_t* s = src.data.data();
            for (int y=0;y<src.height;++y){
                for(int x=0;x<src.width;++x){
                    size_t si=(size_t(y)*src.width+x)*4;
                    size_t di=(size_t(y)*src.width+x)*4;
                    dst[di+0]=s[si+2]; dst[di+1]=s[si+1]; dst[di+2]=s[si+0]; dst[di+3]=s[si+3];
                }
            }
            break; }
        case PixelFormat::YUV420P: {
            VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.yuv420p");
            const int W=src.width,H=src.height; 
            const size_t ysz = static_cast<size_t>(W)*H; const size_t uvsz = ysz/4; const size_t need = ysz + 2*uvsz; 
            if (src.data.size() < need) { ve::log::error("to_rgba_scaled_view.yuv420p undersized buffer (have=" + std::to_string(src.data.size()) + ", need=" + std::to_string(need) + ")"); return std::nullopt; }
            const uint8_t* Y=src.data.data(); const uint8_t* U=Y+ysz; const uint8_t* V=U+uvsz;
            for(int y=0;y<H;++y){
                for(int x=0;x<W;++x){
                    int Yi=Y[y*W+x]; int Ui=U[(y/2)*(W/2)+(x/2)]; int Vi=V[(y/2)*(W/2)+(x/2)];
                    uint8_t* outp=&dst[(size_t(y)*W+x)*4];
                    yuv_to_rgb(Yi,Ui,Vi,src.color_space,src.color_range,outp);
                }
            }
            break; }
        case PixelFormat::RGBA32: {
            VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.rgba32_copy");
            const uint8_t* s = src.data.data();
            std::memcpy(dst, s, size_t(src.width)*src.height*4);
            break; }
        case PixelFormat::GRAY8: {
            VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.gray8");
            const uint8_t* s = src.data.data();
            for(int y=0;y<src.height;++y){
                for(int x=0;x<src.width;++x){
                    uint8_t g=s[size_t(y)*src.width+x];
                    size_t di=(size_t(y)*src.width+x)*4;
                    dst[di]=dst[di+1]=dst[di+2]=g; dst[di+3]=255;
                }
            }
            break; }
        default: return std::nullopt;
    }
    if (need_scale) {
    VE_PROFILE_SCOPE_DETAILED("to_rgba_scaled_view.nn_scale");
        // nearest-neighbor into a second ring buffer slot (advance index first to avoid overwrite if caller still using dst)
        tls.index = (tls.index + 1) & 1; auto& scaled_buf = tls.buffers[tls.index]; size_t scaled_needed = size_t(target_w)*target_h*4; if (scaled_buf.size()<scaled_needed) scaled_buf.resize(scaled_needed);
        uint8_t* scaled = scaled_buf.data();
        for (int y=0; y<target_h; ++y) {
            int sy = (y * convert_h) / target_h;
            const uint8_t* src_row = dst + size_t(sy) * convert_w * 4;
            for (int x=0; x<target_w; ++x) {
                int sx = (x * convert_w) / target_w;
                const uint8_t* sp = src_row + size_t(sx) * 4;
                uint8_t* dp = scaled + (size_t(y)*target_w + x)*4;
                dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2]; dp[3]=sp[3];
            }
        }
        // advance again so next call writes into the other buffer, keeping scaled alive
        RgbaView view{ scaled, target_w, target_h, target_w*4 };
        tls.index = (tls.index + 1) & 1;
        return view;
    }
    RgbaView view{ dst, convert_w, convert_h, static_cast<int>(convert_w*4) };
    tls.index = (tls.index + 1) & 1;
    return view;
}

} // namespace ve::decode
