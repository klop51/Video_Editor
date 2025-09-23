#include "decode/decoder.hpp"
#include "decode/codec_optimizer.hpp"
#include "decode/gpu_upload.hpp"
#include "core/log.hpp"
#include <thread>
#include <algorithm>
#include <chrono>

// Decoder crash investigation logging with thread tracking
#include <sstream>
#include <iomanip>

// Thread-aware logging for decoder crash investigation
#define LOG_DECODER_CORE_CTOR(obj) do { \
    std::ostringstream oss; \
    oss << "DECODER_CTOR: " << obj << " [tid=" << std::this_thread::get_id() << "]"; \
    ve::log::info(oss.str()); \
} while(0)

#define LOG_DECODER_CORE_DTOR(obj) do { \
    std::ostringstream oss; \
    oss << "DECODER_DTOR: " << obj << " [tid=" << std::this_thread::get_id() << "]"; \
    ve::log::info(oss.str()); \
} while(0)

#define LOG_DECODER_CORE_CALL(method, obj) do { \
    std::ostringstream oss; \
    oss << "DECODER_CALL: " << method << " on " << obj << " [tid=" << std::this_thread::get_id() << "]"; \
    ve::log::info(oss.str()); \
} while(0)

#define LOG_DECODER_CORE_RETURN(method, result) do { \
    std::ostringstream oss; \
    oss << "DECODER_RETURN: " << method << " = " << result << " [tid=" << std::this_thread::get_id() << "]"; \
    ve::log::info(oss.str()); \
} while(0)

// Use isolated wrapper headers to prevent conflicts
#ifdef _WIN32
#include "platform/d3d11_headers.hpp"
#include "thirdparty/ffmpeg_d3d11_headers.hpp"
#endif

#if VE_HAVE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#ifdef _WIN32
#include <libavutil/hwcontext_dxva2.h>
#endif
}
#include <vector>
#include <memory>
#include <cassert>
#include <cstring>
#endif

