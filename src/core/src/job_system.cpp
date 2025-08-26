#include "core/job_system.hpp"
#include <cassert>

namespace ve::core {

JobSystem& JobSystem::instance() { static JobSystem js; return js; }

JobSystem::~JobSystem() { stop(); }

void JobSystem::start(unsigned threads) {
    if(running_) return;
    running_ = true;
    if(threads==0) threads = 1;
    workers_.reserve(threads);
    for(unsigned i=0;i<threads;++i){
        workers_.emplace_back([this]{ worker_loop(); });
    }
}

void JobSystem::stop() {
    if(!running_) return;
    {
        std::lock_guard<std::mutex> lk(m_);
        running_ = false;
    }
    cv_.notify_all();
    for(auto& w : workers_) if(w.joinable()) w.join();
    workers_.clear();
}

void JobSystem::enqueue(Job job) {
    if(!running_) start();
    {
        std::lock_guard<std::mutex> lk(m_);
        queue_.push(std::move(job));
    }
    cv_.notify_one();
}

void JobSystem::worker_loop() {
    while(true){
        Job job;
        {
            std::unique_lock<std::mutex> lk(m_);
            cv_.wait(lk, [&]{ return !running_ || !queue_.empty(); });
            if(!running_ && queue_.empty()) break;
            job = std::move(queue_.front()); queue_.pop();
        }
        if(job) job();
    }
}

} // namespace ve::core
