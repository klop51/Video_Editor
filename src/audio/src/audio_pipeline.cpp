/**
 * @file audio_pipeline.cpp
 * @brief Audio Processing Pipeline Implementation
 *
 * Phase 1C: Integrates SimpleMixer and AudioOutput for complete audio processing.
 */

#include "audio/audio_pipeline.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>

namespace ve::audio {

std::unique_ptr<AudioPipeline> AudioPipeline::create(const AudioPipelineConfig& config) {
    return std::unique_ptr<AudioPipeline>(new AudioPipeline(config));
}

AudioPipeline::AudioPipeline(const AudioPipelineConfig& config)
    : config_(config) {
    audio_buffer_.resize(BUFFER_SIZE);
}

AudioPipeline::~AudioPipeline() {
    shutdown();
}

bool AudioPipeline::initialize() {
    if (state_.load() != AudioPipelineState::Uninitialized) {
        set_error("Pipeline already initialized");
        return false;
    }

    ve::log::info("Initializing audio pipeline...");

    // Initialize mixer
    if (!initialize_mixer()) {
        set_error("Failed to initialize audio mixer");
        return false;
    }

    // Initialize output if enabled
    if (config_.enable_output && !initialize_output()) {
        set_error("Failed to initialize audio output");
        return false;
    }

    // Start processing thread
    processing_thread_should_exit_.store(false);
    processing_thread_ = std::thread(&AudioPipeline::processing_thread_main, this);

    set_state(AudioPipelineState::Initialized);
    ve::log::info("Audio pipeline initialized successfully");
    return true;
}

void AudioPipeline::shutdown() {
    if (state_.load() == AudioPipelineState::Uninitialized) {
        return;
    }

    ve::log::info("Shutting down audio pipeline...");

    // Stop processing thread
    processing_thread_should_exit_.store(true);
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }

    // Stop output
    if (output_) {
        output_->stop();
    }

    // Clear components
    mixer_.reset();
    output_.reset();
    device_driven_rendering_enabled_.store(false);

    // Clear buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        std::fill(audio_buffer_.begin(), audio_buffer_.end(), nullptr);
        buffer_read_pos_.store(0);
        buffer_write_pos_.store(0);
    }

    set_state(AudioPipelineState::Uninitialized);
    ve::log::info("Audio pipeline shutdown complete");
}

bool AudioPipeline::process_audio_frame(std::shared_ptr<AudioFrame> frame) {
    // De-dup: if the same AudioFrame* arrives twice, drop the duplicate to avoid echo
    static const AudioFrame* s_last_enqueued = nullptr;
    if (frame.get() == s_last_enqueued) {
        return true; // already enqueued this exact frame
    }
    s_last_enqueued = frame.get();

    if (state_.load() == AudioPipelineState::Uninitialized) {
        set_error("Pipeline not initialized");
        return false;
    }

    if (!frame || frame->sample_count() == 0) {
        return true; // Skip empty frames
    }

    // Process through professional monitoring system if enabled
    if (monitoring_enabled_.load() && monitoring_system_) {
        monitoring_system_->process_audio_frame(*frame);
    }

    // Add frame to buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        size_t write_pos = buffer_write_pos_.load();
        audio_buffer_[write_pos] = frame;
        buffer_write_pos_.store((write_pos + 1) % BUFFER_SIZE);
    }

    // Update statistics  
    update_stats(frame);
    return true;
}

bool AudioPipeline::start_output() {
    if (!output_) {
        set_error("No audio output available");
        return false;
    }

    auto result = output_->start();
    if (result == AudioOutputError::Success) {
        set_state(AudioPipelineState::Playing);
        return true;
    }

    set_error("Failed to start audio output");
    return false;
}

bool AudioPipeline::stop_output() {
    if (output_) {
        output_->stop();
    }
    set_state(AudioPipelineState::Stopped);
    return true;
}

bool AudioPipeline::pause_output() {
    // AudioOutput doesn't have pause, so we stop it
    return stop_output();
}

