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
    output_config.exclusive_mode = true; // Try exclusive mode first to prevent echo
    output_config.buffer_duration_ms = 10; // Further reduced for minimal latency
    output_config.min_periodicity_ms = 2;  // Very responsive

    output_ = AudioOutput::create(output_config);
    if (!output_) {
        return false;
    }

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
        
        result = output_->initialize();
        if (result != AudioOutputError::Success) {
            return false;
        }
    }

    return true; // Already initialized above
}

void AudioPipeline::update_stats(const std::shared_ptr<AudioFrame>& frame) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_frames_processed++;
    stats_.total_samples_processed += frame->sample_count();
}

void AudioPipeline::process_audio_buffer() {
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
    // Get the most recent frame from buffer
    std::shared_ptr<AudioFrame> latest_frame;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        size_t read_pos = buffer_read_pos_.load();
        if (audio_buffer_[read_pos]) {
            latest_frame = audio_buffer_[read_pos];
            buffer_read_pos_.store((read_pos + 1) % BUFFER_SIZE);
        }
    }

    if (!latest_frame) {
        return nullptr;
    }

    // Check if mixer is properly initialized and has channels
    if (!mixer_ || !mixer_->is_initialized()) {
        return latest_frame; // Fallback to direct passthrough
    }

    // If no channels exist, return the frame directly (no mixing needed)
    if (mixer_->get_channel_count() == 0) {
        return latest_frame;
    }

    // Use the mixer to process and mix audio channels
    // Create output frame with requested frame count and latest frame timestamp
    auto mixed_frame = mixer_->mix_channels(frame_count, latest_frame->timestamp());
    
    // Return mixed frame if successful, otherwise fallback to original
    return mixed_frame ? mixed_frame : latest_frame;
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

} // namespace ve::audio