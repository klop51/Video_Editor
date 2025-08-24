#include <catch2/catch_test_macros.hpp>
#include "decode/decoder.hpp"

TEST_CASE("Create decoder instance") {
    auto dec = ve::decode::create_decoder();
#if VE_HAVE_FFMPEG
    REQUIRE(dec != nullptr);
#else
    REQUIRE(dec == nullptr);
#endif
}
