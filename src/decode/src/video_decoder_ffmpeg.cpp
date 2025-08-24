#include "decode/decoder.hpp"
#include "core/log.hpp"
#if VE_HAVE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
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

class FFmpegDecoder final : public IDecoder {
public:
    bool open(const OpenParams& params) override {
        params_ = params;
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
        if(params.video && video_stream_index_ >= 0) {
            if(!open_codec(video_stream_index_, video_codec_ctx_)) return false;
        }
        if(params.audio && audio_stream_index_ >= 0) {
            if(!open_codec(audio_stream_index_, audio_codec_ctx_)) return false;
        }
        packet_ = av_packet_alloc();
        frame_ = av_frame_alloc();
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
                if (frame_->color_range == AVCOL_RANGE_JPEG) {
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
        if(avcodec_open2(ctx, dec, nullptr) < 0) return false;
        return true;
    }

    bool decode_packet(AVCodecContext* ctx, AVPacket* pkt) {
        if(avcodec_send_packet(ctx, pkt) < 0) return false;
        int r = avcodec_receive_frame(ctx, frame_);
        return r == 0;
    }

    OpenParams params_{};
    AVFormatContext* fmt_ = nullptr;
    AVCodecContext* video_codec_ctx_ = nullptr;
    AVCodecContext* audio_codec_ctx_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
    DecoderStats stats_{};
};

} // anonymous namespace

std::unique_ptr<IDecoder> create_ffmpeg_decoder() { return std::make_unique<FFmpegDecoder>(); }

#endif // VE_HAVE_FFMPEG

} // namespace ve::decode
