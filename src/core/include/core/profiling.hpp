#pragma once
#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>

namespace ve::prof {

struct Sample { std::string name; double ms = 0.0; };

// Thread-safe accumulator (very light; not optimized yet)
class Accumulator {
public:
    static Accumulator& instance();
    void add(Sample s);
    std::vector<Sample> snapshot();
private:
    std::mutex mtx_; std::vector<Sample> samples_;
};

class ScopedTimer {
public:
    explicit ScopedTimer(const char* name): name_(name), start_(Clock::now()) {}
    ~ScopedTimer();
private:
    using Clock = std::chrono::steady_clock;
    const char* name_; Clock::time_point start_;
};

} // namespace ve::prof

#define VE_PROFILE_SCOPE(name) ::ve::prof::ScopedTimer ve_prof_scope_##__LINE__{name}