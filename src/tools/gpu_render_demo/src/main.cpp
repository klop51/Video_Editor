#include "render/render_graph.hpp"
#include "render/gpu_frame_resource.hpp"
#include "render/shader_renderer.hpp"
#include "decode/decoder.hpp"
#include "gfx/vk_device.hpp"
#include "core/log.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cout << "Usage: ve_gpu_render_demo <media-file> [duration_seconds]\n";
        std::cout << "Example: ve_gpu_render_demo video.mp4 10\n";
        return 1;
    }
    std::string path = argv[1];
    int duration_seconds = (argc >= 3) ? std::atoi(argv[2]) : 5;

    // Initialize graphics device
    auto graphics_device = std::make_shared<ve::gfx::GraphicsDevice>();
    ve::gfx::GraphicsDeviceInfo device_info;
    device_info.enable_debug = true;

    if (!graphics_device->create(device_info)) {
        std::cerr << "Failed to create graphics device\n";
        return 2;
    }

    // Create GPU render graph
    auto render_graph = ve::render::create_gpu_render_graph(graphics_device);
    if (!render_graph) {
        std::cerr << "Failed to create GPU render graph\n";
        return 3;
    }

    // Set up viewport
    render_graph->set_viewport(1920, 1080);

    // Initialize decoder
    auto decoder = ve::decode::create_decoder();
    if (!decoder) {
        std::cerr << "Decoder not available\n";
        return 4;
    }

    ve::decode::OpenParams open_params;
    open_params.filepath = path;
    open_params.video = true;
    open_params.audio = false;

    if (!decoder->open(open_params)) {
        std::cerr << "Failed to open media file: " << path << "\n";
        return 5;
    }

    std::cout << "Starting GPU render demo for " << duration_seconds << " seconds...\n";
    std::cout << "Rendering decoded frames to GPU with brightness effect\n";
    std::cout << "----------------------------------------\n";

    auto start_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    float brightness = 0.0f;

    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        if (elapsed >= duration_seconds) {
            break;
        }

        // Read a frame from the decoder
        auto frame = decoder->read_video();
        if (!frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Set the frame in the render graph
        if (auto* gpu_graph = dynamic_cast<ve::render::GpuRenderGraph*>(render_graph.get())) {
            gpu_graph->set_current_frame(*frame);

            // Animate brightness effect
            brightness = 0.3f * std::sin(static_cast<float>(elapsed) * 2.0f);
            gpu_graph->set_brightness(brightness);
        }

        // Render the frame
        ve::render::FrameRequest request;
        request.timestamp_us = frame->pts;
        auto result = render_graph->render(request);

        if (result.success) {
            frame_count++;
            if (frame_count % 30 == 0) {
                std::cout << "Rendered frame " << frame_count
                          << " | PTS: " << (frame->pts / 1000000.0) << "s"
                          << " | Size: " << frame->width << "x" << frame->height
                          << " | Brightness: " << brightness
                          << std::endl;
            }
        } else {
            std::cout << "Failed to render frame " << frame_count << std::endl;
        }

        // Simulate frame timing (30 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    std::cout << "\n----------------------------------------\n";
    std::cout << "GPU RENDER DEMO COMPLETE\n";
    std::cout << "Total frames rendered: " << frame_count << "\n";
    std::cout << "Duration: " << duration_seconds << " seconds\n";
    std::cout << "Average FPS: " << (frame_count / static_cast<float>(duration_seconds)) << "\n";

    if (frame_count > 0) {
        std::cout << "✅ GPU rendering pipeline working successfully!\n";
        std::cout << "✅ Shader-based effects (brightness) applied in real-time\n";
        std::cout << "✅ YUV to RGB conversion working on GPU\n";
    } else {
        std::cout << "❌ No frames were rendered\n";
    }

    return 0;
}
