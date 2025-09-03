#include "decode/playback_controller.hpp"
#include "decode/decoder.hpp"
#include "decode/color_convert.hpp"
#include "core/log.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <iomanip>

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cout << "Usage: ve_playback_demo <media-file> [duration_seconds]\n";
        std::cout << "Example: ve_playback_demo video.mp4 10\n";
        return 1;
    }
    std::string path = argv[1];
    int duration_seconds = (argc >= 3) ? std::atoi(argv[2]) : 10;

    auto decoder = ve::decode::create_decoder();
    if(!decoder) {
        std::cerr << "Decoder not available (FFmpeg disabled).\n";
        return 2;
    }

    ve::decode::PlaybackController controller(std::move(decoder));
    controller.set_media_path(path);

    // Performance tracking variables
    std::atomic<int> frames_decoded{0};
    std::atomic<int> frames_displayed{0};
    std::atomic<int64_t> last_frame_pts{0};
    std::atomic<int64_t> frame_drops{0};
    auto start_time = std::chrono::steady_clock::now();
    auto last_fps_time = start_time;

    if(!controller.start(0, [&](const ve::decode::VideoFrame& vf){
        frames_displayed++;
        int64_t current_pts = vf.pts;
        int64_t expected_pts = last_frame_pts.load() + 33333; // ~30fps in microseconds

        // Detect frame drops (simple heuristic)
        if (last_frame_pts.load() > 0 && current_pts > expected_pts + 50000) { // 50ms tolerance
            frame_drops++;
            ve::log::warn("Frame drop detected! Expected: " + std::to_string(expected_pts) +
                         "us, Got: " + std::to_string(current_pts) + "us");
        }

        last_frame_pts.store(current_pts);

        // Frame already converted to RGBA by PlaybackController
        frames_decoded++;

        // FPS calculation every 30 frames
        if(frames_displayed % 30 == 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
            double fps = 30.0 / (elapsed / 1000.0);

            std::cout << std::fixed << std::setprecision(1)
                      << "Frame " << frames_displayed.load()
                      << " | PTS: " << (vf.pts / 1000000.0) << "s"
                      << " | Size: " << vf.width << "x" << vf.height
                      << " | Format: " << static_cast<int>(vf.format)
                      << " | FPS: " << fps
                      << " | Drops: " << frame_drops.load()
                      << " | RGBA: " << frames_decoded.load()
                      << std::endl;

            last_fps_time = now;
        }
    })) {
        std::cerr << "Failed to start playback controller." << std::endl;
        return 3;
    }

    std::cout << "Starting playback for " << duration_seconds << " seconds...\n";
    std::cout << "Monitoring performance: FPS, frame drops, timing\n";
    std::cout << "----------------------------------------\n";

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    controller.stop();

    // Final performance summary
    auto end_time = std::chrono::steady_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double avg_fps = (frames_displayed.load() * 1000.0) / total_elapsed;

    std::cout << "\n----------------------------------------\n";
    std::cout << "PERFORMANCE SUMMARY:\n";
    std::cout << "Total frames displayed: " << frames_displayed.load() << "\n";
    std::cout << "Total frames converted: " << frames_decoded.load() << "\n";
    std::cout << "Total frame drops: " << frame_drops.load() << "\n";
    std::cout << "Average FPS: " << std::fixed << std::setprecision(2) << avg_fps << "\n";
    std::cout << "Playback duration: " << (total_elapsed / 1000.0) << " seconds\n";

    if (avg_fps >= 28.0) {
        std::cout << "✅ EXCELLENT: Achieved target 28+ FPS for software decoding!\n";
    } else if (avg_fps >= 24.0) {
        std::cout << "✅ GOOD: Achieved 24+ FPS, acceptable for software decoding\n";
    } else {
        std::cout << "⚠️  SLOW: Below 24 FPS - may need optimization\n";
    }

    return 0;
}
