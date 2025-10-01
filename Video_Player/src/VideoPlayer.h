#pragma once

#include <string>
#include <SDL.h>
#include "AudioManager.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

class VideoPlayer {
public:
    VideoPlayer(SDL_Renderer* externalRenderer = nullptr);
    ~VideoPlayer();

    // Video loading and control
    bool loadVideo(const std::string& filename);
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return playing; }

    // Display controls
    void toggleFullscreen();

    // Audio controls
    void volumeUp();
    void volumeDown();
    void setVolume(float vol) { volume = vol; audioManager.setVolume(vol); }
    void setAudioDevice(const std::string& deviceName);
    std::string getAudioDevice() const { return selectedAudioDevice; }

    // Playback controls
    void seek(float seconds);

    // Update method (called in main loop)
    void update();

    // Getters for ImGui
    SDL_Window* getWindow() const { return window; }
    SDL_Renderer* getRenderer() const { return renderer; }

private:
    // SDL components
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int windowWidth;
    int windowHeight;

    // FFmpeg components
    AVFormatContext* formatContext;
    AVCodecContext* codecContext;
    AVCodecContext* audioCodecContext;
    AVFrame* frame;
    AVFrame* frameRGB;
    AVPacket* packet;
    SwsContext* swsContext;
    int videoStreamIndex;
    int audioStreamIndex;

    // Audio manager
    AudioManager audioManager;

    // Playback state
    bool playing;
    bool paused;
    bool fullscreen;
    float volume;
    std::string selectedAudioDevice;

    // Timing
    double frameTimer;
    double frameLastDelay;
    double lastTime;

    // Private helper methods
    bool initializeSDL();
    void cleanupSDL();
    bool initializeFFmpeg();
    void cleanupFFmpeg();
    bool decodeFrame();
    void renderFrame();
    void handleResize(int width, int height);
};