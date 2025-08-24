#include "decode/playback_controller.hpp"
#include "decode/decoder.hpp"
#include "core/log.hpp"
#include <iostream>
#include <atomic>
#include <thread>

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cout << "Usage: ve_playback_demo <media-file>\n";
        return 1;
    }
    std::string path = argv[1];

    auto decoder = ve::decode::create_decoder();
    if(!decoder) {
        std::cerr << "Decoder not available (FFmpeg disabled).\n";
        return 2;
    }

    ve::decode::PlaybackController controller(std::move(decoder));
    controller.set_media_path(path);

    std::atomic<int> frames{0};
    if(!controller.start(0, [&](const ve::decode::VideoFrame& vf){
        frames++;
        if(frames % 30 == 0) {
            std::cout << "Frame pts(us)=" << vf.pts << " size=" << vf.width << "x" << vf.height << "\n";
        }
    })) {
        std::cerr << "Failed to start playback controller." << std::endl;
        return 3;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    controller.stop();

    std::cout << "Decoded frames: " << frames.load() << "\n";
    return 0;
}
