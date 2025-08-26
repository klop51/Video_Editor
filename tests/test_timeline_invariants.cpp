// Basic invariant tests for timeline edits
#include <catch2/catch_test_macros.hpp>
#include "timeline/timeline.hpp"

TEST_CASE("Segments do not overlap after add/move/split", "[timeline][invariants]") {
    ve::timeline::Timeline tl;
    auto trackId = tl.add_track(ve::timeline::Track::Video, "V1");
    auto* track = tl.get_track(trackId);
    REQUIRE(track);

    // Add two non-overlapping segments
    ve::timeline::Segment s1; s1.start_time = ve::TimePoint{0}; s1.duration = ve::TimeDuration{1'000'000};
    REQUIRE(track->add_segment(s1));
    auto s1_id = track->last_added_segment_id();
    REQUIRE(s1_id != 0);

    ve::timeline::Segment s2; s2.start_time = ve::TimePoint{1'500'000}; s2.duration = ve::TimeDuration{500'000};
    REQUIRE(track->add_segment(s2));
    auto s2_id = track->last_added_segment_id();
    REQUIRE(s2_id != 0);
    REQUIRE(track->is_non_overlapping());

    // Attempt move causing overlap should fail
    auto* seg2 = track->find_segment(s2_id);
    REQUIRE(seg2);
    auto original_start = seg2->start_time;
    REQUIRE_FALSE(track->move_segment(seg2->id, ve::TimePoint{500'000}));
    REQUIRE(track->is_non_overlapping());
    REQUIRE(seg2->start_time.to_rational().num == original_start.to_rational().num);

    // Split first segment and ensure parts still non-overlapping
    auto* seg1 = track->find_segment(s1_id);
    REQUIRE(seg1);
    auto split_time = ve::TimePoint{500'000};
    REQUIRE(track->split_segment(seg1->id, split_time));
    REQUIRE(track->is_non_overlapping());
}

TEST_CASE("Insert gap shifts subsequent segments", "[timeline][invariants]") {
    ve::timeline::Timeline tl; auto tid = tl.add_track(ve::timeline::Track::Video, "V"); auto* track = tl.get_track(tid); REQUIRE(track);
    ve::timeline::Segment s; s.start_time=ve::TimePoint{0}; s.duration=ve::TimeDuration{1'000'000}; REQUIRE(track->add_segment(s));
    ve::timeline::Segment s3; s3.start_time=ve::TimePoint{2'000'000}; s3.duration=ve::TimeDuration{500'000}; REQUIRE(track->add_segment(s3));
    auto second_id = track->last_added_segment_id();
    REQUIRE(second_id != 0);
    REQUIRE(track->insert_gap(ve::TimePoint{1'000'000}, ve::TimeDuration{500'000}));
    // Second segment should have shifted by 500k
    auto* moved = track->find_segment(second_id);
    REQUIRE(moved);
    REQUIRE(moved->start_time.to_rational().num == 2'500'000);
}
