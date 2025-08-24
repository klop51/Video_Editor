#include "commands/timeline_commands.hpp"
#include "core/log.hpp"
#include <chrono>

namespace ve::commands {

// ============================================================================
// InsertSegmentCommand
// ============================================================================

InsertSegmentCommand::InsertSegmentCommand(ve::timeline::TrackId track_id, 
                                         ve::timeline::Segment segment,
                                         ve::TimePoint at)
    : track_id_(track_id), segment_(segment), position_(at) {
    
    segment_.start_time = at;
}

bool InsertSegmentCommand::execute(ve::timeline::Timeline& timeline) {
    auto* track = timeline.get_track(track_id_);
    if (!track) {
        ve::log::warn("InsertSegmentCommand: Track not found: " + std::to_string(track_id_));
        return false;
    }
    
    bool success = track->add_segment(segment_);
    if (success) {
        inserted_segment_id_ = segment_.id;
        executed_ = true;
        ve::log::debug("Inserted segment '" + segment_.name + "' into track " + std::to_string(track_id_));
    }
    
    return success;
}

bool InsertSegmentCommand::undo(ve::timeline::Timeline& timeline) {
    if (!executed_) {
        return false;
    }
    
    auto* track = timeline.get_track(track_id_);
    if (!track) {
        ve::log::warn("InsertSegmentCommand undo: Track not found: " + std::to_string(track_id_));
        return false;
    }
    
    bool success = track->remove_segment(inserted_segment_id_);
    if (success) {
        executed_ = false;
        ve::log::debug("Removed segment '" + segment_.name + "' from track " + std::to_string(track_id_));
    }
    
    return success;
}

std::string InsertSegmentCommand::description() const {
    return "Insert " + segment_.name + " into track";
}

// ============================================================================
// RemoveSegmentCommand
// ============================================================================

RemoveSegmentCommand::RemoveSegmentCommand(ve::timeline::TrackId track_id, 
                                          ve::timeline::SegmentId segment_id)
    : track_id_(track_id), segment_id_(segment_id) {
}

bool RemoveSegmentCommand::execute(ve::timeline::Timeline& timeline) {
    auto* track = timeline.get_track(track_id_);
    if (!track) {
        ve::log::warn("RemoveSegmentCommand: Track not found: " + std::to_string(track_id_));
        return false;
    }
    
    // Find and store the segment for undo
    const auto& segments = track->segments();
    auto segment_it = std::find_if(segments.begin(), segments.end(),
        [this](const ve::timeline::Segment& seg) { return seg.id == segment_id_; });
    
    if (segment_it == segments.end()) {
        ve::log::warn("RemoveSegmentCommand: Segment not found: " + std::to_string(segment_id_));
        return false;
    }
    
    removed_segment_ = *segment_it;
    
    bool success = track->remove_segment(segment_id_);
    if (success) {
        executed_ = true;
        ve::log::debug("Removed segment '" + removed_segment_.name + "' from track " + std::to_string(track_id_));
    }
    
    return success;
}

bool RemoveSegmentCommand::undo(ve::timeline::Timeline& timeline) {
    if (!executed_) {
        return false;
    }
    
    auto* track = timeline.get_track(track_id_);
    if (!track) {
        ve::log::warn("RemoveSegmentCommand undo: Track not found: " + std::to_string(track_id_));
        return false;
    }
    
    bool success = track->add_segment(removed_segment_);
    if (success) {
        executed_ = false;
        ve::log::debug("Restored segment '" + removed_segment_.name + "' to track " + std::to_string(track_id_));
    }
    
    return success;
}

std::string RemoveSegmentCommand::description() const {
    return "Remove " + removed_segment_.name;
}

// ============================================================================
// MoveSegmentCommand
// ============================================================================

MoveSegmentCommand::MoveSegmentCommand(ve::timeline::SegmentId segment_id,
                                      ve::timeline::TrackId from_track,
                                      ve::timeline::TrackId to_track,
                                      ve::TimePoint from_time,
                                      ve::TimePoint to_time)
    : segment_id_(segment_id), from_track_(from_track), to_track_(to_track),
      from_time_(from_time), to_time_(to_time) {
}

bool MoveSegmentCommand::execute(ve::timeline::Timeline& timeline) {
    auto* from_track = timeline.get_track(from_track_);
    auto* to_track = timeline.get_track(to_track_);
    
    if (!from_track || !to_track) {
        ve::log::warn("MoveSegmentCommand: Track not found");
        return false;
    }
    
    // Find the segment to move
    const auto& segments = from_track->segments();
    auto segment_it = std::find_if(segments.begin(), segments.end(),
        [this](const ve::timeline::Segment& seg) { return seg.id == segment_id_; });
    
    if (segment_it == segments.end()) {
        ve::log::warn("MoveSegmentCommand: Segment not found: " + std::to_string(segment_id_));
        return false;
    }
    
    ve::timeline::Segment segment = *segment_it;
    
    // Remove from source track
    if (!from_track->remove_segment(segment_id_)) {
        return false;
    }
    
    // Update segment position
    segment.start_time = to_time_;
    
    // Add to destination track
    if (!to_track->add_segment(segment)) {
        // Rollback: re-add to source track
        segment.start_time = from_time_;
        from_track->add_segment(segment);
        return false;
    }
    
    executed_ = true;
    ve::log::debug("Moved segment '" + segment.name + "' from track " + 
                   std::to_string(from_track_) + " to " + std::to_string(to_track_));
    
    return true;
}

bool MoveSegmentCommand::undo(ve::timeline::Timeline& timeline) {
    if (!executed_) {
        return false;
    }
    
    auto* from_track = timeline.get_track(from_track_);
    auto* to_track = timeline.get_track(to_track_);
    
    if (!from_track || !to_track) {
        ve::log::warn("MoveSegmentCommand undo: Track not found");
        return false;
    }
    
    // Find the segment in destination track
    const auto& segments = to_track->segments();
    auto segment_it = std::find_if(segments.begin(), segments.end(),
        [this](const ve::timeline::Segment& seg) { return seg.id == segment_id_; });
    
    if (segment_it == segments.end()) {
        return false;
    }
    
    ve::timeline::Segment segment = *segment_it;
    
    // Remove from destination track
    to_track->remove_segment(segment_id_);
    
    // Restore to source track
    segment.start_time = from_time_;
    from_track->add_segment(segment);
    
    executed_ = false;
    return true;
}

std::string MoveSegmentCommand::description() const {
    return "Move segment";
}

bool MoveSegmentCommand::can_merge_with(const Command& other) const {
    const auto* other_move = dynamic_cast<const MoveSegmentCommand*>(&other);
    if (!other_move) {
        return false;
    }
    
    // Can merge if it's the same segment and commands are close in time
    if (segment_id_ != other_move->segment_id_) {
        return false;
    }
    
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        other.timestamp() - timestamp()).count();
    
