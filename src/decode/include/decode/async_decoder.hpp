#pragma once
#include "decode/decoder.hpp"
#include "decode/frame_cache.hpp"
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <functional>

namespace ve::decode {

// Simple asynchronous decoding worker pulling frames ahead of current PTS.
class AsyncDecoder {
public:
    using VideoCallback = std::function<void(const VideoFrame&)>;

    AsyncDecoder(std::unique_ptr<IDecoder> decoder, size_t cache_capacity = 32)
        : decoder_(std::move(decoder)), cache_(cache_capacity) {}

    ~AsyncDecoder() { stop(); }

    bool start(int64_t start_pts_us, VideoCallback cb) {
        if(!decoder_) return false;
        callback_ = std::move(cb);
        running_ = true;
        target_pts_.store(start_pts_us, std::memory_order_relaxed);
        worker_ = std::thread(&AsyncDecoder::run, this);
        return true;
    }

    void request_seek(int64_t pts_us) {
        std::lock_guard lk(seek_mutex_);
        pending_seek_ = pts_us;
        seek_requested_ = true;
    }

    std::optional<VideoFrame> get_cached(int64_t pts_us) { return cache_.get(pts_us); }

    void stop() {
        if(running_) {
            running_ = false;
            if(worker_.joinable()) worker_.join();
        }
    }

private:
    void run();

    std::unique_ptr<IDecoder> decoder_;
    VideoFrameCache cache_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> target_pts_{0};
    VideoCallback callback_;

    std::mutex seek_mutex_;
    bool seek_requested_ = false;
    int64_t pending_seek_ = 0;
};

} // namespace ve::decode