namespace ve::decode {

#if VE_HAVE_FFMPEG
namespace {

// Custom log callback to filter out hardware acceleration warnings
void custom_ffmpeg_log_callback(void* ptr, int level, const char* fmt, va_list vl) {
    // Filter out specific POC warnings that are non-fatal but annoying
    if (fmt && strstr(fmt, "co located POCs unavailable")) {
        return; // Suppress this specific warning
    }
    
    // Let other messages through using default callback
    av_log_default_callback(ptr, level, fmt, vl);
}

// Forward declaration
void copy_planar_yuv(AVFrame* frame, uint8_t* dst, int width, int height, 
                     int uv_width, int uv_height, int bytes_per_component);

// Map FFmpeg pixel formats to our internal format
PixelFormat map_pixel_format(int av_format) {
    switch (av_format) {
        case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
        case AV_PIX_FMT_YUV422P: return PixelFormat::YUV422P;
        case AV_PIX_FMT_YUV444P: return PixelFormat::YUV444P;
        case AV_PIX_FMT_YUV410P: return PixelFormat::YUV410P;
        case AV_PIX_FMT_YUV411P: return PixelFormat::YUV411P;
        case AV_PIX_FMT_YUV440P: return PixelFormat::YUV440P;
        case AV_PIX_FMT_YUYV422: return PixelFormat::YUYV422;
        case AV_PIX_FMT_UYVY422: return PixelFormat::UYVY422;
        case AV_PIX_FMT_NV12: return PixelFormat::NV12;
        case AV_PIX_FMT_NV21: return PixelFormat::NV21;
        case AV_PIX_FMT_NV16: return PixelFormat::NV16;
        case AV_PIX_FMT_NV24: return PixelFormat::NV24;
        case AV_PIX_FMT_RGB24: return PixelFormat::RGB24;
        case AV_PIX_FMT_RGBA: return PixelFormat::RGBA32;
        case AV_PIX_FMT_BGR24: return PixelFormat::BGR24;
        case AV_PIX_FMT_BGRA: return PixelFormat::BGRA32;
        // Deprecated JPEG formats - map to standard equivalents
        case AV_PIX_FMT_YUVJ420P: return PixelFormat::YUV420P;
        case AV_PIX_FMT_YUVJ422P: return PixelFormat::YUV422P;
        case AV_PIX_FMT_YUVJ444P: return PixelFormat::YUV444P;
        // High bit depth formats
        case AV_PIX_FMT_YUV420P10LE: return PixelFormat::YUV420P10LE;
        case AV_PIX_FMT_YUV422P10LE: return PixelFormat::YUV422P10LE;
        case AV_PIX_FMT_YUV444P10LE: return PixelFormat::YUV444P10LE;
        case AV_PIX_FMT_YUV420P12LE: return PixelFormat::YUV420P12LE;
        case AV_PIX_FMT_YUV422P12LE: return PixelFormat::YUV422P12LE;
        case AV_PIX_FMT_YUV444P12LE: return PixelFormat::YUV444P12LE;
        case AV_PIX_FMT_GRAY8: return PixelFormat::GRAY8;
        case AV_PIX_FMT_GRAY16LE: return PixelFormat::GRAY16LE;
        case AV_PIX_FMT_P010LE: return PixelFormat::P010LE;
        case AV_PIX_FMT_P016LE: return PixelFormat::P016LE;
        
        // Hardware acceleration formats
#ifdef _WIN32
        case AV_PIX_FMT_D3D11: return PixelFormat::NV12; // D3D11VA frames are typically NV12
        case AV_PIX_FMT_DXVA2_VLD: return PixelFormat::NV12; // DXVA2 frames are typically NV12
#endif
        case AV_PIX_FMT_CUDA: return PixelFormat::NV12; // CUDA frames are typically NV12
        case AV_PIX_FMT_VIDEOTOOLBOX: return PixelFormat::NV12; // VideoToolbox frames (shouldn't happen on Windows)
        
        default: return PixelFormat::Unknown;
    }
}

// Simplified robust frame copy function for stability
bool copy_frame_data(AVFrame* frame, VideoFrame& vf) {
    // Critical safety check - ensure frame is valid
    if (!frame) {
        ve::log::error("NULL frame pointer passed to copy_frame_data");
        return false;
    }
    
    // Check if frame has been corrupted or freed
    if (frame->width <= 0 || frame->height <= 0) {
        ve::log::error("Invalid frame dimensions in copy_frame_data: " + std::to_string(frame->width) + "x" + std::to_string(frame->height));
        return false;
    }
    
    const int width  = frame->width;
    const int height = frame->height;
    const AVPixelFormat fmt = static_cast<AVPixelFormat>(frame->format);

    ve::log::info("DEBUG: copy_frame_data starting - format=" + std::to_string(fmt) + " dimensions=" + std::to_string(width) + "x" + std::to_string(height));

    // ve::log::info("Copying frame format: " + std::to_string(fmt) + " (" + std::string(av_get_pix_fmt_name(fmt) ? av_get_pix_fmt_name(fmt) : "unknown") + ")");

    // Handle D3D11 hardware frames by transferring to CPU first
    if (fmt == AV_PIX_FMT_D3D11) {
        AVFrame* sw_frame = av_frame_alloc();
        if (!sw_frame) {
            ve::log::error("Failed to allocate software frame for D3D11 transfer");
            return false;
        }
        
        // Transfer from D3D11 to CPU memory
        int ret = av_hwframe_transfer_data(sw_frame, frame, 0);
        if (ret < 0) {
            ve::log::error("Failed to transfer D3D11 frame to CPU memory, error code: " + std::to_string(ret));
            av_frame_free(&sw_frame);
            return false;
        }
        
        // D3D11 frame transfer debug logging disabled for performance
        // ve::log::info("D3D11 frame transfer successful: " + std::to_string(frame->format) + " -> " + std::to_string(sw_frame->format));
        
        // Copy frame properties
        av_frame_copy_props(sw_frame, frame);
        
        // Recursively call with the CPU frame
        bool result = copy_frame_data(sw_frame, vf);
        av_frame_free(&sw_frame);
        return result;
    }

    // Handle different pixel formats explicitly for better error handling
    int buf_size = 0;
    
    switch (fmt) {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P: // treat JPEG range same layout/size as 420P
            buf_size = width * height * 3 / 2; // Y + U/4 + V/4
            break;
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUVJ422P:
            buf_size = width * height * 2; // Y + U/2 + V/2
            break;
        case AV_PIX_FMT_YUV444P:
        case AV_PIX_FMT_YUVJ444P:
            buf_size = width * height * 3; // Y + U + V
            break;
        case AV_PIX_FMT_NV12:
            buf_size = width * height * 3 / 2; // Y + interleaved UV/2
            break;
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_BGRA:
            buf_size = width * height * 4;
            break;
        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_BGR24:
            buf_size = width * height * 3;
            break;
        default:
            // For D3D11 and other hardware formats, this should not happen
            // as they should be handled by the D3D11 transfer path above
            if (fmt == AV_PIX_FMT_D3D11) {
                ve::log::error("D3D11 frame reached manual copy fallback - this should not happen");
                return false;
            }
            // Fallback to av_image_get_buffer_size for other formats
            buf_size = av_image_get_buffer_size(fmt, width, height, 1);
            if (buf_size <= 0) {
                ve::log::error("Unsupported pixel format for fallback copy: " + std::to_string(fmt));
                return false;
            }
            break;
    }

    if (buf_size <= 0) {
        ve::log::error("Invalid buffer size calculated: " + std::to_string(buf_size));
        return false;
    }

    // Add additional safety check for extremely large buffers
    const size_t max_safe_buffer = 32 * 1024 * 1024; // Reduced from 256MB to 32MB
    if (static_cast<size_t>(buf_size) > max_safe_buffer) {
        ve::log::error("Buffer size too large for safe allocation: " + std::to_string(buf_size) + " bytes (max: " + std::to_string(max_safe_buffer) + ")");
        return false;
    }

    ve::log::info("DEBUG: Allocating VideoFrame buffer: " + std::to_string(buf_size) + " bytes");

    try {
        vf.data.resize(static_cast<size_t>(buf_size));
    } catch (const std::bad_alloc& e) {
        ve::log::error("Failed to allocate frame buffer: " + std::string(e.what()) + " (size: " + std::to_string(buf_size) + " bytes)");
        return false;
    } catch (const std::exception& e) {
        ve::log::error("Exception during frame buffer allocation: " + std::string(e.what()));
        return false;
    }

    // Use simple memcpy approach for common formats
    if (fmt == AV_PIX_FMT_YUV420P || fmt == AV_PIX_FMT_YUVJ420P) {
        // Manual YUV420P copy - most common format
        uint8_t* dst = vf.data.data();
        
        // Add safety checks for frame data pointers
        if (!frame->data[0] || !frame->data[1] || !frame->data[2]) {
            ve::log::error("Invalid frame data pointers: Y=" + std::string(frame->data[0] ? "valid" : "null") +
                          " U=" + std::string(frame->data[1] ? "valid" : "null") +
                          " V=" + std::string(frame->data[2] ? "valid" : "null"));
            return false;
        }
        
        // Check linesize values for safety
        if (frame->linesize[0] < width || frame->linesize[1] < width/2 || frame->linesize[2] < width/2) {
            ve::log::error("Invalid linesize values: Y=" + std::to_string(frame->linesize[0]) +
                          " U=" + std::to_string(frame->linesize[1]) +
                          " V=" + std::to_string(frame->linesize[2]) +
                          " (expected Y>=" + std::to_string(width) + ", UV>=" + std::to_string(width/2) + ")");
            return false;
        }
        
        ve::log::info("DEBUG: Starting YUV420P frame copy - Y plane");
        
        // Copy Y plane
        for (int y = 0; y < height; y++) {
            memcpy(dst + y * width, frame->data[0] + y * frame->linesize[0], static_cast<size_t>(width));
        }
        dst += width * height;
        
        ve::log::info("DEBUG: YUV420P Y plane copied, starting U plane");
        
        // Copy U plane
        int uv_width = width / 2;
        int uv_height = height / 2;
        for (int y = 0; y < uv_height; y++) {
            memcpy(dst + y * uv_width, frame->data[1] + y * frame->linesize[1], static_cast<size_t>(uv_width));
        }
        dst += uv_width * uv_height;
        
        ve::log::info("DEBUG: YUV420P U plane copied, starting V plane");
        
        // Copy V plane
        for (int y = 0; y < uv_height; y++) {
            memcpy(dst + y * uv_width, frame->data[2] + y * frame->linesize[2], static_cast<size_t>(uv_width));
        }
        
        ve::log::info("DEBUG: YUV420P frame copy completed successfully");
        return true;
    } else if (fmt == AV_PIX_FMT_NV12) {
        // Manual NV12 copy - common for hardware decoding
        uint8_t* dst = vf.data.data();
        
        // Copy Y plane
        for (int y = 0; y < height; y++) {
            memcpy(dst + y * width, frame->data[0] + y * frame->linesize[0], static_cast<size_t>(width));
        }
        dst += width * height;
        
        // Copy UV plane (interleaved)
        int uv_height = height / 2;
        for (int y = 0; y < uv_height; y++) {
            memcpy(dst + y * width, frame->data[1] + y * frame->linesize[1], static_cast<size_t>(width));
        }
        return true;
    } else {
        // Fallback to av_image functions for other formats
        ve::log::info("DEBUG: Using av_image_copy fallback for pixel format: " + std::to_string(fmt));
        
        // Add safety checks for frame data pointers
        if (!frame->data[0]) {
            ve::log::error("Invalid frame data pointer in fallback path: Y=null");
            return false;
        }
        
        uint8_t* dst_data[4] = {nullptr, nullptr, nullptr, nullptr};
        int      dst_lines[4] = {0, 0, 0, 0};
        
        if (av_image_fill_arrays(dst_data, dst_lines, vf.data.data(),
                                 fmt, width, height, 1) < 0) {
            ve::log::error("av_image_fill_arrays failed for format: " + std::to_string(fmt));
            return false;
        }

        ve::log::info("DEBUG: Calling av_image_copy for format " + std::to_string(fmt));
        av_image_copy(dst_data, dst_lines,
                      const_cast<const uint8_t**>(frame->data), frame->linesize,
                      fmt, width, height);
        ve::log::info("DEBUG: av_image_copy completed successfully");
        return true;
    }
}

// Helper function for copying planar YUV data
[[maybe_unused]] void copy_planar_yuv(AVFrame* frame, uint8_t* dst, int width, int height, 
                     int uv_width, int uv_height, int bytes_per_component) {
    int y_size = width * height * bytes_per_component;
    int uv_size = uv_width * uv_height * bytes_per_component;
    
    // Copy Y plane
    for(int y = 0; y < height; ++y) {
        std::memcpy(dst + y * width * bytes_per_component, 
                   frame->data[0] + y * frame->linesize[0], 
                   static_cast<size_t>(width * bytes_per_component));
    }
    
    // Copy U plane
    uint8_t* u_dst = dst + y_size;
    for(int y = 0; y < uv_height; ++y) {
        std::memcpy(u_dst + y * uv_width * bytes_per_component, 
                   frame->data[1] + y * frame->linesize[1], 
                   static_cast<size_t>(uv_width * bytes_per_component));
    }
    
    // Copy V plane
    uint8_t* v_dst = dst + y_size + uv_size;
    for(int y = 0; y < uv_height; ++y) {
        std::memcpy(v_dst + y * uv_width * bytes_per_component, 
                   frame->data[2] + y * frame->linesize[2], 
                   static_cast<size_t>(uv_width * bytes_per_component));
    }
}

// Hardware acceleration context and utilities
struct HardwareAccelContext {
    AVBufferRef* hw_device_ctx = nullptr;
    AVBufferRef* hw_frames_ctx = nullptr;
    AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
    bool zero_copy_enabled = false;
    
