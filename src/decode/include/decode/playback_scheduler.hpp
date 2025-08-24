#pragma once
#include <cstdint>
#include <functional>
#include <chrono>

namespace ve::decode {

// Very basic playback scheduler that maps wall-clock to media time.
class PlaybackScheduler {
public:
    using Clock = std::chrono::steady_clock;

    void start(int64_t start_pts_us, double rate = 1.0) {
        start_media_pts_us_ = start_pts_us;
        rate_ = rate;
        start_wall_ = Clock::now();
        running_ = true;
    }

    void stop() { running_ = false; }

    int64_t current_media_pts() const {
        if(!running_) return start_media_pts_us_;
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_wall_).count();
        return start_media_pts_us_ + static_cast<int64_t>(elapsed * rate_);
    }

    void set_rate(double r) { rate_ = r; }

private:
    bool running_ = false;
    int64_t start_media_pts_us_ = 0;
    double rate_ = 1.0;
    Clock::time_point start_wall_{};
};

} // namespace ve::decode