    return std::abs(time_diff) < 500; // Merge if within 500ms
}

std::unique_ptr<Command> MoveSegmentCommand::merge_with(std::unique_ptr<Command> other) const {
    auto other_move = dynamic_cast<MoveSegmentCommand*>(other.get());
    if (!other_move || !can_merge_with(*other_move)) {
        return nullptr;
    }
    
    // Create a new move command from the original position to the final position
    auto merged = std::make_unique<MoveSegmentCommand>(
        segment_id_, from_track_, other_move->to_track_, from_time_, other_move->to_time_);
    
    other.release(); // We're consuming the other command
    return merged;
}

// ============================================================================
// SplitSegmentCommand
// ============================================================================

SplitSegmentCommand::SplitSegmentCommand(ve::timeline::SegmentId segment_id,
                                        ve::TimePoint split_time)
    : original_segment_id_(segment_id), split_time_(split_time) {
}

bool SplitSegmentCommand::execute(ve::timeline::Timeline& timeline) {
    // Find the segment and its track
    ve::timeline::Track* track = nullptr;
    const ve::timeline::Segment* segment = nullptr;
    
    for (const auto& t : timeline.tracks()) {
        const auto& segments = t->segments();
        auto it = std::find_if(segments.begin(), segments.end(),
            [this](const ve::timeline::Segment& seg) { return seg.id == original_segment_id_; });
        
        if (it != segments.end()) {
            track = t.get();
            segment = &(*it);
            track_id_ = t->id();
            break;
        }
    }
    
    if (!track || !segment) {
        ve::log::warn("SplitSegmentCommand: Segment not found: " + std::to_string(original_segment_id_));
        return false;
    }
    
    // Validate split time
    if (split_time_ <= segment->start_time || split_time_ >= segment->end_time()) {
        ve::log::warn("SplitSegmentCommand: Invalid split time");
        return false;
    }
    
    // Store original segment for undo
    original_segment_ = *segment;
    
    // Create two new segments
    ve::timeline::Segment first_segment = *segment;
    ve::timeline::Segment second_segment = *segment;
    
    // Update timing for first segment
    first_segment.id = track->generate_segment_id();
    first_segment.duration = ve::TimeDuration{
        split_time_.to_rational().num - segment->start_time.to_rational().num,
        split_time_.to_rational().den
    };
    
    // Update timing for second segment
    second_segment.id = track->generate_segment_id();
    second_segment.start_time = split_time_;
    second_segment.duration = ve::TimeDuration{
        segment->end_time().to_rational().num - split_time_.to_rational().num,
        segment->end_time().to_rational().den
    };
    
    // Remove original segment
    if (!track->remove_segment(original_segment_id_)) {
        return false;
    }
    
    // Add new segments
    if (!track->add_segment(first_segment) || !track->add_segment(second_segment)) {
        // Rollback
        track->add_segment(original_segment_);
        return false;
    }
    
    first_segment_id_ = first_segment.id;
    second_segment_id_ = second_segment.id;
    executed_ = true;
    
    ve::log::debug("Split segment '" + original_segment_.name + "' at " + 
                   std::to_string(split_time_.to_rational().num));
    
    return true;
}

