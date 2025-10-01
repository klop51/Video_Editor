#include "VideoPlayer.h"
#include <iostream>

VideoPlayer::VideoPlayer(SDL_Renderer* externalRenderer)
    : window(nullptr), renderer(externalRenderer), texture(nullptr),
      formatContext(nullptr), codecContext(nullptr), audioCodecContext(nullptr),
      frame(nullptr), frameRGB(nullptr), packet(nullptr), swsContext(nullptr),
      videoStreamIndex(-1), audioStreamIndex(-1), playing(false), paused(false), fullscreen(false),
      volume(1.0f), frameTimer(0.0), frameLastDelay(0.0), lastTime(0.0),
      windowWidth(800), windowHeight(600), selectedAudioDevice("") {
}

VideoPlayer::~VideoPlayer() {
    cleanupFFmpeg();
    cleanupSDL();
}

bool VideoPlayer::initializeSDL() {
    // If we have an external renderer, don't create our own window/renderer
    if (renderer) {
        return true;
    }

    window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void VideoPlayer::cleanupSDL() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

bool VideoPlayer::initializeFFmpeg() {
    frame = av_frame_alloc();
    frameRGB = av_frame_alloc();
    packet = av_packet_alloc();

    if (!frame || !frameRGB || !packet) {
        std::cerr << "Failed to allocate FFmpeg frames/packet" << std::endl;
        return false;
    }

    return true;
}

void VideoPlayer::cleanupFFmpeg() {
    if (packet) {
        av_packet_free(&packet);
    }
    if (frameRGB) {
        av_frame_free(&frameRGB);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (audioCodecContext) {
        avcodec_free_context(&audioCodecContext);
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }
    if (formatContext) {
        avformat_close_input(&formatContext);
    }
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
}

bool VideoPlayer::loadVideo(const std::string& filename) {
    // Initialize SDL if not already done
    if (!window && !initializeSDL()) {
        return false;
    }

    // Initialize FFmpeg components
    if (!initializeFFmpeg()) {
        return false;
    }

    // Open video file
    if (avformat_open_input(&formatContext, filename.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Failed to open video file: " << filename << std::endl;
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Failed to find stream information" << std::endl;
        return false;
    }

    // Debug: Print stream info
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        AVCodecParameters* par = formatContext->streams[i]->codecpar;
        int nb_channels = par->ch_layout.nb_channels;
        std::cout << "Stream " << i << ": type=" << par->codec_type << ", channels=" << nb_channels << std::endl;
    }

    // Find video and audio streams (prefer stereo audio over surround)
    int preferredAudioIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        AVCodecParameters* par = formatContext->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex == -1) {
            videoStreamIndex = i;
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
            int nb_channels = par->ch_layout.nb_channels;
            if (nb_channels == 2 && preferredAudioIndex == -1) {
                preferredAudioIndex = i; // Prefer stereo
            } else if (preferredAudioIndex == -1) {
                preferredAudioIndex = i; // Fallback to any audio
            }
        }
    }
    audioStreamIndex = preferredAudioIndex;

    if (videoStreamIndex == -1) {
        std::cerr << "No video stream found" << std::endl;
        return false;
    }

    // Get codec parameters
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);

    if (!codec) {
        std::cerr << "Unsupported codec" << std::endl;
        return false;
    }

    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }

    // Copy codec parameters to context
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        std::cerr << "Failed to copy codec parameters" << std::endl;
        return false;
    }

    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return false;
    }

    // Initialize audio if available
    if (audioStreamIndex != -1) {
        AVCodecParameters* audioCodecParameters = formatContext->streams[audioStreamIndex]->codecpar;
        const AVCodec* audioCodec = avcodec_find_decoder(audioCodecParameters->codec_id);

        if (audioCodec) {
            audioCodecContext = avcodec_alloc_context3(audioCodec);
            if (audioCodecContext) {
                if (avcodec_parameters_to_context(audioCodecContext, audioCodecParameters) >= 0 &&
                    avcodec_open2(audioCodecContext, audioCodec, nullptr) >= 0) {
                    // Initialize audio manager
                    if (!audioManager.initialize(audioCodecContext, formatContext, audioStreamIndex, selectedAudioDevice)) {
                        std::cerr << "Failed to initialize audio manager" << std::endl;
                        // Continue without audio
                        avcodec_free_context(&audioCodecContext);
                        audioCodecContext = nullptr;
                        audioStreamIndex = -1;
                    }
                } else {
                    avcodec_free_context(&audioCodecContext);
                    audioCodecContext = nullptr;
                    audioStreamIndex = -1;
                }
            }
        } else {
            audioStreamIndex = -1;
        }
    }

    // Allocate video frames
    frame->format = codecContext->pix_fmt;
    frame->width = codecContext->width;
    frame->height = codecContext->height;

    frameRGB->format = AV_PIX_FMT_RGB24;
    frameRGB->width = codecContext->width;
    frameRGB->height = codecContext->height;

    if (av_frame_get_buffer(frame, 0) < 0 || av_frame_get_buffer(frameRGB, 0) < 0) {
        std::cerr << "Failed to allocate frame buffers" << std::endl;
        return false;
    }

    // Initialize scaling context
    swsContext = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt,
                               codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsContext) {
        std::cerr << "Failed to initialize scaling context" << std::endl;
        return false;
    }

    // Create SDL texture
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                               codecContext->width, codecContext->height);

    if (!texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set window size to video size
    SDL_SetWindowSize(window, codecContext->width, codecContext->height);
    windowWidth = codecContext->width;
    windowHeight = codecContext->height;

    return true;
}

