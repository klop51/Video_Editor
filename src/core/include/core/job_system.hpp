#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace ve::core {

class JobSystem {
public:
    using Job = std::function<void()>;
    static JobSystem& instance();
    void start(unsigned threads = std::thread::hardware_concurrency());
    void stop();
    void enqueue(Job job);
private:
    JobSystem() = default;
    ~JobSystem();
    void worker_loop();
    std::vector<std::thread> workers_;
    std::queue<Job> queue_;
    std::mutex m_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
};

} // namespace ve::core
