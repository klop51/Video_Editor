#include <catch2/catch_test_macros.hpp>
#include "core/time.hpp"

TEST_CASE("Time rational to ticks conversion", "[time]") {
    using namespace ve;
    auto t = make_time(48000, 48000); // 1 second
    REQUIRE(to_ticks(t) == TICKS_PER_SECOND);
}

TEST_CASE("Make time handles negative denominator", "[time]") {
    using namespace ve;
    auto t = make_time(1, -2);
    REQUIRE(t.num == -1);
    REQUIRE(t.den == 2);
}

TEST_CASE("Make time reduces fractions", "[time]") {
    using namespace ve;
    auto t = make_time(2, 4);
    REQUIRE(t.num == 1);
    REQUIRE(t.den == 2);
}

TEST_CASE("Format timecode basic", "[time]") {
    using namespace ve;
    auto t = make_time(48000 * 3661, 48000); // 1h 1m 1s
    auto tc = format_timecode(t, 24, 1);
    REQUIRE(tc.rfind("01:01:01", 0) == 0);
}

TEST_CASE("Normalize rational", "[time]") {
    using namespace ve;
    TimeRational r{120, 480};
    auto n = normalize(r);
    REQUIRE(n.num == 1);
    REQUIRE(n.den == 4); // 120/480 reduces to 1/4

    TimeRational r2{-300, -600}; // double negative
    auto n2 = normalize(r2);
    REQUIRE(n2.num == 1);
    REQUIRE(n2.den == 2);

    TimeRational r3{0, 500};
    auto n3 = normalize(r3);
    REQUIRE(n3.num == 0);
    REQUIRE(n3.den == 1); // canonical zero
}
