#include <catch2/catch_test_macros.hpp>
#include "decode/frame_cache.hpp"

TEST_CASE("VideoFrameCache basic LRU behavior") {
    ve::decode::VideoFrameCache cache(2);
    ve::decode::VideoFrame f1; f1.pts = 10; f1.width=1; f1.height=1;
    ve::decode::VideoFrame f2; f2.pts = 20; f2.width=1; f2.height=1;
    ve::decode::VideoFrame f3; f3.pts = 30; f3.width=1; f3.height=1;

    cache.put(f1.pts, f1);
    cache.put(f2.pts, f2);
    REQUIRE(cache.get(10).has_value());
    // Access order now 10 (MRU), 20 (LRU)
    cache.put(f3.pts, f3); // should evict 20
    REQUIRE(cache.get(20) == std::nullopt);
    REQUIRE(cache.get(10).has_value());
    REQUIRE(cache.get(30).has_value());
}