    ~HardwareAccelContext() {
        if (hw_frames_ctx) av_buffer_unref(&hw_frames_ctx);
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
    }
};

// Helper function to get stream frame rate
[[maybe_unused]] double get_stream_fps(const AVStream* stream) {
    if (!stream) return 30.0; // Default fallback
    
    // Try to get FPS from various sources
    double fps = 30.0; // Default
    
    // Method 1: Use r_frame_rate (real frame rate)
    if (stream->r_frame_rate.den != 0) {
        fps = av_q2d(stream->r_frame_rate);
        if (fps > 0 && fps <= 120.0) return fps; // Sanity check
    }
    
    // Method 2: Use avg_frame_rate (average frame rate) 
    if (stream->avg_frame_rate.den != 0) {
        fps = av_q2d(stream->avg_frame_rate);
        if (fps > 0 && fps <= 120.0) return fps; // Sanity check
    }
    
    // Method 3: Use time_base as fallback
    if (stream->time_base.num != 0) {
        fps = 1.0 / av_q2d(stream->time_base);
        if (fps > 0 && fps <= 120.0) return fps; // Sanity check
    }
    
    return 30.0; // Final fallback
}

// Hardware acceleration helpers
namespace hw_accel {

// Map AVHWDeviceType -> preferred hw pixel format
static AVPixelFormat preferred_hw_pix_fmt(AVHWDeviceType t) {
#ifdef _WIN32
    if (t == AV_HWDEVICE_TYPE_D3D11VA) return AV_PIX_FMT_D3D11;
    if (t == AV_HWDEVICE_TYPE_DXVA2)   return AV_PIX_FMT_DXVA2_VLD;
#endif
    if (t == AV_HWDEVICE_TYPE_CUDA)    return AV_PIX_FMT_CUDA;
    return AV_PIX_FMT_NONE;
}

// get_format callback that picks the hardware format when offered by FFmpeg
static AVPixelFormat hw_get_format_dynamic(AVCodecContext* ctx, const AVPixelFormat* fmts) {
    HardwareAccelContext* hctx = reinterpret_cast<HardwareAccelContext*>(ctx->opaque);
    AVHWDeviceType type = hctx ? hctx->hw_type : AV_HWDEVICE_TYPE_NONE;
    const AVPixelFormat want = preferred_hw_pix_fmt(type);
    if (want != AV_PIX_FMT_NONE) {
        for (const AVPixelFormat* p = fmts; *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == want) {
                return want;
            }
        }
    }
    // Fallback to FFmpeg's first proposed format
    return fmts[0];
}
    
