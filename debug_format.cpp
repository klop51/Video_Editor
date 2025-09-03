#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

int main() {
    AVFormatContext* fmt = nullptr;
    if(avformat_open_input(&fmt, "LOL.mp4", nullptr, nullptr) < 0) {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }
    
    if(avformat_find_stream_info(fmt, nullptr) < 0) {
        std::cerr << "Failed to find stream info" << std::endl;
        return 1;
    }
    
    int video_index = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index < 0) {
        std::cerr << "No video stream found" << std::endl;
        return 1;
    }
    
    AVStream* stream = fmt->streams[video_index];
    std::cout << "Codec: " << avcodec_get_name(stream->codecpar->codec_id) << std::endl;
    std::cout << "Format: " << av_get_pix_fmt_name(static_cast<AVPixelFormat>(stream->codecpar->format)) << std::endl;
    std::cout << "Width: " << stream->codecpar->width << std::endl;
    std::cout << "Height: " << stream->codecpar->height << std::endl;
    
    // Try to open decoder and decode first frame
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "No decoder found" << std::endl;
        return 1;
    }
    
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, stream->codecpar);
    
    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return 1;
    }
    
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    // Read first video frame
    while (av_read_frame(fmt, packet) >= 0) {
        if (packet->stream_index == video_index) {
            if (avcodec_send_packet(ctx, packet) >= 0) {
                if (avcodec_receive_frame(ctx, frame) >= 0) {
                    std::cout << "Decoded frame format: " << av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format)) << std::endl;
                    std::cout << "Frame width: " << frame->width << std::endl;
                    std::cout << "Frame height: " << frame->height << std::endl;
                    
                    // Test the problematic function
                    int buf_size = av_image_get_buffer_size(static_cast<AVPixelFormat>(frame->format), frame->width, frame->height, 1);
                    std::cout << "Buffer size: " << buf_size << std::endl;
                    
                    if (buf_size > 0) {
                        std::vector<uint8_t> data(buf_size);
                        uint8_t* dst_data[4] = {nullptr, nullptr, nullptr, nullptr};
                        int dst_lines[4] = {0, 0, 0, 0};
                        
                        int result = av_image_fill_arrays(dst_data, dst_lines, data.data(),
                                                         static_cast<AVPixelFormat>(frame->format), 
                                                         frame->width, frame->height, 1);
                        std::cout << "Fill arrays result: " << result << std::endl;
                        
                        if (result >= 0) {
                            av_image_copy(dst_data, dst_lines,
                                         const_cast<const uint8_t**>(frame->data), frame->linesize,
                                         static_cast<AVPixelFormat>(frame->format), 
                                         frame->width, frame->height);
                            std::cout << "Copy successful!" << std::endl;
                        } else {
                            std::cout << "Fill arrays failed!" << std::endl;
                        }
                    } else {
                        std::cout << "Invalid buffer size!" << std::endl;
                    }
                    break;
                }
            }
        }
        av_packet_unref(packet);
    }
    
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
    
    return 0;
}
