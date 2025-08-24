#include "core/log.hpp"
#include "core/time.hpp"
#include "media_io/media_probe.hpp"
#include <iostream>
#include <filesystem>
#include <system_error>
#include <sstream>

// Placeholder media probe tool. Future: integrate FFmpeg to print stream info.
int main(int argc, char** argv) {
    using namespace ve;
    ve::log::info("Video Editor media_probe starting.");

    if(argc < 2) {
        std::cout << "Usage: ve_media_probe [--json] <media-file>\n";
        return 1;
    }

    bool json = false;
    std::string path;
    for(int i=1;i<argc;++i){
        std::string a = argv[i];
        if(a == "--json") { json = true; continue; }
        path = a; // last non-flag wins
    }
    if(path.empty()) { std::cerr << "No media file provided." << std::endl; return 1; }
    auto result = ve::media::probe_file(path);
    if(!result.success) {
#if VE_HAVE_FFMPEG
    ve::log::error(std::string("Probe failed: ") + result.error_message);
    return 2;
#else
    // Fallback: basic filesystem stats when FFmpeg is disabled
    ve::log::warn("FFmpeg disabled; showing basic file info only.");
    std::error_code ec; 
    uintmax_t sz = std::filesystem::exists(path, ec) ? std::filesystem::file_size(path, ec) : 0;
    std::cout << "File: " << path << "\n";
    std::cout << "Size: " << sz << " bytes\n";
    return 0;
#endif
    }

    if(json){
        std::ostringstream oss;
        oss << '{';
        oss << "\"file\":\"" << result.filepath << "\",";
        oss << "\"format\":\"" << result.format << "\",";
        oss << "\"size_bytes\":" << result.size_bytes << ',';
        oss << "\"duration_us\":" << result.duration_us << ',';
        oss << "\"streams\":[";
        for(size_t i=0;i<result.streams.size();++i){
            const auto& s = result.streams[i];
            oss << '{'
                << "\"type\":\"" << s.type << "\",";
            oss << "\"codec\":\"" << s.codec << "\",";
            oss << "\"bitrate\":" << s.bitrate << ',';
            oss << "\"width\":" << s.width << ',';
            oss << "\"height\":" << s.height << ',';
            oss << "\"fps\":" << s.fps << ',';
            oss << "\"channels\":" << s.channels << ',';
            oss << "\"sample_rate\":" << s.sample_rate << ',';
            oss << "\"duration_us\":" << s.duration_us << '}';
            if(i+1 < result.streams.size()) oss << ',';
        }
        oss << "]}";
        std::cout << oss.str() << std::endl;
    } else {
        std::cout << "File: " << result.filepath << "\n";
        std::cout << "Format: " << result.format << "\n";
        std::cout << "Size: " << result.size_bytes << " bytes\n";
        std::cout << "Duration(us): " << result.duration_us << "\n";
        for(const auto& s : result.streams) {
            std::cout << "  Stream: type=" << s.type
                      << " codec=" << s.codec
                      << " bitrate=" << s.bitrate
                      << " WxH=" << s.width << 'x' << s.height
                      << " fps=" << s.fps
                      << " channels=" << s.channels
                      << " sample_rate=" << s.sample_rate
                      << " duration_us=" << s.duration_us
                      << "\n";
        }
    }

    ve::log::info("Exiting media_probe.");
    return 0;
}
