#pragma once
#include "core/time.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace ve::timeline {

using ClipId = uint64_t;
using TrackId = uint64_t;
using SegmentId = uint64_t;

struct MediaSource {
    std::string path;
    std::string hash;  // For relink detection
    ve::TimeDuration duration{};
    
    // Cached metadata
    int width = 0, height = 0;
    ve::TimeRational frame_rate{};
    int sample_rate = 0, channels = 0;
    
    std::string format_name;
    std::unordered_map<std::string, std::string> metadata;
};

struct MediaClip {
    ClipId id;
    std::shared_ptr<MediaSource> source;
    ve::TimePoint in_time{};   // Source timecode in
    ve::TimePoint out_time{};  // Source timecode out  
    std::string name;
    
    ve::TimeDuration duration() const {
        return ve::TimeDuration{out_time.to_rational().num - in_time.to_rational().num, 
                               out_time.to_rational().den};
    }
};

struct Segment {
    SegmentId id;
    ClipId clip_id;
    ve::TimePoint start_time{};    // Timeline position
    ve::TimeDuration duration{};   // Duration on timeline (may differ from clip due to speed)
    
    // Per-instance properties
    double speed = 1.0;
    bool enabled = true;
    std::string name;
    
    ve::TimePoint end_time() const {
        return ve::TimePoint{start_time.to_rational().num + duration.to_rational().num,
                            start_time.to_rational().den};
    }
};

class Track {
public:
    enum Type { Video, Audio };
    
    Track(TrackId id, Type type, const std::string& name = "");
    ~Track() = default;
    
    // Properties
    TrackId id() const { return id_; }
    Type type() const { return type_; }
    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    
    bool is_muted() const { return muted_; }
    void set_muted(bool muted) { muted_ = muted; }
    
    bool is_solo() const { return solo_; }
    void set_solo(bool solo) { solo_ = solo; }
    
    // Segment management
    bool add_segment(const Segment& segment);
    bool remove_segment(SegmentId segment_id);
    bool move_segment(SegmentId segment_id, ve::TimePoint new_start);
    Segment* find_segment(SegmentId segment_id);
    const Segment* find_segment(SegmentId segment_id) const;
    
    // Query operations
    std::vector<Segment*> get_segments_in_range(ve::TimePoint start, ve::TimePoint end);
    std::vector<const Segment*> get_segments_in_range(ve::TimePoint start, ve::TimePoint end) const;
    Segment* get_segment_at_time(ve::TimePoint time);
    const Segment* get_segment_at_time(ve::TimePoint time) const;
    
    const std::vector<Segment>& segments() const { return segments_; }
    
    // Edit operations
    bool insert_gap(ve::TimePoint at, ve::TimeDuration duration);
    bool delete_range(ve::TimePoint start, ve::TimeDuration duration, bool ripple = true);
    bool split_segment(SegmentId segment_id, ve::TimePoint split_time);
    
    // ID generation
    SegmentId generate_segment_id() { return next_segment_id_++; }
    
private:
    TrackId id_;
    Type type_;
    std::string name_;
    bool muted_ = false;
    bool solo_ = false;
    
    std::vector<Segment> segments_;  // Always sorted by start_time
    SegmentId next_segment_id_ = 1;
    
    void sort_segments();
    bool validate_no_overlap() const;
    size_t find_segment_index(SegmentId segment_id) const;
};

} // namespace ve::timeline
