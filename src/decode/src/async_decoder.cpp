#include "decode/async_decoder.hpp"
#include "core/log.hpp"
#include "decode/color_convert.hpp"
#include <chrono>

namespace ve::decode {

void AsyncDecoder::run() {
    using namespace std::chrono_literals;
    int64_t last_pts = 0;
    int frames_behind = 0;

    // Simple loop: read video frames sequentially with lookahead
    while(running_) {
        {
            std::lock_guard lk(seek_mutex_);
            if(seek_requested_) {
                if(decoder_->seek_microseconds(pending_seek_)) {
                    ve::log::debug("AsyncDecoder: seeked to " + std::to_string(pending_seek_) + "us");
                    cache_.clear(); // reset cache contents
                    last_pts = 0;
                    frames_behind = 0;
                }
                seek_requested_ = false;
            }
        }

        auto vf = decoder_->read_video();
        if(!vf) {
            std::this_thread::sleep_for(5ms);
            continue; // EOF or no frame; real impl would break or loop
        }

        // Basic frame drop detection - if we're more than 2 frames behind target
        int64_t target_pts = target_pts_.load(std::memory_order_relaxed);
        if (last_pts > 0 && vf->pts < target_pts - 66666) { // More than 2 frames behind at 30fps
            frames_behind++;
            if (frames_behind % 10 == 0) { // Log every 10th dropped frame group
                ve::log::warn("Decoder behind by " + std::to_string(frames_behind) +
                             " frames (target: " + std::to_string(target_pts) +
                             "us, current: " + std::to_string(vf->pts) + "us)");
            }
        } else {
            frames_behind = 0; // Reset when caught up
        }

        cache_.put(vf->pts, *vf);
        last_pts = vf->pts;

        if(callback_) callback_(*vf);

        // Small yield to prevent busy waiting
        std::this_thread::sleep_for(1ms);
    }
}

} // namespace ve::decode
