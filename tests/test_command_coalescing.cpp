#include <catch2/catch_test_macros.hpp>
#include "commands/timeline_commands.hpp"
#include "commands/command.hpp"
#include "timeline/timeline.hpp"
#include <thread>

static ve::TimePoint tp_us(int64_t us){ return ve::TimePoint{us}; }
static ve::TimeDuration dur_us(int64_t us){ return ve::TimeDuration{us}; }

TEST_CASE("MoveSegmentCommand coalesces rapid consecutive moves", "[commands][coalesce]") {
    ve::timeline::Timeline tl;
    auto track_id = tl.add_track(ve::timeline::Track::Video, "V");
    auto* track = tl.get_track(track_id); REQUIRE(track);
    ve::timeline::Segment s; s.id = track->generate_segment_id(); s.start_time = tp_us(0); s.duration = dur_us(2'000'000); s.name = "Clip"; REQUIRE(track->add_segment(s));

    ve::commands::CommandHistory history;

    // Issue first move (baseline) - executes and enters history
    auto move1 = std::make_unique<ve::commands::MoveSegmentCommand>(s.id, track_id, track_id, tp_us(0), tp_us(100'000));
    REQUIRE(history.execute(std::move(move1), tl));
    REQUIRE(history.current_index() == 1);
    REQUIRE(history.commands().size() == 1);

    // Rapid subsequent small moves inside merge window (400ms) should coalesce
    auto move2 = std::make_unique<ve::commands::MoveSegmentCommand>(s.id, track_id, track_id, tp_us(100'000), tp_us(200'000));
    REQUIRE(history.execute(std::move(move2), tl));
    REQUIRE(history.current_index() == 1); // still one logical command
    REQUIRE(history.commands().size() == 1);

    auto move3 = std::make_unique<ve::commands::MoveSegmentCommand>(s.id, track_id, track_id, tp_us(200'000), tp_us(300'000));
    REQUIRE(history.execute(std::move(move3), tl));
    REQUIRE(history.current_index() == 1);
    REQUIRE(history.commands().size() == 1);

    // Undo should restore original position (0)
    REQUIRE(history.undo(tl));
    auto* seg = track->find_segment(s.id); REQUIRE(seg);
    REQUIRE(seg->start_time.to_rational().num == 0);
}

TEST_CASE("TrimSegmentCommand coalesces rapid trims", "[commands][coalesce]") {
    ve::timeline::Timeline tl;
    auto track_id = tl.add_track(ve::timeline::Track::Video, "V");
    auto* track = tl.get_track(track_id); REQUIRE(track);
    ve::timeline::Segment s; s.id = track->generate_segment_id(); s.start_time = tp_us(500'000); s.duration = dur_us(2'000'000); s.name = "Trim"; REQUIRE(track->add_segment(s));

    ve::commands::CommandHistory history;

    auto trim1 = std::make_unique<ve::commands::TrimSegmentCommand>(s.id, tp_us(600'000), dur_us(1'900'000));
    REQUIRE(history.execute(std::move(trim1), tl));
    REQUIRE(history.commands().size() == 1);

    auto trim2 = std::make_unique<ve::commands::TrimSegmentCommand>(s.id, tp_us(650'000), dur_us(1'800'000));
    REQUIRE(history.execute(std::move(trim2), tl));
    REQUIRE(history.commands().size() == 1);

    auto trim3 = std::make_unique<ve::commands::TrimSegmentCommand>(s.id, tp_us(700'000), dur_us(1'700'000));
    REQUIRE(history.execute(std::move(trim3), tl));
    REQUIRE(history.commands().size() == 1);

    // Undo should restore original start and duration
    REQUIRE(history.undo(tl));
    auto* seg = track->find_segment(s.id); REQUIRE(seg);
    REQUIRE(seg->start_time.to_rational().num == 500'000);
    REQUIRE(seg->duration.to_rational().num == 2'000'000);
}

TEST_CASE("MoveSegmentCommand beyond merge window creates new history entry", "[commands][coalesce]") {
    ve::timeline::Timeline tl; auto track_id = tl.add_track(ve::timeline::Track::Video, "V"); auto* track = tl.get_track(track_id); REQUIRE(track);
    ve::timeline::Segment s; s.id = track->generate_segment_id(); s.start_time = tp_us(0); s.duration = dur_us(1'000'000); s.name = "Clip"; REQUIRE(track->add_segment(s));
    ve::commands::CommandHistory history;
    auto move1 = std::make_unique<ve::commands::MoveSegmentCommand>(s.id, track_id, track_id, tp_us(0), tp_us(50'000));
    REQUIRE(history.execute(std::move(move1), tl));
    REQUIRE(history.commands().size() == 1);

    // Simulate time passing beyond merge window by manipulating timestamp of last command (test-only hack)
    // We can't modify timestamp directly (protected), so instead sleep slightly over window (ensure >400ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    auto move2 = std::make_unique<ve::commands::MoveSegmentCommand>(s.id, track_id, track_id, tp_us(50'000), tp_us(60'000));
    REQUIRE(history.execute(std::move(move2), tl));
    REQUIRE(history.commands().size() == 2);
}
