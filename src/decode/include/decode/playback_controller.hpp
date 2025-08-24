#pragma once
#include "decode/async_decoder.hpp"
#include "decode/playback_scheduler.hpp"
#include "decode/color_convert.hpp"
#include <atomic>
#include <functional>

namespace ve::decode {

// High-level controller tying scheduler and async decoder, invoking a frame callback near target time.
class PlaybackController {
public:
    using FrameCallback = std::function<void(const VideoFrame& rgba)>;

    explicit PlaybackController(std::unique_ptr<IDecoder> dec)
        : decoder_(std::move(dec)) {}

    bool start(int64_t start_pts_us, FrameCallback cb) {
        if(!decoder_) return false;
        frame_cb_ = std::move(cb);
        if(!decoder_->open({pending_path_, true, false, true})) return false; // path must be set before start
        scheduler_.start(start_pts_us);
        async_ = std::make_unique<AsyncDecoder>(std::move(decoder_));
        async_->start(start_pts_us, [this](const VideoFrame& vf){ this->on_decoded(vf); });
        running_ = true;
        return true;
    }

    void set_media_path(std::string path) { pending_path_ = std::move(path); }

    void stop() {
        running_ = false;
        if(async_) async_->stop();
        scheduler_.stop();
    }

    int64_t current_pts() const { return scheduler_.current_media_pts(); }

private:
    void on_decoded(const VideoFrame& vf) {
        if(!running_) return;
        auto rgba = to_rgba(vf);
        if(rgba && frame_cb_) frame_cb_(*rgba);
    }

    std::unique_ptr<IDecoder> decoder_;
    std::unique_ptr<AsyncDecoder> async_;
    PlaybackScheduler scheduler_;
    FrameCallback frame_cb_;
    std::atomic<bool> running_{false};
    std::string pending_path_;
};

} // namespace ve::decode
