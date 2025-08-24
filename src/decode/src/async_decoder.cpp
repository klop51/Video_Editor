#include "decode/async_decoder.hpp"
#include "core/log.hpp"
#include "decode/color_convert.hpp"
#include <chrono>

namespace ve::decode {

void AsyncDecoder::run() {
    using namespace std::chrono_literals;
    // Simple loop: read video frames sequentially
    while(running_) {
        {
            std::lock_guard lk(seek_mutex_);
            if(seek_requested_) {
                if(decoder_->seek_microseconds(pending_seek_)) {
                    ve::log::debug("AsyncDecoder: seeked");
                    cache_.clear(); // reset cache contents
                }
                seek_requested_ = false;
            }
        }
        auto vf = decoder_->read_video();
        if(!vf) {
            std::this_thread::sleep_for(5ms);
            continue; // EOF or no frame; real impl would break or loop
        }
        cache_.put(vf->pts, *vf);
        if(callback_) callback_(*vf);
    }
}

} // namespace ve::decode