bool SplitSegmentCommand::undo(ve::timeline::Timeline& timeline) {
    if (!executed_) {
        return false;
    }
    
    auto* track = timeline.get_track(track_id_);
    if (!track) {
        return false;
    }
    
    // Remove the split segments
    track->remove_segment(first_segment_id_);
    track->remove_segment(second_segment_id_);
    
    // Restore original segment
    bool success = track->add_segment(original_segment_);
    if (success) {
        executed_ = false;
    }
    
    return success;
}

std::string SplitSegmentCommand::description() const {
    return "Split " + original_segment_.name;
}

// ============================================================================
// TrimSegmentCommand
// ============================================================================

TrimSegmentCommand::TrimSegmentCommand(ve::timeline::SegmentId segment_id,
                                      ve::TimePoint new_start,
                                      ve::TimeDuration new_duration)
    : segment_id_(segment_id), new_start_(new_start), new_duration_(new_duration) {
}

bool TrimSegmentCommand::execute(ve::timeline::Timeline& timeline) {
    // Find the segment and its track
    for (const auto& track : timeline.tracks()) {
        auto& segments = const_cast<std::vector<ve::timeline::Segment>&>(track->segments());
        auto it = std::find_if(segments.begin(), segments.end(),
            [this](ve::timeline::Segment& seg) { return seg.id == segment_id_; });
        
        if (it != segments.end()) {
            // Store original values
            original_start_ = it->start_time;
            original_duration_ = it->duration;
            
            // Apply new values
            it->start_time = new_start_;
            it->duration = new_duration_;
            
            executed_ = true;
            ve::log::debug("Trimmed segment '" + it->name + "'");
            return true;
        }
    }
    
    ve::log::warn("TrimSegmentCommand: Segment not found: " + std::to_string(segment_id_));
    return false;
}

bool TrimSegmentCommand::undo(ve::timeline::Timeline& timeline) {
    if (!executed_) {
        return false;
    }
    
    // Find the segment and restore original values
    for (const auto& track : timeline.tracks()) {
        auto& segments = const_cast<std::vector<ve::timeline::Segment>&>(track->segments());
        auto it = std::find_if(segments.begin(), segments.end(),
            [this](ve::timeline::Segment& seg) { return seg.id == segment_id_; });
        
        if (it != segments.end()) {
            it->start_time = original_start_;
            it->duration = original_duration_;
            executed_ = false;
            return true;
        }
    }
    
    return false;
}

std::string TrimSegmentCommand::description() const {
    return "Trim segment";
}

bool TrimSegmentCommand::can_merge_with(const Command& other) const {
    const auto* other_trim = dynamic_cast<const TrimSegmentCommand*>(&other);
    if (!other_trim) {
        return false;
    }
    
    // Can merge if it's the same segment and commands are close in time
    if (segment_id_ != other_trim->segment_id_) {
        return false;
    }
    
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        other.timestamp() - timestamp()).count();
    
    return std::abs(time_diff) < 500; // Merge if within 500ms
}

