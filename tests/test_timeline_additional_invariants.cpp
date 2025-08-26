// Additional invariant and edge-case tests for timeline & track operations
#include <catch2/catch_test_macros.hpp>
#include "timeline/timeline.hpp"
#include "timeline/track.hpp" // for direct track helper/tests

using namespace ve;

static ve::TimePoint tp(int64_t us){ return ve::TimePoint{us}; }
static ve::TimeDuration dur(int64_t us){ return ve::TimeDuration{us}; }

TEST_CASE("Track add_segment assigns id when zero and preserves explicit ids", "[timeline][invariants]") {
    ve::timeline::Track t{1, ve::timeline::Track::Video, "T"};
    ve::timeline::Segment a; a.id = 0; a.start_time = tp(0); a.duration = dur(1'000); REQUIRE(t.add_segment(a));
    auto auto_id = t.last_added_segment_id();
    REQUIRE(auto_id != 0);
    ve::timeline::Segment b; b.id = auto_id + 5; b.start_time = tp(2'000); b.duration = dur(500); REQUIRE(t.add_segment(b));
    REQUIRE(t.last_added_segment_id() == b.id);
    // Next generated id must advance past explicit id
    auto next = t.generate_segment_id();
    REQUIRE(next > b.id);
}

TEST_CASE("Track remove_segment returns false for missing id and leaves state", "[timeline][invariants]") {
    ve::timeline::Track t{2, ve::timeline::Track::Video, "T2"};
    ve::timeline::Segment a; a.start_time = tp(0); a.duration = dur(1'000); REQUIRE(t.add_segment(a));
    auto existing = t.last_added_segment_id();
    REQUIRE_FALSE(t.remove_segment(existing + 999));
    REQUIRE(t.find_segment(existing) != nullptr);
    REQUIRE(t.is_non_overlapping());
}

TEST_CASE("Track move_segment rejects non-existent id", "[timeline][invariants]") {
    ve::timeline::Track t{3, ve::timeline::Track::Video, "T3"};
    REQUIRE_FALSE(t.move_segment(42, tp(10'000))); // nothing there
}

TEST_CASE("Split segment invalid times rejected", "[timeline][invariants]") {
    ve::timeline::Track t{4, ve::timeline::Track::Video, "T4"};
    ve::timeline::Segment s; s.id = t.generate_segment_id(); s.start_time = tp(0); s.duration = dur(1'000'000); REQUIRE(t.add_segment(s));
    // Before start
    REQUIRE_FALSE(t.split_segment(s.id, tp(-1)));
    // At start
    REQUIRE_FALSE(t.split_segment(s.id, tp(0)));
    // At end
    REQUIRE_FALSE(t.split_segment(s.id, tp(1'000'000)));
}

TEST_CASE("Delete range trims partial overlaps and removes fully contained segments", "[timeline][invariants]") {
    ve::timeline::Track t{5, ve::timeline::Track::Video, "T5"};
    // Layout: [0,1000), [1500,2300), [2400,3000)
    for(auto spec : { std::pair<int64_t,int64_t>{0,1000}, {1500,800}, {2400,600} }){
        ve::timeline::Segment s; s.start_time = tp(spec.first); s.duration = dur(spec.second); REQUIRE(t.add_segment(s));
    }
    // Delete [1600, 600). Semantics (current impl):
    //  - Middle segment partially overlaps: it is trimmed to [1500,1600) (tail removed, no splitting)
    //  - First segment unaffected
    //  - Third segment unaffected (ripple = false)
    REQUIRE(t.delete_range(tp(1600), dur(600), false));
    REQUIRE(t.segments().size() == 3); // Trim keeps leading fragment
    // Verify trimmed middle segment duration now 100us (1500 -> 1600)
    auto segs = t.segments();
    REQUIRE(segs[1].start_time.to_rational().num == 1500);
    REQUIRE(segs[1].duration.to_rational().num == 100);
    REQUIRE(t.is_non_overlapping());
}

TEST_CASE("Global insert_gap_all_tracks shifts all tracks symmetrically", "[timeline][invariants]") {
    ve::timeline::Timeline tl; auto v1 = tl.add_track(ve::timeline::Track::Video, "V1"); auto v2 = tl.add_track(ve::timeline::Track::Audio, "A1");
    auto* t1 = tl.get_track(v1); auto* t2 = tl.get_track(v2);
    REQUIRE(t1 != nullptr);
    REQUIRE(t2 != nullptr);
    ve::timeline::Segment s1; s1.start_time=tp(0); s1.duration=dur(1'000'000); REQUIRE(t1->add_segment(s1));
    ve::timeline::Segment s2; s2.start_time=tp(2'000'000); s2.duration=dur(500'000); REQUIRE(t2->add_segment(s2));
    auto v_before = tl.version();
    REQUIRE(tl.insert_gap_all_tracks(tp(1'000'000), dur(200'000)));
    REQUIRE(tl.version() == v_before + 1);
    auto* moved2 = t2->find_segment(s2.id ? s2.id : t2->last_added_segment_id());
    REQUIRE(moved2);
    REQUIRE(moved2->start_time.to_rational().num == 2'200'000);
}

TEST_CASE("Global delete_range_all_tracks ripple adjusts later segments", "[timeline][invariants]") {
    ve::timeline::Timeline tl; auto v1 = tl.add_track(ve::timeline::Track::Video, "V1");
    auto* t1 = tl.get_track(v1); REQUIRE(t1 != nullptr);
    // Create spaced segments
    ve::timeline::Segment a; a.start_time=tp(0); a.duration=dur(500'000); REQUIRE(t1->add_segment(a));
    ve::timeline::Segment b; b.start_time=tp(800'000); b.duration=dur(400'000); REQUIRE(t1->add_segment(b));
    ve::timeline::Segment c; c.start_time=tp(1'500'000); c.duration=dur(300'000); REQUIRE(t1->add_segment(c));
    auto version_before = tl.version();
    // Delete range covering end of b and start gap before c with ripple so c shifts earlier by 200k
    REQUIRE(tl.delete_range_all_tracks(tp(1'000'000), dur(400'000), true));
    REQUIRE(tl.version() == version_before + 1);
    auto* c_after = t1->find_segment(c.id ? c.id : t1->last_added_segment_id());
    REQUIRE(c_after);
    // Ripple shifts segment c earlier by full deleted duration (400k)
    REQUIRE(c_after->start_time.to_rational().num == 1'100'000);
}
