#include <catch2/catch_test_macros.hpp>
#include "media_io/media_probe.hpp"
#include <fstream>

TEST_CASE("Probe handles missing file gracefully") {
    auto res = ve::media::probe_file("this_file_should_not_exist_12345.xyz");
    REQUIRE_FALSE(res.success); // Should indicate failure
}
