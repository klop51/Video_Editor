#pragma once
#include "frame.hpp"
#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>
#include <cstdint>

namespace ve::decode {

// Simple LRU cache for VideoFrame objects (copying frames; optimize later for shared ownership)
class VideoFrameCache {
public:
    explicit VideoFrameCache(size_t capacity) : capacity_(capacity) {}

    // Non-copyable (std::mutex) but movable so we can reset via assignment
    VideoFrameCache(const VideoFrameCache&) = delete;
    VideoFrameCache& operator=(const VideoFrameCache&) = delete;
    VideoFrameCache(VideoFrameCache&& other) noexcept {
        std::scoped_lock lock(other.mutex_);
        capacity_ = other.capacity_;
        lru_ = std::move(other.lru_);
        map_ = std::move(other.map_);
    }
    VideoFrameCache& operator=(VideoFrameCache&& other) noexcept {
        if(this != &other) {
            std::scoped_lock lock_other(other.mutex_);
            std::scoped_lock lock_this(mutex_);
            capacity_ = other.capacity_;
            lru_ = std::move(other.lru_);
            map_ = std::move(other.map_);
        }
        return *this;
    }

    void put(int64_t pts_us, const VideoFrame& frame) {
        std::scoped_lock lock(mutex_);
        auto it = map_.find(pts_us);
        if(it != map_.end()) {
            // Move to front
            lru_.erase(it->second.it);
            lru_.push_front(pts_us);
            it->second.it = lru_.begin();
            it->second.frame = frame;
            return;
        }
        if(lru_.size() >= capacity_) {
            int64_t evict = lru_.back();
            lru_.pop_back();
            map_.erase(evict);
        }
        lru_.push_front(pts_us);
        Entry e{frame, lru_.begin()};
        map_.emplace(pts_us, std::move(e));
    }

    std::optional<VideoFrame> get(int64_t pts_us) {
        std::scoped_lock lock(mutex_);
        auto it = map_.find(pts_us);
        if(it == map_.end()) return std::nullopt;
        // Move to front
        lru_.erase(it->second.it);
        lru_.push_front(pts_us);
        it->second.it = lru_.begin();
        return it->second.frame;
    }

    size_t size() const {
        std::scoped_lock lock(mutex_);
        return lru_.size();
    }

    void clear() {
        std::scoped_lock lock(mutex_);
        lru_.clear();
        map_.clear();
    }

private:
    struct Entry {
        VideoFrame frame;
        std::list<int64_t>::iterator it;
    };
    size_t capacity_;
    mutable std::mutex mutex_;
    std::list<int64_t> lru_;
    std::unordered_map<int64_t, Entry> map_;
};

} // namespace ve::decode