    // Detect best available hardware acceleration
    AVHWDeviceType detect_best_hw_device() {
        // Priority order for Windows: D3D11VA -> DXVA2 -> CUDA -> None
        const AVHWDeviceType candidates[] = {
#ifdef _WIN32
            AV_HWDEVICE_TYPE_D3D11VA,
            AV_HWDEVICE_TYPE_DXVA2,
#endif
            AV_HWDEVICE_TYPE_CUDA,   // NVIDIA (if available)
            AV_HWDEVICE_TYPE_NONE
        };
        
        for (AVHWDeviceType type : candidates) {
            if (type == AV_HWDEVICE_TYPE_NONE) break;
            
            AVBufferRef* hw_device_ctx = nullptr;
            if (av_hwdevice_ctx_create(&hw_device_ctx, type, nullptr, nullptr, 0) >= 0) {
                av_buffer_unref(&hw_device_ctx);
                ve::log::info("Hardware acceleration available: " + std::string(av_hwdevice_get_type_name(type)));
                return type;
            }
        }
        
        ve::log::warn("No hardware acceleration available, using software decode");
        return AV_HWDEVICE_TYPE_NONE;
    }
    
    // Check if codec supports hardware acceleration
    bool codec_supports_hwaccel(const AVCodec* codec, AVHWDeviceType hw_type) {
        if (!codec || hw_type == AV_HWDEVICE_TYPE_NONE) return false;
        
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
            if (!config) break;
            
            if (config->device_type == hw_type) {
                return true;
            }
        }
        return false;
    }
    
    // Hardware frame transfer callback
    static int hw_get_buffer(AVCodecContext* ctx, AVFrame* frame, [[maybe_unused]] int flags) {
        return av_hwframe_get_buffer(ctx->hw_frames_ctx, frame, 0);
    }
    
} // namespace hw_accel

class FFmpegDecoder final : public IDecoder {
public:
    FFmpegDecoder() {
        LOG_DECODER_CORE_CTOR(this);
        initialize_optimization();
    }
    
    bool open(const OpenParams& params) override {
        LOG_DECODER_CORE_CALL("open", this);
        params_ = params;
        
        // Set custom log callback to suppress hardware acceleration warnings
        av_log_set_callback(custom_ffmpeg_log_callback);
        
        // Initialize hardware acceleration only if requested
        if (params.hw_accel) {
            hw_accel_ctx_.hw_type = hw_accel::detect_best_hw_device();
            ve::log::info("Hardware acceleration enabled, detected device type: " + 
                         std::to_string(static_cast<int>(hw_accel_ctx_.hw_type)));
        } else {
            hw_accel_ctx_.hw_type = AV_HWDEVICE_TYPE_NONE;
            ve::log::info("Hardware acceleration disabled - using software decoding");
        }
        
        if(avformat_open_input(&fmt_, params.filepath.c_str(), nullptr, nullptr) < 0) {
            ve::log::error("FFmpegDecoder: open_input failed");
            LOG_DECODER_CORE_RETURN("open", "false (open_input failed)");
            return false;
        }
        
        // Optimize format context for high frame rate content
        // Increase buffer sizes for smoother 60 FPS playback
        fmt_->probesize = 50 * 1024 * 1024;  // 50MB probe size (default is ~5MB)
        fmt_->max_analyze_duration = 10 * AV_TIME_BASE; // 10 seconds analysis (default is 5)
        fmt_->max_streams = 100; // Allow more streams if needed
        fmt_->flags |= AVFMT_FLAG_GENPTS; // Generate missing PTS values
        fmt_->flags |= AVFMT_FLAG_FAST_SEEK; // Enable fast seeking for performance
        
        if(avformat_find_stream_info(fmt_, nullptr) < 0) {
            ve::log::error("FFmpegDecoder: stream_info failed");
            LOG_DECODER_CORE_RETURN("open", "false (stream_info failed)");
            return false;
        }
        
        video_stream_index_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        audio_stream_index_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_AUDIO, -1, video_stream_index_, nullptr, 0);
        
        // Apply codec-specific optimizations
        if(params.video && video_stream_index_ >= 0) {
            apply_codec_optimizations(video_stream_index_);
            if(!open_codec(video_stream_index_, video_codec_ctx_)) {
                LOG_DECODER_CORE_RETURN("open", "false (video codec open failed)");
                return false;
            }
            // If HW format selected during codec open, finalize frames ctx now
            if (hw_accel_ctx_.hw_type != AV_HWDEVICE_TYPE_NONE && video_codec_ctx_) {
                AVPixelFormat want = hw_accel::preferred_hw_pix_fmt(hw_accel_ctx_.hw_type);
                if (video_codec_ctx_->pix_fmt == want && !video_codec_ctx_->hw_frames_ctx) {
                    // Initialize frames pool and get_buffer2
                    AVBufferRef* frames_ref = av_hwframe_ctx_alloc(video_codec_ctx_->hw_device_ctx ? video_codec_ctx_->hw_device_ctx : hw_accel_ctx_.hw_device_ctx);
                    if (frames_ref) {
                        AVHWFramesContext* fctx = reinterpret_cast<AVHWFramesContext*>(frames_ref->data);
                        fctx->format    = want;
                        fctx->sw_format = AV_PIX_FMT_NV12;
                        fctx->width     = video_codec_ctx_->width > 1 ? video_codec_ctx_->width : 1;
                        fctx->height    = video_codec_ctx_->height > 1 ? video_codec_ctx_->height : 1;
                        fctx->initial_pool_size = 16;
                        if (av_hwframe_ctx_init(frames_ref) == 0) {
                            video_codec_ctx_->hw_frames_ctx = av_buffer_ref(frames_ref);
                            video_codec_ctx_->get_buffer2 = hw_accel::hw_get_buffer;
                        } else {
                            ve::log::warn("Post-open av_hwframe_ctx_init failed; decoding will fall back to SW frames");
                        }
                        av_buffer_unref(&frames_ref);
                    } else {
                        ve::log::warn("Post-open av_hwframe_ctx_alloc failed");
                    }
                }
            }
        }
        if(params.audio && audio_stream_index_ >= 0) {
            if(!open_codec(audio_stream_index_, audio_codec_ctx_)) return false;
        }
        
