#include "timeline/timeline.hpp"
#include "core/log.hpp"
#include <algorithm>

namespace ve::timeline {

Timeline::Timeline() {
    ve::log::debug("Created new timeline: " + name_);
}


std::shared_ptr<Timeline::Snapshot> Timeline::snapshot() const {
    auto snap = std::make_shared<Snapshot>();
    snap->name = name_;
    snap->frame_rate = frame_rate_;
    snap->version = version_;
    snap->tracks.reserve(tracks_.size());
    for (const auto& tptr : tracks_) {
        Track copy = *tptr; // relies on implicit copy of Track (segments vector etc.)
        snap->tracks.push_back(std::move(copy));
    }
    for (const auto& [id, cptr] : clips_) {
        snap->clips.emplace(id, *cptr);
    }
    return snap;
}

TrackId Timeline::add_track(Track::Type type, const std::string& name) {
    TrackId id = next_track_id_++;
    std::string track_name = name;
    
    if (track_name.empty()) {
        int count = static_cast<int>(get_tracks_by_type(type).size()) + 1;
        track_name = (type == Track::Video ? "Video " : "Audio ") + std::to_string(count);
    }
    
    auto track = std::make_unique<Track>(id, type, track_name);
    tracks_.push_back(std::move(track));
    
    ve::log::info("Added track: " + track_name + " (ID: " + std::to_string(id) + ")");
    mark_modified();
    return id;
}

bool Timeline::remove_track(TrackId track_id) {
    size_t index = find_track_index(track_id);
    if (index >= tracks_.size()) {
        return false;
    }
    
    std::string track_name = tracks_[index]->name();
    tracks_.erase(tracks_.begin() + index);
    
    ve::log::info("Removed track: " + track_name + " (ID: " + std::to_string(track_id) + ")");
    mark_modified();
    return true;
}

Track* Timeline::get_track(TrackId track_id) {
    size_t index = find_track_index(track_id);
    return index < tracks_.size() ? tracks_[index].get() : nullptr;
}

const Track* Timeline::get_track(TrackId track_id) const {
    size_t index = find_track_index(track_id);
    return index < tracks_.size() ? tracks_[index].get() : nullptr;
}

std::vector<Track*> Timeline::get_tracks_by_type(Track::Type type) {
    std::vector<Track*> result;
    for (auto& track : tracks_) {
        if (track->type() == type) {
            result.push_back(track.get());
        }
    }
    return result;
}

std::vector<const Track*> Timeline::get_tracks_by_type(Track::Type type) const {
    std::vector<const Track*> result;
    for (const auto& track : tracks_) {
        if (track->type() == type) {
            result.push_back(track.get());
        }
    }
    return result;
}

ClipId Timeline::add_clip(std::shared_ptr<MediaSource> source, const std::string& name) {
    ClipId id = next_clip_id_++;
    
    auto clip = std::make_unique<MediaClip>();
    clip->id = id;
    clip->source = source;
    clip->name = name.empty() ? source->path : name;
    clip->in_time = ve::TimePoint{0};
    clip->out_time = ve::TimePoint{source->duration.to_rational().num, source->duration.to_rational().den};
    
    clips_[id] = std::move(clip);
    
    ve::log::info("Added clip: " + clips_[id]->name + " (ID: " + std::to_string(id) + ")");
    mark_modified();
    return id;
}


ClipId Timeline::commit_prepared_clip(const PreparedClip& pc) {
    // Commit must be lightweight: no I/O, no decoding, just data structure mutation
    ClipId id = next_clip_id_++;
    auto clip = std::make_unique<MediaClip>();
    clip->id = id;
    clip->source = pc.source;
    clip->name = pc.name.empty() ? pc.source->path : pc.name;
    clip->in_time = ve::TimePoint{0};
    clip->out_time = ve::TimePoint{pc.source->duration.to_rational().num, pc.source->duration.to_rational().den};
    clips_[id] = std::move(clip);
    ve::log::info("Committed prepared clip: " + clips_[id]->name + " (ID: " + std::to_string(id) + ")");
    mark_modified();
    return id;
}

ClipId Timeline::add_clip_with_id(ClipId id, std::shared_ptr<MediaSource> source, const std::string& name,
                            ve::TimePoint in_time, ve::TimePoint out_time) {
    if(id >= next_clip_id_) next_clip_id_ = id + 1;
    auto clip = std::make_unique<MediaClip>();
    clip->id = id; clip->source = source; clip->name = name; clip->in_time = in_time; clip->out_time = out_time;
    clips_[id] = std::move(clip);
    return id;
}

bool Timeline::remove_clip(ClipId clip_id) {
    auto it = clips_.find(clip_id);
    if (it == clips_.end()) {
        return false;
    }
    
    std::string clip_name = it->second->name;
    clips_.erase(it);
    
    ve::log::info("Removed clip: " + clip_name + " (ID: " + std::to_string(clip_id) + ")");
    mark_modified();
    return true;
}

MediaClip* Timeline::get_clip(ClipId clip_id) {
    auto it = clips_.find(clip_id);
    return it != clips_.end() ? it->second.get() : nullptr;
}

const MediaClip* Timeline::get_clip(ClipId clip_id) const {
    auto it = clips_.find(clip_id);
    return it != clips_.end() ? it->second.get() : nullptr;
}

ve::TimeDuration Timeline::duration() const {
    ve::TimePoint max_end{0};
    
    for (const auto& track : tracks_) {
        for (const auto& segment : track->segments()) {
            ve::TimePoint segment_end = segment.end_time();
            if (segment_end > max_end) {
                max_end = segment_end;
            }
        }
    }
    
    return ve::TimeDuration{max_end.to_rational().num, max_end.to_rational().den};
}

bool Timeline::insert_gap_all_tracks(ve::TimePoint at, ve::TimeDuration duration) {
    bool success = true;
    for (auto& track : tracks_) {
        if (!track->insert_gap(at, duration)) {
            success = false;
        }
    }
    if(success) mark_modified();
    return success;
}

bool Timeline::delete_range_all_tracks(ve::TimePoint start, ve::TimeDuration duration, bool ripple) {
    bool success = true;
    for (auto& track : tracks_) {
        if (!track->delete_range(start, duration, ripple)) {
            success = false;
        }
    }
    if(success) mark_modified();
    return success;
}

size_t Timeline::find_track_index(TrackId track_id) const {
    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i]->id() == track_id) {
            return i;
        }
    }
    return tracks_.size();
}

} // namespace ve::timeline