bool AudioPipeline::resume_output() {
    // AudioOutput doesn't have resume, so we start it
    return start_output();
}

uint32_t AudioPipeline::add_audio_channel(const std::string& name,
                                         float initial_gain_db,
                                         float initial_pan) {
    if (!mixer_) {
        set_error("No mixer available");
        return 0;
    }

    return mixer_->add_channel(name, initial_gain_db, initial_pan);
}

bool AudioPipeline::remove_audio_channel(uint32_t channel_id) {
    if (!mixer_) {
        set_error("No mixer available");
        return false;
    }

    return mixer_->remove_channel(channel_id);
}

bool AudioPipeline::set_channel_gain(uint32_t channel_id, float gain_db) {
    if (!mixer_) {
        set_error("No mixer available");
        return false;
    }

    return mixer_->set_channel_gain(channel_id, gain_db) == MixerError::Success;
}

bool AudioPipeline::set_channel_pan(uint32_t channel_id, float pan) {
    if (!mixer_) {
        set_error("No mixer available");
        return false;
    }

    return mixer_->set_channel_pan(channel_id, pan) == MixerError::Success;
}

bool AudioPipeline::set_channel_mute(uint32_t channel_id, bool muted) {
    if (!mixer_) {
        set_error("No mixer available");
        return false;
    }

    return mixer_->set_channel_mute(channel_id, muted) == MixerError::Success;
}

bool AudioPipeline::set_channel_solo(uint32_t channel_id, bool solo) {
    if (!mixer_) {
        set_error("No mixer available");
        return false;
    }

    return mixer_->set_channel_solo(channel_id, solo) == MixerError::Success;
}

bool AudioPipeline::set_master_volume(float volume_db) {
    if (!mixer_) {
        set_error("No mixer available");
        return false;
    }

    return mixer_->set_master_volume(volume_db) == MixerError::Success;
}

bool AudioPipeline::set_master_mute(bool muted) {
    if (!mixer_) {
        set_error("No mixer available");
        return false;
    }

    return mixer_->set_master_mute(muted) == MixerError::Success;
}

float AudioPipeline::get_master_volume() const {
    if (!mixer_) {
        return 0.0f;
    }
    return mixer_->get_master_volume();
}

bool AudioPipeline::is_master_muted() const {
    if (!mixer_) {
        return false;
    }
    return mixer_->is_master_muted();
}

AudioPipelineStats AudioPipeline::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    AudioPipelineStats stats = stats_;

    // Add mixer stats if available
    if (mixer_) {
        auto mixer_stats = mixer_->get_stats();
        stats.active_channels = mixer_stats.active_channels;
        stats.master_volume_db = mixer_->get_master_volume();
        stats.master_muted = mixer_->is_master_muted();
    }

    return stats;
}

void AudioPipeline::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = AudioPipelineStats{};

    if (mixer_) {
        mixer_->reset_stats();
    }
}

bool AudioPipeline::set_config(const AudioPipelineConfig& config) {
    if (state_.load() != AudioPipelineState::Uninitialized) {
        set_error("Cannot change config while initialized");
        return false;
    }

    config_ = config;
    return true;
}

std::string AudioPipeline::get_last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void AudioPipeline::clear_error() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

std::shared_ptr<AudioFrame> AudioPipeline::get_mixed_audio(uint32_t frame_count) {
    if (state_.load() != AudioPipelineState::Playing) {
        return nullptr;
    }
    
    return mix_buffered_audio(frame_count);
}

// Private methods

void AudioPipeline::set_state(AudioPipelineState new_state) {
    state_.store(new_state);
}

void AudioPipeline::set_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    ve::log::error("Audio pipeline error: " + error);
}

bool AudioPipeline::initialize_mixer() {
    SimpleMixerConfig mixer_config;
    mixer_config.sample_rate = config_.sample_rate;
    mixer_config.channel_count = config_.channel_count;
    mixer_config.format = config_.format;
    mixer_config.max_channels = config_.max_channels;
    mixer_config.enable_clipping_protection = config_.enable_clipping_protection;
    mixer_config.master_volume_db = 0.0f;
    mixer_config.master_mute = false;

    mixer_ = SimpleMixer::create(mixer_config);
    if (!mixer_) {
        return false;
    }

    return mixer_->initialize() == MixerError::Success;
}

