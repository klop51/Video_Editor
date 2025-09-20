/**
 * @file timeline_audio_manager.cpp
 * @brief Timeline Audio Integration Manager Implementation
 */

#include "audio/timeline_audio_manager.hpp"
#include "audio/audio_frame.hpp"
#include "core/log.hpp"
#include "decode/decoder.hpp"
#include <algorithm>
#include <chrono>

namespace ve::audio {

std::unique_ptr<TimelineAudioManager> TimelineAudioManager::create(
    ve::audio::AudioPipeline* pipeline) {
    
    if (!pipeline) {
        ve::log::error("Cannot create TimelineAudioManager: null audio pipeline");
        return nullptr;
    }
    
    return std::unique_ptr<TimelineAudioManager>(new TimelineAudioManager(pipeline));
}

TimelineAudioManager::TimelineAudioManager(ve::audio::AudioPipeline* pipeline)
    : audio_pipeline_(pipeline) {
    current_position_.store(ve::TimePoint{0, 1});
}

TimelineAudioManager::~TimelineAudioManager() {
    shutdown();
}

bool TimelineAudioManager::initialize() {
    if (initialized_.load()) {
        return true;  // Already initialized
    }
    
    if (!audio_pipeline_) {
        set_error("Audio pipeline not available");
        return false;
    }
    
    // Check if audio pipeline is in a valid state (not uninitialized or error)
    auto pipeline_state = audio_pipeline_->get_state();
    if (pipeline_state == ve::audio::AudioPipelineState::Uninitialized ||
        pipeline_state == ve::audio::AudioPipelineState::Error) {
        set_error("Audio pipeline not available or not initialized");
        return false;
    }
    
    ve::log::info("Initializing timeline audio manager");
    
    // Clear any existing channels
    {
        std::lock_guard<std::mutex> lock(channels_mutex_);
        channels_.clear();
    }
    
    // Reset statistics
    reset_stats();
    
    initialized_.store(true);
    ve::log::info("Timeline audio manager initialized successfully");
    return true;
}

void TimelineAudioManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    ve::log::info("Shutting down timeline audio manager");
    
    // Stop playback
    stop_playback();
    
    // Clear all channels
    {
        std::lock_guard<std::mutex> lock(channels_mutex_);
        for (auto& [track_id, channel] : channels_) {
            if (channel && channel->pipeline_channel_id != 0) {
                audio_pipeline_->remove_audio_channel(channel->pipeline_channel_id);
            }
        }
        channels_.clear();
    }
    
    timeline_ = nullptr;
    initialized_.store(false);
    ve::log::info("Timeline audio manager shutdown complete");
}

bool TimelineAudioManager::set_timeline(ve::timeline::Timeline* timeline) {
    if (!initialized_.load()) {
        set_error("Timeline audio manager not initialized");
        return false;
    }
    
    timeline_ = timeline;
    timeline_version_ = 0;  // Reset version to force sync
    
    // Synchronize with new timeline
    return sync_tracks();
}

bool TimelineAudioManager::sync_tracks() {
    if (!timeline_) {
        ve::log::warn("No timeline set for audio manager");
        return true;  // Not an error, just no timeline to sync
    }
    
    ve::log::info("Synchronizing audio channels with timeline tracks");
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    
    // Get current audio tracks from timeline
    auto audio_tracks = timeline_->get_tracks_by_type(ve::timeline::Track::Audio);
    
    // Remove channels for tracks that no longer exist
    std::vector<ve::timeline::TrackId> tracks_to_remove;
    for (const auto& [track_id, channel] : channels_) {
        bool track_exists = std::any_of(audio_tracks.begin(), audio_tracks.end(),
            [track_id](const ve::timeline::Track* track) {
                return track && track->id() == track_id;
            });
        
        if (!track_exists) {
            tracks_to_remove.push_back(track_id);
        }
    }
    
    for (auto track_id : tracks_to_remove) {
        remove_channel_for_track(track_id);
    }
    
    // Add channels for new tracks
    for (const ve::timeline::Track* track : audio_tracks) {
        if (!track) continue;
        
        if (channels_.find(track->id()) == channels_.end()) {
            create_channel_for_track(*track);
        }
    }
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_tracks = static_cast<uint32_t>(audio_tracks.size());
        stats_.active_tracks = static_cast<uint32_t>(channels_.size());
    }
    
    ve::log::info("Synchronized " + std::to_string(channels_.size()) + " audio channels");
    return true;
}

bool TimelineAudioManager::start_playback() {
    if (!initialized_.load()) {
        set_error("Timeline audio manager not initialized");
        return false;
    }
    
    is_playing_.store(true);
    ve::log::info("Timeline audio playback started");
    return true;
}

