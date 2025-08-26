#include "playback/controller.hpp" // renamed class PlaybackController
#include "core/log.hpp"
#include "media_io/media_probe.hpp"
#include <vector>
#include <chrono>
#include <thread>
#include "timeline/timeline.hpp"

namespace ve::playback {

PlaybackController::PlaybackController() {
    playback_thread_ = std::thread(&PlaybackController::playback_thread_main, this);
}

PlaybackController::~PlaybackController() {
    thread_should_exit_.store(true);
    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }
}

bool PlaybackController::load_media(const std::string& path) {
    ve::log::info("Loading media: " + path);
    
    decoder_ = decode::create_decoder();
    if (!decoder_) {
        ve::log::error("Failed to create decoder");
        return false;
    }
    
    decode::OpenParams params;
    params.filepath = path;
    params.video = true;
    params.audio = true;
    
    if (!decoder_->open(params)) {
        ve::log::error("Failed to open media file: " + path);
        decoder_.reset();
        return false;
    }
    
    // Get duration from media probe
    auto probe_result = ve::media::probe_file(path);
    if (probe_result.success && probe_result.duration_us > 0) {
        duration_us_ = probe_result.duration_us;
        ve::log::info("Media duration: " + std::to_string(duration_us_) + " us (" + 
                     std::to_string(duration_us_ / 1000000.0) + " seconds)");
    } else {
        ve::log::warn("Could not determine media duration");
        duration_us_ = 0;
    }
    
    ve::log::info("Media loaded successfully: " + path);
    set_state(PlaybackState::Stopped);
    current_time_us_.store(0);
    
    return true;
}

void PlaybackController::close_media() {
    stop();
    decoder_.reset();
    duration_us_ = 0;
    current_time_us_.store(0);
    
    // Reset stats
    stats_ = Stats{};
    frame_count_.store(0);
    total_frame_time_ms_.store(0.0);
}

void PlaybackController::play() {
    if (!has_media()) {
        ve::log::warn("Cannot play: no media loaded");
        return;
    }
    
    ve::log::info("Starting playback");
    set_state(PlaybackState::Playing);
}

void PlaybackController::pause() {
    if (state_.load() == PlaybackState::Playing) {
        ve::log::info("Pausing playback");
        set_state(PlaybackState::Paused);
    }
}

void PlaybackController::stop() {
    if (state_.load() != PlaybackState::Stopped) {
        ve::log::info("Stopping playback");
        set_state(PlaybackState::Stopped);
        current_time_us_.store(0);
    }
}

bool PlaybackController::seek(int64_t timestamp_us) {
    if (!has_media()) {
        return false;
    }
    
    ve::log::debug("Seeking to: " + std::to_string(timestamp_us) + " us");
    
    seek_target_us_.store(timestamp_us);
    seek_requested_.store(true);
    
    return true;
}

void PlaybackController::set_state(PlaybackState new_state) {
    PlaybackState old_state = state_.exchange(new_state);
    if (old_state != new_state && state_callback_) {
        state_callback_(new_state);
    }
}

void PlaybackController::update_frame_stats(double frame_time_ms) {
    int64_t count = frame_count_.fetch_add(1) + 1;
    double total = total_frame_time_ms_.fetch_add(frame_time_ms) + frame_time_ms;
    
    stats_.frames_displayed = count;
    stats_.avg_frame_time_ms = total / count;
}

void PlaybackController::playback_thread_main() {
    ve::log::debug("Playback thread started");
    
    auto last_frame_time = std::chrono::high_resolution_clock::now();
    int64_t last_pts_us = 0;
    bool first_frame = true;
    
    while (!thread_should_exit_.load()) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        // Refresh timeline snapshot if timeline attached and version changed
        if(timeline_) {
            uint64_t v = timeline_->version();
            if(v != observed_timeline_version_) {
                timeline_snapshot_ = timeline_->snapshot(); // immutable copy
                observed_timeline_version_ = v;
                ve::log::debug("Playback refreshed timeline snapshot version " + std::to_string(v));
            }
        }
        
        // Handle seek requests
        if (seek_requested_.load()) {
            int64_t seek_target = seek_target_us_.load();
            if (decoder_ && decoder_->seek_microseconds(seek_target)) {
                current_time_us_.store(seek_target);
                ve::log::debug("Seek completed to: " + std::to_string(seek_target) + " us");
                first_frame = true; // Reset timing after seek
            }
            seek_requested_.store(false);
        }
        
        // Process frames if playing
        if (state_.load() == PlaybackState::Playing && decoder_) {
            // Attempt cache reuse first (only exact pts match for now)
            if(seek_requested_.load()==false) { // if not in middle of seek handling
                    ve::cache::CachedFrame cached; cache_lookups_.fetch_add(1);
                ve::cache::FrameKey key; key.pts_us = current_time_us_.load();
                if(frame_cache_.get(key, cached)) {
                    cache_hits_.fetch_add(1);
                    decode::VideoFrame vf; vf.width = cached.width; vf.height = cached.height; vf.pts = current_time_us_.load(); vf.data = cached.data; // copy buffer
                    vf.format = cached.format;
                    vf.color_space = cached.color_space;
                    vf.color_range = cached.color_range;
                    if(video_callback_) video_callback_(vf);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue; // Skip decoding new frame this iteration
                }
            }

            // Read video frame (decode path)
            auto video_frame = decoder_->read_video();
            if (video_frame) {
                // Future: traverse snapshot (immutable) to determine which clip/segment is active
                // For now we rely solely on decoder PTS ordering.
                // Cache store (basic) - store raw data buffer
                ve::cache::CachedFrame cf; cf.width = video_frame->width; cf.height = video_frame->height; cf.data = video_frame->data; // copy
                cf.format = video_frame->format;
                cf.color_space = video_frame->color_space;
                cf.color_range = video_frame->color_range;
                ve::cache::FrameKey putKey; putKey.pts_us = video_frame->pts;
                frame_cache_.put(putKey, std::move(cf));
                current_time_us_.store(video_frame->pts);

                if (video_callback_) {
                    video_callback_(*video_frame);
                }

                // Adaptive frame timing based on PTS
                if (!first_frame) {
                    int64_t pts_diff_us = video_frame->pts - last_pts_us;
                    if (pts_diff_us > 0 && pts_diff_us < 200000) { // Sanity check: 0-200ms
                        auto target_interval = std::chrono::microseconds(pts_diff_us);
                        auto frame_end = std::chrono::high_resolution_clock::now();
                        auto actual_duration = frame_end - last_frame_time;

                        if (actual_duration < target_interval) {
                            auto sleep_time = target_interval - actual_duration;
                            std::this_thread::sleep_for(sleep_time);
                        }
                    }
                }

                last_pts_us = video_frame->pts;
                first_frame = false;
                last_frame_time = std::chrono::high_resolution_clock::now();

                // Update performance stats
                auto frame_time = std::chrono::duration<double, std::milli>(last_frame_time - frame_start);
                update_frame_stats(frame_time.count());
            }
            
            // Read audio frame
            auto audio_frame = decoder_->read_audio();
            if (audio_frame && audio_callback_) {
                audio_callback_(*audio_frame);
            }
        } else {
            // Not playing, sleep longer
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
    
    ve::log::debug("Playback thread ended");
}

} // namespace ve::playback
