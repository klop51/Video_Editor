#pragma once
#include "command.hpp"
#include "../../timeline/include/timeline/timeline.hpp"

namespace ve::commands {

/**
 * @brief Command to insert a segment into a track
 */
class InsertSegmentCommand : public Command {
public:
    InsertSegmentCommand(ve::timeline::TrackId track_id, 
                        ve::timeline::Segment segment,
                        ve::TimePoint at);
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
private:
    ve::timeline::TrackId track_id_;
    ve::timeline::Segment segment_;
    ve::TimePoint position_;
    ve::timeline::SegmentId inserted_segment_id_;
    bool executed_{false};
};

/**
 * @brief Command to remove a segment from a track
 */
class RemoveSegmentCommand : public Command {
public:
    RemoveSegmentCommand(ve::timeline::TrackId track_id, 
                        ve::timeline::SegmentId segment_id);
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
private:
    ve::timeline::TrackId track_id_;
    ve::timeline::SegmentId segment_id_;
    ve::timeline::Segment removed_segment_; // Store for undo
    bool executed_{false};
};

/**
 * @brief Command to move a segment to a new position/track
 */
class MoveSegmentCommand : public Command {
public:
    MoveSegmentCommand(ve::timeline::SegmentId segment_id,
                      ve::timeline::TrackId from_track,
                      ve::timeline::TrackId to_track,
                      ve::TimePoint from_time,
                      ve::TimePoint to_time);
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
    bool can_merge_with(const Command& other) const override;
    std::unique_ptr<Command> merge_with(std::unique_ptr<Command> other) const override;
    
private:
    ve::timeline::SegmentId segment_id_;
    ve::timeline::TrackId from_track_;
    ve::timeline::TrackId to_track_;
    ve::TimePoint from_time_;
    ve::TimePoint to_time_;
    bool executed_{false};
};

/**
 * @brief Command to split a segment at a specific time
 */
class SplitSegmentCommand : public Command {
public:
    SplitSegmentCommand(ve::timeline::SegmentId segment_id,
                       ve::TimePoint split_time);
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
private:
    ve::timeline::SegmentId original_segment_id_;
    ve::TimePoint split_time_;
    ve::timeline::SegmentId first_segment_id_;
    ve::timeline::SegmentId second_segment_id_;
    ve::timeline::Segment original_segment_; // Store for undo
    ve::timeline::TrackId track_id_;
    bool executed_{false};
};

/**
 * @brief Command to trim a segment (change in/out points)
 */
class TrimSegmentCommand : public Command {
public:
    TrimSegmentCommand(ve::timeline::SegmentId segment_id,
                      ve::TimePoint new_start,
                      ve::TimeDuration new_duration);
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
    bool can_merge_with(const Command& other) const override;
    std::unique_ptr<Command> merge_with(std::unique_ptr<Command> other) const override;
    
private:
    ve::timeline::SegmentId segment_id_;
    ve::TimePoint new_start_;
    ve::TimeDuration new_duration_;
    ve::TimePoint original_start_;
    ve::TimeDuration original_duration_;
    bool executed_{false};
};

/**
 * @brief Command to add a new track
 */
class AddTrackCommand : public Command {
public:
    AddTrackCommand(ve::timeline::Track::Type type, 
                   const std::string& name = "");
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
private:
    ve::timeline::Track::Type track_type_;
    std::string track_name_;
    ve::timeline::TrackId created_track_id_;
    bool executed_{false};
};

/**
 * @brief Command to remove a track
 */
class RemoveTrackCommand : public Command {
public:
    RemoveTrackCommand(ve::timeline::TrackId track_id);
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
private:
    ve::timeline::TrackId track_id_;
    std::unique_ptr<ve::timeline::Track> removed_track_; // Store for undo
    size_t track_position_; // Position in track list for proper restoration
    bool executed_{false};
};

/**
 * @brief Macro command that groups multiple commands together
 */
class MacroCommand : public Command {
public:
    MacroCommand(const std::string& description);
    
    void add_command(std::unique_ptr<Command> command);
    
    bool execute(ve::timeline::Timeline& timeline) override;
    bool undo(ve::timeline::Timeline& timeline) override;
    std::string description() const override;
    
    bool empty() const { return commands_.empty(); }
    size_t size() const { return commands_.size(); }
    
private:
    std::vector<std::unique_ptr<Command>> commands_;
    std::string description_;
    bool executed_{false};
};

} // namespace ve::commands
