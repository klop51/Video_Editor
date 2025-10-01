#include "AudioManager.h"
#include <iostream>
#include <algorithm>

std::vector<std::string> AudioManager::getAvailableAudioDevices() {
    std::vector<std::string> devices;
    
    int numDevices = SDL_GetNumAudioDevices(0); // 0 for output devices
    for (int i = 0; i < numDevices; ++i) {
        const char* deviceName = SDL_GetAudioDeviceName(i, 0);
        if (deviceName) {
            devices.push_back(deviceName);
        }
    }
    
    return devices;
}

std::string AudioManager::getDefaultAudioDevice() {
    return ""; // Empty string means use default device
}

AudioManager::AudioManager()
    : audioDevice(0), codecContext(nullptr),
      audioStreamIndex(-1), playing(false), volume(1.0f),
      bufferSize(0), bufferPos(0), currentDeviceName("") {
    audioBuffer = std::make_unique<Uint8[]>(AUDIO_BUFFER_SIZE);
}

AudioManager::~AudioManager() {
    closeAudioDevice();
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }
}

bool AudioManager::initialize(AVCodecContext* audioCodecContext, AVFormatContext* formatContext, int audioStreamIndex, const std::string& deviceName) {
    this->codecContext = audioCodecContext;
    this->audioStreamIndex = audioStreamIndex;

    // Initialize SDL audio with a simple format
    if (!openAudioDevice(deviceName)) {
        return false;
    }

    // For now, skip resampling - assume the audio format is compatible
    // The video player now selects stereo streams, so this should work better

    return true;
}

void AudioManager::play() {
    playing = true;
    SDL_PauseAudioDevice(audioDevice, 0); // Start audio playback
}

void AudioManager::pause() {
    playing = false;
    SDL_PauseAudioDevice(audioDevice, 1); // Pause audio playback
}

void AudioManager::stop() {
    playing = false;
    SDL_PauseAudioDevice(audioDevice, 1);
    bufferPos = 0;
}

void AudioManager::setVolume(float volume) {
    this->volume = std::max(0.0f, std::min(1.0f, volume));
}

bool AudioManager::isQueueFull() const {
    return bufferSize >= AUDIO_BUFFER_SIZE / 2; // Consider full when half buffer is used
}

void AudioManager::queueAudio(const AVFrame* frame) {
    if (!playing || !frame) return;

    // Debug: Print audio frame info once
    static bool firstFrame = true;
    if (firstFrame) {
        std::cout << "Audio frame format: " << frame->format
                  << ", sample_rate: " << frame->sample_rate
                  << ", nb_samples: " << frame->nb_samples
                  << ", buffer_size: " << AUDIO_BUFFER_SIZE << std::endl;
        firstFrame = false;
    }

    // Handle stereo planar float format (most common)
    if (frame->format == AV_SAMPLE_FMT_FLTP) {
        // Check if we have stereo data
        int numChannels = frame->ch_layout.nb_channels;
        bool hasStereo = numChannels >= 2;

        if (hasStereo) {
            // Stereo audio
            int frameSize = frame->nb_samples * 2 * sizeof(float);

            // Debug: Log buffer status
            static int frameCount = 0;
            if (++frameCount % 100 == 0) { // Log every 100 frames
                std::cout << "Buffer status: " << bufferSize << "/" << AUDIO_BUFFER_SIZE
                          << " bytes (" << (bufferSize * 100.0f / AUDIO_BUFFER_SIZE) << "%)" << std::endl;
            }

            if (bufferSize + frameSize > AUDIO_BUFFER_SIZE) {
                static int dropCount = 0;
                if (++dropCount % 10 == 0) { // Log every 10th drop
                    std::cout << "Dropped audio frame - buffer full: " << bufferSize
                              << "/" << AUDIO_BUFFER_SIZE << " bytes" << std::endl;
                }
                return; // Buffer full
            }

            float* dst = reinterpret_cast<float*>(audioBuffer.get() + bufferSize);
            const float* srcL = reinterpret_cast<const float*>(frame->data[0]);
            const float* srcR = reinterpret_cast<const float*>(frame->data[1]);

            // Copy interleaved stereo data
            for (int i = 0; i < frame->nb_samples; ++i) {
                dst[i * 2] = srcL[i];     // Left channel
                dst[i * 2 + 1] = srcR[i]; // Right channel
            }

            bufferSize += frameSize;
        }
    }
}