bool TimelineAudioManager::stop_playback() {
    is_playing_.store(false);
    
    // Reset all channel positions
    std::lock_guard<std::mutex> lock(channels_mutex_);
    for (auto& [track_id, channel] : channels_) {
        if (channel) {
            channel->current_position = ve::TimePoint{0, 1};
            channel->is_active = false;
        }
    }
    
    ve::log::info("Timeline audio playback stopped");
    return true;
}

bool TimelineAudioManager::pause_playback() {
    is_playing_.store(false);
    ve::log::info("Timeline audio playback paused");
    return true;
}

bool TimelineAudioManager::seek_to(ve::TimePoint position) {
    current_position_.store(position);
    
    // Update all channel positions
    std::lock_guard<std::mutex> lock(channels_mutex_);
    for (auto& [track_id, channel] : channels_) {
        if (channel) {
            channel->current_position = position;
        }
    }
    
    ve::log::debug("Timeline audio seeked to: " + std::to_string(static_cast<double>(position.to_rational().num) / position.to_rational().den) + "s");
    return true;
}

bool TimelineAudioManager::set_track_mute(ve::timeline::TrackId track_id, bool muted) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto* channel = get_channel(track_id);
    if (!channel) {
        set_error("Track not found: " + std::to_string(track_id));
        return false;
    }
    
    channel->is_muted = muted;
    if (channel->pipeline_channel_id != 0) {
        return audio_pipeline_->set_channel_mute(channel->pipeline_channel_id, muted);
    }
    
    return true;
}

bool TimelineAudioManager::set_track_solo(ve::timeline::TrackId track_id, bool solo) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto* channel = get_channel(track_id);
    if (!channel) {
        set_error("Track not found: " + std::to_string(track_id));
        return false;
    }
    
    channel->is_solo = solo;
    if (channel->pipeline_channel_id != 0) {
        return audio_pipeline_->set_channel_solo(channel->pipeline_channel_id, solo);
    }
    
    return true;
}

bool TimelineAudioManager::set_track_gain(ve::timeline::TrackId track_id, float gain_db) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto* channel = get_channel(track_id);
    if (!channel) {
        set_error("Track not found: " + std::to_string(track_id));
        return false;
    }
    
    channel->gain_db = gain_db;
    if (channel->pipeline_channel_id != 0) {
        return audio_pipeline_->set_channel_gain(channel->pipeline_channel_id, gain_db);
    }
    
    return true;
}

bool TimelineAudioManager::set_track_pan(ve::timeline::TrackId track_id, float pan) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    auto* channel = get_channel(track_id);
    if (!channel) {
        set_error("Track not found: " + std::to_string(track_id));
        return false;
    }
    
    channel->pan = pan;
    if (channel->pipeline_channel_id != 0) {
        return audio_pipeline_->set_channel_pan(channel->pipeline_channel_id, pan);
    }
    
    return true;
}

bool TimelineAudioManager::process_timeline_audio(ve::TimePoint position, uint32_t frame_count) {
    if (!is_playing_.load() || !timeline_) {
        return true;  // Not playing or no timeline
    }
    
    current_position_.store(position);
    
    std::lock_guard<std::mutex> lock(channels_mutex_);
    
    uint32_t active_tracks = 0;
    uint32_t segments_processed = 0;
    
    // Process each audio track
    for (auto& [track_id, channel] : channels_) {
        if (!channel || channel->is_muted) {
            continue;
        }
        
        const ve::timeline::Track* track = timeline_->get_track(track_id);
        if (!track || track->type() != ve::timeline::Track::Audio) {
            continue;
        }
        
        // Find segments that overlap with current position
        const auto& segments = track->segments();
        bool track_has_audio = false;
        
        for (const auto& segment : segments) {
            if (position >= segment.start_time && position < segment.end_time()) {
                // This segment is active at current position
                if (decode_segment_audio(segment, *channel, position, frame_count)) {
                    track_has_audio = true;
                    segments_processed++;
                }
            }
        }
        
        if (track_has_audio) {
            active_tracks++;
            channel->is_active = true;
        } else {
            channel->is_active = false;
        }
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.active_tracks = active_tracks;
        stats_.segments_processed += segments_processed;
        stats_.frames_mixed++;
    }
    
    return true;
}

TimelineAudioStats TimelineAudioManager::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void TimelineAudioManager::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = TimelineAudioStats{};
}

std::vector<TimelineAudioChannel> TimelineAudioManager::get_active_channels() const {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    std::vector<TimelineAudioChannel> active_channels;
    
    for (const auto& [track_id, channel] : channels_) {
        if (channel && channel->is_active) {
            // Copy channel data (excluding decoder which can't be copied)
            TimelineAudioChannel channel_copy;
            channel_copy.pipeline_channel_id = channel->pipeline_channel_id;
            channel_copy.timeline_track_id = channel->timeline_track_id;
            channel_copy.track_name = channel->track_name;
            channel_copy.is_muted = channel->is_muted;
            channel_copy.is_solo = channel->is_solo;
            channel_copy.gain_db = channel->gain_db;
            channel_copy.pan = channel->pan;
            channel_copy.current_position = channel->current_position;
            channel_copy.is_active = channel->is_active;
            
            active_channels.push_back(std::move(channel_copy));
        }
    }
    
    return active_channels;
}

