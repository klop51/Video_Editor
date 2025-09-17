#include "audio/decoder.hpp"
#include <core/log.hpp>
#include <sstream>

namespace ve::audio {

// ThreadedAudioDecoder implementation
ThreadedAudioDecoder::ThreadedAudioDecoder() {
    start_worker_thread();
}

ThreadedAudioDecoder::~ThreadedAudioDecoder() {
    stop_worker_thread();
}

uint64_t ThreadedAudioDecoder::submit_decode_request(std::unique_ptr<DecodeRequest> request) {
    if (!request) return 0;

    uint64_t request_id = next_request_id_.fetch_add(1);
    request->request_id = request_id;

    {
        std::lock_guard<std::mutex> lock(request_queue_mutex_);
        request_queue_.push(std::move(request));
        pending_count_.fetch_add(1);
    }
    
    request_available_.notify_one();
    return request_id;
}

bool ThreadedAudioDecoder::cancel_decode_request(uint64_t request_id) {
    std::lock_guard<std::mutex> lock(request_queue_mutex_);
    
    // Create new queue without the cancelled request
    std::queue<std::unique_ptr<DecodeRequest>> new_queue;
    bool found = false;
    
    while (!request_queue_.empty()) {
        auto req = std::move(request_queue_.front());
        request_queue_.pop();
        
        if (req->request_id == request_id) {
            req->cancelled.store(true);
            found = true;
            pending_count_.fetch_sub(1);
        } else {
            new_queue.push(std::move(req));
        }
    }
    
    request_queue_ = std::move(new_queue);
    return found;
}

void ThreadedAudioDecoder::cancel_all_requests() {
    std::lock_guard<std::mutex> lock(request_queue_mutex_);
    
    while (!request_queue_.empty()) {
        auto req = std::move(request_queue_.front());
        request_queue_.pop();
        req->cancelled.store(true);
        pending_count_.fetch_sub(1);
    }
}

size_t ThreadedAudioDecoder::get_pending_request_count() const {
    return pending_count_.load();
}

bool ThreadedAudioDecoder::is_busy() const {
    return worker_busy_.load() || pending_count_.load() > 0;
}

void ThreadedAudioDecoder::start_worker_thread() {
    stop_worker_.store(false);
    worker_thread_ = std::thread(&ThreadedAudioDecoder::worker_thread_loop, this);
}

void ThreadedAudioDecoder::stop_worker_thread() {
    stop_worker_.store(true);
    request_available_.notify_all();
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void ThreadedAudioDecoder::worker_thread_loop() {
    ve::log::info("Audio decoder worker thread started");
    
    while (!stop_worker_.load()) {
        std::unique_ptr<DecodeRequest> request;
        
        // Wait for work
        {
            std::unique_lock<std::mutex> lock(request_queue_mutex_);
            request_available_.wait(lock, [this] {
                return !request_queue_.empty() || stop_worker_.load();
            });
            
            if (stop_worker_.load()) break;
            
            if (!request_queue_.empty()) {
                request = std::move(request_queue_.front());
                request_queue_.pop();
                pending_count_.fetch_sub(1);
            }
        }
        
        if (!request) continue;
        
        // Check if request was cancelled
        if (request->cancelled.load()) {
            continue;
        }
        
        worker_busy_.store(true);
        
        // Process the request
        auto result = process_decode_request(*request);
        
        worker_busy_.store(false);
        
        // Call the callback if request wasn't cancelled
        if (!request->cancelled.load() && request->callback) {
            try {
                request->callback(result, result ? AudioError::None : last_error());
            } catch (const std::exception& e) {
                std::ostringstream oss;
                oss << "Exception in audio decode callback: " << e.what();
                ve::log::error(oss.str());
            }
        }
    }
    
    ve::log::info("Audio decoder worker thread stopped");
}

// AudioDecoderFactory implementation
std::unique_ptr<AudioDecoder> AudioDecoderFactory::create_decoder(AudioCodec codec) {
    // This will be implemented when we create concrete decoder classes
    std::ostringstream oss;
    oss << "AudioDecoderFactory::create_decoder not yet implemented for codec: " << static_cast<int>(codec);
    ve::log::warn(oss.str());
    return nullptr;
}

std::unique_ptr<AudioDecoder> AudioDecoderFactory::create_decoder_from_data(
    const std::vector<uint8_t>& stream_data
) {
    AudioCodec codec = detect_codec(stream_data);
    if (codec == AudioCodec::Unknown) {
        return nullptr;
    }
    return create_decoder(codec);
}

AudioCodec AudioDecoderFactory::detect_codec(const std::vector<uint8_t>& stream_data) {
    if (stream_data.size() < 4) {
        return AudioCodec::Unknown;
    }
    
    const uint8_t* data = stream_data.data();
    
    // MP3 detection - check for sync word
    if (stream_data.size() >= 2) {
        if ((data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)) {
            return AudioCodec::MP3;
        }
    }
    
    // FLAC detection - check for signature
    if (stream_data.size() >= 4) {
        if (data[0] == 'f' && data[1] == 'L' && data[2] == 'a' && data[3] == 'C') {
            return AudioCodec::FLAC;
        }
    }
    
    // PCM detection would require additional context (container format)
    // AAC detection requires more sophisticated parsing
    
    return AudioCodec::Unknown;
}

std::vector<AudioCodec> AudioDecoderFactory::get_supported_codecs() {
    return {
        AudioCodec::PCM,
        AudioCodec::MP3,
        AudioCodec::AAC,
        AudioCodec::FLAC
    };
}

bool AudioDecoderFactory::is_codec_supported(AudioCodec codec) {
    auto supported = get_supported_codecs();
    return std::find(supported.begin(), supported.end(), codec) != supported.end();
}

const char* AudioDecoderFactory::get_codec_name(AudioCodec codec) {
    switch (codec) {
        case AudioCodec::AAC:     return "AAC";
        case AudioCodec::MP3:     return "MP3";
        case AudioCodec::PCM:     return "PCM";
        case AudioCodec::FLAC:    return "FLAC";
        case AudioCodec::Vorbis:  return "Vorbis";
        case AudioCodec::Opus:    return "Opus";
        case AudioCodec::AC3:     return "AC-3";
        case AudioCodec::EAC3:    return "Enhanced AC-3";
        case AudioCodec::Unknown: return "Unknown";
    }
    return "Unknown";
}

// Decoder utilities
namespace decoder_utils {

    const char* error_to_string(AudioError error) {
        switch (error) {
            case AudioError::None:                return "No error";
            case AudioError::InvalidFormat:      return "Invalid format";
            case AudioError::DecodeFailed:       return "Decode failed";
            case AudioError::EndOfStream:        return "End of stream";
            case AudioError::InvalidTimestamp:   return "Invalid timestamp";
            case AudioError::InsufficientData:   return "Insufficient data";
            case AudioError::HardwareError:      return "Hardware error";
            case AudioError::MemoryError:        return "Memory error";
            case AudioError::ConfigurationError: return "Configuration error";
            case AudioError::NetworkError:       return "Network error";
            case AudioError::Interrupted:        return "Interrupted";
            case AudioError::Unknown:            return "Unknown error";
        }
        return "Unknown error";
    }

    bool is_recoverable_error(AudioError error) {
        switch (error) {
            case AudioError::None:
            case AudioError::EndOfStream:
                return true;
            case AudioError::InsufficientData:
            case AudioError::NetworkError:
            case AudioError::Interrupted:
                return true; // Can retry
            case AudioError::InvalidFormat:
            case AudioError::DecodeFailed:
            case AudioError::InvalidTimestamp:
            case AudioError::HardwareError:
            case AudioError::MemoryError:
            case AudioError::ConfigurationError:
            case AudioError::Unknown:
                return false; // Fatal errors
        }
        return false;
    }

    float get_decode_complexity(AudioCodec codec) {
        switch (codec) {
            case AudioCodec::PCM:     return 0.1f;  // Very light
            case AudioCodec::FLAC:    return 0.3f;  // Light
            case AudioCodec::MP3:     return 0.5f;  // Medium
            case AudioCodec::AAC:     return 0.7f;  // Medium-heavy
            case AudioCodec::Vorbis:  return 0.6f;  // Medium
            case AudioCodec::Opus:    return 0.8f;  // Heavy
            case AudioCodec::AC3:     return 0.6f;  // Medium
            case AudioCodec::EAC3:    return 0.8f;  // Heavy
            case AudioCodec::Unknown: return 1.0f;  // Unknown, assume worst
        }
        return 1.0f;
    }

    uint32_t get_recommended_buffer_size(AudioCodec codec, uint32_t sample_rate) {
        // Base buffer size for 10ms at given sample rate
        uint32_t base_size = sample_rate / 100;
        
        float complexity = get_decode_complexity(codec);
        
        // Scale buffer size based on complexity
        uint32_t recommended = static_cast<uint32_t>(base_size * (1.0f + complexity));
        
        // Ensure power of 2 and reasonable bounds
        recommended = std::max(64u, recommended);
        recommended = std::min(4096u, recommended);
        
        // Round to next power of 2
        recommended--;
        recommended |= recommended >> 1;
        recommended |= recommended >> 2;
        recommended |= recommended >> 4;
        recommended |= recommended >> 8;
        recommended |= recommended >> 16;
        recommended++;
        
        return recommended;
    }

} // namespace decoder_utils

} // namespace ve::audio