void VideoPlayer::play() {
    playing = true;
    paused = false;
    audioManager.play();
}

void VideoPlayer::pause() {
    paused = !paused;
    if (paused) {
        audioManager.pause();
    } else {
        audioManager.play();
    }
}

void VideoPlayer::stop() {
    playing = false;
    paused = false;
    audioManager.stop();
    // Seek to beginning
    av_seek_frame(formatContext, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    frameTimer = 0.0;
    lastTime = 0.0;
}

void VideoPlayer::toggleFullscreen() {
    fullscreen = !fullscreen;
    SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void VideoPlayer::volumeUp() {
    volume = std::min(1.0f, volume + 0.1f);
    audioManager.setVolume(volume);
}

void VideoPlayer::volumeDown() {
    volume = std::max(0.0f, volume - 0.1f);
    audioManager.setVolume(volume);
}

void VideoPlayer::setAudioDevice(const std::string& deviceName) {
    audioManager.setAudioDevice(deviceName);
}

void VideoPlayer::seek(float seconds) {
    if (!formatContext || videoStreamIndex == -1) return;

    int64_t timestamp = av_rescale_q(seconds * AV_TIME_BASE,
                                   AV_TIME_BASE_Q,
                                   formatContext->streams[videoStreamIndex]->time_base);

    av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
    frameTimer = seconds;
}

void VideoPlayer::update() {
    if (!playing || paused || !formatContext) return;

    double currentTime = SDL_GetTicks() / 1000.0;
    if (lastTime == 0.0) lastTime = currentTime;
    double elapsed = currentTime - lastTime;
    lastTime = currentTime;
    frameTimer += elapsed;

    double frameDelay = 1.0 / av_q2d(formatContext->streams[videoStreamIndex]->r_frame_rate);

    while (frameTimer >= frameDelay) {
        if (decodeFrame()) {
            renderFrame();
            frameTimer -= frameDelay;
        } else {
            break;
        }
    }
}

bool VideoPlayer::decodeFrame() {
    int response = av_read_frame(formatContext, packet);
    if (response < 0) {
        // End of file or error
        if (response == AVERROR_EOF) {
            stop(); // Stop at end of video
        }
        return false;
    }

    if (packet->stream_index == videoStreamIndex) {
        // Process video packet
        response = avcodec_send_packet(codecContext, packet);
        if (response < 0) {
            std::cerr << "Error sending packet to decoder" << std::endl;
            av_packet_unref(packet);
            return false;
        }

        response = avcodec_receive_frame(codecContext, frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(packet);
            return false;
        } else if (response < 0) {
            std::cerr << "Error receiving frame from decoder" << std::endl;
            av_packet_unref(packet);
            return false;
        }

        // Convert frame to RGB
        sws_scale(swsContext, frame->data, frame->linesize, 0, codecContext->height,
                 frameRGB->data, frameRGB->linesize);

        av_packet_unref(packet);
        return true;
    } else if (packet->stream_index == audioStreamIndex && audioCodecContext) {
        // Process audio packet
        response = avcodec_send_packet(audioCodecContext, packet);
        if (response < 0) {
            std::cerr << "Error sending audio packet to decoder" << std::endl;
            av_packet_unref(packet);
            return false;
        }

        AVFrame* audioFrame = av_frame_alloc();
        int frameCount = 0;
        while (response >= 0) {
            response = avcodec_receive_frame(audioCodecContext, audioFrame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                break;
            } else if (response < 0) {
                std::cerr << "Error receiving audio frame from decoder" << std::endl;
                break;
            }

            frameCount++;
            // Queue audio frame for playback
            if (!audioManager.isQueueFull()) {
                audioManager.queueAudio(audioFrame);
            } else {
                static int skipCount = 0;
                if (++skipCount % 50 == 0) { // Log every 50th skip
                    std::cout << "Skipped audio frame - queue full" << std::endl;
                }
            }
        }

        // Debug: Log audio decoding stats
        static int totalFrames = 0;
        static Uint32 lastLogTime = 0;
        totalFrames += frameCount;
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastLogTime > 2000) { // Log every 2 seconds
            std::cout << "Audio decode: " << frameCount << " frames this packet, "
                      << totalFrames << " total frames" << std::endl;
            lastLogTime = currentTime;
        }

        av_frame_free(&audioFrame);
    }

    av_packet_unref(packet);
    return false; // No video frame decoded
}

void VideoPlayer::renderFrame() {
    if (!texture || !renderer) return;

    // Update texture with RGB frame data
    SDL_UpdateTexture(texture, nullptr, frameRGB->data[0], frameRGB->linesize[0]);

    // Clear renderer
    SDL_RenderClear(renderer);

    // Copy texture to renderer
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);

    // Present renderer only if we own it (not external)
    if (window) {
        SDL_RenderPresent(renderer);
    }

    // Handle frame timing (basic implementation)
    double frameDelay = 1.0 / av_q2d(formatContext->streams[videoStreamIndex]->r_frame_rate);
    frameLastDelay = frameDelay;
    // Removed SDL_Delay to allow continuous decoding
}

void VideoPlayer::handleResize(int width, int height) {
    windowWidth = width;
    windowHeight = height;
    // Note: Texture size remains the video size, SDL_RenderCopy will scale it
}