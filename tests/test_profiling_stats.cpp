#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "core/profiling.hpp"
#include <thread>
#include <fstream>

// New filename to invalidate any stale build cache issues from prior test_profiling.cpp
static_assert(true, "profiling stats tests active file identifier");

static inline bool ve_approx(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

TEST_CASE("ScopedTimer records samples") {
    ve::prof::Accumulator::instance().clear();
    {
        VE_PROFILE_SCOPE("sleep_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto samples = ve::prof::Accumulator::instance().snapshot();
    bool found = false;
    for(auto& s : samples){ if(s.name == "sleep_test") { found = true; REQUIRE(s.ms >= 1.0); break; } }
    REQUIRE(found);
}

TEST_CASE("profiling stats basic ordering and bounds", "[profiling]") {
    using namespace ve::prof;
    Accumulator::instance().clear();
    Accumulator::instance().add({"task", 10.0});
    Accumulator::instance().add({"task", 20.0});
    Accumulator::instance().add({"task", 30.0});
    Accumulator::instance().add({"task", 40.0});
    auto agg = Accumulator::instance().aggregate();
    REQUIRE(agg.contains("task"));
    auto st = agg["task"];
    REQUIRE(st.count == 4);
    REQUIRE(ve_approx(st.min_ms, 10.0));
    REQUIRE(ve_approx(st.max_ms, 40.0));
    REQUIRE(ve_approx(st.total_ms, 100.0));
    REQUIRE(ve_approx(st.avg_ms, 25.0));
    REQUIRE(st.min_ms <= st.p50_ms);
    REQUIRE(st.p50_ms <= st.p95_ms);
    REQUIRE(st.p95_ms <= st.max_ms);
}

TEST_CASE("profiling percentiles single sample", "[profiling]") {
    using namespace ve::prof;
    Accumulator::instance().clear();
    Accumulator::instance().add({"one", 7.5});
    auto agg = Accumulator::instance().aggregate();
    auto st = agg["one"];
    REQUIRE(st.count == 1);
    REQUIRE(ve_approx(st.min_ms, 7.5));
    REQUIRE(ve_approx(st.max_ms, 7.5));
    REQUIRE(ve_approx(st.p50_ms, 7.5));
    REQUIRE(ve_approx(st.p95_ms, 7.5));
}

TEST_CASE("profiling percentiles two samples midpoint and high", "[profiling]") {
    using namespace ve::prof;
    Accumulator::instance().clear();
    Accumulator::instance().add({"two", 10.0});
    Accumulator::instance().add({"two", 30.0});
    auto st = Accumulator::instance().aggregate()["two"];
    REQUIRE(st.count == 2);
    REQUIRE(ve_approx(st.p50_ms, 10.0)); // floor(0.5*(2-1)) -> 0
    REQUIRE(ve_approx(st.p95_ms, 10.0)); // floor(0.95*(2-1)) -> 0
    REQUIRE(ve_approx(st.max_ms, 30.0));
}

TEST_CASE("profiling percentiles skewed distribution", "[profiling]") {
    using namespace ve::prof;
    Accumulator::instance().clear();
    for(int i=0;i<19;++i) Accumulator::instance().add({"skew", 1.0});
    Accumulator::instance().add({"skew", 100.0});
    auto st = Accumulator::instance().aggregate()["skew"];
    REQUIRE(st.count == 20);
    REQUIRE(ve_approx(st.p95_ms, 1.0)); // floor(0.95*(20-1)) = 18
    REQUIRE(ve_approx(st.max_ms, 100.0));
}

TEST_CASE("profiling json write", "[profiling]") {
    using namespace ve::prof;
    Accumulator::instance().clear();
    Accumulator::instance().add({"io", 5.0});
    auto path = "profiling_test_output.json";
    REQUIRE(Accumulator::instance().write_json(path));
    std::ifstream ifs(path);
    REQUIRE(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    REQUIRE(content.find("\"samples\"") != std::string::npos);
    REQUIRE(content.find("\"io\"") != std::string::npos);
}
