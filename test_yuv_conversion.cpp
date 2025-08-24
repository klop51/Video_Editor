#include "decode/decoder.hpp"
#include "core/log.hpp"
#include <iostream>
#include <fstream>

// Simple YUV420P to RGB conversion function (same as in viewer_panel)
void convert_yuv420p_to_rgb(const ve::decode::VideoFrame& frame, const std::string& output_file) {
    int width = frame.width;
    int height = frame.height;
    int uv_width = width / 2;
    int uv_height = height / 2;
    
    size_t y_size = width * height;
    size_t uv_size = uv_width * uv_height;
    size_t expected_size = y_size + 2 * uv_size;
    
    if (frame.data.size() < expected_size) {
        std::cout << "ERROR: Insufficient data for YUV420P conversion" << std::endl;
        return;
    }
    
    const uint8_t* y_plane = frame.data.data();
    const uint8_t* u_plane = y_plane + y_size;
    const uint8_t* v_plane = u_plane + uv_size;
    
    // Create RGB data
    std::vector<uint8_t> rgb_data(width * height * 3);
    
    // Convert YUV to RGB using standard BT.601 coefficients
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Get Y, U, V values
            int Y = y_plane[y * width + x];
            int U = u_plane[(y/2) * uv_width + (x/2)] - 128;
            int V = v_plane[(y/2) * uv_width + (x/2)] - 128;
            
            // YUV to RGB conversion (BT.601)
            int R = Y + (1.370705 * V);
            int G = Y - (0.337633 * U) - (0.698001 * V);
            int B = Y + (1.732446 * U);
            
            // Clamp to 0-255 range
            R = std::max(0, std::min(255, R));
            G = std::max(0, std::min(255, G));
            B = std::max(0, std::min(255, B));
            
            // Set RGB pixel
            int pixel_index = (y * width + x) * 3;
            rgb_data[pixel_index + 0] = static_cast<uint8_t>(R);
            rgb_data[pixel_index + 1] = static_cast<uint8_t>(G);
            rgb_data[pixel_index + 2] = static_cast<uint8_t>(B);
        }
    }
    
    // Write simple PPM file for verification
    std::ofstream file(output_file, std::ios::binary);
    file << "P6\n" << width << " " << height << "\n255\n";
    file.write(reinterpret_cast<const char*>(rgb_data.data()), rgb_data.size());
    file.close();
    
    std::cout << "RGB conversion complete. Saved to: " << output_file << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <video_file>" << std::endl;
        return 1;
    }
    
    std::string video_file = argv[1];
    std::cout << "Testing YUV to RGB conversion with: " << video_file << std::endl;
    
    // Create decoder
    auto decoder = ve::decode::create_decoder();
    if (!decoder) {
        std::cout << "ERROR: Failed to create decoder" << std::endl;
        return 1;
    }
    
    // Open video file
    ve::decode::OpenParams params;
    params.filepath = video_file;
    params.video = true;
    params.audio = false;
    
    if (!decoder->open(params)) {
        std::cout << "ERROR: Failed to open video file: " << video_file << std::endl;
        return 1;
    }
    
    std::cout << "Successfully opened video file" << std::endl;
    
    // Read first frame
    auto frame = decoder->read_video();
    if (!frame) {
        std::cout << "ERROR: Failed to read video frame" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully read frame:" << std::endl;
    std::cout << "  Size: " << frame->width << "x" << frame->height << std::endl;
    std::cout << "  Format: " << static_cast<int>(frame->format) << std::endl;
    std::cout << "  Data size: " << frame->data.size() << " bytes" << std::endl;
    
    if (frame->format == ve::decode::PixelFormat::YUV420P) {
        std::cout << "Converting YUV420P to RGB..." << std::endl;
        convert_yuv420p_to_rgb(*frame, "test_output.ppm");
    } else {
        std::cout << "Frame is not YUV420P format" << std::endl;
    }
    
    return 0;
}
