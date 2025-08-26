#include <catch2/catch_test_macros.hpp>
#include "timeline/timeline.hpp"
#include "timeline/track.hpp"
#include "persistence/project_serializer.hpp"
#include <filesystem>
#include <fstream>

static std::string write_temp(const std::string& name, const std::string& content){
    auto path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream ofs(path, std::ios::binary); ofs<<content; return path; }

TEST_CASE("timeline save/load round trip basic", "[persistence]") {
    ve::timeline::Timeline tl;
    tl.set_name("Test TL");
    tl.set_frame_rate({24,1});
    auto vtrack = tl.add_track(ve::timeline::Track::Video, "Video 1");
    // fake clip
    auto src = std::make_shared<ve::timeline::MediaSource>(); src->path = "dummy.mov"; src->duration = ve::TimeDuration{1000000,1};
    auto clip_id = tl.commit_prepared_clip({src, "Clip1"});
    ve::timeline::Segment seg; seg.id = 1; seg.clip_id = clip_id; seg.start_time = ve::TimePoint{0}; seg.duration = ve::TimeDuration{500000,1};
    auto tptr = tl.get_track(vtrack); REQUIRE(tptr); REQUIRE(tptr->add_segment(seg));

    auto tmp = std::filesystem::temp_directory_path() / "ve_tl_test.json";
    auto save = ve::persistence::save_timeline_json(tl, tmp.string());
    REQUIRE(save.success);

    ve::timeline::Timeline tl2; // load into empty timeline
    auto load = ve::persistence::load_timeline_json(tl2, tmp.string());
    REQUIRE(load.success);
    // Basic assertions (frame rate restored)
    REQUIRE(tl2.frame_rate().num == 24);
    // Tracks count (>=1) and clip existence
    REQUIRE(!tl2.tracks().empty());
}

TEST_CASE("segments and clips reconstructed", "[persistence]") {
    ve::timeline::Timeline tl;
    tl.set_frame_rate({30,1});
    auto vtrack = tl.add_track(ve::timeline::Track::Video, "V1");
    auto src = std::make_shared<ve::timeline::MediaSource>(); src->path="clipA.mp4"; src->duration = ve::TimeDuration{2000000,1};
    auto clip_id = tl.commit_prepared_clip({src, "ClipA"});
    ve::timeline::Segment seg; seg.id=5; seg.clip_id=clip_id; seg.start_time=ve::TimePoint{100000}; seg.duration=ve::TimeDuration{400000,1};
    REQUIRE(tl.get_track(vtrack)->add_segment(seg));
    auto tmp = std::filesystem::temp_directory_path() / "ve_tl_seg_test.json";
    REQUIRE(ve::persistence::save_timeline_json(tl, tmp.string()).success);
    ve::timeline::Timeline tl2;
    REQUIRE(ve::persistence::load_timeline_json(tl2, tmp.string()).success);
    REQUIRE(!tl2.tracks().empty());
    bool found=false; for(auto& trU : tl2.tracks()){ for(auto& s : trU->segments()){ if(s.id==5){ found=true; break; } } }
    REQUIRE(found);
}

TEST_CASE("persistence handles unknown keys and skips them", "[persistence]") {
    std::string json = R"({"version":1,"name":"X","frame_rate":{"num":25,"den":1},"tracks":[{"id":1,"type":"video","name":"Video 1","segments":[],"extra":"ignore"}],"clips":[],"future":123})";
    ve::timeline::Timeline tl; auto path = write_temp("ve_unknown_keys.json", json);
    auto load = ve::persistence::load_timeline_json(tl, path); REQUIRE(load.success);
    REQUIRE(tl.tracks().size() == 1);
}

TEST_CASE("persistence rejects unsupported version", "[persistence]") {
    std::string json = R"({"version":42,"name":"Y","frame_rate":{"num":24,"den":1},"tracks":[],"clips":[]})";
    ve::timeline::Timeline tl; auto path = write_temp("ve_bad_version.json", json);
    auto load = ve::persistence::load_timeline_json(tl, path); REQUIRE_FALSE(load.success);
}

TEST_CASE("round trip multiple tracks non overlapping", "[persistence]") {
    ve::timeline::Timeline tl; tl.set_frame_rate({60,1}); tl.set_name("RT");
    auto v1 = tl.add_track(ve::timeline::Track::Video, "V1");
    auto v2 = tl.add_track(ve::timeline::Track::Video, "V2");
    auto src = std::make_shared<ve::timeline::MediaSource>(); src->path="a.mov"; src->duration=ve::TimeDuration{5000000,1};
    auto clipA = tl.commit_prepared_clip({src, "A"});
    ve::timeline::Segment s1; s1.clip_id=clipA; s1.start_time=ve::TimePoint{0}; s1.duration=ve::TimeDuration{1000000,1};
    ve::timeline::Segment s2; s2.clip_id=clipA; s2.start_time=ve::TimePoint{2000000}; s2.duration=ve::TimeDuration{500000,1};
    REQUIRE(tl.get_track(v1)->add_segment(s1));
    REQUIRE(tl.get_track(v2)->add_segment(s2));
    auto tmp = std::filesystem::temp_directory_path() / "ve_rt_multi.json";
    REQUIRE(ve::persistence::save_timeline_json(tl, tmp.string()).success);
    ve::timeline::Timeline tl2; REQUIRE(ve::persistence::load_timeline_json(tl2, tmp.string()).success);
    REQUIRE(tl2.tracks().size() == 2);
}

TEST_CASE("overlapping segments still rejected on add", "[timeline]") {
    ve::timeline::Timeline tl; auto v = tl.add_track(ve::timeline::Track::Video, "V1");
    auto src = std::make_shared<ve::timeline::MediaSource>(); src->path="b.mov"; src->duration=ve::TimeDuration{3000000,1};
    auto clip = tl.commit_prepared_clip({src, "B"});
    ve::timeline::Segment s1; s1.clip_id=clip; s1.start_time=ve::TimePoint{0}; s1.duration=ve::TimeDuration{2000000,1};
    ve::timeline::Segment s2; s2.clip_id=clip; s2.start_time=ve::TimePoint{1000000}; s2.duration=ve::TimeDuration{1500000,1};
    auto* tr = tl.get_track(v); REQUIRE(tr);
    REQUIRE(tr->add_segment(s1));
    REQUIRE_FALSE(tr->add_segment(s2));
}