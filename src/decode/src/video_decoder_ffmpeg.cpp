#include "decode/decoder.hpp"
#include "decode/codec_optimizer.hpp"
#include "decode/gpu_upload.hpp"
#include "core/log.hpp"

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
        default: return PixelFormat::Unknown;
    }
}

// Copy frame data for different pixel formats
bool copy_frame_data(AVFrame* frame, VideoFrame& vf) {
    const int width = frame->width;
    const int height = frame->height;
    
    switch (vf.format) {
        case PixelFormat::YUV420P:
        case PixelFormat::YUV420P10LE:
        case PixelFormat::YUV420P12LE: {
            int bytes_per_component = (vf.format == PixelFormat::YUV420P) ? 1 : 
                                    (vf.format == PixelFormat::YUV420P10LE) ? 2 : 2;
            int y_size = width * height * bytes_per_component;
            int uv_w = width / 2;
            int uv_h = height / 2;
            int uv_size = uv_w * uv_h * bytes_per_component;
            vf.data.resize(y_size + 2 * uv_size);
            
            // Copy Y plane
            for(int y = 0; y < height; ++y) {
                std::memcpy(&vf.data[y * width * bytes_per_component], 
                           frame->data[0] + y * frame->linesize[0], 
                           width * bytes_per_component);
            }
            // Copy U plane
            uint8_t* u_dst = vf.data.data() + y_size;
            for(int y = 0; y < uv_h; ++y) {
                std::memcpy(u_dst + y * uv_w * bytes_per_component, 
                           frame->data[1] + y * frame->linesize[1], 
                           uv_w * bytes_per_component);
            }
            // Copy V plane
            uint8_t* v_dst = vf.data.data() + y_size + uv_size;
            for(int y = 0; y < uv_h; ++y) {
                std::memcpy(v_dst + y * uv_w * bytes_per_component, 
                           frame->data[2] + y * frame->linesize[2], 
                           uv_w * bytes_per_component);
            }
            break;
        }
        
        case PixelFormat::YUV422P:
        case PixelFormat::YUV422P10LE:
        case PixelFormat::YUV422P12LE: {
            int bytes_per_component = (vf.format == PixelFormat::YUV422P) ? 1 : 2;
            int y_size = width * height * bytes_per_component;
            int uv_w = width / 2;
            int uv_h = height; // 422 has full height for UV
            int uv_size = uv_w * uv_h * bytes_per_component;
            vf.data.resize(y_size + 2 * uv_size);
            
            // Copy planes similar to 420P but with different UV dimensions
            copy_planar_yuv(frame, vf.data.data(), width, height, uv_w, uv_h, bytes_per_component);
            break;
        }
        
        case PixelFormat::YUV444P:
        case PixelFormat::YUV444P10LE:
        case PixelFormat::YUV444P12LE: {
            int bytes_per_component = (vf.format == PixelFormat::YUV444P) ? 1 : 2;
            int plane_size = width * height * bytes_per_component;
            vf.data.resize(3 * plane_size);
            
            // Copy all three planes (same size for 444)
            copy_planar_yuv(frame, vf.data.data(), width, height, width, height, bytes_per_component);
            break;
        }
        
        case PixelFormat::NV12:
        case PixelFormat::P010LE:
        case PixelFormat::P016LE: {
            int bytes_per_component = (vf.format == PixelFormat::NV12) ? 1 : 2;
            int y_size = width * height * bytes_per_component;
            int uv_size = (width * height / 2) * bytes_per_component; // Interleaved UV
            vf.data.resize(y_size + uv_size);
            
            // Copy Y plane
            for(int y = 0; y < height; ++y) {
                std::memcpy(&vf.data[y * width * bytes_per_component], 
                           frame->data[0] + y * frame->linesize[0], 
                           width * bytes_per_component);
            }
            // Copy interleaved UV plane
            uint8_t* uv_dst = vf.data.data() + y_size;
            for(int y = 0; y < height / 2; ++y) {
                std::memcpy(uv_dst + y * width * bytes_per_component, 
                           frame->data[1] + y * frame->linesize[1], 
                           width * bytes_per_component);
            }
            break;
        }
        
        case PixelFormat::RGB24:
        case PixelFormat::BGR24: {
            int row_bytes = width * 3;
            vf.data.resize(row_bytes * height);
            for(int y = 0; y < height; ++y) {
                std::memcpy(&vf.data[y * row_bytes], 
                           frame->data[0] + y * frame->linesize[0], 
                           row_bytes);
            }
            break;
        }
        
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32: {
            int row_bytes = width * 4;
            vf.data.resize(row_bytes * height);
            for(int y = 0; y < height; ++y) {
                std::memcpy(&vf.data[y * row_bytes], 
                           frame->data[0] + y * frame->linesize[0], 
                           row_bytes);
            }
            break;
        }
        
        case PixelFormat::YUYV422:
        case PixelFormat::UYVY422: {
            int row_bytes = width * 2; // Packed format
            vf.data.resize(row_bytes * height);
            for(int y = 0; y < height; ++y) {
                std::memcpy(&vf.data[y * row_bytes], 
                           frame->data[0] + y * frame->linesize[0], 
                           row_bytes);
            }
            break;
        }
        
        case PixelFormat::GRAY8: {
            int row_bytes = width;
            vf.data.resize(row_bytes * height);
            for(int y = 0; y < height; ++y) {
                std::memcpy(&vf.data[y * row_bytes], 
                           frame->data[0] + y * frame->linesize[0], 
                           row_bytes);
            }
            break;
        }
        
        case PixelFormat::GRAY16LE: {
            int row_bytes = width * 2;
            vf.data.resize(row_bytes * height);
            for(int y = 0; y < height; ++y) {
                std::memcpy(&vf.data[y * row_bytes], 
                           frame->data[0] + y * frame->linesize[0], 
                           row_bytes);
            }
            break;
        }
        
        default:
            return false;
    }
    
    return true;
}

