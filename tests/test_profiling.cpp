#include <catch2/catch_test_macros.hpp>
#include "core/profiling.hpp"
#include <thread>

TEST_CASE("ScopedTimer records samples") {
    {
        VE_PROFILE_SCOPE("sleep_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto samples = ve::prof::Accumulator::instance().snapshot();
    bool found = false;
    for(auto& s : samples){ if(s.name == "sleep_test") { found = true; REQUIRE(s.ms >= 5.0); break; } }
    REQUIRE(found);
}
