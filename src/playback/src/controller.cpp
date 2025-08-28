#include "playback/controller.hpp" // renamed class PlaybackController
#include "core/log.hpp"
#include "media_io/media_probe.hpp"
#include "../../config/debug.hpp"
#include <vector>
#include <chrono>
#include <thread>

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
        // Derive fps from first video stream if present
        for(const auto& s : probe_result.streams) {
            if(s.type == "video" && s.fps > 0) { 
                probed_fps_ = s.fps; 
                // Initialize drift-proof stepping with detected fps
                if (s.fps == 29.97) {
                    step_.num = 30000; step_.den = 1001;
                } else if (s.fps == 23.976) {
                    step_.num = 24000; step_.den = 1001;
                } else {
                    step_.num = static_cast<int64_t>(s.fps + 0.5); step_.den = 1;
                }
                step_.rem = 0; // reset accumulator
                break; 
            }
        }
        ve::log::info("Media duration: " + std::to_string(duration_us_) + " us (" + 
                     std::to_string(duration_us_ / 1000000.0) + " seconds)");
    } else {
        ve::log::warn("Could not determine media duration");
        duration_us_ = 0;
    }
    
    ve::log::info("Media loaded successfully: " + path);
    set_state(PlaybackState::Stopped);
    current_time_us_.store(0);

    // Immediately decode first frame for instant preview (avoid blank)
    decode_one_frame_if_paused(0);
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

void PlaybackController::step_once() {
    if(!has_media()) return;
    single_step_.store(true);
    // Bypass cache for the next iteration so we decode strictly the next frame
    bypass_cache_once_.store(true);
    advance_one_frame_.store(false);
    // Force one-frame decode path even if currently paused/stopped
    if(state_.load() != PlaybackState::Playing) {
        set_state(PlaybackState::Playing);
    }
}

void PlaybackController::set_state(PlaybackState new_state) {
    PlaybackState old_state = state_.exchange(new_state);
    if (old_state != new_state) {
        VE_DEBUG_ONLY(ve::log::info(std::string("Playback state change: ") + std::to_string(static_cast<int>(old_state)) + " -> " + std::to_string(static_cast<int>(new_state))));
        std::vector<CallbackEntry<StateChangeCallback>> copy;
        {
            std::scoped_lock lk(callbacks_mutex_);
            copy = state_entries_;
        }
        for(auto &entry : copy) if(entry.fn) entry.fn(new_state);
    }
}

void PlaybackController::update_frame_stats(double frame_time_ms) {
    int64_t count = frame_count_.fetch_add(1) + 1;
    double total = total_frame_time_ms_.fetch_add(frame_time_ms) + frame_time_ms;
    
    stats_.frames_displayed = count;
    stats_.avg_frame_time_ms = total / count;
}

