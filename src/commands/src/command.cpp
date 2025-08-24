#include "commands/command.hpp"
#include "core/log.hpp"
#include <algorithm>

namespace ve::commands {

CommandHistory::CommandHistory(size_t max_history) 
    : max_history_(max_history) {
    ve::log::debug("Created command history with max size: " + std::to_string(max_history));
}

bool CommandHistory::execute(std::unique_ptr<Command> command, ve::timeline::Timeline& timeline) {
    if (!command) {
        ve::log::warn("Attempted to execute null command");
        return false;
    }
    
    // Try to coalesce with the last command first
    if (try_coalesce(command)) {
        ve::log::debug("Command coalesced with previous command");
        return true;
    }
    
    // Execute the command
    bool success = command->execute(timeline);
    if (!success) {
        ve::log::warn("Command execution failed: " + command->description());
        return false;
    }
    
    // Remove any commands after the current position (for redo branch pruning)
    if (current_index_ < commands_.size()) {
        commands_.erase(commands_.begin() + current_index_, commands_.end());
    }
    
    // Add the command to history
    commands_.push_back(std::move(command));
    current_index_ = commands_.size();
    
    // Trim history if needed
    trim_history();
    
    ve::log::debug("Command executed and added to history. Position: " + 
                   std::to_string(current_index_) + "/" + std::to_string(commands_.size()));
    
    return true;
}

bool CommandHistory::undo(ve::timeline::Timeline& timeline) {
    if (!can_undo()) {
        ve::log::debug("Cannot undo: no commands in history");
        return false;
    }
    
    // Get the command to undo (at current_index_ - 1)
    auto& command = commands_[current_index_ - 1];
    
    bool success = command->undo(timeline);
    if (success) {
        current_index_--;
        ve::log::info("Undid command: " + command->description());
        ve::log::debug("Undo successful. Position: " + 
                       std::to_string(current_index_) + "/" + std::to_string(commands_.size()));
    } else {
        ve::log::warn("Failed to undo command: " + command->description());
    }
    
    return success;
}

bool CommandHistory::redo(ve::timeline::Timeline& timeline) {
    if (!can_redo()) {
        ve::log::debug("Cannot redo: at end of history");
        return false;
    }
    
    // Get the command to redo (at current_index_)
    auto& command = commands_[current_index_];
    
    bool success = command->execute(timeline);
    if (success) {
        current_index_++;
        ve::log::info("Redid command: " + command->description());
        ve::log::debug("Redo successful. Position: " + 
                       std::to_string(current_index_) + "/" + std::to_string(commands_.size()));
    } else {
        ve::log::warn("Failed to redo command: " + command->description());
    }
    
    return success;
}

std::string CommandHistory::undo_description() const {
    if (!can_undo()) {
        return "";
    }
    return commands_[current_index_ - 1]->description();
}

std::string CommandHistory::redo_description() const {
    if (!can_redo()) {
        return "";
    }
    return commands_[current_index_]->description();
}

void CommandHistory::clear() {
    commands_.clear();
    current_index_ = 0;
    ve::log::debug("Command history cleared");
}

void CommandHistory::trim_history() {
    if (commands_.size() <= max_history_) {
        return;
    }
    
    // Remove oldest commands
    size_t excess = commands_.size() - max_history_;
    commands_.erase(commands_.begin(), commands_.begin() + excess);
    
    // Adjust current index
    current_index_ = std::max(static_cast<size_t>(0), current_index_ - excess);
    
    ve::log::debug("Trimmed command history. Removed " + std::to_string(excess) + 
                   " old commands. New position: " + std::to_string(current_index_) + 
                   "/" + std::to_string(commands_.size()));
}

bool CommandHistory::try_coalesce(std::unique_ptr<Command>& new_command) {
    if (commands_.empty() || current_index_ == 0) {
        return false;
    }
    
    // Get the last executed command
    auto& last_command = commands_[current_index_ - 1];
    
    // Check if commands can be merged
    if (!last_command->can_merge_with(*new_command)) {
        return false;
    }
    
    // Try to merge
    auto merged_command = last_command->merge_with(std::move(new_command));
    if (!merged_command) {
        return false;
    }
    
    // Replace the last command with the merged one
    commands_[current_index_ - 1] = std::move(merged_command);
    
    ve::log::debug("Commands successfully coalesced");
    return true;
}

} // namespace ve::commands
