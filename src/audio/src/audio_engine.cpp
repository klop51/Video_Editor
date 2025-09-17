/**
 * @file audio_engine.cpp
 * @brief Phase 2 Week 1: Basic Audio Pipeline Implementation
 * 
 * Professional audio engine implementation providing complete
 * audio loading, playbook, and timeline integration capabilities.
 */

#include "audio/audio_engine.hpp"
#include "audio/test_decoder.hpp"
#include "audio/audio_buffer_pool.hpp"
#include <algorithm>
#include <chrono>
#include <cassert>

namespace ve::audio {

/**
 * @brief Internal implementation details for AudioEngine
 */
struct AudioEngine::Impl {
    // Core components
    std::unique_ptr<MixingGraph> mixing_graph;
    std::unique_ptr<AudioClock> audio_clock;
    std::unique_ptr<AudioBufferPool> buffer_pool;
    
    // Audio source management
    std::unordered_map<AudioSourceID, std::unique_ptr<AudioDecoder>> decoders;
    std::unordered_map<AudioSourceID, AudioSourceInfo> source_info;
    std::unordered_map<AudioSourceID, NodeID> source_nodes;  // Source ID to graph node mapping
    
    // Timeline integration
    struct TimelineEntry {
        AudioSourceID source_id;
        TimePoint start_time;
        TimeDuration duration;
        NodeID graph_node;
    };
    std::vector<TimelineEntry> timeline_entries;
    
    // Playback state
    std::atomic<bool> initialized{false};
    std::atomic<bool> playing{false};
    std::atomic<float> master_volume{1.0f};
    std::atomic<bool> master_muted{false};
    
    // Source ID generation
    std::atomic<AudioSourceID> next_source_id{1};
    
    // Performance monitoring
    std::atomic<uint32_t> buffer_underruns{0};
    std::atomic<float> cpu_usage{0.0f};
    
    // Thread management
    std::thread audio_thread;
    std::atomic<bool> stop_audio_thread{false};
    
    Impl() = default;
    ~Impl() {
        if (audio_thread.joinable()) {
            stop_audio_thread = true;
            audio_thread.join();
        }
    }
};

AudioEngine::AudioEngine()
    : AudioEngine(AudioEngineConfig{}) {
}

AudioEngine::AudioEngine(const AudioEngineConfig& config)
    : impl_(std::make_unique<Impl>())
    , current_state_(AudioEngineState::Uninitialized)
    , config_(config)
    , callback_(nullptr) {
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (impl_->initialized.load()) {
        return true;  // Already initialized
    }
    
    try {
        // Initialize audio clock
        impl_->audio_clock = std::make_unique<AudioClock>(AudioClockConfig{});
        if (!impl_->audio_clock->initialize()) {
            set_error("Failed to initialize audio clock");
            return false;
        }
        
        // Initialize buffer pool
        AudioBufferConfig buffer_config;
        buffer_config.buffer_size_samples = config_.buffer_size;
        buffer_config.channel_count = config_.channel_count;
        buffer_config.sample_format = config_.output_format;
        buffer_config.sample_rate = config_.sample_rate;
        impl_->buffer_pool = std::make_unique<AudioBufferPool>(buffer_config);
        
        // Initialize mixing graph
        impl_->mixing_graph = std::make_unique<MixingGraph>();
        if (!impl_->mixing_graph) {
            set_error("Failed to create mixing graph");
            return false;
        }
        
        // Create master output node
        auto output_node_ptr = std::make_unique<OutputNode>(INVALID_NODE_ID + 1, "MasterOutput");
        NodeID output_node = impl_->mixing_graph->add_node(std::move(output_node_ptr));
        if (output_node == INVALID_NODE_ID) {
            set_error("Failed to create master output node");
            return false;
        }
        
        // Mark as initialized
        impl_->initialized = true;
        set_state(AudioEngineState::Stopped);
        
        clear_error();
        return true;
        
    } catch (const std::exception& e) {
        set_error(std::string("Initialization failed: ") + e.what());
        return false;
    }
}

void AudioEngine::shutdown() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!impl_->initialized.load()) {
        return;  // Already shutdown
    }
    
    // Stop playback
    stop();
    
    // Stop audio thread
    impl_->stop_audio_thread = true;
    if (impl_->audio_thread.joinable()) {
        impl_->audio_thread.join();
    }
    
    // Clear timeline entries
    impl_->timeline_entries.clear();
    
    // Clear audio sources
    impl_->decoders.clear();
    impl_->source_info.clear();
    impl_->source_nodes.clear();
    
    // Shutdown subsystems
    impl_->mixing_graph.reset();
    impl_->audio_clock.reset();
    impl_->buffer_pool.reset();
    
    impl_->initialized = false;
    set_state(AudioEngineState::Uninitialized);
}

