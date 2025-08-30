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

// Macro utilities to ensure correct expansion of __COUNTER__ when concatenated.
#define VE_PP_CAT(a,b) VE_PP_CAT_INNER(a,b)
#define VE_PP_CAT_INNER(a,b) a##b

// Standard scope macro – guarantees a unique identifier per expansion using __COUNTER__.
// Two-level concatenation avoids MSVC keeping the literal '__COUNTER__' token (which caused shadow warnings).
#define VE_PROFILE_SCOPE(name) ::ve::prof::ScopedTimer VE_PP_CAT(ve_prof_scope_u_, __COUNTER__){name}

namespace ve::prof { inline std::atomic<uint64_t>& uniq_counter(){ static std::atomic<uint64_t> c{0}; return c; } }

// Alias kept for legacy call sites; both behave identically now.
#define VE_PROFILE_SCOPE_UNIQ(name) VE_PROFILE_SCOPE(name)

// Optional detailed profiling gating: fine-grained hotspots (per-format, inner loops, GL sub-stages)
// can generate large sample counts. Define VE_ENABLE_DETAILED_PROFILING=0 to compile them out.
#ifndef VE_ENABLE_DETAILED_PROFILING
#define VE_ENABLE_DETAILED_PROFILING 1
#endif

#if VE_ENABLE_DETAILED_PROFILING
#define VE_PROFILE_SCOPE_DETAILED(name) VE_PROFILE_SCOPE(name)
#else
#define VE_PROFILE_SCOPE_DETAILED(name) do{}while(0)
#endif