// Helper function for copying planar YUV data
void copy_planar_yuv(AVFrame* frame, uint8_t* dst, int width, int height, 
                     int uv_width, int uv_height, int bytes_per_component) {
    int y_size = width * height * bytes_per_component;
    int uv_size = uv_width * uv_height * bytes_per_component;
    
    // Copy Y plane
    for(int y = 0; y < height; ++y) {
        std::memcpy(dst + y * width * bytes_per_component, 
                   frame->data[0] + y * frame->linesize[0], 
                   width * bytes_per_component);
    }
    
    // Copy U plane
    uint8_t* u_dst = dst + y_size;
    for(int y = 0; y < uv_height; ++y) {
        std::memcpy(u_dst + y * uv_width * bytes_per_component, 
                   frame->data[1] + y * frame->linesize[1], 
                   uv_width * bytes_per_component);
    }
    
    // Copy V plane
    uint8_t* v_dst = dst + y_size + uv_size;
    for(int y = 0; y < uv_height; ++y) {
        std::memcpy(v_dst + y * uv_width * bytes_per_component, 
                   frame->data[2] + y * frame->linesize[2], 
                   uv_width * bytes_per_component);
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

// Hardware acceleration helpers
namespace hw_accel {
    
    // Detect best available hardware acceleration
    AVHWDeviceType detect_best_hw_device() {
        // Priority order for Windows: D3D11VA -> DXVA2 -> CUDA -> None
        const AVHWDeviceType candidates[] = {
#ifdef _WIN32
#ifdef HAVE_D3D11VA
            AV_HWDEVICE_TYPE_D3D11VA,
#endif
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
    static int hw_get_buffer(AVCodecContext* ctx, AVFrame* frame, int flags) {
        return av_hwframe_get_buffer(ctx->hw_frames_ctx, frame, 0);
    }
    
} // namespace hw_accel

class FFmpegDecoder final : public IDecoder {
public:
    FFmpegDecoder() {
        initialize_optimization();
    }
    
    bool open(const OpenParams& params) override {
        params_ = params;
        
        // Initialize hardware acceleration
        hw_accel_ctx_.hw_type = hw_accel::detect_best_hw_device();
        
        if(avformat_open_input(&fmt_, params.filepath.c_str(), nullptr, nullptr) < 0) {
            ve::log::error("FFmpegDecoder: open_input failed");
            return false;
        }
        if(avformat_find_stream_info(fmt_, nullptr) < 0) {
            ve::log::error("FFmpegDecoder: stream_info failed");
            return false;
        }
        
        video_stream_index_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        audio_stream_index_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_AUDIO, -1, video_stream_index_, nullptr, 0);
        
        // Apply codec-specific optimizations
        if(params.video && video_stream_index_ >= 0) {
            apply_codec_optimizations(video_stream_index_);
            if(!open_codec(video_stream_index_, video_codec_ctx_)) return false;
        }
        if(params.audio && audio_stream_index_ >= 0) {
            if(!open_codec(audio_stream_index_, audio_codec_ctx_)) return false;
        }
        
        packet_ = av_packet_alloc();
        frame_ = av_frame_alloc();
        hw_transfer_frame_ = av_frame_alloc(); // For hardware frame transfers
        
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
        if(!video_codec_ctx_) return std::nullopt;
        while(true) {
            if(av_read_frame(fmt_, packet_) < 0) return std::nullopt; // EOF
            if(packet_->stream_index != video_stream_index_) {
                // If it's audio packet and we have audio decoder, stash for later? For simplicity just push back by re-queue: drop.
                av_packet_unref(packet_);
                continue;
            }
            if(decode_packet(video_codec_ctx_, packet_)) {
                av_packet_unref(packet_);
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
                
                // Copy frame data based on format
                if (!copy_frame_data(frame_, vf)) {
                    ve::log::error("Failed to copy frame data");
                    return std::nullopt;
                }
                stats_.video_frames_decoded++;
                return vf;
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
                    af.data.resize(sizeof(float) * samples * af.channels);
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
                    af.data.resize(bytes);
                    std::memcpy(af.data.data(), frame_->data[0], bytes);
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
        if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && hw_accel_ctx_.hw_type != AV_HWDEVICE_TYPE_NONE) {
            if (hw_accel::codec_supports_hwaccel(dec, hw_accel_ctx_.hw_type)) {
                if (setup_hardware_acceleration(ctx)) {
                    ve::log::info("Hardware acceleration enabled for codec: " + std::string(avcodec_get_name(dec->id)));
                } else {
                    ve::log::warn("Hardware acceleration setup failed, falling back to software");
                }
            }
        }
        
        if(avcodec_open2(ctx, dec, nullptr) < 0) return false;
        return true;
    }
    
    bool setup_hardware_acceleration(AVCodecContext* ctx) {
        // Create hardware device context if not already created
        if (!hw_accel_ctx_.hw_device_ctx) {
            if (av_hwdevice_ctx_create(&hw_accel_ctx_.hw_device_ctx, hw_accel_ctx_.hw_type, 
                                      nullptr, nullptr, 0) < 0) {
                return false;
            }
        }
        
        // Set hardware device context
        ctx->hw_device_ctx = av_buffer_ref(hw_accel_ctx_.hw_device_ctx);
        ctx->get_buffer2 = hw_accel::hw_get_buffer;
        
        // Create hardware frames context for zero-copy
        AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(hw_accel_ctx_.hw_device_ctx);
        if (!hw_frames_ref) return false;
        
        AVHWFramesContext* frames_ctx = (AVHWFramesContext*)hw_frames_ref->data;
        frames_ctx->format = ctx->pix_fmt; // Will be set by hardware
        frames_ctx->sw_format = AV_PIX_FMT_NV12; // Preferred format for hardware decode
        frames_ctx->width = ctx->width;
        frames_ctx->height = ctx->height;
        frames_ctx->initial_pool_size = 20; // Pool of hardware frames
        
        if (av_hwframe_ctx_init(hw_frames_ref) < 0) {
            av_buffer_unref(&hw_frames_ref);
            return false;
        }
        
        ctx->hw_frames_ctx = hw_frames_ref;
        hw_accel_ctx_.zero_copy_enabled = true;
        
        return true;
    }
    
    // Transfer hardware frame to system memory if needed
    bool transfer_hardware_frame(AVFrame* hw_frame, AVFrame* sw_frame) {
        if (hw_frame->format == AV_PIX_FMT_DXVA2_VLD ||
            hw_frame->format == AV_PIX_FMT_CUDA
#ifdef HAVE_D3D11VA
            || hw_frame->format == AV_PIX_FMT_D3D11
#endif
            ) {
            
            // Transfer from GPU to CPU memory
            sw_frame->format = AV_PIX_FMT_NV12; // Common output format
            if (av_hwframe_transfer_data(sw_frame, hw_frame, 0) < 0) {
                return false;
            }
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
        if (r == 0 && ctx == video_codec_ctx_ && hw_accel_ctx_.zero_copy_enabled) {
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
