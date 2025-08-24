#pragma once
#include "timeline/track.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace ve::timeline {

class Timeline {
public:
    Timeline();
    ~Timeline() = default;
    
    // Track management
    TrackId add_track(Track::Type type, const std::string& name = "");
    bool remove_track(TrackId track_id);
    Track* get_track(TrackId track_id);
    const Track* get_track(TrackId track_id) const;
    
    std::vector<Track*> get_tracks_by_type(Track::Type type);
    std::vector<const Track*> get_tracks_by_type(Track::Type type) const;
    const std::vector<std::unique_ptr<Track>>& tracks() const { return tracks_; }
    
    // Clip management
    ClipId add_clip(std::shared_ptr<MediaSource> source, const std::string& name = "");
    bool remove_clip(ClipId clip_id);
    MediaClip* get_clip(ClipId clip_id);
    const MediaClip* get_clip(ClipId clip_id) const;
    const std::unordered_map<ClipId, std::unique_ptr<MediaClip>>& clips() const { return clips_; }
    
    // Timeline properties
    ve::TimeRational frame_rate() const { return frame_rate_; }
    void set_frame_rate(ve::TimeRational rate) { frame_rate_ = rate; }
    
    ve::TimeDuration duration() const;
    
    // Project metadata
    std::string name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    
    // Global edit operations
    bool insert_gap_all_tracks(ve::TimePoint at, ve::TimeDuration duration);
    bool delete_range_all_tracks(ve::TimePoint start, ve::TimeDuration duration, bool ripple = true);
    
    // Playback state (for UI synchronization)
    ve::TimePoint playhead_position() const { return playhead_position_; }
    void set_playhead_position(ve::TimePoint position) { playhead_position_ = position; }
    
    // Selection
    struct Selection {
        std::vector<TrackId> selected_tracks;
        std::vector<SegmentId> selected_segments;
        ve::TimePoint in_point{};
        ve::TimePoint out_point{};
        bool has_range = false;
    };
    
    const Selection& selection() const { return selection_; }
    Selection& selection() { return selection_; }
    
private:
    std::vector<std::unique_ptr<Track>> tracks_;
    std::unordered_map<ClipId, std::unique_ptr<MediaClip>> clips_;
    
    ve::TimeRational frame_rate_{30, 1};  // Default 30fps
    std::string name_ = "Untitled Timeline";
    
    TrackId next_track_id_ = 1;
    ClipId next_clip_id_ = 1;
    
    ve::TimePoint playhead_position_{};
    Selection selection_;
    
    size_t find_track_index(TrackId track_id) const;
};

} // namespace ve::timeline
