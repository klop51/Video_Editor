#include "media_io/media_probe.hpp"
#include "core/log.hpp"
#include <filesystem>
#include <system_error>

#if VE_HAVE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#endif

namespace ve::media {

// to_us helper only needed when FFmpeg enabled
#if VE_HAVE_FFMPEG
static int64_t to_us(int64_t ts, AVRational tb) {
    if(ts == AV_NOPTS_VALUE) return 0;
    return av_rescale_q(ts, tb, AVRational{1, 1000000});
}
#endif

ProbeResult probe_file(const std::string& path) noexcept {
    ProbeResult res; res.filepath = path;
    std::error_code ec;
    if(std::filesystem::exists(path, ec)) {
        auto file_size = std::filesystem::file_size(path, ec);
        res.size_bytes = static_cast<int64_t>(file_size);
    }
#if VE_HAVE_FFMPEG
    AVFormatContext* fmt = nullptr;
    if(avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) {
        res.error_message = "Failed to open input";
        return res;
    }
    if(avformat_find_stream_info(fmt, nullptr) < 0) {
        res.error_message = "Failed to find stream info";
        avformat_close_input(&fmt);
        return res;
    }
    res.format = fmt->iformat ? fmt->iformat->name : "";
    if(fmt->duration > 0) {
        res.duration_us = to_us(fmt->duration, AVRational{1, AV_TIME_BASE});
    }
    for(unsigned i=0;i<fmt->nb_streams;++i) {
        AVStream* s = fmt->streams[i];
        StreamInfo si;
        if(s->codecpar) {
            si.bitrate = s->codecpar->bit_rate;
            si.width = s->codecpar->width;
            si.height = s->codecpar->height;
            if(s->codecpar->codec_id != AV_CODEC_ID_NONE) {
                const AVCodecDescriptor* desc = avcodec_descriptor_get(s->codecpar->codec_id);
                if(desc) si.codec = desc->name;
            }
            if(s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                si.type = "video";
                if(s->avg_frame_rate.num && s->avg_frame_rate.den) {
                    si.fps = av_q2d(s->avg_frame_rate);
                }
            } else if(s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                si.type = "audio";
                si.channels = s->codecpar->ch_layout.nb_channels;
                si.sample_rate = s->codecpar->sample_rate;
            } else {
                si.type = "other";
            }
        }
    if(s->duration > 0) si.duration_us = to_us(s->duration, s->time_base);
        res.streams.push_back(std::move(si));
    }
    res.success = true;
    avformat_close_input(&fmt);
    return res;
#else
    res.success = false;
    res.error_message = "FFmpeg not enabled";
    return res;
#endif
}

} // namespace ve::media
