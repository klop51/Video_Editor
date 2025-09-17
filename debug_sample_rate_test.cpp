/**
 * @file debug_sample_rate_test.cpp
 * @brief Quick test to debug baby talk audio issue - sample rate detection
 * 
 * This simple test will help us understand the sample rate mismatch causing
 * the "baby talk" audio where voices sound garbled like baby babbling.
 */

#include <iostream>
#include <string>

// Include minimal video editor components
#include "src/media_io/include/media_io/media_probe.hpp"
#include "src/core/include/core/log.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <video_file>" << std::endl;
        std::cout << "This tool will analyze the sample rate of your video to debug baby talk audio" << std::endl;
        return 1;
    }

    std::string video_path = argv[1];
    std::cout << "=== Baby Talk Audio Debug Tool ===" << std::endl;
    std::cout << "Analyzing: " << video_path << std::endl;
    std::cout << std::endl;

    try {
        // Probe the media file
        auto result = ve::media::probe_file(video_path);
        
        if (!result.success) {
            std::cout << "❌ Failed to probe file: " << result.error_message << std::endl;
            return 1;
        }
        
        std::cout << "Media File Analysis:" << std::endl;
        std::cout << "- Duration: " << (result.duration_us / 1000000.0) << " seconds" << std::endl;
        std::cout << "- Stream count: " << result.streams.size() << std::endl;
        std::cout << "- Format: " << result.format << std::endl;
        std::cout << std::endl;

        bool found_audio = false;
        bool found_video = false;

        for (size_t i = 0; i < result.streams.size(); ++i) {
            const auto& stream = result.streams[i];
            std::cout << "Stream " << i << " (" << stream.type << "):" << std::endl;
            
            if (stream.type == "audio") {
                found_audio = true;
                std::cout << "  AUDIO DETAILS (Baby Talk Debug):" << std::endl;
                std::cout << "  - Sample Rate: " << stream.sample_rate << " Hz" << std::endl;
                std::cout << "  - Channels: " << stream.channels << std::endl;
                std::cout << "  - Bitrate: " << stream.bitrate << " bps" << std::endl;
                std::cout << "  - Codec: " << stream.codec << std::endl;
                
                // Analyze common problematic sample rates
                if (stream.sample_rate == 44100) {
                    std::cout << "  ⚠️  POTENTIAL ISSUE: 44.1kHz audio" << std::endl;
                    std::cout << "      If WASAPI is set to 48kHz, this causes baby talk!" << std::endl;
                    std::cout << "      44.1kHz played at 48kHz = 1.088x faster = higher pitch" << std::endl;
                } else if (stream.sample_rate == 48000) {
                    std::cout << "  ✅  Standard 48kHz - should work well with WASAPI" << std::endl;
                } else if (stream.sample_rate == 22050 || stream.sample_rate == 11025) {
                    std::cout << "  ⚠️  LOW SAMPLE RATE: " << stream.sample_rate << " Hz" << std::endl;
                    std::cout << "      This will definitely cause baby talk if upsampled incorrectly!" << std::endl;
                } else {
                    std::cout << "  ⚠️  UNUSUAL SAMPLE RATE: " << stream.sample_rate << " Hz" << std::endl;
                    std::cout << "      This may require special handling in WASAPI" << std::endl;
                }
            } else if (stream.type == "video") {
                found_video = true;
                std::cout << "  - Resolution: " << stream.width << "x" << stream.height << std::endl;
                std::cout << "  - FPS: " << stream.fps << std::endl;
                std::cout << "  - Codec: " << stream.codec << std::endl;
            }
            std::cout << std::endl;
        }

        // Diagnosis
        std::cout << "=== BABY TALK DIAGNOSIS ===" << std::endl;
        if (!found_audio) {
            std::cout << "❌ NO AUDIO STREAM FOUND - This video has no audio" << std::endl;
        } else if (!found_video) {
            std::cout << "ℹ️  AUDIO-ONLY FILE" << std::endl;
        } else {
            std::cout << "✅ Audio + Video file detected" << std::endl;
        }

        std::cout << std::endl;
        std::cout << "=== RECOMMENDATIONS ===" << std::endl;
        std::cout << "1. Ensure WASAPI uses the EXACT same sample rate as the video" << std::endl;
        std::cout << "2. Common baby talk cause: 44.1kHz audio played through 48kHz output" << std::endl;
        std::cout << "3. Solution: Configure WASAPI to match video sample rate exactly" << std::endl;
        std::cout << "4. Alternative: Use proper sample rate conversion (not pitch shifting)" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "❌ Error analyzing file: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}