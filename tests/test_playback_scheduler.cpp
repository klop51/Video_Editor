#include <catch2/catch_test_macros.hpp>
#include "decode/playback_scheduler.hpp"
#include <thread>

TEST_CASE("PlaybackScheduler advances time") {
    using namespace ve::decode;
    PlaybackScheduler sched;
    sched.start(1000000 /*1s*/, 1.0);
    auto t0 = sched.current_media_pts();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto t1 = sched.current_media_pts();
    REQUIRE(t1 >= t0 + 5000); // at least 5ms progression
}