void AudioManager::audioCallback(void* userdata, Uint8* stream, int len) {
    AudioManager* audioManager = static_cast<AudioManager*>(userdata);

    // Debug: Log callback frequency
    static Uint32 lastCallbackTime = 0;
    static int callbackCount = 0;
    Uint32 currentTime = SDL_GetTicks();
    if (++callbackCount % 100 == 0) { // Log every 100 callbacks
        Uint32 timeDiff = currentTime - lastCallbackTime;
        std::cout << "Audio callback: " << callbackCount << " calls, "
                  << timeDiff << "ms since last batch" << std::endl;
        lastCallbackTime = currentTime;
    }

    if (!audioManager->playing) {
        memset(stream, 0, len);
        return;
    }

    if (audioManager->bufferSize == 0) {
        // Debug: Log underruns
        static int underrunCount = 0;
        if (++underrunCount % 10 == 0) { // Log every 10th underrun
            std::cout << "Audio buffer underrun #" << underrunCount
                      << " - no data available" << std::endl;
        }
        memset(stream, 0, len);
        return;
    }

    int bytesToCopy = std::min(len, static_cast<int>(audioManager->bufferSize));

    // Debug: Log buffer usage
    static int bufferCheckCount = 0;
    if (++bufferCheckCount % 50 == 0) { // Log every 50 callbacks
        float bufferUsage = audioManager->bufferSize * 100.0f / AUDIO_BUFFER_SIZE;
        std::cout << "Buffer usage: " << bufferUsage << "% ("
                  << audioManager->bufferSize << "/" << AUDIO_BUFFER_SIZE << " bytes)" << std::endl;
    }

    // Copy audio data and apply volume scaling
    const float* src = reinterpret_cast<const float*>(audioManager->audioBuffer.get() + audioManager->bufferPos);
    float* dst = reinterpret_cast<float*>(stream);
    int samplesToCopy = bytesToCopy / sizeof(float);

    for (int i = 0; i < samplesToCopy; ++i) {
        dst[i] = src[i] * audioManager->volume;
    }

    // Update buffer position and size
    audioManager->bufferPos += bytesToCopy;
    audioManager->bufferSize -= bytesToCopy;

    // If we didn't fill the entire buffer, fill the rest with silence
    if (bytesToCopy < len) {
        static int partialFillCount = 0;
        if (++partialFillCount % 20 == 0) { // Log every 20th partial fill
            std::cout << "Partial buffer fill: " << bytesToCopy << "/" << len << " bytes" << std::endl;
        }
        memset(stream + bytesToCopy, 0, len - bytesToCopy);
    }

    // If buffer is getting low, shift remaining data to beginning
    if (audioManager->bufferSize < AUDIO_BUFFER_SIZE / 4 && audioManager->bufferPos > 0) {
        static int compactCount = 0;
        if (++compactCount % 30 == 0) { // Log every 30th compaction
            std::cout << "Buffer compacted: " << audioManager->bufferSize << " bytes remaining" << std::endl;
        }
        memmove(audioManager->audioBuffer.get(),
                audioManager->audioBuffer.get() + audioManager->bufferPos,
                audioManager->bufferSize);
        audioManager->bufferPos = 0;
    }
}

bool AudioManager::setAudioDevice(const std::string& deviceName) {
    // For now, just store the device name - it will be used when initialize is called
    // We can't change audio devices while playing
    currentDeviceName = deviceName;
    return true;
}

std::string AudioManager::getCurrentAudioDevice() const {
    return currentDeviceName;
}

bool AudioManager::openAudioDevice(const std::string& deviceName) {
    SDL_AudioSpec desiredSpec;
    SDL_zero(desiredSpec);

    desiredSpec.freq = AUDIO_SAMPLE_RATE;
    desiredSpec.format = AUDIO_F32SYS; // 32-bit float
    desiredSpec.channels = AUDIO_CHANNELS;
    desiredSpec.samples = AUDIO_BUFFER_SAMPLES;
    desiredSpec.callback = audioCallback;
    desiredSpec.userdata = this;

    // Open audio device with specified name (empty string means default)
    const char* device = deviceName.empty() ? nullptr : deviceName.c_str();
    audioDevice = SDL_OpenAudioDevice(device, 0, &desiredSpec, &audioSpec, 0);
    if (audioDevice == 0) {
        std::cerr << "Failed to open audio device";
        if (!deviceName.empty()) {
            std::cerr << " '" << deviceName << "'";
        }
        std::cerr << ": " << SDL_GetError() << std::endl;
        return false;
    }

    currentDeviceName = deviceName;
    return true;
}

void AudioManager::closeAudioDevice() {
    if (audioDevice != 0) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
}