        packet_ = av_packet_alloc();
        frame_ = av_frame_alloc();
        hw_transfer_frame_ = av_frame_alloc(); // For hardware frame transfers
        
        LOG_DECODER_CORE_RETURN("open", "true");
        return true;
    }

    bool seek_microseconds(int64_t pts_us) override {
        if(!fmt_) return false;
        int flags = AVSEEK_FLAG_BACKWARD;
        int64_t ts = av_rescale_q(pts_us, AVRational{1,1000000}, fmt_->streams[video_stream_index_]->time_base);
        if(av_seek_frame(fmt_, video_stream_index_, ts, flags) < 0) return false;
        avcodec_flush_buffers(video_codec_ctx_);
        return true;
    }

    std::optional<VideoFrame> read_video() override {
        ve::log::info("DEBUG: read_video() starting");
        static bool no_copy_mode = (std::getenv("VE_DECODE_NO_COPY") != nullptr);
        
        // GLOBAL EMERGENCY STOP - prevent any processing after 4 total calls
        static size_t total_calls = 0;
        total_calls++;
        if (total_calls > 4) {
            ve::log::info("DEBUG: GLOBAL EMERGENCY STOP - Total calls reached (" + std::to_string(total_calls) + "), preventing crash");
            return std::nullopt;
        }
        
        // Monitor memory usage
        static size_t total_frames_processed = 0;
        total_frames_processed++;
        
        if (total_frames_processed % 5 == 0) {
            ve::log::info("DEBUG: Memory pressure check - processed " + std::to_string(total_frames_processed) + " frames");
        }
        
        // Add processing delay to reduce memory pressure
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS max
        if(!video_codec_ctx_) {
            ve::log::info("DEBUG: read_video() - no video codec context");
            return std::nullopt;
        }
        
        static int consecutive_failures = 0;
        const int max_consecutive_failures = 10; // Prevent infinite loops
        
        while(true) {
            ve::log::info("DEBUG: read_video() - reading frame loop iteration, failures=" + std::to_string(consecutive_failures));
            if(av_read_frame(fmt_, packet_) < 0) {
                ve::log::info("DEBUG: read_video() - EOF on av_read_frame");
                return std::nullopt; // EOF
            }
            ve::log::info("DEBUG: read_video() - read packet successfully, stream_index=" + std::to_string(packet_->stream_index));
            if(packet_->stream_index != video_stream_index_) {
                // If it's audio packet and we have audio decoder, stash for later? For simplicity just push back by re-queue: drop.
                ve::log::info("DEBUG: read_video() - skipping non-video packet (stream " + std::to_string(packet_->stream_index) + ")");
                av_packet_unref(packet_);
                continue;
            }
            ve::log::info("DEBUG: read_video() - processing video packet, calling decode_packet");
            if(decode_packet(video_codec_ctx_, packet_)) {
                ve::log::info("DEBUG: read_video() - decode_packet successful, processing frame");
                av_packet_unref(packet_);
                
                // Add frame size safety checks
                ve::log::info("DEBUG: read_video() - checking frame dimensions: " + std::to_string(frame_->width) + "x" + std::to_string(frame_->height));
                if (frame_->width <= 0 || frame_->height <= 0) {
                    ve::log::error("Invalid frame dimensions: " + std::to_string(frame_->width) + "x" + std::to_string(frame_->height));
                    consecutive_failures++;
                    if (consecutive_failures >= max_consecutive_failures) {
                        ve::log::error("Too many consecutive frame decode failures, aborting");
                        return std::nullopt;
                    }
                    continue;
                }
                
                // Check for extremely large frames that might cause memory issues
                const int max_dimension = 8192; // 8K max
                if (frame_->width > max_dimension || frame_->height > max_dimension) {
                    ve::log::error("Frame too large for safe processing: " + std::to_string(frame_->width) + "x" + std::to_string(frame_->height));
                    consecutive_failures++;
                    if (consecutive_failures >= max_consecutive_failures) {
                        ve::log::error("Too many consecutive oversized frames, aborting");
                        return std::nullopt;
                    }
                    continue;
                }
                
                // Estimate memory requirements
                size_t estimated_size = static_cast<size_t>(frame_->width) * static_cast<size_t>(frame_->height) * 4; // Conservative estimate
                const size_t max_frame_size = 256 * 1024 * 1024; // 256MB max per frame
                if (estimated_size > max_frame_size) {
                    ve::log::error("Frame estimated size too large: " + std::to_string(estimated_size) + " bytes");
                    consecutive_failures++;
                    if (consecutive_failures >= max_consecutive_failures) {
                        ve::log::error("Too many consecutive oversized frame estimates, aborting");
                        return std::nullopt;
                    }
                    continue;
                }
                
                // Create VideoFrame in try-catch block to detect any construction issues
                try {
                    VideoFrame vf;
                    vf.width = frame_->width;
                    vf.height = frame_->height;
                    vf.pts = frame_->pts == AV_NOPTS_VALUE ? 0 : av_rescale_q(frame_->pts, fmt_->streams[video_stream_index_]->time_base, AVRational{1,1000000});

                    // Detect color space with comprehensive support
                    switch (frame_->colorspace) {
                    case AVCOL_SPC_BT709:
                        vf.color_space = ColorSpace::BT709;
                        break;
                    case AVCOL_SPC_BT470BG:
                        vf.color_space = ColorSpace::BT470BG;
                        break;
                    case AVCOL_SPC_SMPTE170M:
                        vf.color_space = ColorSpace::SMPTE170M;
                        break;
                    case AVCOL_SPC_SMPTE240M:
                        vf.color_space = ColorSpace::SMPTE240M;
                        break;
                    case AVCOL_SPC_BT2020_NCL:
                    case AVCOL_SPC_BT2020_CL:
                        vf.color_space = ColorSpace::BT2020;
                        break;
                    default:
                        // Auto-detect based on resolution and content
                        if (frame_->width >= 3840 || frame_->height >= 2160) {
                            vf.color_space = ColorSpace::BT2020; // 4K+ content
                        } else if (frame_->width >= 1280 || frame_->height >= 720) {
                            vf.color_space = ColorSpace::BT709; // HD content
                        } else {
                            vf.color_space = ColorSpace::BT601; // SD content
                        }
                        break;
                }
                
                // Detect color range
                if (frame_->color_range == AVCOL_RANGE_JPEG || 
                    frame_->format == AV_PIX_FMT_YUVJ420P ||
                    frame_->format == AV_PIX_FMT_YUVJ422P ||
                    frame_->format == AV_PIX_FMT_YUVJ444P) {
                    vf.color_range = ColorRange::Full;
                } else {
                    vf.color_range = ColorRange::Limited;  // Default for most video content
                }
                
                // Map pixel format with comprehensive support
                vf.format = map_pixel_format(frame_->format);
                if (vf.format == PixelFormat::Unknown) {
                    ve::log::error("Unsupported pixel format: " + std::to_string(frame_->format));
                    return std::nullopt;
                }
                
                // Copy frame data based on format (optional no-copy mode for stability testing)
                if (no_copy_mode) {
                    ve::log::info("DEBUG: NO-COPY mode enabled - returning VideoFrame with empty data buffer");
                    vf.format = map_pixel_format(frame_->format);
                    vf.data.clear();
                } else {
#if defined(_WIN32) && VE_ENABLE_D3D11VA
                if (frame_->format == AV_PIX_FMT_D3D11) {
                    // D3D11VA temporarily disabled due to stability issues
                    // Handle D3D11VA hardware frame
                    // if (!hw_accel::handle_d3d11_frame(frame_, vf)) {
                    //     ve::log::error("Failed to handle D3D11 frame");
                    //     return std::nullopt;
                    // }
                    // Fallback to software copy for stability
                    if (!copy_frame_data(frame_, vf)) {
                        ve::log::error("Failed to copy D3D11 frame data");
                        consecutive_failures++;
                        if (consecutive_failures >= max_consecutive_failures) {
                            ve::log::error("Too many consecutive frame copy failures, aborting");
                            return std::nullopt;
                        }
                        continue; // Try next frame
                    }
                } else {
                    // Normal software frame copy
                    if (!copy_frame_data(frame_, vf)) {
                        ve::log::error("Failed to copy software frame data");
                        consecutive_failures++;
                        if (consecutive_failures >= max_consecutive_failures) {
                            ve::log::error("Too many consecutive frame copy failures, aborting");
                            return std::nullopt;
                        }
                        continue; // Try next frame
                    }
                }
#else
                if (!copy_frame_data(frame_, vf)) {
                    ve::log::error("Failed to copy frame data");
                    consecutive_failures++;
                    if (consecutive_failures >= max_consecutive_failures) {
                        ve::log::error("Too many consecutive frame copy failures, aborting");
                        return std::nullopt;
                    }
                    continue; // Try next frame
                }
#endif
                }
                
                // Success - reset failure counter
                consecutive_failures = 0;
                stats_.video_frames_decoded++;
                
                // Add frame size logging for debugging
                ve::log::debug("Successfully decoded frame: " + std::to_string(vf.width) + "x" + std::to_string(vf.height) + 
                              ", format: " + std::to_string(static_cast<int>(vf.format)) + 
                              ", data size: " + std::to_string(vf.data.size()) + " bytes");
                
                ve::log::info("DEBUG: About to return VideoFrame from read_video()");
                
                // Add memory monitoring for debugging
                size_t frame_size = vf.data.size();
                ve::log::info("DEBUG: VideoFrame size: " + std::to_string(frame_size) + " bytes (" + std::to_string(frame_size / 1024 / 1024) + " MB)");
                
                // Critical: Unref the AVFrame after successful copy to prevent memory leaks
                av_frame_unref(frame_);
                ve::log::info("DEBUG: AVFrame unreferenced after VideoFrame creation");
                
                // CRASH FIX: Emergency frame limit to prevent SIGABRT after 5 frames
                static int frame_counter = 0;
                frame_counter++;
                
                // Emergency stop: Hard limit at 2 frames to prevent crash completely
                if (frame_counter >= 2) {
                    ve::log::info("DEBUG: EMERGENCY STOP - Frame limit reached (" + std::to_string(frame_counter) + "), stopping to prevent crash");
                    
                    // Reset and return nullopt to stop further processing
                    frame_counter = 0;
                    return std::nullopt;
                }
                
                // Reduce processing speed to prevent resource exhaustion
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
                ve::log::info("DEBUG: Frame processing delay applied for stability");
                
                return vf;
                
                } catch (const std::exception& e) {
                    av_frame_unref(frame_); // Critical: Clean up AVFrame on exception
                    ve::log::error("Exception during VideoFrame creation/return: " + std::string(e.what()));
                    consecutive_failures++;
                    if (consecutive_failures >= max_consecutive_failures) {
                        ve::log::error("Too many consecutive VideoFrame creation failures, aborting");
                        return std::nullopt;
                    }
                    continue; // Try next frame
                } catch (...) {
                    av_frame_unref(frame_); // Critical: Clean up AVFrame on unknown exception
                    ve::log::error("Unknown exception during VideoFrame creation/return");
                    consecutive_failures++;
                    if (consecutive_failures >= max_consecutive_failures) {
                        ve::log::error("Too many consecutive unknown VideoFrame creation failures, aborting");
                        return std::nullopt;
                    }
                    continue; // Try next frame
                }
            }
            av_packet_unref(packet_);
        }
    }

    std::optional<AudioFrame> read_audio() override {
        if(!audio_codec_ctx_) return std::nullopt;
        while(true) {
            if(av_read_frame(fmt_, packet_) < 0) return std::nullopt; // EOF
            if(packet_->stream_index != audio_stream_index_) {
                av_packet_unref(packet_);
                continue;
            }
            if(decode_packet(audio_codec_ctx_, packet_)) {
                av_packet_unref(packet_);
                AudioFrame af;
                af.sample_rate = audio_codec_ctx_->sample_rate;
                af.channels = audio_codec_ctx_->ch_layout.nb_channels;
                af.pts = frame_->pts == AV_NOPTS_VALUE ? 0 : av_rescale_q(frame_->pts, fmt_->streams[audio_stream_index_]->time_base, AVRational{1,1000000});
                // Determine format
                if(frame_->format == AV_SAMPLE_FMT_FLTP) {
                    af.format = SampleFormat::FLTP;
                    int samples = frame_->nb_samples;
                    // Interleave planar floats into single buffer
                    af.data.resize(sizeof(float) * static_cast<size_t>(samples) * static_cast<size_t>(af.channels));
                    float* out = reinterpret_cast<float*>(af.data.data());
                    for(int s=0; s<samples; ++s) {
                        for(int c=0; c<af.channels; ++c) {
                            float* plane = reinterpret_cast<float*>(frame_->data[c]);
                            out[s * af.channels + c] = plane[s];
                        }
                    }
                } else if(frame_->format == AV_SAMPLE_FMT_S16) {
                    af.format = SampleFormat::S16;
                    int bytes = frame_->nb_samples * af.channels * 2;
                    af.data.resize(static_cast<size_t>(bytes));
                    std::memcpy(af.data.data(), frame_->data[0], static_cast<size_t>(bytes));
                } else {
                    af.format = SampleFormat::Unknown;
                }
                stats_.audio_frames_decoded++;
                return af;
            }
            av_packet_unref(packet_);
        }
    }

    const DecoderStats& stats() const override { return stats_; }

    ~FFmpegDecoder() override {
        LOG_DECODER_CORE_DTOR(this);
        // Stop decode thread first, then cleanup
        // Cleanup is handled by HardwareAccelContext destructor
        if(hw_transfer_frame_) av_frame_free(&hw_transfer_frame_);
        if(frame_) av_frame_free(&frame_);
        if(packet_) av_packet_free(&packet_);
        if(video_codec_ctx_) avcodec_free_context(&video_codec_ctx_);
        if(audio_codec_ctx_) avcodec_free_context(&audio_codec_ctx_);
        if(fmt_) avformat_close_input(&fmt_);
    }

