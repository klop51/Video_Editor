#pragma once
#include <cstdint>
#include <unordered_map>
#include <list>
#include <vector>
#include "decode/frame.hpp" // For PixelFormat & color metadata

namespace ve::cache {

struct FrameKey { int64_t pts_us = 0; };
struct CachedFrame {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    ve::decode::PixelFormat format = ve::decode::PixelFormat::Unknown;
    ve::decode::ColorSpace color_space = ve::decode::ColorSpace::Unknown;
    ve::decode::ColorRange color_range = ve::decode::ColorRange::Unknown;
};

class FrameCache {
public:
    explicit FrameCache(size_t max_items = 128): max_items_(max_items) {}
    void clear();
    void set_capacity(size_t n) { max_items_ = n; evict_if_needed(); }
    bool get(FrameKey key, CachedFrame& out);
    void put(FrameKey key, CachedFrame frame);
    size_t size() const { return map_.size(); }
private:
    void evict_if_needed();
    size_t max_items_;
    std::list<FrameKey> lru_;
    struct Entry { CachedFrame frame; std::list<FrameKey>::iterator it; };
    struct KeyHash { size_t operator()(const FrameKey& k) const { return std::hash<int64_t>()(k.pts_us); } };
    struct KeyEq { bool operator()(const FrameKey&a,const FrameKey&b) const { return a.pts_us==b.pts_us; } };
    std::unordered_map<FrameKey, Entry, KeyHash, KeyEq> map_;
};

} // namespace ve::cache
