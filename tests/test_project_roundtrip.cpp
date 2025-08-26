#include <catch2/catch_test_macros.hpp>
#include "timeline/timeline.hpp"
#include "persistence/project_serializer.hpp"
#include <filesystem>

TEST_CASE("timeline project round-trip save/load", "[persistence]") {
    using namespace ve; using namespace ve::timeline; using namespace ve::persistence;
    Timeline tl; tl.set_name("RoundTripTest"); tl.set_frame_rate({24,1});
    auto clipSrc = std::make_shared<MediaSource>(); clipSrc->path = "clipA"; clipSrc->duration = TimeDuration{10'000'000};
    auto cid = tl.add_clip(clipSrc, "clipA");
    auto trackId = tl.add_track(Track::Video, "V1");
    auto track = tl.get_track(trackId);
    REQUIRE(track);
    Segment seg{}; seg.id = 1; seg.clip_id = cid; seg.start_time = TimePoint{0}; seg.duration = TimeDuration{1'000'000};
    REQUIRE(track->add_segment(seg));

    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "ve_roundtrip_timeline.json";
    auto saveRes = save_timeline_json(tl, tmp.string());
    REQUIRE(saveRes.success);

    Timeline loaded; auto loadRes = load_timeline_json(loaded, tmp.string());
    REQUIRE(loadRes.success);

    CHECK(loaded.name() == tl.name());
    CHECK(loaded.frame_rate().num == 24);
    CHECK(loaded.frame_rate().den == 1);
    // Validate clip and segment existence (best-effort due to unordered maps)
    REQUIRE(loaded.clips().size() == 1);
    const auto& trackList = loaded.tracks();
    REQUIRE(trackList.size() == 1);
    auto& t0 = trackList[0];
    CHECK(t0->segments().size() == 1);
}