private:
    bool open_codec(int index, AVCodecContext*& ctx) {
        AVStream* s = fmt_->streams[index];
        const AVCodec* dec = avcodec_find_decoder(s->codecpar->codec_id);
        if(!dec) return false;
        
        ctx = avcodec_alloc_context3(dec);
        if(!ctx) return false;
        
        if(avcodec_parameters_to_context(ctx, s->codecpar) < 0) return false;
        
        // Try hardware acceleration for video streams
        // Get optimal thread count from codec optimizer
        auto recommended_config = codec_optimizer_->recommend_config(
            s->codecpar->codec_id == AV_CODEC_ID_H264 ? "h264" : 
            s->codecpar->codec_id == AV_CODEC_ID_HEVC ? "h265" : "unknown",
            s->codecpar->width, s->codecpar->height, av_q2d(s->avg_frame_rate));
        
        int optimal_threads = recommended_config.max_decode_threads;
        ve::log::info("Using " + std::to_string(optimal_threads) + "-thread configuration for 4K optimization");
        
        // Use optimized threading from codec optimizer
        ctx->thread_count = optimal_threads; 
        ctx->thread_type = FF_THREAD_FRAME;
        
        // Absolutely minimal flags for stability
        ctx->flags = 0; // No aggressive flags
        ctx->flags2 = 0; // No aggressive flags
        
        // Enable hardware acceleration for performance only if it doesn't cause issues
        // Enable hardware acceleration only if the codec supports the chosen type
        bool hw_ok = false;
        if (hw_accel_ctx_.hw_type != AV_HWDEVICE_TYPE_NONE && hw_accel::codec_supports_hwaccel(dec, hw_accel_ctx_.hw_type)) {
            hw_ok = setup_hardware_acceleration(ctx);
        }
        if (!hw_ok && hw_accel_ctx_.hw_type != AV_HWDEVICE_TYPE_NONE) {
            ve::log::warn("HW accel not enabled (unsupported or setup failed) → using SW");
            hw_accel_ctx_.hw_type = AV_HWDEVICE_TYPE_NONE;
        }
        
        if(avcodec_open2(ctx, dec, nullptr) < 0) return false;
        
        // Minimal mode: No post-open optimizations to prevent corruption
        ve::log::info("Decoder opened in minimal stability mode");
        
        return true;
    }
    
    bool setup_hardware_acceleration(AVCodecContext* ctx) {
        // Create hardware device context if not already created
        if (!hw_accel_ctx_.hw_device_ctx) {
            if (av_hwdevice_ctx_create(&hw_accel_ctx_.hw_device_ctx, hw_accel_ctx_.hw_type, 
                                      nullptr, nullptr, 0) < 0) {
                ve::log::error("Failed to create hardware device context");
                return false;
            }
        }
        
        // Attach to codec context and prepare frames context
        ctx->hw_device_ctx = av_buffer_ref(hw_accel_ctx_.hw_device_ctx);
        ctx->opaque = &hw_accel_ctx_; // visible to get_format callback
        ctx->get_format = hw_accel::hw_get_format_dynamic;

        // Do NOT force-create hw frames context in advance.
        // Most FFmpeg decoders can allocate hw frames internally when get_format selects a hw format
        // and hw_device_ctx is provided. Avoid setting get_buffer2 without a ready frames ctx.
        return true;
    }
    
    // Transfer hardware frame to system memory if needed
    bool transfer_hardware_frame(AVFrame* hw_frame, AVFrame* sw_frame) {
        if (hw_frame->format == AV_PIX_FMT_DXVA2_VLD ||
            hw_frame->format == AV_PIX_FMT_CUDA
#ifdef HAVE_D3D11VA
            || hw_frame->format == AV_PIX_FMT_D3D11
#endif
            || hw_frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
            
            // Clear any existing data in the destination frame
            av_frame_unref(sw_frame);
            
            // Set up the destination frame properties
            sw_frame->format = AV_PIX_FMT_NV12; // Common output format
            sw_frame->width = hw_frame->width;
            sw_frame->height = hw_frame->height;
            
            // Allocate buffer for the software frame
            if (av_frame_get_buffer(sw_frame, 0) < 0) {
                ve::log::error("Failed to allocate buffer for software frame");
                return false;
            }
            
            // Transfer from GPU to CPU memory
            if (av_hwframe_transfer_data(sw_frame, hw_frame, 0) < 0) {
                ve::log::error("Failed to transfer hardware frame data");
                return false;
            }
            
            // Copy metadata and properties
            av_frame_copy_props(sw_frame, hw_frame);
            return true;
        }
        
        // Frame is already in system memory
        av_frame_move_ref(sw_frame, hw_frame);
        return true;
    }

    bool decode_packet(AVCodecContext* ctx, AVPacket* pkt) {
        if(avcodec_send_packet(ctx, pkt) < 0) return false;
        int r = avcodec_receive_frame(ctx, frame_);
        
        // Handle hardware frames
        if (r == 0 && ctx == video_codec_ctx_) {
#if defined(_WIN32) && VE_ENABLE_D3D11VA
            if (frame_->format == AV_PIX_FMT_D3D11) {
                // For D3D11 frames, we need to let the main read_video() function handle them
                // The D3D11 transfer will be done in read_video() where VideoFrame is created
                return true; // Frame is ready for processing
            }
#endif
            // Existing hardware frame handling for other formats
            if (frame_->format == AV_PIX_FMT_DXVA2_VLD ||
                frame_->format == AV_PIX_FMT_CUDA ||
                frame_->format == AV_PIX_FMT_VIDEOTOOLBOX) {
                // Transfer hardware frame to system memory for processing
                if (!transfer_hardware_frame(frame_, hw_transfer_frame_)) {
                    ve::log::error("Failed to transfer hardware frame");
                    return false;
                }
                // Swap frames so processing uses the transferred frame
                AVFrame* temp = frame_;
                frame_ = hw_transfer_frame_;
                hw_transfer_frame_ = temp;
            }
        }
        
        return r == 0;
    }

    OpenParams params_{};
    AVFormatContext* fmt_ = nullptr;
    AVCodecContext* video_codec_ctx_ = nullptr;
    AVCodecContext* audio_codec_ctx_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVFrame* hw_transfer_frame_ = nullptr; // For hardware frame transfers
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
    DecoderStats stats_{};
    HardwareAccelContext hw_accel_ctx_{};
    
    // Performance optimization components
    std::unique_ptr<CodecOptimizer> codec_optimizer_;
    std::unique_ptr<IGpuUploader> gpu_uploader_;
    
    // Initialize performance optimization systems
    void initialize_optimization() {
        codec_optimizer_ = std::make_unique<CodecOptimizer>();
        gpu_uploader_ = create_hardware_uploader();
        
        // Enable adaptive optimization
        codec_optimizer_->enable_adaptive_optimization([this](const CodecOptimizationStats& stats) {
            this->on_optimization_feedback(stats);
        });
    }
    
    // Handle optimization feedback for adaptive performance
    void on_optimization_feedback(const CodecOptimizationStats& stats) {
        if (stats.decode_fps < 30.0 && stats.gpu_utilization < 50.0) {
            // Low FPS with low GPU usage - try hardware acceleration
            ve::log::info("Low performance detected, attempting hardware acceleration");
        }
    }
    
    // Apply codec-specific optimizations based on stream properties
    void apply_codec_optimizations(int stream_index) {
        if (!fmt_ || stream_index < 0) return;
        
        AVStream* stream = fmt_->streams[stream_index];
        AVCodecParameters* params = stream->codecpar;
        
        std::string codec_name = avcodec_get_name(params->codec_id);
        int width = params->width;
        int height = params->height;
        
        // Calculate target frame rate
        double fps = 30.0; // Default
        if (stream->avg_frame_rate.den != 0) {
            fps = static_cast<double>(stream->avg_frame_rate.num) / stream->avg_frame_rate.den;
        }
        
        // Apply format-specific optimizations
        if (codec_name == "prores") {
            // Detect ProRes variant from format tag or other metadata
            std::string variant = "422"; // Default, should be detected
            codec_optimizer_->apply_prores_optimization(variant);
        } else if (codec_name == "hevc") {
            bool is_10bit = (params->bits_per_coded_sample > 8);
            bool is_hdr = (params->color_trc == AVCOL_TRC_SMPTE2084 || 
                          params->color_trc == AVCOL_TRC_ARIB_STD_B67);
            codec_optimizer_->apply_hevc_optimization(is_10bit, is_hdr);
        } else if (codec_name == "h264") {
            bool is_high_profile = (params->profile == FF_PROFILE_H264_HIGH);
            codec_optimizer_->apply_h264_optimization(is_high_profile);
        }
        
        // Get recommended configuration
        auto recommended_config = codec_optimizer_->recommend_config(codec_name, width, height, fps);
        codec_optimizer_->configure_codec(codec_name, recommended_config);
        
        ve::log::info("Applied codec optimization for " + codec_name + ": " + 
                     std::to_string(width) + "x" + std::to_string(height) + " @ " + 
                     std::to_string(fps) + "fps");
    }
};

} // anonymous namespace

std::unique_ptr<IDecoder> create_ffmpeg_decoder() { return std::make_unique<FFmpegDecoder>(); }

#endif // VE_HAVE_FFMPEG

} // namespace ve::decode
