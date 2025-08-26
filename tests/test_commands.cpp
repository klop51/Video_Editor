// Tests for command rollback and undo correctness
#include <catch2/catch_test_macros.hpp>
#include "timeline/timeline.hpp"
#include "commands/timeline_commands.hpp"

static ve::TimePoint tp_us(int64_t us){ return ve::TimePoint{us}; }
static ve::TimeDuration dur_us(int64_t us){ return ve::TimeDuration{us}; }

TEST_CASE("MoveSegmentCommand rollback on overlap", "[commands]") {
    ve::timeline::Timeline tl;
    auto track_id = tl.add_track(ve::timeline::Track::Video, "V");
    auto* track = tl.get_track(track_id); REQUIRE(track);

    ve::timeline::Segment s1; s1.id = track->generate_segment_id(); s1.start_time = tp_us(0); s1.duration = dur_us(1'000'000);
    ve::timeline::Segment s2; s2.id = track->generate_segment_id(); s2.start_time = tp_us(1'200'000); s2.duration = dur_us(1'000'000);
    REQUIRE(track->add_segment(s1));
    REQUIRE(track->add_segment(s2));

    // Attempt to move s1 so it would overlap s2 (new range 1.4s - 2.4s overlaps s2 1.2s - 2.2s)
    ve::commands::MoveSegmentCommand move_cmd(s1.id, track_id, track_id, s1.start_time, tp_us(1'400'000));
    bool exec_ok = move_cmd.execute(tl);
    REQUIRE_FALSE(exec_ok); // Should fail due to overlap

    // Original segment should still exist at original position
    auto* restored = track->find_segment(s1.id);
    REQUIRE(restored);
    REQUIRE(restored->start_time.to_rational().num == 0);
    // No duplicate segments
    REQUIRE(track->segments().size() == 2);
}

TEST_CASE("SplitSegmentCommand success and undo", "[commands]") {
    ve::timeline::Timeline tl;
    auto track_id = tl.add_track(ve::timeline::Track::Video, "V");
    auto* track = tl.get_track(track_id); REQUIRE(track);

    ve::timeline::Segment s; s.id = track->generate_segment_id(); s.start_time = tp_us(0); s.duration = dur_us(2'000'000); s.name = "Seg";
    REQUIRE(track->add_segment(s));
    size_t before = track->segments().size();

    ve::commands::SplitSegmentCommand split_cmd(s.id, tp_us(1'000'000));
    REQUIRE(split_cmd.execute(tl));
    REQUIRE(track->segments().size() == before + 1); // one removed, two added
    // Undo
    REQUIRE(split_cmd.undo(tl));
    REQUIRE(track->segments().size() == before);
    auto* orig = track->find_segment(s.id);
    REQUIRE(orig);
    REQUIRE(orig->duration.to_rational().num == 2'000'000);
}

TEST_CASE("SplitSegmentCommand invalid split time leaves timeline unchanged", "[commands]") {
    ve::timeline::Timeline tl;
    auto track_id = tl.add_track(ve::timeline::Track::Video, "V");
    auto* track = tl.get_track(track_id); REQUIRE(track);

    // Original segment
    ve::timeline::Segment base; base.id = track->generate_segment_id(); base.start_time = tp_us(0); base.duration = dur_us(2'000'000); base.name="Base";
    REQUIRE(track->add_segment(base));
    size_t before = track->segments().size();

    // Attempt to split at the segment end (invalid)
    ve::commands::SplitSegmentCommand split_cmd(base.id, tp_us(2'000'000));
    REQUIRE_FALSE(split_cmd.execute(tl));
    REQUIRE(track->segments().size() == before);
    auto* still = track->find_segment(base.id);
    REQUIRE(still);
    REQUIRE(still->duration.to_rational().num == 2'000'000);
}

TEST_CASE("TrimSegmentCommand execute and undo", "[commands]") {
    ve::timeline::Timeline tl;
    auto track_id = tl.add_track(ve::timeline::Track::Video, "V");
    auto* track = tl.get_track(track_id); REQUIRE(track);

    ve::timeline::Segment s; s.id = track->generate_segment_id(); s.start_time = tp_us(2'000'000); s.duration = dur_us(2'000'000); s.name = "Trim";
    REQUIRE(track->add_segment(s));

    ve::commands::TrimSegmentCommand trim_cmd(s.id, tp_us(2'500'000), dur_us(1'000'000));
    REQUIRE(trim_cmd.execute(tl));
    auto* trimmed = track->find_segment(s.id);
    REQUIRE(trimmed);
    REQUIRE(trimmed->start_time.to_rational().num == 2'500'000);
    REQUIRE(trimmed->duration.to_rational().num == 1'000'000);
    REQUIRE(trim_cmd.undo(tl));
    auto* restored = track->find_segment(s.id);
    REQUIRE(restored);
    REQUIRE(restored->start_time.to_rational().num == 2'000'000);
    REQUIRE(restored->duration.to_rational().num == 2'000'000);
}
