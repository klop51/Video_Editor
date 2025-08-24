#include "media_io/demuxer.hpp"
#include "core/log.hpp"
#include <stdexcept>

#ifdef ENABLE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}
#endif

namespace ve::media_io {

#ifdef ENABLE_FFMPEG
class FFmpegDemuxer : public Demuxer {
public:
    FFmpegDemuxer() = default;
    ~FFmpegDemuxer() override { close(); }
    
    bool open(const std::string& path) override {
        close();
        
        // Open input file
        int ret = avformat_open_input(&format_ctx_, path.c_str(), nullptr, nullptr);
        if (ret < 0) {
            ve::log::error("Failed to open input file: " + path);
            return false;
        }
        
        // Find stream info
        ret = avformat_find_stream_info(format_ctx_, nullptr);
        if (ret < 0) {
            ve::log::error("Failed to find stream info");
            close();
            return false;
        }
        
        ve::log::info("Opened media file: " + path);
        ve::log::info("Format: " + std::string(format_ctx_->iformat->name));
        ve::log::info("Duration: " + std::to_string(format_ctx_->duration / AV_TIME_BASE) + " seconds");
        
        return true;
    }
    
    void close() override {
        if (format_ctx_) {
            avformat_close_input(&format_ctx_);
            format_ctx_ = nullptr;
        }
    }
    
    bool is_open() const override {
        return format_ctx_ != nullptr;
    }
    
    std::vector<StreamInfo> get_streams() const override {
        std::vector<StreamInfo> streams;
        if (!format_ctx_) return streams;
        
        for (unsigned int i = 0; i < format_ctx_->nb_streams; ++i) {
            AVStream* stream = format_ctx_->streams[i];
            AVCodecParameters* codecpar = stream->codecpar;
            
            StreamInfo info;
            info.index = i;
            
            switch (codecpar->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    info.type = StreamInfo::Video;
                    info.width = codecpar->width;
                    info.height = codecpar->height;
                    if (stream->avg_frame_rate.den != 0) {
                        info.frame_rate = ve::TimeRational{stream->avg_frame_rate.num, stream->avg_frame_rate.den};
                    }
                    info.pixel_format = av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecpar->format));
                    break;
                    
                case AVMEDIA_TYPE_AUDIO:
                    info.type = StreamInfo::Audio;
                    info.sample_rate = codecpar->sample_rate;
                    info.channels = codecpar->channels;
                    info.sample_format = av_get_sample_fmt_name(static_cast<AVSampleFormat>(codecpar->format));
                    break;
                    
                case AVMEDIA_TYPE_SUBTITLE:
                    info.type = StreamInfo::Subtitle;
                    break;
                    
                default:
                    continue; // Skip unknown types
            }
            
            const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
            info.codec_name = codec ? codec->name : "unknown";
            info.bit_rate = codecpar->bit_rate;
            
            if (stream->duration != AV_NOPTS_VALUE) {
                info.duration = ve::TimeDuration{stream->duration * stream->time_base.num, stream->time_base.den};
            }
            
            streams.push_back(info);
        }
        
        return streams;
    }
    
    ve::TimeDuration get_duration() const override {
        if (!format_ctx_ || format_ctx_->duration == AV_NOPTS_VALUE) {
            return ve::TimeDuration{0};
        }
        return ve::TimeDuration{format_ctx_->duration, AV_TIME_BASE};
    }
    
    bool seek(ve::TimePoint timestamp) override {
        if (!format_ctx_) return false;
        
        int64_t seek_target = timestamp.to_rational().num * AV_TIME_BASE / timestamp.to_rational().den;
        int ret = av_seek_frame(format_ctx_, -1, seek_target, AVSEEK_FLAG_BACKWARD);
        
        return ret >= 0;
    }
    
    bool read_packet(Packet& packet) override {
        if (!format_ctx_) return false;
        
        AVPacket* av_packet = av_packet_alloc();
        if (!av_packet) return false;
        
        int ret = av_read_frame(format_ctx_, av_packet);
        if (ret < 0) {
            av_packet_free(&av_packet);
            return false;
        }
        
        packet.stream_index = av_packet->stream_index;
        packet.data.assign(av_packet->data, av_packet->data + av_packet->size);
        packet.is_keyframe = (av_packet->flags & AV_PKT_FLAG_KEY) != 0;
        
        // Convert timestamps
        AVStream* stream = format_ctx_->streams[av_packet->stream_index];
        if (av_packet->pts != AV_NOPTS_VALUE) {
            packet.pts = ve::TimePoint{av_packet->pts * stream->time_base.num, stream->time_base.den};
        }
        if (av_packet->dts != AV_NOPTS_VALUE) {
            packet.dts = ve::TimePoint{av_packet->dts * stream->time_base.num, stream->time_base.den};
        }
        
        av_packet_free(&av_packet);
        return true;
    }
    
    std::string get_format_name() const override {
        return format_ctx_ ? format_ctx_->iformat->name : "";
    }
    
    std::string get_metadata(const std::string& key) const override {
        if (!format_ctx_) return "";
        
        AVDictionaryEntry* entry = av_dict_get(format_ctx_->metadata, key.c_str(), nullptr, 0);
        return entry ? entry->value : "";
    }
    
private:
    AVFormatContext* format_ctx_ = nullptr;
};
#endif

std::unique_ptr<Demuxer> Demuxer::create(const std::string& path) {
#ifdef ENABLE_FFMPEG
    auto demuxer = std::make_unique<FFmpegDemuxer>();
    if (demuxer->open(path)) {
        return demuxer;
    }
    return nullptr;
#else
    ve::log::warn("FFmpeg disabled; cannot create demuxer for: " + path);
    return nullptr;
#endif
}

} // namespace ve::media_io