bool AudioPipeline::initialize_output() {
    AudioOutputConfig output_config;
    output_config.sample_rate = config_.sample_rate;
    output_config.channel_count = config_.channel_count;
    output_config.format = config_.format;
    output_config.exclusive_mode = false; // Use shared mode for better stability and quality
    output_config.buffer_duration_ms = 50; // Increased buffer for stable continuous audio
    output_config.min_periodicity_ms = 10;  // Balanced latency and stability

    output_ = AudioOutput::create(output_config);
    if (!output_) {
        return false;
    }

    device_driven_rendering_enabled_.store(false);

    // Set up audio render callback to provide real audio data
    output_->set_render_callback([this](void* buffer, uint32_t frame_count, SampleFormat format, uint16_t channels) -> size_t {
        return this->audio_render_callback(buffer, frame_count, format, channels);
    });

    // Try to initialize with exclusive mode first
    auto result = output_->initialize();
    if (result != AudioOutputError::Success) {
        // Reset and try shared mode with minimal buffers
        output_->shutdown();
        output_config.exclusive_mode = false;
        output_config.buffer_duration_ms = 5; // Minimal buffer for shared mode
        output_ = AudioOutput::create(output_config);
        if (!output_) {
            return false;
        }

        // Set up callback for the recreated output
        output_->set_render_callback([this](void* buffer, uint32_t frame_count, SampleFormat format, uint16_t channels) -> size_t {
            return this->audio_render_callback(buffer, frame_count, format, channels);
        });
        
        result = output_->initialize();
        if (result != AudioOutputError::Success) {
            device_driven_rendering_enabled_.store(false);
            return false;
        }
    }

    // Synchronize our configuration with the negotiated device format
    const auto& device_config = output_->get_config();
    if (config_.sample_rate != device_config.sample_rate || config_.channel_count != device_config.channel_count) {
        ve::log::info("AudioPipeline: Updating configuration to match negotiated device format: " +
                     std::to_string(config_.sample_rate) + "Hz/" + std::to_string(config_.channel_count) + "ch → " +
                     std::to_string(device_config.sample_rate) + "Hz/" + std::to_string(device_config.channel_count) + "ch");
        
        config_.sample_rate = device_config.sample_rate;
        config_.channel_count = device_config.channel_count;
        config_.format = device_config.format;
    }

    device_driven_rendering_enabled_.store(true);
    return true; // Already initialized above
}

void AudioPipeline::update_stats(const std::shared_ptr<AudioFrame>& frame) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_frames_processed++;
    stats_.total_samples_processed += frame->sample_count();
}

void AudioPipeline::process_audio_buffer() {
    if (device_driven_rendering_enabled_.load()) {
        return;
    }

    while (true) {
        auto mixed_frame = mix_buffered_audio(config_.buffer_size);
        if (!mixed_frame) {
            break;
        }

        if (!output_ || state_.load() != AudioPipelineState::Playing) {
            break;
        }

        output_->submit_frame(mixed_frame);
    }
}