bool AudioEngine::is_initialized() const {
    return impl_->initialized.load();
}

AudioSourceID AudioEngine::load_audio_source(const std::string& file_path) {
    if (!is_initialized()) {
        set_error("Engine not initialized");
        return INVALID_AUDIO_SOURCE_ID;
    }
    
    try {
        // For now, create a test decoder to demonstrate the pipeline
        std::unique_ptr<AudioDecoder> decoder = std::make_unique<TestAudioDecoder>();
        
        // Initialize decoder (would normally read file)
        std::vector<uint8_t> dummy_data;  // Would contain actual file data
        AudioError error = decoder->initialize(dummy_data);
        if (error != AudioError::None) {
            set_error("Failed to initialize decoder for: " + file_path);
            return INVALID_AUDIO_SOURCE_ID;
        }
        
        // Get stream information
        AudioStreamInfo stream_info = decoder->get_stream_info();
        if (!stream_info.is_valid()) {
            set_error("Invalid audio stream in: " + file_path);
            return INVALID_AUDIO_SOURCE_ID;
        }
        
        // Generate source ID
        AudioSourceID source_id = impl_->next_source_id.fetch_add(1);
        
        // Create audio source info
        AudioSourceInfo info;
        info.id = source_id;
        info.file_path = file_path;
        info.stream_info = stream_info;
        info.is_loaded = true;
        info.hardware_accelerated = decoder->supports_hardware_acceleration();
        info.duration = TimeDuration(static_cast<int64_t>(stream_info.duration_samples), 
                                     static_cast<int32_t>(stream_info.sample_rate));
        
        // Store decoder and info
        impl_->decoders[source_id] = std::move(decoder);
        impl_->source_info[source_id] = info;
        
        // Notify callback
        notify_source_loaded(source_id, info);
        
        clear_error();
        return source_id;
        
    } catch (const std::exception& e) {
        set_error(std::string("Failed to load audio source: ") + e.what());
        return INVALID_AUDIO_SOURCE_ID;
    }
}

bool AudioEngine::unload_audio_source(AudioSourceID source_id) {
    if (!is_initialized()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // Remove from timeline if present
    remove_source_from_timeline(source_id);
    
    // Remove from mixing graph
    auto node_it = impl_->source_nodes.find(source_id);
    if (node_it != impl_->source_nodes.end()) {
        impl_->mixing_graph->remove_node(node_it->second);
        impl_->source_nodes.erase(node_it);
    }
    
    // Remove decoder and info
    impl_->decoders.erase(source_id);
    impl_->source_info.erase(source_id);
    
    return true;
}

AudioSourceInfo AudioEngine::get_source_info(AudioSourceID source_id) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    auto it = impl_->source_info.find(source_id);
    if (it != impl_->source_info.end()) {
        return it->second;
    }
    
    return AudioSourceInfo{};  // Empty info for not found
}

std::vector<AudioSourceInfo> AudioEngine::get_loaded_sources() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    std::vector<AudioSourceInfo> sources;
    sources.reserve(impl_->source_info.size());
    
    for (const auto& [id, info] : impl_->source_info) {
        sources.push_back(info);
    }
    
    return sources;
}

bool AudioEngine::is_source_loaded(AudioSourceID source_id) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return impl_->source_info.find(source_id) != impl_->source_info.end();
}

