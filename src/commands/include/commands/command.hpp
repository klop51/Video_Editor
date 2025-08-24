#pragma once
#include "timeline/timeline.hpp"
#include <memory>
#include <string>
#include <vector>
#include <chrono>

namespace ve::commands {

/**
 * @brief Base class for all timeline edit commands
 * 
 * Implements the Command pattern for undo/redo functionality.
 * All timeline modifications should go through the command system.
 */
class Command {
public:
    virtual ~Command() = default;
    
    /**
     * @brief Execute the command
     * @param timeline The timeline to execute the command on
     * @return true if successful, false otherwise
     */
    virtual bool execute(ve::timeline::Timeline& timeline) = 0;
    
    /**
     * @brief Undo the command
     * @param timeline The timeline to undo the command on
     * @return true if successful, false otherwise
     */
    virtual bool undo(ve::timeline::Timeline& timeline) = 0;
    
    /**
     * @brief Get a human-readable description of the command
     * @return Command description for UI display
     */
    virtual std::string description() const = 0;
    
    /**
     * @brief Check if this command can be merged with another
     * @param other The other command to potentially merge with
     * @return true if commands can be merged
     */
    virtual bool can_merge_with(const Command& /*other*/) const { return false; }
    
    /**
     * @brief Merge this command with another command
     * @param other The command to merge with (will be consumed)
     * @return A new merged command, or nullptr if merge failed
     */
    virtual std::unique_ptr<Command> merge_with(std::unique_ptr<Command> /*other*/) const { 
        return nullptr; 
    }
    
    /**
     * @brief Get the timestamp when this command was created
     */
    std::chrono::system_clock::time_point timestamp() const { return timestamp_; }
    
protected:
    std::chrono::system_clock::time_point timestamp_{std::chrono::system_clock::now()};
};

/**
 * @brief Manages command history and provides undo/redo functionality
 */
class CommandHistory {
public:
    explicit CommandHistory(size_t max_history = 200);
    
    /**
     * @brief Execute a command and add it to history
     * @param command The command to execute
     * @param timeline The timeline to execute on
     * @return true if successful
     */
    bool execute(std::unique_ptr<Command> command, ve::timeline::Timeline& timeline);
    
    /**
     * @brief Undo the last command
     * @param timeline The timeline to undo on
     * @return true if successful
     */
    bool undo(ve::timeline::Timeline& timeline);
    
    /**
     * @brief Redo the next command
     * @param timeline The timeline to redo on
     * @return true if successful
     */
    bool redo(ve::timeline::Timeline& timeline);
    
    /**
     * @brief Check if undo is available
     */
    bool can_undo() const { return current_index_ > 0; }
    
    /**
     * @brief Check if redo is available
     */
    bool can_redo() const { return current_index_ < commands_.size(); }
    
    /**
     * @brief Get the description of the command that would be undone
     */
    std::string undo_description() const;
    
    /**
     * @brief Get the description of the command that would be redone
     */
    std::string redo_description() const;
    
    /**
     * @brief Clear all command history
     */
    void clear();
    
    /**
     * @brief Get the current command history
     */
    const std::vector<std::unique_ptr<Command>>& commands() const { return commands_; }
    
    /**
     * @brief Get the current position in history
     */
    size_t current_index() const { return current_index_; }
    
    /**
     * @brief Set maximum history size
     */
    void set_max_history(size_t max_history) { max_history_ = max_history; }
    
private:
    std::vector<std::unique_ptr<Command>> commands_;
    size_t current_index_{0};
    size_t max_history_;
    
    /**
     * @brief Remove old commands when history exceeds maximum
     */
    void trim_history();
    
    /**
     * @brief Try to coalesce the new command with the last command
     * @param new_command The command to potentially coalesce
     * @return true if commands were coalesced
     */
    bool try_coalesce(std::unique_ptr<Command>& new_command);
};

} // namespace ve::commands