std::shared_ptr<AudioFrame> AudioPipeline::mix_buffered_audio(uint32_t frame_count) {
    // Smart audio buffering: accumulate available frames and provide smooth playback
    std::shared_ptr<AudioFrame> latest_frame;
    
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        // Look for ANY available frame in the circular buffer, not just at read_pos
        size_t search_pos = buffer_read_pos_.load();
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            size_t check_pos = (search_pos + i) % BUFFER_SIZE;
            if (audio_buffer_[check_pos]) {
                latest_frame = audio_buffer_[check_pos];
                audio_buffer_[check_pos].reset();
                
                // Update read position to continue from the next position
                buffer_read_pos_.store((check_pos + 1) % BUFFER_SIZE);
                break;
            }
        }
    }

    // If no new frame available, return silence - but don't advance timestamps manually
    if (!latest_frame) {
        // Create a silent frame with a neutral timestamp to avoid timing drift
        // Use a simple timestamp that won't interfere with actual video audio timing
        TimePoint silent_timestamp(0, 1); // Simple zero timestamp
        auto silent_frame = AudioFrame::create(
            config_.sample_rate,
            config_.channel_count,
            frame_count,
            SampleFormat::Float32, // Use float32 for consistency with audio pipeline
            silent_timestamp
        );
        
        // AudioFrame::create() automatically zero-initializes data (silent)
        // Don't manually advance timestamps - let the actual audio frames control timing
        
        // Log silent frame generation occasionally (this is normal during video playback gaps)
        static auto last_silent_log = std::chrono::steady_clock::now();
        static int silent_count = 0;
        silent_count++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_silent_log).count() >= 5) {
            ve::log::info("AudioPipeline: Generated " + std::to_string(silent_count) + 
                         " silent frames in last 5 seconds - normal during video decoder gaps");
            last_silent_log = now;
            silent_count = 0;
        }
        
        return silent_frame;
    }
    
    // Debug: Log when we get real audio frames (not silent ones)
    if (latest_frame) {
        static auto last_real_audio_log = std::chrono::steady_clock::now();
        static int real_audio_count = 0;
        real_audio_count++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_real_audio_log).count() >= 5) {
            ve::log::info("AudioPipeline: Received " + std::to_string(real_audio_count) + 
                         " REAL audio frames in last 5 seconds - this should be much higher for smooth audio");
            last_real_audio_log = now;
            real_audio_count = 0;
        }
    }

    // Check if mixer is properly initialized and has channels
    if (!mixer_ || !mixer_->is_initialized()) {
        // Resize frame to match requested count for WASAPI compatibility
        if (latest_frame->sample_count() != frame_count) {
            return resize_audio_frame(latest_frame, frame_count);
        }
        return latest_frame; // Fallback to direct passthrough
    }

    // If no channels exist, resize frame to match requested count
    if (mixer_->get_channel_count() == 0) {
        if (latest_frame->sample_count() != frame_count) {
            return resize_audio_frame(latest_frame, frame_count);
        }
        return latest_frame;
    }

    // Use the mixer to process and mix audio channels
    // Create output frame with requested frame count and latest frame timestamp
    auto mixed_frame = mixer_->mix_channels(frame_count, latest_frame->timestamp());
    
    // Return mixed frame if successful, otherwise fallback to resized original
    if (mixed_frame) {
        return mixed_frame;
    } else {
        // Fallback: resize original frame to match requirements
        if (latest_frame->sample_count() != frame_count) {
            return resize_audio_frame(latest_frame, frame_count);
        }
        return latest_frame;
    }
}

void AudioPipeline::processing_thread_main() {
    ve::log::info("Audio pipeline processing thread started");

    const uint64_t frame_period_us = config_.sample_rate > 0
        ? (static_cast<uint64_t>(config_.buffer_size) * 1000000ull) / config_.sample_rate
        : 0;
    const auto sleep_duration = frame_period_us > 0
        ? std::chrono::microseconds(std::max<uint64_t>(frame_period_us / 2, 500))
        : std::chrono::milliseconds(1);

    while (!processing_thread_should_exit_.load()) {
        if (state_.load() == AudioPipelineState::Playing) {
            process_audio_buffer();
        }

        // Sleep to avoid busy waiting while keeping up with tiny buffers
        std::this_thread::sleep_for(sleep_duration);
    }

    ve::log::info("Audio pipeline processing thread ended");
}

// Professional monitoring system integration
bool AudioPipeline::enable_professional_monitoring(const ProfessionalAudioMonitoringSystem::MonitoringConfig& config) {
    try {
        if (!monitoring_system_) {
            monitoring_system_ = std::make_shared<ProfessionalAudioMonitoringSystem>(config);
        }
        
        if (!monitoring_system_->initialize(config_.sample_rate, config_.channel_count)) {
            set_error("Failed to initialize professional monitoring system");
            return false;
        }
        
        monitoring_enabled_.store(true);
        ve::log::info("Professional audio monitoring enabled");
        return true;
        
    } catch (const std::exception& e) {
        set_error("Exception enabling professional monitoring: " + std::string(e.what()));
        return false;
    }
}

