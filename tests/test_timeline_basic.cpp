// Basic timeline invariants & snapshot tests
#include <catch2/catch_test_macros.hpp>
#include "timeline/timeline.hpp"

using namespace ve;

static std::shared_ptr<ve::timeline::MediaSource> make_source(int64_t dur_us){
    auto src = std::make_shared<ve::timeline::MediaSource>();
    src->path = "dummy.mp4"; src->duration = ve::TimeDuration{dur_us,1};
    return src;
}

TEST_CASE("Timeline add/remove track increments version") {
    ve::timeline::Timeline tl; auto v0 = tl.version();
    auto id = tl.add_track(ve::timeline::Track::Video, "V1");
    REQUIRE(tl.version() == v0+1);
    tl.remove_track(id);
    REQUIRE(tl.version() == v0+2);
}

TEST_CASE("Timeline snapshot is immutable copy") {
    ve::timeline::Timeline tl; tl.add_track(ve::timeline::Track::Video, "V1");
    auto src = make_source(1'000'000); tl.add_clip(src, "Clip");
    auto snap = tl.snapshot();
    REQUIRE(snap->version == tl.version());
    REQUIRE(snap->tracks.size() == 1);
    // mutate timeline after snapshot
    tl.add_track(ve::timeline::Track::Audio, "A1");
    REQUIRE(snap->tracks.size() == 1); // unchanged
}