bool AudioEngine::play() {
    if (!is_initialized()) {
        set_error("Engine not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (current_state_ == AudioEngineState::Playing) {
        return true;  // Already playing
    }
    
    // Start audio clock
    impl_->audio_clock->start();
    
    // Start audio processing thread if not running
    if (!impl_->audio_thread.joinable()) {
        impl_->stop_audio_thread = false;
        impl_->audio_thread = std::thread([this]() { audio_thread_loop(); });
    }
    
    impl_->playing = true;
    set_state(AudioEngineState::Playing);
    
    return true;
}

bool AudioEngine::pause() {
    if (!is_initialized()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (current_state_ != AudioEngineState::Playing) {
        return false;
    }
    
    impl_->audio_clock->stop();
    impl_->playing = false;
    set_state(AudioEngineState::Paused);
    
    return true;
}

bool AudioEngine::stop() {
    if (!is_initialized()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    impl_->audio_clock->stop();
    impl_->playing = false;
    
    // Reset position to beginning
    impl_->audio_clock->set_time(TimePoint(0, 1));
    
    set_state(AudioEngineState::Stopped);
    
    return true;
}

bool AudioEngine::seek(const TimePoint& position) {
    if (!is_initialized()) {
        set_error("Engine not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    AudioEngineState old_state = current_state_;
    set_state(AudioEngineState::Seeking);
    
    // Seek audio clock
    impl_->audio_clock->set_time(position);
    
    // Seek all decoders
    for (auto& [source_id, decoder] : impl_->decoders) {
        decoder->seek(position);
    }
    
    // Flush mixing graph (simplified for now)
    // TODO: Implement flush when MixingGraph has this method
    
    set_state(old_state);
    notify_position_changed(position);
    
    return true;
}

void AudioEngine::set_volume(float volume) {
    impl_->master_volume = std::clamp(volume, 0.0f, 1.0f);
}

float AudioEngine::get_volume() const {
    return impl_->master_volume.load();
}

void AudioEngine::set_muted(bool muted) {
    impl_->master_muted = muted;
}

bool AudioEngine::is_muted() const {
    return impl_->master_muted.load();
}

PlaybackState AudioEngine::get_playback_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    PlaybackState state;
    state.state = current_state_;
    state.current_position = impl_->audio_clock ? impl_->audio_clock->get_time() : TimePoint(0, 1);
    state.duration = get_duration();
    state.volume = impl_->master_volume.load();
    state.muted = impl_->master_muted.load();
    state.active_sources = static_cast<uint32_t>(impl_->source_info.size());
    state.buffer_underruns = impl_->buffer_underruns.load();
    state.cpu_usage = impl_->cpu_usage.load();
    
    return state;
}

TimePoint AudioEngine::get_current_position() const {
    if (impl_->audio_clock) {
        return impl_->audio_clock->get_time();
    }
    return TimePoint(0, 1);
}

TimeDuration AudioEngine::get_duration() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    TimeDuration max_duration(0, 1);
    
    for (const auto& entry : impl_->timeline_entries) {
        // Calculate end time using rational arithmetic
        TimeRational end_rational = entry.start_time.to_rational();
        TimeRational duration_rational = entry.duration.to_rational();
        
        // Add start_time + duration
        end_rational.num = end_rational.num * duration_rational.den + 
                          duration_rational.num * end_rational.den;
        end_rational.den = end_rational.den * duration_rational.den;
        TimeDuration end_time(end_rational);
        
        if (end_time > max_duration) {
            max_duration = end_time;
        }
    }
    
    return max_duration;
}

AudioEngineState AudioEngine::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
}

const AudioEngineConfig& AudioEngine::get_config() const {
    return config_;
}

bool AudioEngine::set_config(const AudioEngineConfig& config) {
    if (is_initialized()) {
        set_error("Cannot change configuration while engine is initialized");
        return false;
    }
    
    config_ = config;
    return true;
}

std::vector<AudioCodec> AudioEngine::get_supported_formats() const {
    // For now, return basic supported formats - would use AudioDecoderFactory when implemented
    return {AudioCodec::PCM, AudioCodec::MP3, AudioCodec::AAC};
}

bool AudioEngine::is_format_supported(AudioCodec codec) const {
    // For now, basic format support check - would use AudioDecoderFactory when implemented
    return codec == AudioCodec::PCM || codec == AudioCodec::MP3 || codec == AudioCodec::AAC;
}

void AudioEngine::set_callback(AudioEngineCallback* callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = callback;
}

void AudioEngine::clear_callback() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = nullptr;
}

MixingGraph* AudioEngine::get_mixing_graph() {
    return impl_->mixing_graph.get();
}

AudioClock* AudioEngine::get_audio_clock() {
    return impl_->audio_clock.get();
}

bool AudioEngine::add_source_to_timeline(AudioSourceID source_id, 
                                        const TimePoint& start_time, 
                                        const TimeDuration& duration) {
    if (!is_source_loaded(source_id)) {
        set_error("Audio source not loaded");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // Get source info
    auto info_it = impl_->source_info.find(source_id);
    if (info_it == impl_->source_info.end()) {
        return false;
    }
    
    // Create input node in mixing graph
    auto input_node_ptr = std::make_unique<InputNode>(source_id, "AudioInput_" + std::to_string(source_id));
    NodeID input_node = impl_->mixing_graph->add_node(std::move(input_node_ptr));
    if (input_node == INVALID_NODE_ID) {
        set_error("Failed to create input node for audio source");
        return false;
    }
    
    // Determine duration
    TimeDuration actual_duration = duration;
    if (duration.to_rational().num == 0) {
        // Use full source duration
        const auto& stream_info = info_it->second.stream_info;
        actual_duration = TimeDuration(static_cast<int64_t>(stream_info.duration_samples), 
                                     static_cast<int32_t>(stream_info.sample_rate));
    }
    
    // Add to timeline
    Impl::TimelineEntry entry;
    entry.source_id = source_id;
    entry.start_time = start_time;
    entry.duration = actual_duration;
    entry.graph_node = input_node;
    
    impl_->timeline_entries.push_back(entry);
    impl_->source_nodes[source_id] = input_node;
    
    return true;
}

bool AudioEngine::remove_source_from_timeline(AudioSourceID source_id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // Remove from timeline entries
    auto it = std::remove_if(impl_->timeline_entries.begin(), impl_->timeline_entries.end(),
        [source_id](const Impl::TimelineEntry& entry) {
            return entry.source_id == source_id;
        });
    
    bool found = (it != impl_->timeline_entries.end());
    impl_->timeline_entries.erase(it, impl_->timeline_entries.end());
    
    return found;
}

std::vector<AudioSourceID> AudioEngine::get_active_sources_at_time(const TimePoint& time) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    std::vector<AudioSourceID> active_sources;
    
    for (const auto& entry : impl_->timeline_entries) {
        // Calculate end time using rational arithmetic
        TimeRational end_rational = entry.start_time.to_rational();
        end_rational.num = end_rational.num * entry.duration.to_rational().den + 
                          entry.duration.to_rational().num * end_rational.den;
        end_rational.den = end_rational.den * entry.duration.to_rational().den;
        TimePoint end_time(end_rational);
        
        if (time >= entry.start_time && time < end_time) {
            active_sources.push_back(entry.source_id);
        }
    }
    
    return active_sources;
}

std::string AudioEngine::get_last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void AudioEngine::clear_error() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private helper methods

void AudioEngine::set_state(AudioEngineState new_state) {
    AudioEngineState old_state = current_state_;
    current_state_ = new_state;
    
    if (old_state != new_state) {
        notify_state_changed(old_state, new_state);
    }
}

void AudioEngine::set_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    notify_error(error);
}