void AudioPipeline::disable_professional_monitoring() {
    monitoring_enabled_.store(false);
    
    if (monitoring_system_) {
        monitoring_system_->shutdown();
        monitoring_system_.reset();
    }
    
    ve::log::info("Professional audio monitoring disabled");
}

size_t AudioPipeline::audio_render_callback(void* buffer, uint32_t frame_count, SampleFormat format, uint16_t channels) {
    const size_t bytes_per_sample = AudioFrame::bytes_per_sample(format);
    const size_t bytes_per_frame = bytes_per_sample * channels;

    auto fill_silence = [&](uint32_t frames_to_fill) -> size_t {
        const size_t total_bytes = static_cast<size_t>(frames_to_fill) * bytes_per_frame;
        if (total_bytes > 0) {
            std::memset(buffer, 0, total_bytes);
        }
        return frames_to_fill;
    };

    if (bytes_per_sample == 0) {
        return fill_silence(frame_count);
    }

    if (state_.load() != AudioPipelineState::Playing) {
        return fill_silence(frame_count);
    }

    auto audio_frame = get_mixed_audio(frame_count);
    if (!audio_frame) {
        static auto last_frame_warning = std::chrono::steady_clock::now();
        static uint32_t silence_count = 0;
        silence_count++;
        
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_frame_warning).count() >= 5) {
            ve::log::warn("AudioPipeline: No audio frames available for rendering (silence count: " + 
                         std::to_string(silence_count) + " in last 5 seconds) - this causes frame skipping");
            last_frame_warning = now;
            silence_count = 0;
        }
        return fill_silence(frame_count);
    }

    // Handle format conversion when device format differs from video format
    if (audio_frame->format() != format ||
        audio_frame->channel_count() != channels ||
        audio_frame->sample_count() != frame_count) {
        
        static auto last_format_warning = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_format_warning).count() >= 5) {
            ve::log::warn("AudioPipeline: Converting audio format - video(" +
                         std::to_string(audio_frame->channel_count()) + "ch/" + std::to_string(static_cast<int>(audio_frame->format())) + 
                         "fmt/" + std::to_string(audio_frame->sample_count()) + " frames) → device(" +
                         std::to_string(channels) + "ch/" + std::to_string(static_cast<int>(format)) + "fmt/" + 
                         std::to_string(frame_count) + " frames)");
            last_format_warning = now;
        }
        
        // Create a new frame with the target device format
        auto converted_frame = AudioFrame::create(
            config_.sample_rate,
            channels,
            frame_count,
            format,
            audio_frame->timestamp()
        );
        
        if (!converted_frame) {
            return fill_silence(frame_count);
        }
        
        // Perform channel conversion and format conversion
        convert_audio_format(audio_frame.get(), converted_frame.get());
        audio_frame = converted_frame;
    }

    const size_t target_bytes = static_cast<size_t>(frame_count) * bytes_per_frame;
    const size_t available_bytes = audio_frame->data_size();
    const size_t bytes_to_copy = std::min(target_bytes, available_bytes);

    if (bytes_to_copy > 0) {
        std::memcpy(buffer, audio_frame->data(), bytes_to_copy);
    }

    if (bytes_to_copy < target_bytes) {
        auto* dst = static_cast<uint8_t*>(buffer) + bytes_to_copy;
        std::memset(dst, 0, target_bytes - bytes_to_copy);
    }

    return frame_count;
}

