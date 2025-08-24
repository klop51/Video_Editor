#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace ve::media {

struct StreamInfo {
    std::string codec;
    std::string type; // video/audio/other
    int64_t bitrate = 0;
    int32_t width = 0;
    int32_t height = 0;
    double fps = 0.0;
    int64_t duration_us = 0; // microseconds
    int64_t channels = 0;
    int64_t sample_rate = 0;
};

struct ProbeResult {
    std::string filepath;
    int64_t size_bytes = 0;
    int64_t duration_us = 0;
    std::vector<StreamInfo> streams;
    std::string format;
    bool success = false;
    std::string error_message;
};

ProbeResult probe_file(const std::string& path) noexcept;

} // namespace ve::media