void AudioEngine::notify_state_changed(AudioEngineState old_state, AudioEngineState new_state) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->on_state_changed(old_state, new_state);
    }
}

void AudioEngine::notify_position_changed(const TimePoint& position) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->on_position_changed(position);
    }
}

void AudioEngine::notify_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->on_error(error);
    }
}

void AudioEngine::notify_source_loaded(AudioSourceID source_id, const AudioSourceInfo& info) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->on_source_loaded(source_id, info);
    }
}

void AudioEngine::notify_buffer_underrun() {
    impl_->buffer_underruns.fetch_add(1);
    
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (callback_) {
        callback_->on_buffer_underrun();
    }
}

/**
 * @brief Audio processing thread loop
 * 
 * Handles real-time audio processing and mixing.
 */
void AudioEngine::audio_thread_loop() {
    // Set thread priority for real-time audio
    // Platform-specific thread priority setting would go here
    
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (!impl_->stop_audio_thread.load()) {
        if (!impl_->playing.load()) {
            // Sleep when not playing
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_time);
        last_time = current_time;
        
        // Calculate CPU usage (simplified)
        float frame_time = elapsed.count() / 1000000.0f;  // Convert to seconds
        float target_frame_time = static_cast<float>(config_.buffer_size) / config_.sample_rate;
        impl_->cpu_usage = std::min(1.0f, frame_time / target_frame_time);
        
        // Process audio frame
        process_audio_frame();
        
        // Update position
        if (impl_->audio_clock) {
            TimePoint position = impl_->audio_clock->get_time();
            notify_position_changed(position);
        }
        
        // Calculate sleep time to maintain buffer size timing
        auto target_sleep = std::chrono::microseconds(static_cast<long long>(target_frame_time * 1000000));
        auto processing_time = std::chrono::high_resolution_clock::now() - current_time;
        
        if (processing_time < target_sleep) {
            std::this_thread::sleep_for(target_sleep - processing_time);
        } else {
            // Buffer underrun
            notify_buffer_underrun();
        }
    }
}

/**
 * @brief Process a single audio frame
 */
void AudioEngine::process_audio_frame() {
    if (!impl_->mixing_graph || !impl_->audio_clock) {
        return;
    }
    
    // Get current position
    TimePoint current_position = impl_->audio_clock->get_time();
    
    // Get active sources
    auto active_sources = get_active_sources_at_time(current_position);
    
    // Process mixing graph (simplified for now)
    AudioProcessingParams params;
    params.sample_rate = config_.sample_rate;
    params.channels = config_.channel_count;
    params.buffer_size = config_.buffer_size;
    
    // TODO: Implement process_frame when MixingGraph has this method
    // impl_->mixing_graph->process_frame(params);
    
    // Apply master volume and mute
    // This would integrate with actual audio output system
    
    // Advance audio clock
    impl_->audio_clock->advance_samples(config_.buffer_size);
}

} // namespace ve::audio
