#include <catch2/catch_test_macros.hpp>
#include "decode/color_convert.hpp"

// Regression tests for stride/size guard logic and undersized frame detection
TEST_CASE("color_convert undersized source detection does not crash") {
    using namespace ve::decode;
    VideoFrame f; f.width = 16; f.height = 16; f.format = PixelFormat::YUV420P; f.pts = 42;
    // Intentionally undersized: real need = 16*16 + 2*(8*8) = 256 + 2*64 = 384 bytes
    f.data.resize(100, 0x55);
    // Should log an error internally but still not crash; conversion may proceed or fail gracefully.
    auto out = to_rgba(f);
    // Either nullopt (refusing) or produced RGBA of expected dimensions is acceptable; just ensure no UB.
    if (out) {
        REQUIRE(out->width == 16);
        REQUIRE(out->height == 16);
        REQUIRE(out->format == PixelFormat::RGBA32);
    }
}

TEST_CASE("color_convert valid minimal yuv420p size succeeds") {
    using namespace ve::decode;
    VideoFrame f; f.width = 2; f.height = 2; f.format = PixelFormat::YUV420P; f.pts = 7;
    // Y plane 4, U plane 1, V plane 1
    f.data = { 10, 20, 30, 40, 128, 128 };
    auto out = to_rgba(f);
    REQUIRE(out);
    REQUIRE(out->data.size() == 2*2*4);
}
