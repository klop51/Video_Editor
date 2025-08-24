#include "playback/controller.hpp"
#include "core/log.hpp"
#include <chrono>
#include <thread>

namespace ve::playback {

Controller::Controller() {
    playback_thread_ = std::thread(&Controller::playback_thread_main, this);
}

Controller::~Controller() {
    thread_should_exit_.store(true);
    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }
}

bool Controller::load_media(const std::string& path) {
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
    
    ve::log::info("Media loaded successfully: " + path);
    set_state(PlaybackState::Stopped);
    current_time_us_.store(0);
    
    return true;
}

void Controller::close_media() {
    stop();
    decoder_.reset();
    duration_us_ = 0;
    current_time_us_.store(0);
    
    // Reset stats
    stats_ = Stats{};
    frame_count_.store(0);
    total_frame_time_ms_.store(0.0);
}

void Controller::play() {
    if (!has_media()) {
        ve::log::warn("Cannot play: no media loaded");
        return;
    }
    
    ve::log::info("Starting playback");
    set_state(PlaybackState::Playing);
}

void Controller::pause() {
    if (state_.load() == PlaybackState::Playing) {
        ve::log::info("Pausing playback");
        set_state(PlaybackState::Paused);
    }
}

void Controller::stop() {
    if (state_.load() != PlaybackState::Stopped) {
        ve::log::info("Stopping playback");
        set_state(PlaybackState::Stopped);
        current_time_us_.store(0);
    }
}

bool Controller::seek(int64_t timestamp_us) {
    if (!has_media()) {
        return false;
    }
    
    ve::log::debug("Seeking to: " + std::to_string(timestamp_us) + " us");
    
    seek_target_us_.store(timestamp_us);
    seek_requested_.store(true);
    
    return true;
}

void Controller::set_state(PlaybackState new_state) {
    PlaybackState old_state = state_.exchange(new_state);
    if (old_state != new_state && state_callback_) {
        state_callback_(new_state);
    }
}

void Controller::update_frame_stats(double frame_time_ms) {
    int64_t count = frame_count_.fetch_add(1) + 1;
    double total = total_frame_time_ms_.fetch_add(frame_time_ms) + frame_time_ms;
    
    stats_.frames_displayed = count;
    stats_.avg_frame_time_ms = total / count;
}

void Controller::playback_thread_main() {
    ve::log::debug("Playback thread started");
    
    auto last_frame_time = std::chrono::high_resolution_clock::now();
    int64_t last_pts_us = 0;
    bool first_frame = true;
    
    while (!thread_should_exit_.load()) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
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
            // Read video frame
            auto video_frame = decoder_->read_video();
            if (video_frame) {
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
