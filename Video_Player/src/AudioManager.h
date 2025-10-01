#pragma once

#include <SDL.h>
#include <memory>
#include <vector>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

// Audio constants - updated to match common video audio rates
#define AUDIO_SAMPLE_RATE 48000  // Changed from 44100 to match video audio
#define AUDIO_CHANNELS 2
#define AUDIO_BUFFER_SIZE (384000)  // 1 second at 48kHz stereo float
#define AUDIO_BUFFER_SAMPLES 1024  // Larger buffer for stability

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Audio device enumeration
    static std::vector<std::string> getAvailableAudioDevices();
    static std::string getDefaultAudioDevice();

    // Audio initialization
    bool initialize(AVCodecContext* audioCodecContext, AVFormatContext* formatContext, int audioStreamIndex, const std::string& deviceName = "");

    // Playback control
    void play();
    void pause();
    void stop();

    // Volume control
    void setVolume(float volume); // 0.0 to 1.0
    float getVolume() const { return volume; }

    // Audio device control
    bool setAudioDevice(const std::string& deviceName);
    std::string getCurrentAudioDevice() const;

    // Audio queue management
    void queueAudio(const AVFrame* frame);
    bool isQueueFull() const;

    // SDL audio callback
    static void audioCallback(void* userdata, Uint8* stream, int len);

private:
    // SDL audio components
    SDL_AudioDeviceID audioDevice;
    SDL_AudioSpec audioSpec;
    std::string currentDeviceName;

    // FFmpeg audio components
    SwrContext* swrContext;
    AVCodecContext* codecContext;
    int audioStreamIndex;

    // Audio buffer
    std::unique_ptr<Uint8[]> audioBuffer;
    size_t bufferSize;
    size_t bufferPos;

    // Playback state
    bool playing;
    float volume;

    // Helper methods
    bool openAudioDevice(const std::string& deviceName = "");
    void closeAudioDevice();
    void resampleAudio(const AVFrame* frame);
};