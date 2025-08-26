#include "cache/frame_cache.hpp"

namespace ve::cache {

void FrameCache::clear() {
    map_.clear();
    lru_.clear();
}

bool FrameCache::get(FrameKey key, CachedFrame& out) {
    auto it = map_.find(key);
    if(it==map_.end()) return false;
    // promote
    lru_.splice(lru_.begin(), lru_, it->second.it);
    out = it->second.frame;
    return true;
}

void FrameCache::put(FrameKey key, CachedFrame frame) {
    auto it = map_.find(key);
    if(it!=map_.end()) {
        it->second.frame = std::move(frame);
        lru_.splice(lru_.begin(), lru_, it->second.it);
    } else {
        lru_.push_front(key);
        map_[key] = Entry{std::move(frame), lru_.begin()};
        evict_if_needed();
    }
}

void FrameCache::evict_if_needed() {
    while(map_.size()>max_items_ && !lru_.empty()) {
        auto last = lru_.back();
        map_.erase(last);
        lru_.pop_back();
    }
}

} // namespace ve::cache