std::shared_ptr<AudioFrame> AudioPipeline::resize_audio_frame(const std::shared_ptr<AudioFrame>& source_frame, uint32_t target_frame_count) {
    if (!source_frame || target_frame_count == 0) {
        return nullptr;
    }

    uint32_t source_frame_count = source_frame->sample_count();
    if (source_frame_count == target_frame_count) {
        return source_frame; // No resizing needed
    }

    // Create new frame with target size
    auto resized_frame = AudioFrame::create(
        source_frame->sample_rate(),
        source_frame->channel_count(),
        target_frame_count,
        source_frame->format(),
        source_frame->timestamp()
    );

    if (!resized_frame) {
        return source_frame; // Fallback to original if creation fails
    }

    // Get source and target data pointers
    const float* source_data = static_cast<const float*>(source_frame->data());
    float* target_data = static_cast<float*>(resized_frame->data());
    
    uint16_t channels = source_frame->channel_count();
    
    if (target_frame_count <= source_frame_count) {
        // Truncate: copy first N frames
        uint32_t samples_to_copy = target_frame_count * channels;
        std::memcpy(target_data, source_data, samples_to_copy * sizeof(float));
    } else {
        // Extend: copy all source frames, then pad with silence
        uint32_t source_samples = source_frame_count * channels;
        uint32_t target_samples = target_frame_count * channels;
        
        // Copy source audio data
        std::memcpy(target_data, source_data, source_samples * sizeof(float));
        
        // Fill remaining with silence
        std::memset(target_data + source_samples, 0, (target_samples - source_samples) * sizeof(float));
    }

    return resized_frame;
}

void AudioPipeline::convert_audio_format(const AudioFrame* source, AudioFrame* target) {
    if (!source || !target) {
        return;
    }

    const float* source_data = static_cast<const float*>(source->data());
    float* target_data = static_cast<float*>(target->data());
    
    uint16_t source_channels = source->channel_count();
    uint16_t target_channels = target->channel_count();
    uint32_t frames_to_process = std::min(source->sample_count(), target->sample_count());
    
    // Handle channel conversion
    for (uint32_t frame = 0; frame < frames_to_process; ++frame) {
        if (source_channels == target_channels) {
            // Same channel count - direct copy
            for (uint16_t ch = 0; ch < target_channels; ++ch) {
                target_data[frame * target_channels + ch] = source_data[frame * source_channels + ch];
            }
        } else if (source_channels == 6 && target_channels == 2) {
            // 5.1 surround to stereo - mix down using standard coefficients
            // 5.1 layout: L, R, C, LFE, LS, RS
            float left = source_data[frame * 6 + 0] +   // L
                        0.707f * source_data[frame * 6 + 2] +  // C * 0.707
                        0.5f * source_data[frame * 6 + 4];     // LS * 0.5
            
            float right = source_data[frame * 6 + 1] +  // R
                         0.707f * source_data[frame * 6 + 2] +  // C * 0.707
                         0.5f * source_data[frame * 6 + 5];     // RS * 0.5
            
            target_data[frame * 2 + 0] = std::clamp(left, -1.0f, 1.0f);
            target_data[frame * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
        } else if (source_channels == 1 && target_channels == 2) {
            // Mono to stereo - duplicate channel
            float mono_sample = source_data[frame];
            target_data[frame * 2 + 0] = mono_sample;
            target_data[frame * 2 + 1] = mono_sample;
        } else if (source_channels == 2 && target_channels == 1) {
            // Stereo to mono - average channels
            float mono_sample = (source_data[frame * 2 + 0] + source_data[frame * 2 + 1]) * 0.5f;
            target_data[frame] = mono_sample;
        } else {
            // Generic fallback - take first N channels or pad with zeros
            for (uint16_t ch = 0; ch < target_channels; ++ch) {
                if (ch < source_channels) {
                    target_data[frame * target_channels + ch] = source_data[frame * source_channels + ch];
                } else {
                    target_data[frame * target_channels + ch] = 0.0f;
                }
            }
        }
    }
    
    // Fill any remaining frames with silence
    uint32_t remaining_frames = target->sample_count() - frames_to_process;
    if (remaining_frames > 0) {
        uint32_t silence_samples = remaining_frames * target_channels;
        std::memset(target_data + (frames_to_process * target_channels), 0, silence_samples * sizeof(float));
    }
}

} // namespace ve::audio