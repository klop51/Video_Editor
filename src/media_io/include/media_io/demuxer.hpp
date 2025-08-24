#pragma once
#include "core/time.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ve::media_io {

struct StreamInfo {
    int index;
    enum Type { Video, Audio, Subtitle } type;
    std::string codec_name;
    
    // Video specific
    int width = 0, height = 0;
    ve::TimeRational frame_rate{};
    std::string pixel_format;
    
    // Audio specific  
    int sample_rate = 0, channels = 0;
    std::string sample_format;
    
    // Common
    ve::TimeDuration duration{};
    int64_t bit_rate = 0;
};

struct Packet {
    int stream_index;
    ve::TimePoint pts, dts;
    std::vector<uint8_t> data;
    bool is_keyframe = false;
    size_t size() const { return data.size(); }
};

class Demuxer {
public:
    static std::unique_ptr<Demuxer> create(const std::string& path);
    
    virtual ~Demuxer() = default;
    
    virtual bool open(const std::string& path) = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;
    
    virtual std::vector<StreamInfo> get_streams() const = 0;
    virtual ve::TimeDuration get_duration() const = 0;
    virtual bool seek(ve::TimePoint timestamp) = 0;
    virtual bool read_packet(Packet& packet) = 0;
    
    virtual std::string get_format_name() const = 0;
    virtual std::string get_metadata(const std::string& key) const = 0;
    
protected:
    Demuxer() = default;
};

} // namespace ve::media_io
