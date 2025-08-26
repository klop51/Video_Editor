// Additional command and robustness tests
#include <catch2/catch_test_macros.hpp>
#include <random>
#include "commands/timeline_commands.hpp"
#include "timeline/timeline.hpp"
#include "persistence/project_serializer.hpp"
#include <filesystem>

static ve::TimePoint tp_us(int64_t us){ return ve::TimePoint{us}; }
static ve::TimeDuration dur_us(int64_t us){ return ve::TimeDuration{us}; }

TEST_CASE("MacroCommand rollback on partial failure", "[commands][macro]") {
    ve::timeline::Timeline tl;
    auto t1 = tl.add_track(ve::timeline::Track::Video, "V1");
    auto* track = tl.get_track(t1); REQUIRE(track);

    // Base segment occupying 0-2s
    ve::timeline::Segment base; base.id = track->generate_segment_id(); base.start_time=tp_us(0); base.duration=dur_us(2'000'000); REQUIRE(track->add_segment(base));

    // Prepare macro: split base at 1s then insert overlapping segment (should fail) so macro rolls back
    ve::commands::MacroCommand macro("split then invalid insert");
    macro.add_command(std::make_unique<ve::commands::SplitSegmentCommand>(base.id, tp_us(1'000'000)));
    // Construct an overlapping insert for first half range 0-1s so it collides after split
    ve::timeline::Segment overlap; overlap.id = track->generate_segment_id(); overlap.start_time=tp_us(500'000); overlap.duration=dur_us(500'000);
    macro.add_command(std::make_unique<ve::commands::InsertSegmentCommand>(t1, overlap, overlap.start_time));

    bool ok = macro.execute(tl); // execute returns [[nodiscard]]; we store it to satisfy compiler
    REQUIRE_FALSE(ok); // should fail overall
    // Original timeline must remain with single base segment intact
    REQUIRE(track->segments().size() == 1);
    auto* still = track->find_segment(base.id);
    REQUIRE(still);
    REQUIRE(still->duration.to_rational().num == 2'000'000);
}

TEST_CASE("Randomized add/move/remove preserves non-overlap", "[timeline][fuzz]") {
    ve::timeline::Timeline tl; auto tid = tl.add_track(ve::timeline::Track::Video, "V"); auto* track = tl.get_track(tid); REQUIRE(track);
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<int> action_dist(0,2); // 0 add,1 move,2 remove
    std::uniform_int_distribution<int> start_step(0,199); // *10ms
    std::uniform_int_distribution<int> len_step(1,20);

    struct FuzzStats { int adds=0; int add_success=0; int moves=0; int move_success=0; int removes=0; int remove_success=0; } stats; // simple counters to ensure we use results

    for(int i=0;i<400;i++){
        int act = action_dist(rng);
        if(act==0){
            ve::timeline::Segment s; s.start_time = tp_us(int64_t(start_step(rng))*10'000); s.duration = dur_us(int64_t(len_step(rng))*10'000);
            bool added_ok = track->add_segment(s); // failure due to overlap is fine
            ++stats.adds; if(added_ok) ++stats.add_success;
        } else if(act==1 && !track->segments().empty()){
            auto& segs = track->segments();
            size_t idx = rng() % segs.size();
            auto id = segs[idx].id;
            ve::TimePoint newStart = tp_us(int64_t(start_step(rng))*10'000);
            bool moved_ok = track->move_segment(id, newStart); // may fail; acceptable
            ++stats.moves; if(moved_ok) ++stats.move_success;
        } else if(act==2 && !track->segments().empty()){
            auto& segs = track->segments();
            size_t idx = rng() % segs.size();
            auto id = segs[idx].id;
            bool removed_ok = track->remove_segment(id);
            ++stats.removes; if(removed_ok) ++stats.remove_success;
        }
    }
    // Basic sanity: we performed operations
    REQUIRE(stats.adds + stats.moves + stats.removes > 0);
    REQUIRE(ve::timeline::track_is_sorted(*track));
    REQUIRE(track->is_non_overlapping());
}

TEST_CASE("Persistence round trip complex project", "[persistence][roundtrip]") {
    ve::timeline::Timeline tl; tl.set_name("Complex"); tl.set_frame_rate({48,1});
    auto v1 = tl.add_track(ve::timeline::Track::Video, "V1");
    auto a1 = tl.add_track(ve::timeline::Track::Audio, "A1");
    auto src = std::make_shared<ve::timeline::MediaSource>(); src->path="media.mov"; src->duration=dur_us(5'000'000);
    auto clip = tl.commit_prepared_clip({src, "Media"});
    ve::timeline::Segment s1; s1.clip_id=clip; s1.start_time=tp_us(0); s1.duration=dur_us(1'500'000);
    ve::timeline::Segment s2; s2.clip_id=clip; s2.start_time=tp_us(2'000'000); s2.duration=dur_us(1'000'000);
    ve::timeline::Segment aSeg; aSeg.clip_id=clip; aSeg.start_time=tp_us(0); aSeg.duration=dur_us(2'500'000);
    REQUIRE(tl.get_track(v1)->add_segment(s1));
    REQUIRE(tl.get_track(v1)->add_segment(s2));
    REQUIRE(tl.get_track(a1)->add_segment(aSeg));

    auto tmp = std::filesystem::temp_directory_path() / "ve_complex_rt.json";
    auto save = ve::persistence::save_timeline_json(tl, tmp.string()); // capture result (nodiscard)
    REQUIRE(save.success);
    ve::timeline::Timeline loaded;
    auto load = ve::persistence::load_timeline_json(loaded, tmp.string()); // capture result (nodiscard)
    REQUIRE(load.success);
    REQUIRE(loaded.frame_rate().num == 48);
    REQUIRE(loaded.tracks().size() == 2);
    // Count segments and ensure ordering
    size_t segCount = 0; for(auto& trU : loaded.tracks()){ REQUIRE(ve::timeline::track_is_sorted(*trU)); REQUIRE(trU->is_non_overlapping()); segCount += trU->segments().size(); }
    REQUIRE(segCount >= 3);
}
