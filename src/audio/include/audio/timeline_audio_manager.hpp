/**
 * @file timeline_audio_manager.hpp
 * @brief Timeline Audio Integration Manager
 *
 * Connects timeline audio tracks to the audio pipeline for real-time mixing.
 * Manages audio channels for each timeline track and handles segment audio processing.
 */

#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>

#include "audio/audio_pipeline.hpp"
#include "audio/audio_frame.hpp"
#include "timeline/timeline.hpp"
#include "timeline/track.hpp"
#include "core/time.hpp"
#include "decode/decoder.hpp"

namespace ve::audio {

/**
 * @brief Audio channel mapping for timeline tracks
 */
struct TimelineAudioChannel {
    uint32_t pipeline_channel_id = 0;  // Audio pipeline channel ID
    ve::timeline::TrackId timeline_track_id = 0;  // Timeline track ID
    std::string track_name;
    bool is_muted = false;
    bool is_solo = false;
    float gain_db = 0.0f;
    float pan = 0.0f;
    
    // Audio processing state
    std::unique_ptr<decode::IDecoder> decoder;  // Decoder for this track's audio
    ve::TimePoint current_position{0, 1};  // Current playback position
    bool is_active = false;  // Whether this track is currently playing audio
};

/**
 * @brief Timeline audio processing statistics
 */
struct TimelineAudioStats {
    uint32_t active_tracks = 0;
    uint32_t total_tracks = 0;
    uint32_t frames_mixed = 0;
    uint32_t segments_processed = 0;
    float cpu_usage_percent = 0.0f;
    uint32_t buffer_underruns = 0;
    uint32_t decode_errors = 0;
};

/**
 * @brief Manages audio integration between timeline and audio pipeline
 *
 * Creates audio channels for timeline tracks, decodes segment audio,
 * and feeds processed audio to the audio pipeline for mixing and output.
 */
class TimelineAudioManager {
public:
    /**
     * @brief Create timeline audio manager
     */
    static std::unique_ptr<TimelineAudioManager> create(
        ve::audio::AudioPipeline* pipeline);
    
    ~TimelineAudioManager();
    
    // Lifecycle management
    bool initialize();
    void shutdown();
    bool is_initialized() const { return initialized_.load(); }
    
    // Timeline integration
    bool set_timeline(ve::timeline::Timeline* timeline);
    bool sync_tracks();  // Synchronize audio channels with timeline tracks
    
    // Playback control
    bool start_playback();
    bool stop_playback();
    bool pause_playback();
    bool seek_to(ve::TimePoint position);
    
    // Track audio control
    bool set_track_mute(ve::timeline::TrackId track_id, bool muted);
    bool set_track_solo(ve::timeline::TrackId track_id, bool solo);
    bool set_track_gain(ve::timeline::TrackId track_id, float gain_db);
    bool set_track_pan(ve::timeline::TrackId track_id, float pan);
    
    // Audio processing
    bool process_timeline_audio(ve::TimePoint position, uint32_t frame_count);
    
    // State and monitoring
    TimelineAudioStats get_stats() const;
    void reset_stats();
    std::vector<TimelineAudioChannel> get_active_channels() const;
    
    // Error handling
    std::string get_last_error() const;
    void clear_error();

private:
    TimelineAudioManager(ve::audio::AudioPipeline* pipeline);
    
    // Audio pipeline integration
    ve::audio::AudioPipeline* audio_pipeline_;
    
    // Timeline integration
    ve::timeline::Timeline* timeline_ = nullptr;  // Non-owning
    uint64_t timeline_version_ = 0;  // Track timeline changes
    
    // Channel management
    mutable std::mutex channels_mutex_;
    std::unordered_map<ve::timeline::TrackId, std::unique_ptr<TimelineAudioChannel>> channels_;
    
    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> is_playing_{false};
    std::atomic<ve::TimePoint> current_position_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    TimelineAudioStats stats_;
    
    // Error handling
    mutable std::mutex error_mutex_;
    std::string last_error_;
    
    // Helper methods
    void set_error(const std::string& error);
    bool create_channel_for_track(const ve::timeline::Track& track);
    bool remove_channel_for_track(ve::timeline::TrackId track_id);
    TimelineAudioChannel* get_channel(ve::timeline::TrackId track_id);
    const TimelineAudioChannel* get_channel(ve::timeline::TrackId track_id) const;
    
    // Audio processing helpers
    bool decode_segment_audio(const ve::timeline::Segment& segment, 
                             TimelineAudioChannel& channel,
                             ve::TimePoint position, 
                             uint32_t frame_count);
    std::shared_ptr<AudioFrame> mix_track_audio(const TimelineAudioChannel& channel,
                                               ve::TimePoint position,
                                               uint32_t frame_count);
    bool submit_track_audio(uint32_t pipeline_channel_id, 
                           std::shared_ptr<AudioFrame> audio_frame);
};

} // namespace ve::audio