#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "playback/controller.hpp"

#include <atomic>
#include <limits>

namespace ve::playback::testing {

class PlaybackControllerTestAccessor {
public:
    static double predict(const PlaybackController& ctrl,
                          double frame_pts_sec,
                          double device_head_sec) {
        return ctrl.predict_present_time_on_audio_clock(frame_pts_sec, device_head_sec);
    }

    static void force_av_map(PlaybackController& ctrl,
                             double offset_sec,
                             int64_t first_video_pts_us,
                             bool learned = true) {
        ctrl.av_map_offset_sec_.store(offset_sec, std::memory_order_release);
        ctrl.first_video_pts_us_.store(first_video_pts_us, std::memory_order_release);
        ctrl.av_map_learned_.store(learned, std::memory_order_release);
    }

    static void set_first_audio_pts(PlaybackController& ctrl, int64_t pts_us) {
        ctrl.first_audio_pts_us_.store(pts_us, std::memory_order_release);
    }

    static void set_video_latency(PlaybackController& ctrl, double latency_sec) {
        ctrl.video_latency_sec_ = latency_sec;
    }

    static void reset_learning(PlaybackController& ctrl) {
        ctrl.reset_av_sync_learning();
    }

    static bool av_map_learned(const PlaybackController& ctrl) {
        return ctrl.av_map_learned_.load(std::memory_order_acquire);
    }

    static double av_map_offset(const PlaybackController& ctrl) {
        return ctrl.av_map_offset_sec_.load(std::memory_order_acquire);
    }

    static int64_t first_video_pts(const PlaybackController& ctrl) {
        return ctrl.first_video_pts_us_.load(std::memory_order_acquire);
    }

    static int64_t first_audio_pts(const PlaybackController& ctrl) {
        return ctrl.first_audio_pts_us_.load(std::memory_order_acquire);
    }
};

} // namespace ve::playback::testing

namespace {

struct ControllerGuard {
    ve::playback::PlaybackController& ctrl;
    ~ControllerGuard() { ctrl.stopAndJoin(); }
};

} // namespace

TEST_CASE("AV map returns device head before learning") {
    ve::playback::PlaybackController controller;
    ControllerGuard guard{controller};
    controller.stopAndJoin();

    ve::playback::testing::PlaybackControllerTestAccessor::reset_learning(controller);

    const double device_head = 4.25;
    const double predicted = ve::playback::testing::PlaybackControllerTestAccessor::predict(controller, 12.0, device_head);

    REQUIRE(predicted == Catch::Approx(device_head).margin(1e-6));
}

TEST_CASE("AV map applies normalization and latency after learning") {
    ve::playback::PlaybackController controller;
    ControllerGuard guard{controller};
    controller.stopAndJoin();

    constexpr int64_t first_video_pts_us = 2'000'000; // 2 seconds
    constexpr double offset_sec = 1.25;
    constexpr double latency_sec = 0.080;
    constexpr double frame_pts_sec = 3.5;

    ve::playback::testing::PlaybackControllerTestAccessor::force_av_map(controller, offset_sec, first_video_pts_us);
    ve::playback::testing::PlaybackControllerTestAccessor::set_video_latency(controller, latency_sec);

    const double predicted = ve::playback::testing::PlaybackControllerTestAccessor::predict(controller, frame_pts_sec, 0.0);
    const double expected = (frame_pts_sec - 2.0) + offset_sec + latency_sec;

    REQUIRE(predicted == Catch::Approx(expected).margin(1e-6));
}

TEST_CASE("Reset clears AV map learning state") {
    ve::playback::PlaybackController controller;
    ControllerGuard guard{controller};
    controller.stopAndJoin();

    ve::playback::testing::PlaybackControllerTestAccessor::force_av_map(controller, 0.5, 1'500'000);
    ve::playback::testing::PlaybackControllerTestAccessor::set_first_audio_pts(controller, 900'000);

    ve::playback::testing::PlaybackControllerTestAccessor::reset_learning(controller);

    REQUIRE_FALSE(ve::playback::testing::PlaybackControllerTestAccessor::av_map_learned(controller));
    REQUIRE(ve::playback::testing::PlaybackControllerTestAccessor::av_map_offset(controller) == Catch::Approx(0.0).margin(1e-9));
    REQUIRE(ve::playback::testing::PlaybackControllerTestAccessor::first_video_pts(controller) == std::numeric_limits<int64_t>::lowest());
    REQUIRE(ve::playback::testing::PlaybackControllerTestAccessor::first_audio_pts(controller) == std::numeric_limits<int64_t>::lowest());
}
