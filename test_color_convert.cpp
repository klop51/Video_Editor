#include "decode/color_convert.hpp"
#include "decode/frame.hpp"
#include "core/log.hpp"
#include <iostream>

int main() {
    // Test our color conversion system
    ve::decode::VideoFrame test_frame;
    test_frame.width = 8;
    test_frame.height = 8;
    test_frame.format = ve::decode::PixelFormat::RGB24;
    test_frame.color_space = ve::decode::ColorSpace::BT709;
    test_frame.color_range = ve::decode::ColorRange::Full;
    test_frame.data.resize(8 * 8 * 3); // RGB24 data
    
    // Fill with some test data
    for(size_t i = 0; i < test_frame.data.size(); i += 3) {
        test_frame.data[i] = 255;   // R
        test_frame.data[i+1] = 0;   // G
        test_frame.data[i+2] = 0;   // B
    }
    
    // Test conversion
    auto rgba_result = ve::decode::to_rgba(test_frame);
    if (rgba_result) {
        std::cout << "SUCCESS: to_rgba function works!" << std::endl;
        std::cout << "Converted frame size: " << rgba_result->data.size() << std::endl;
        return 0;
    } else {
        std::cout << "FAILED: to_rgba conversion failed" << std::endl;
        return 1;
    }
}
