#include <catch2/catch_test_macros.hpp>
#include "decode/color_convert.hpp"

TEST_CASE("YUV420P to RGBA conversion") {
    using namespace ve::decode;
    VideoFrame src; src.width=2; src.height=2; src.format=PixelFormat::YUV420P; src.pts=1000;
    // Build minimal YUV420 frame: Y(4 bytes) U(1) V(1)
    src.data = {128,128,128,128, 128, 128};
    auto out = to_rgba(src);
    REQUIRE(out.has_value());
    REQUIRE(out->format == PixelFormat::RGBA32);
    REQUIRE(out->data.size() == 2*2*4);
}