void PlaybackController::playback_thread_main() {
    ve::log::info("Playback thread started");
    
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
                ve::log::info("Seek completed to: " + std::to_string(seek_target) + " us");
                first_frame = true; // Reset timing after seek
                // If not playing, decode a preview frame immediately
                if(state_.load() != PlaybackState::Playing) {
                    decode_one_frame_if_paused(seek_target);
                }
            }
            seek_requested_.store(false);
        }
        
        // Process frames if playing
        if (state_.load() == PlaybackState::Playing && decoder_) {
            VE_DEBUG_ONLY({
                bool seek_req = seek_requested_.load();
                ve::log::info("Processing frame in playback thread, state=Playing, seek_requested=" + std::string(seek_req ? "true" : "false"));
            });
            
            // If a single-step is pending, we will bypass cache below and ask decoder for next frame.
            bool bypass_cache = bypass_cache_once_.exchange(false);

            int64_t current_pts = current_time_us_.load();
            
            // Attempt cache reuse first (only exact pts match for now), unless bypassed for single-step
            if(seek_requested_.load()==false && !bypass_cache) { // if not in middle of seek handling
                    ve::cache::CachedFrame cached; cache_lookups_.fetch_add(1);
                ve::cache::FrameKey key; key.pts_us = current_pts;
                VE_DEBUG_ONLY(ve::log::info("Cache lookup for pts: " + std::to_string(key.pts_us)));
                if(frame_cache_.get(key, cached)) {
                    cache_hits_.fetch_add(1);
                    VE_DEBUG_ONLY(ve::log::info("Cache HIT for pts: " + std::to_string(key.pts_us)));
                    decode::VideoFrame vf; vf.width = cached.width; vf.height = cached.height; vf.pts = current_pts; vf.data = cached.data; // copy buffer
                    vf.format = cached.format;
                    vf.color_space = cached.color_space;
                    vf.color_range = cached.color_range;
                    {
                        std::vector<CallbackEntry<VideoFrameCallback>> copy;
                        { std::scoped_lock lk(callbacks_mutex_); copy = video_video_entries_; }
                        VE_DEBUG_ONLY(ve::log::info("Dispatching " + std::to_string(copy.size()) + " video callbacks (cache hit) for pts=" + std::to_string(vf.pts)));
                        for(auto &entry : copy) if(entry.fn) entry.fn(vf);
                        VE_DEBUG_ONLY(ve::log::info("Dispatched video callbacks (cache hit) for pts=" + std::to_string(vf.pts)));
                    }
                    // IMPORTANT: Advance time for next frame even when cache hits
                    // Prefer drift-proof step accumulator for fractional framerates
                    int64_t delta = step_.next_delta_us();
                    int64_t next_pts = current_pts + delta;
                    if (duration_us_ > 0 && next_pts >= duration_us_) {
                        current_time_us_.store(duration_us_);
                        set_state(ve::playback::PlaybackState::Stopped);   // notify UI we're done
                        ve::log::info("Reached end of stream at: " + std::to_string(duration_us_) + " us (cache hit path) - stopping");
                        continue; // exit playback loop cleanly
                    }
                    current_time_us_.store(next_pts);
                    VE_DEBUG_ONLY(ve::log::info("Advanced time to: " + std::to_string(next_pts) + " (cache hit path)"));
                    // Frame pacing for cache hit: sleep remaining time of this frame interval
                    {
                        auto target_interval = std::chrono::microseconds(delta);
                        auto frame_end = std::chrono::high_resolution_clock::now();
                        auto actual_duration = frame_end - last_frame_time;
                        if (actual_duration < target_interval) {
                            auto sleep_time = target_interval - actual_duration;
                            std::this_thread::sleep_for(sleep_time);
                        }
                        last_frame_time = std::chrono::high_resolution_clock::now();
                        first_frame = false;
                        last_pts_us = current_pts;
                    }
                    continue; // Skip decoding new frame this iteration
                } else {
                    VE_DEBUG_ONLY(ve::log::info("Cache MISS for pts: " + std::to_string(key.pts_us) + ", proceeding to decoder"));
                }
            } else {
                VE_DEBUG_ONLY(ve::log::info(bypass_cache ? "Bypassing cache for single-step" : "Skipping frame processing because seek_requested=true"));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (!bypass_cache) continue; // for seek, skip; for bypass, fall-through to decoder read
            }

            // Read video frame (decode path)
            auto video_frame = decoder_->read_video();
            VE_DEBUG_ONLY(ve::log::info(std::string("Called decoder_->read_video(), result: ") + (video_frame ? "got frame" : "no frame")));
            if (video_frame) {
                // Future: traverse snapshot (immutable) to determine which clip/segment is active
                // For now we rely solely on decoder PTS ordering.
                // Cache store (basic) - store raw data buffer
                ve::cache::CachedFrame cf; cf.width = video_frame->width; cf.height = video_frame->height; cf.data = video_frame->data; // copy
                cf.format = video_frame->format;
                cf.color_space = video_frame->color_space;
                cf.color_range = video_frame->color_range;
                ve::cache::FrameKey putKey; putKey.pts_us = video_frame->pts;
                VE_DEBUG_ONLY(ve::log::info("Cache PUT for pts=" + std::to_string(putKey.pts_us) + 
                                             ", size=" + std::to_string(cf.width) + "x" + std::to_string(cf.height)));
                frame_cache_.put(putKey, std::move(cf));
                VE_DEBUG_ONLY(ve::log::info("Cache size now=" + std::to_string(frame_cache_.size())));
                current_time_us_.store(video_frame->pts);

                {
                    std::vector<CallbackEntry<VideoFrameCallback>> copy;
                    { std::scoped_lock lk(callbacks_mutex_); copy = video_video_entries_; }
                    VE_DEBUG_ONLY(ve::log::info("Dispatching " + std::to_string(copy.size()) + " video callbacks (decode) for pts=" + std::to_string(video_frame->pts)));
                    for(auto &entry : copy) if(entry.fn) entry.fn(*video_frame);
                    VE_DEBUG_ONLY(ve::log::info("Dispatched video callbacks (decode) for pts=" + std::to_string(video_frame->pts)));
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

                // Periodic frame log (every 30 frames) for diagnostics
                if ((stats_.frames_displayed % 30) == 0) {
                    ve::log::debug("Playback frames displayed=" + std::to_string(stats_.frames_displayed));
                }
            }
            
            // Read audio frame
            auto audio_frame = decoder_->read_audio();
            if (audio_frame) {
                {
                    std::vector<CallbackEntry<AudioFrameCallback>> copy;
                    { std::scoped_lock lk(callbacks_mutex_); copy = audio_entries_; }
                    VE_DEBUG_ONLY(ve::log::info("Dispatching " + std::to_string(copy.size()) + " audio callbacks for pts=" + std::to_string(audio_frame->pts)));
                    for(auto &entry : copy) if(entry.fn) entry.fn(*audio_frame);
                    VE_DEBUG_ONLY(ve::log::info("Dispatched audio callbacks for pts=" + std::to_string(audio_frame->pts)));
                }
            } else {
                // Handle end-of-stream and avoid busy loop when decoder has no more frames
                int64_t cur = current_time_us_.load();
                int64_t delta = step_.next_delta_us();
                int64_t next_pts = cur + delta;
                if (duration_us_ > 0 && next_pts >= duration_us_) {
                    current_time_us_.store(duration_us_);
                    set_state(ve::playback::PlaybackState::Stopped);
                    ve::log::info("Reached end of stream (decoder no frame) at: " + std::to_string(duration_us_) + " us - stopping");
                    continue;
                }
                current_time_us_.store(next_pts);
                VE_DEBUG_ONLY(ve::log::info("Advanced time to: " + std::to_string(next_pts) + " (no-frame path)"));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else {
            // Not playing, sleep longer
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        // Handle single-step completion (after any frame decode above)
        if(single_step_.load() && state_.load() == PlaybackState::Playing) {
            // After issuing single step we pause right after first new pts displayed
            // We track last_pts_us; when it changes post-step we pause.
            static thread_local int64_t step_start_pts = -1;
            if(step_start_pts < 0) step_start_pts = current_time_us_.load();
            if(current_time_us_.load() != step_start_pts) {
                single_step_.store(false);
                step_start_pts = -1;
                set_state(PlaybackState::Paused);
            }
        }
    }
    
    ve::log::debug("Playback thread ended");
}

int64_t PlaybackController::frame_duration_guess_us() const {
    // Prefer probed fps
    // If timeline snapshot present, attempt to use active clip's fps
    if(timeline_snapshot_) {
        // Determine active time
        int64_t cur = current_time_us_.load();
        for(const auto &trk : timeline_snapshot_->tracks) {
            if(trk.type() != ve::timeline::Track::Video) continue;
            for(const auto &seg : trk.segments()) {
                auto start = seg.start_time.to_rational();
                auto dur = seg.duration.to_rational();
                int64_t seg_start_us = (start.num * 1'000'000) / start.den;
                int64_t seg_end_us = seg_start_us + (dur.num * 1'000'000) / dur.den;
                if(cur >= seg_start_us && cur < seg_end_us) {
                    auto clip_it = timeline_snapshot_->clips.find(seg.clip_id);
                    if(clip_it != timeline_snapshot_->clips.end() && clip_it->second.source) {
                        auto fr = clip_it->second.source->frame_rate;
                        if(fr.num > 0 && fr.den > 0) {
                            long double fps = (long double)fr.num / (long double)fr.den;
                            if(fps > 1.0 && fps < 480.0) return (int64_t)((1'000'000.0L / fps) + 0.5L);
                        }
                    }
                }
            }
        }
    }
    if(probed_fps_ > 1.0 && probed_fps_ < 480.0) {
        return static_cast<int64_t>( (1'000'000.0 / probed_fps_) + 0.5 );
    }
    // Fallback: derive from recent frame stats if available, else 33_333
    if(stats_.frames_displayed >= 2) {
        double avg_ms = stats_.avg_frame_time_ms;
        if(avg_ms > 5 && avg_ms < 100) return static_cast<int64_t>(avg_ms * 1000.0);
    }
    return 33'333; // ~30fps
}

void PlaybackController::decode_one_frame_if_paused(int64_t seek_target_us) {
    if(!decoder_ || state_.load() == PlaybackState::Playing) return;
    // Attempt to read a single frame at/after seek target immediately for preview
    auto frame = decoder_->read_video();
    if(frame) {
        current_time_us_.store(frame->pts);
        // Cache it
        ve::cache::CachedFrame cf; cf.width = frame->width; cf.height = frame->height; cf.data = frame->data;
        cf.format = frame->format; cf.color_space = frame->color_space; cf.color_range = frame->color_range;
        ve::cache::FrameKey putKey; putKey.pts_us = frame->pts; frame_cache_.put(putKey, std::move(cf));
        std::vector<CallbackEntry<VideoFrameCallback>> copy;
        { std::scoped_lock lk(callbacks_mutex_); copy = video_video_entries_; }
        for(auto &entry : copy) if(entry.fn) entry.fn(*frame);
    } else {
        // Fallback: keep current_time at seek target
        current_time_us_.store(seek_target_us);
    }
}

bool PlaybackController::remove_video_callback(CallbackId id) {
    if(id==0) return false; std::scoped_lock lk(callbacks_mutex_);
    auto it = std::remove_if(video_video_entries_.begin(), video_video_entries_.end(), [id](auto &e){return e.id==id;});
    bool removed = it != video_video_entries_.end();
    video_video_entries_.erase(it, video_video_entries_.end());
    return removed;
}
bool PlaybackController::remove_audio_callback(CallbackId id) {
    if(id==0) return false; std::scoped_lock lk(callbacks_mutex_);
    auto it = std::remove_if(audio_entries_.begin(), audio_entries_.end(), [id](auto &e){return e.id==id;});
    bool removed = it != audio_entries_.end();
    audio_entries_.erase(it, audio_entries_.end());
    return removed;
}
bool PlaybackController::remove_state_callback(CallbackId id) {
    if(id==0) return false; std::scoped_lock lk(callbacks_mutex_);
    auto it = std::remove_if(state_entries_.begin(), state_entries_.end(), [id](auto &e){return e.id==id;});
    bool removed = it != state_entries_.end();
    state_entries_.erase(it, state_entries_.end());
    return removed;
}

} // namespace ve::playback
