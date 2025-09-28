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

    // Pre-size device FIFO to ~2 seconds of audio in device format
    fifo_init_seconds(2.0);

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

    // Convert to device format here (rate/channels/format) and push to device FIFO
    std::vector<float> devSamples;
    devSamples.reserve(frame->sample_count() * config_.channel_count); // avoid re-alloc on convert
    if (!convert_to_device_format(*frame, devSamples, config_)) {
        // on conversion failure, skip quietly
        return true;
    }
    {
        std::lock_guard<std::mutex> lk(fifo_mtx_);
        fifo_write(devSamples.data(), devSamples.size());
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

std::shared_ptr<AudioFrame> AudioPipeline::mix_buffered_audio(uint32_t /*frame_count*/) {
    // Strict FIFO: pop the next enqueued frame, or nullptr if empty.
    std::shared_ptr<AudioFrame> next;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        size_t r = buffer_read_pos_.load();
        if (audio_buffer_[r]) {
            next = audio_buffer_[r];
            audio_buffer_[r].reset();
            buffer_read_pos_.store((r + 1) % BUFFER_SIZE);
        }
    }
    return next; // no synthetic frames here
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
    // Contract: device output is float32, channel_count == config_.channel_count
    // Enforce at runtime to catch surprises
    if (format != SampleFormat::Float32 || channels != config_.channel_count) {
        // Fallback: zero (should not happen after initialize_output sync)
        std::memset(buffer, 0, frame_count * channels * sizeof(float));
        return frame_count;
    }
    
    const size_t need = static_cast<size_t>(frame_count) * channels;
    size_t got = 0;
    {
        std::lock_guard<std::mutex> lk(fifo_mtx_);
        got = fifo_read(static_cast<float*>(buffer), need);
    }
    
    if (got < need) {
        // zero-fill remainder
        auto* out = static_cast<float*>(buffer);
        std::memset(out + got, 0, (need - got) * sizeof(float));
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

// Helper: robust conversion to device format
bool AudioPipeline::convert_to_device_format(const AudioFrame& in, std::vector<float>& out, const AudioPipelineConfig& cfg) {
    // Simple conversion - we'll improve this with proper resampling later if needed
    uint32_t in_rate = in.sample_rate();
    uint16_t in_channels = in.channel_count();
    uint32_t in_samples = in.sample_count();
    
    // For now, handle same-rate cases and basic channel conversion
    if (in_rate != cfg.sample_rate) {
        // TODO: Add proper resampling with libswresample
        // For now, skip frames that need resampling to avoid artifacts
        return false;
    }
    
    // Convert to float32 and handle channel conversion
    const size_t out_samples = in_samples * cfg.channel_count;
    out.resize(out_samples);
    
    if (in.format() == SampleFormat::Float32) {
        const float* in_data = reinterpret_cast<const float*>(in.data());
        
        if (in_channels == cfg.channel_count) {
            // Same channel count - direct copy
            std::memcpy(out.data(), in_data, in_samples * in_channels * sizeof(float));
        } else if (in_channels == 6 && cfg.channel_count == 2) {
            // 6ch -> 2ch downmix (5.1 -> stereo)
            for (uint32_t i = 0; i < in_samples; ++i) {
                const float* src = &in_data[i * 6];
                float* dst = &out[i * 2];
                
                // Simple 5.1 to stereo downmix
                // L, R, C, LFE, Ls, Rs -> L, R
                dst[0] = src[0] + 0.707f * src[2] + 0.5f * src[4]; // L + 0.707*C + 0.5*Ls
                dst[1] = src[1] + 0.707f * src[2] + 0.5f * src[5]; // R + 0.707*C + 0.5*Rs
            }
        } else if (in_channels == 1 && cfg.channel_count == 2) {
            // Mono -> Stereo
            for (uint32_t i = 0; i < in_samples; ++i) {
                out[i * 2] = in_data[i];     // L
                out[i * 2 + 1] = in_data[i]; // R
            }
        } else {
            // Unsupported channel conversion for now
            return false;
        }
    } else {
        // TODO: Add format conversion for other sample formats
        return false;
    }
    
    return true;
}

// ===== FIFO helpers (non-RT allocations happen at init only) =====
void AudioPipeline::fifo_init_seconds(double seconds) {
    const size_t cap = static_cast<size_t>(seconds * config_.sample_rate * config_.channel_count);
    device_fifo_.assign(cap, 0.0f);
    fifo_capacity_ = device_fifo_.size();
    fifo_head_ = fifo_tail_ = fifo_size_ = 0;
}

size_t AudioPipeline::fifo_write(const float* samples, size_t count) {
    size_t written = 0;
    while (written < count) {
        if (fifo_size_ == fifo_capacity_) break; // full: drop or overwrite? (keep drop for now)
        device_fifo_[fifo_head_] = samples[written++];
        fifo_head_ = (fifo_head_ + 1) % fifo_capacity_;
        ++fifo_size_;
    }
    return written;
}

size_t AudioPipeline::fifo_read(float* dst, size_t count) {
    size_t read = 0;
    while (read < count && fifo_size_ > 0) {
        dst[read++] = device_fifo_[fifo_tail_];
        fifo_tail_ = (fifo_tail_ + 1) % fifo_capacity_;
        --fifo_size_;
    }
    return read;
}

} // namespace ve::audio