std::unique_ptr<Command> TrimSegmentCommand::merge_with(std::unique_ptr<Command> other) const {
    auto other_trim = dynamic_cast<TrimSegmentCommand*>(other.get());
    if (!other_trim || !can_merge_with(*other_trim)) {
        return nullptr;
    }
    
    // Create a new trim command from the original values to the final values
    auto merged = std::make_unique<TrimSegmentCommand>(
        segment_id_, other_trim->new_start_, other_trim->new_duration_);
    
    // The merged command should have the original values from this command
    merged->original_start_ = original_start_;
    merged->original_duration_ = original_duration_;
    
    other.release(); // We're consuming the other command
    return merged;
}

// ============================================================================
// AddTrackCommand
// ============================================================================

AddTrackCommand::AddTrackCommand(ve::timeline::Track::Type type, const std::string& name)
    : track_type_(type), track_name_(name) {
}

bool AddTrackCommand::execute(ve::timeline::Timeline& timeline) {
    created_track_id_ = timeline.add_track(track_type_, track_name_);
    executed_ = true;
    
    ve::log::debug(std::string("Added ") + (track_type_ == ve::timeline::Track::Type::Video ? "video" : "audio") + 
                   " track: " + track_name_);
    
    return true;
}

bool AddTrackCommand::undo(ve::timeline::Timeline& timeline) {
    if (!executed_) {
        return false;
    }
    
    bool success = timeline.remove_track(created_track_id_);
    if (success) {
        executed_ = false;
    }
    
    return success;
}

std::string AddTrackCommand::description() const {
    return std::string("Add ") + (track_type_ == ve::timeline::Track::Type::Video ? "video" : "audio") + " track";
}

// ============================================================================
// RemoveTrackCommand
// ============================================================================

RemoveTrackCommand::RemoveTrackCommand(ve::timeline::TrackId track_id)
    : track_id_(track_id) {
}

bool RemoveTrackCommand::execute(ve::timeline::Timeline& timeline) {
    auto* track = timeline.get_track(track_id_);
    if (!track) {
        ve::log::warn("RemoveTrackCommand: Track not found: " + std::to_string(track_id_));
        return false;
    }
    
    // Store track position for proper restoration
    const auto& tracks = timeline.tracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i]->id() == track_id_) {
            track_position_ = i;
            break;
        }
    }
    
    // Store the track for undo (this is a simplification - real implementation
    // would need to properly clone the track)
    // For now, we'll just track the ID and name
    
    bool success = timeline.remove_track(track_id_);
    if (success) {
        executed_ = true;
        ve::log::debug("Removed track: " + std::to_string(track_id_));
    }
    
    return success;
}

bool RemoveTrackCommand::undo(ve::timeline::Timeline& /*timeline*/) {
    if (!executed_) {
        return false;
    }
    
    // This is a simplified implementation
    // In a real system, we would need to restore the exact track with all its segments
    ve::log::warn("RemoveTrackCommand undo not fully implemented");
    executed_ = false;
    return false;
}

std::string RemoveTrackCommand::description() const {
    return "Remove track";
}

// ============================================================================
// MacroCommand
// ============================================================================

MacroCommand::MacroCommand(const std::string& description)
    : description_(description) {
}

void MacroCommand::add_command(std::unique_ptr<Command> command) {
    if (command) {
        commands_.push_back(std::move(command));
    }
}

bool MacroCommand::execute(ve::timeline::Timeline& timeline) {
    if (executed_) {
        return false; // Already executed
    }
    
    // Execute all commands in order
    for (auto& command : commands_) {
        if (!command->execute(timeline)) {
            // Rollback: undo all previously executed commands in reverse order
            for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
                if (it->get() == command.get()) {
                    break; // Stop at the failed command
                }
                (*it)->undo(timeline);
            }
            return false;
        }
    }
    
    executed_ = true;
    ve::log::debug("Executed macro command: " + description_ + 
                   " (" + std::to_string(commands_.size()) + " sub-commands)");
    
    return true;
}

bool MacroCommand::undo(ve::timeline::Timeline& timeline) {
    if (!executed_) {
        return false;
    }
    
    // Undo all commands in reverse order
    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
        if (!(*it)->undo(timeline)) {
            ve::log::warn("Failed to undo sub-command in macro: " + (*it)->description());
            return false;
        }
    }
    
    executed_ = false;
    ve::log::debug("Undid macro command: " + description_);
    
    return true;
}

std::string MacroCommand::description() const {
    return description_;
}

} // namespace ve::commands
