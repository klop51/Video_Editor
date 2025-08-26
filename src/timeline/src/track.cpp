#include "timeline/track.hpp"
#include "core/log.hpp"
#include <algorithm>

namespace ve::timeline {

Track::Track(TrackId id, Type type, const std::string& name)
    : id_(id), type_(type), name_(name.empty() ? (type == Video ? "Video" : "Audio") : name) {
}

bool Track::add_segment(const Segment& segment) {
#ifdef VE_TIMELINE_DEBUG
    ve::log::debug("[Track::add_segment] incoming segment id=" + std::to_string(segment.id) +
                   " start=" + std::to_string(segment.start_time.to_rational().num) +
                   " dur=" + std::to_string(segment.duration.to_rational().num));
#endif
    // Check for overlaps
    for (const auto& existing : segments_) {
        ve::TimePoint existing_end = existing.end_time();
        ve::TimePoint new_end = segment.end_time();
        
        if (!(segment.start_time >= existing_end || new_end <= existing.start_time)) {
            ve::log::warn("Cannot add segment: overlaps with existing segment");
            return false;
        }
    }
    
    Segment new_segment = segment;
    if (new_segment.id == 0) {
        new_segment.id = next_segment_id_++;
    } else {
        // Keep next_segment_id_ ahead of any explicitly provided IDs
        if (new_segment.id >= next_segment_id_) {
            next_segment_id_ = new_segment.id + 1;
        }
    }
    
    segments_.push_back(new_segment);
    last_added_segment_id_ = new_segment.id;
    sort_segments();
    // Dump segments post-add
    #ifdef VE_TIMELINE_DEBUG
    std::string dump = "[Track::add_segment] segments now:";
    for (const auto& s : segments_) {
        dump += " (id=" + std::to_string(s.id) + " st=" + std::to_string(s.start_time.to_rational().num) +
                " end=" + std::to_string(s.end_time().to_rational().num) + ")";
    }
    ve::log::debug(dump);
    #endif
    
    return true;
}

SegmentId Track::add_segment_get_id(const Segment& segment) {
    if (!add_segment(segment)) return 0;
    return last_added_segment_id_;
}

bool Track::remove_segment(SegmentId segment_id) {
    size_t index = find_segment_index(segment_id);
    if (index >= segments_.size()) {
        return false;
    }
    
    segments_.erase(segments_.begin() + index);
    return true;
}

bool Track::move_segment(SegmentId segment_id, ve::TimePoint new_start) {
#ifdef VE_TIMELINE_DEBUG
    ve::log::debug("[Track::move_segment] request id=" + std::to_string(segment_id) +
                   " new_start=" + std::to_string(new_start.to_rational().num));
#endif
    size_t index = find_segment_index(segment_id);
    if (index >= segments_.size()) {
        ve::log::debug("[Track::move_segment] segment not found");
        return false;
    }
    
    Segment& segment = segments_[index];
    ve::TimePoint old_start = segment.start_time;
    segment.start_time = new_start;
    
    // Log pre-move layout
    #ifdef VE_TIMELINE_DEBUG
    {
        std::string before = "[Track::move_segment] layout before overlap check:";
        for (const auto& s : segments_) {
            before += " (id=" + std::to_string(s.id) + " st=" + std::to_string(s.start_time.to_rational().num) +
                      " end=" + std::to_string(s.end_time().to_rational().num) + ")";
        }
        ve::log::debug(before);
    }
    #endif
    // Check for overlaps with other segments
    for (size_t i = 0; i < segments_.size(); ++i) {
        if (i == index) continue;
        
        const auto& other = segments_[i];
        ve::TimePoint other_end = other.end_time();
        ve::TimePoint segment_end = segment.end_time();
        
        if (!(segment.start_time >= other_end || segment_end <= other.start_time)) {
            // Revert the move
            segment.start_time = old_start;
            ve::log::warn("Cannot move segment: would overlap with existing segment");
            return false;
        }
    }
    
    sort_segments();
    #ifdef VE_TIMELINE_DEBUG
    // Log after successful move
    {
        std::string after = "[Track::move_segment] layout after move:";
        for (const auto& s : segments_) {
            after += " (id=" + std::to_string(s.id) + " st=" + std::to_string(s.start_time.to_rational().num) +
                     " end=" + std::to_string(s.end_time().to_rational().num) + ")";
        }
        ve::log::debug(after);
    }
    #endif
    return true;
}

Segment* Track::find_segment(SegmentId segment_id) {
    size_t index = find_segment_index(segment_id);
    return index < segments_.size() ? &segments_[index] : nullptr;
}

const Segment* Track::find_segment(SegmentId segment_id) const {
    size_t index = find_segment_index(segment_id);
    return index < segments_.size() ? &segments_[index] : nullptr;
}

std::vector<Segment*> Track::get_segments_in_range(ve::TimePoint start, ve::TimePoint end) {
    std::vector<Segment*> result;
    
    for (auto& segment : segments_) {
        ve::TimePoint segment_end = segment.end_time();
        
        // Check if segment overlaps with range
        if (!(segment.start_time >= end || segment_end <= start)) {
            result.push_back(&segment);
        }
    }
    
    return result;
}

std::vector<const Segment*> Track::get_segments_in_range(ve::TimePoint start, ve::TimePoint end) const {
    std::vector<const Segment*> result;
    
    for (const auto& segment : segments_) {
        ve::TimePoint segment_end = segment.end_time();
        
        // Check if segment overlaps with range
        if (!(segment.start_time >= end || segment_end <= start)) {
            result.push_back(&segment);
        }
    }
    
    return result;
}

Segment* Track::get_segment_at_time(ve::TimePoint time) {
    for (auto& segment : segments_) {
        if (time >= segment.start_time && time < segment.end_time()) {
            return &segment;
        }
    }
    return nullptr;
}

const Segment* Track::get_segment_at_time(ve::TimePoint time) const {
    for (const auto& segment : segments_) {
        if (time >= segment.start_time && time < segment.end_time()) {
            return &segment;
        }
    }
    return nullptr;
}

bool Track::insert_gap(ve::TimePoint at, ve::TimeDuration duration) {
#ifdef VE_TIMELINE_DEBUG
    ve::log::debug("[Track::insert_gap] at=" + std::to_string(at.to_rational().num) +
                   " dur=" + std::to_string(duration.to_rational().num));
    std::string before = "[Track::insert_gap] before:";
    for (const auto& s : segments_) {
        before += " (id=" + std::to_string(s.id) + " st=" + std::to_string(s.start_time.to_rational().num) +
                  " end=" + std::to_string(s.end_time().to_rational().num) + ")";
    }
    ve::log::debug(before);
#endif
    for (auto& segment : segments_) {
        if (segment.start_time >= at) {
            segment.start_time = ve::TimePoint{
                segment.start_time.to_rational().num + duration.to_rational().num,
                segment.start_time.to_rational().den
            };
        }
    }
    #ifdef VE_TIMELINE_DEBUG
    std::string after = "[Track::insert_gap] after:";
    for (const auto& s : segments_) {
        after += " (id=" + std::to_string(s.id) + " st=" + std::to_string(s.start_time.to_rational().num) +
                 " end=" + std::to_string(s.end_time().to_rational().num) + ")";
    }
    ve::log::debug(after);
    #endif
    return true;
}

bool Track::delete_range(ve::TimePoint start, ve::TimeDuration duration, bool ripple) {
    ve::TimePoint end = ve::TimePoint{
        start.to_rational().num + duration.to_rational().num,
        start.to_rational().den
    };
    
    // Remove segments that are completely within the range
    segments_.erase(
        std::remove_if(segments_.begin(), segments_.end(),
            [start, end](const Segment& segment) {
                return segment.start_time >= start && segment.end_time() <= end;
            }),
        segments_.end()
    );
    
    // Trim segments that partially overlap
    for (auto& segment : segments_) {
        ve::TimePoint segment_end = segment.end_time();
        
        if (segment.start_time < start && segment_end > start) {
            // Segment starts before range and extends into it - trim the end
            ve::TimeDuration new_duration{
                start.to_rational().num - segment.start_time.to_rational().num,
                segment.start_time.to_rational().den
            };
            segment.duration = new_duration;
        } else if (segment.start_time < end && segment_end > end) {
            // Segment starts in range and extends beyond - trim the start
            ve::TimeDuration trim_amount{
                end.to_rational().num - segment.start_time.to_rational().num,
                segment.start_time.to_rational().den
            };
            segment.start_time = end;
            segment.duration = ve::TimeDuration{
                segment.duration.to_rational().num - trim_amount.to_rational().num,
                segment.duration.to_rational().den
            };
        }
    }
    
    // If ripple mode, shift segments after the deleted range
    if (ripple) {
        for (auto& segment : segments_) {
            if (segment.start_time >= end) {
                segment.start_time = ve::TimePoint{
                    segment.start_time.to_rational().num - duration.to_rational().num,
                    segment.start_time.to_rational().den
                };
            }
        }
    }
    
    return true;
}

bool Track::split_segment(SegmentId segment_id, ve::TimePoint split_time) {
    size_t index = find_segment_index(segment_id);
    if (index >= segments_.size()) {
        return false;
    }
    
    Segment& original = segments_[index];
    
    // Check if split time is within the segment
    if (split_time <= original.start_time || split_time >= original.end_time()) {
        return false;
    }
    
    // Create new segment for the second part
    Segment second_part = original;
    second_part.id = next_segment_id_++;
    second_part.start_time = split_time;
    second_part.duration = ve::TimeDuration{
        original.end_time().to_rational().num - split_time.to_rational().num,
        split_time.to_rational().den
    };
    
    // Adjust original segment duration
    original.duration = ve::TimeDuration{
        split_time.to_rational().num - original.start_time.to_rational().num,
        original.start_time.to_rational().den
    };
    
    segments_.push_back(second_part);
    sort_segments();
    
    return true;
}

void Track::sort_segments() {
    std::sort(segments_.begin(), segments_.end(),
        [](const Segment& a, const Segment& b) {
            auto a_rational = a.start_time.to_rational();
            auto b_rational = b.start_time.to_rational();
            return a_rational.num * b_rational.den < b_rational.num * a_rational.den;
        });
}

bool Track::validate_no_overlap() const {
    for (size_t i = 0; i < segments_.size(); ++i) {
        for (size_t j = i + 1; j < segments_.size(); ++j) {
            const auto& a = segments_[i];
            const auto& b = segments_[j];
            
            ve::TimePoint a_end = a.end_time();
            ve::TimePoint b_end = b.end_time();
            
            if (!(a.start_time >= b_end || a_end <= b.start_time)) {
                return false;
            }
        }
    }
    return true;
}

size_t Track::find_segment_index(SegmentId segment_id) const {
    for (size_t i = 0; i < segments_.size(); ++i) {
        if (segments_[i].id == segment_id) {
            return i;
        }
    }
    return segments_.size();
}

} // namespace ve::timeline