std::string TimelineAudioManager::get_last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void TimelineAudioManager::clear_error() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private helper methods

void TimelineAudioManager::set_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    ve::log::error("Timeline audio manager error: " + error);
}

bool TimelineAudioManager::create_channel_for_track(const ve::timeline::Track& track) {
    if (track.type() != ve::timeline::Track::Audio) {
        return false;  // Not an audio track
    }
    
    auto channel = std::make_unique<TimelineAudioChannel>();
    channel->timeline_track_id = track.id();
    channel->track_name = track.name();
    
    // Create audio pipeline channel
    channel->pipeline_channel_id = audio_pipeline_->add_audio_channel(
        track.name(), 0.0f, 0.0f);
    
    if (channel->pipeline_channel_id == 0) {
        set_error("Failed to create audio pipeline channel for track: " + track.name());
        return false;
    }
    
    ve::log::info("Created audio channel for track: " + track.name() + 
                 " (pipeline_id=" + std::to_string(channel->pipeline_channel_id) + ")");
    
    channels_[track.id()] = std::move(channel);
    return true;
}

bool TimelineAudioManager::remove_channel_for_track(ve::timeline::TrackId track_id) {
    auto it = channels_.find(track_id);
    if (it == channels_.end()) {
        return false;  // Channel doesn't exist
    }
    
    auto& channel = it->second;
    if (channel && channel->pipeline_channel_id != 0) {
        audio_pipeline_->remove_audio_channel(channel->pipeline_channel_id);
        ve::log::info("Removed audio channel for track: " + channel->track_name);
    }
    
    channels_.erase(it);
    return true;
}

TimelineAudioChannel* TimelineAudioManager::get_channel(ve::timeline::TrackId track_id) {
    auto it = channels_.find(track_id);
    return (it != channels_.end()) ? it->second.get() : nullptr;
}

const TimelineAudioChannel* TimelineAudioManager::get_channel(ve::timeline::TrackId track_id) const {
    auto it = channels_.find(track_id);
    return (it != channels_.end()) ? it->second.get() : nullptr;
}

bool TimelineAudioManager::decode_segment_audio(const ve::timeline::Segment& segment,
                                               TimelineAudioChannel& channel,
                                               ve::TimePoint position,
                                               uint32_t frame_count) {
    // TODO: Implement actual segment audio decoding
    // For now, this is a placeholder that creates silent audio
    
    // Calculate relative position within segment  
    // Note: TimePoint subtraction would need operator implementation
    // For now, calculate manually using rational arithmetic
    auto segment_offset_num = position.to_rational().num * segment.start_time.to_rational().den - 
                             segment.start_time.to_rational().num * position.to_rational().den;
    auto segment_offset_den = position.to_rational().den * segment.start_time.to_rational().den;
    
    // Create a simple audio frame with the requested frame count
    // This should be replaced with actual audio decoding from the segment's media source
    auto audio_frame = ve::audio::AudioFrame::create(
        48000,  // sample_rate
        2,      // channel_count 
        frame_count,  // sample_count
        ve::audio::SampleFormat::Float32,  // format
        position  // timestamp
    );
    
    // Fill with silence for now (placeholder)
    // In a real implementation, this would:
    // 1. Get the media source from the segment
    // 2. Decode audio from the source at the correct position
    // 3. Apply any segment-specific effects or transformations
    auto* samples = reinterpret_cast<float*>(audio_frame->data());
    std::fill(samples, samples + (frame_count * 2), 0.0f);  // Silent audio
    
    // Submit to audio pipeline
    return audio_pipeline_->process_audio_frame(audio_frame);
}

std::shared_ptr<AudioFrame> TimelineAudioManager::mix_track_audio(const TimelineAudioChannel& channel,
                                                                 ve::TimePoint position,
                                                                 uint32_t frame_count) {
    // TODO: Implement track-level audio mixing
    // This would combine multiple segments on the same track
    // For now, return null to indicate no audio to mix
    return nullptr;
}

bool TimelineAudioManager::submit_track_audio(uint32_t pipeline_channel_id,
                                             std::shared_ptr<AudioFrame> audio_frame) {
    if (!audio_frame) {
        return false;
    }
    
    // Submit the audio frame to the specific pipeline channel
    return audio_pipeline_->process_audio_frame(audio_frame);
}

} // namespace ve::audio