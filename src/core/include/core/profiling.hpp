#pragma once
#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <fstream>

namespace ve::prof {

struct Sample { std::string name; double ms = 0.0; };

// Thread-safe accumulator (very light; not optimized yet)
class Accumulator {
public:
    static Accumulator& instance();
    void add(Sample s);
    std::vector<Sample> snapshot();
    // Aggregate by name -> stats structure
    struct Stats { size_t count=0; double total_ms=0.0; double min_ms=0.0; double max_ms=0.0; double p50_ms=0.0; double p95_ms=0.0; double avg_ms=0.0; };
    std::unordered_map<std::string, Stats> aggregate();
    // Write aggregation to JSON file (very small hand-rolled JSON). Includes avg, p50, p95, max, min.
    bool write_json(const std::string& path);
    // Testing / diagnostic helper: clear all collected samples (not thread-safe with concurrent add usage).
    void clear();
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