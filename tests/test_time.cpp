#include <catch2/catch_test_macros.hpp>
#include "core/time.hpp"

TEST_CASE("Time rational to ticks conversion", "[time]") {
    using namespace ve;
    auto t = make_time(48000, 48000); // 1 second
    REQUIRE(to_ticks(t) == TICKS_PER_SECOND);
}

TEST_CASE("Format timecode basic", "[time]") {
    using namespace ve;
    auto t = make_time(48000 * 3661, 48000); // 1h 1m 1s
    auto tc = format_timecode(t, 24, 1);
    REQUIRE(tc.rfind("01:01:01", 0) == 0);
}
