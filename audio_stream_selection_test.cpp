#include <iostream>
#include <memory>
#include "src/decode/include/decode/video_decoder_ffmpeg.hpp"
#include "src/core/include/core/log.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <video_file>" << std::endl;
        return 1;
    }

    // Set up basic logging
    ve::log::set_sink([](ve::log::Level lvl, const std::string& msg) {
        std::cout << "[LOG] " << msg << std::endl;
    });

    try {
        ve::decode::VideoDecoderFFmpeg decoder;
        ve::decode::VideoDecoderParams params{};
        params.video = true;
        params.audio = true;
        
        std::cout << "=== Audio Stream Selection Test ===" << std::endl;
        std::cout << "Testing file: " << argv[1] << std::endl;
        
        if (decoder.open(argv[1], params)) {
            std::cout << "✓ Successfully opened video file" << std::endl;
            std::cout << "✓ Audio stream selection logic executed" << std::endl;
            std::cout << "✓ Check logs above for stream selection details" << std::endl;
        } else {
            std::cout << "✗ Failed to open video file" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "=== Test Complete ===" << std::endl;
    return 0;
}