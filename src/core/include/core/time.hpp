#pragma once
#include <cstdint>
#include <string>

namespace ve {

// Rational time representation to avoid floating drift.
struct TimeRational {
    int64_t num{0}; // numerator
    int32_t den{1}; // denominator ( >0 )
    
    // Comparison operators
    bool operator==(const TimeRational& other) const {
        return num * other.den == other.num * den;
    }
    bool operator!=(const TimeRational& other) const { return !(*this == other); }
    bool operator<(const TimeRational& other) const {
        return num * other.den < other.num * den;
    }
    bool operator<=(const TimeRational& other) const { return *this < other || *this == other; }
    bool operator>(const TimeRational& other) const { return !(*this <= other); }
    bool operator>=(const TimeRational& other) const { return !(*this < other); }
};

// Time point (absolute position in timeline)
class TimePoint {
public:
    TimePoint() = default;
    TimePoint(int64_t num, int32_t den = 1) : rational_{num, den} {}
    explicit TimePoint(const TimeRational& rational) : rational_(rational) {}
    
    const TimeRational& to_rational() const { return rational_; }
    
    // Comparison operators
    bool operator==(const TimePoint& other) const { return rational_ == other.rational_; }
    bool operator!=(const TimePoint& other) const { return rational_ != other.rational_; }
    bool operator<(const TimePoint& other) const { return rational_ < other.rational_; }
    bool operator<=(const TimePoint& other) const { return rational_ <= other.rational_; }
    bool operator>(const TimePoint& other) const { return rational_ > other.rational_; }
    bool operator>=(const TimePoint& other) const { return rational_ >= other.rational_; }
    
private:
    TimeRational rational_;
};

// Time duration (relative time span)
class TimeDuration {
public:
    TimeDuration() = default;
    TimeDuration(int64_t num, int32_t den = 1) : rational_{num, den} {}
    explicit TimeDuration(const TimeRational& rational) : rational_(rational) {}
    
    const TimeRational& to_rational() const { return rational_; }
    
    // Comparison operators
    bool operator==(const TimeDuration& other) const { return rational_ == other.rational_; }
    bool operator!=(const TimeDuration& other) const { return rational_ != other.rational_; }
    bool operator<(const TimeDuration& other) const { return rational_ < other.rational_; }
    bool operator<=(const TimeDuration& other) const { return rational_ <= other.rational_; }
    bool operator>(const TimeDuration& other) const { return rational_ > other.rational_; }
    bool operator>=(const TimeDuration& other) const { return rational_ >= other.rational_; }
    
private:
    TimeRational rational_;
};

// Compact tick representation (optional pathway) - 1 tick = 1/48000 second (audio rate)
using Ticks = int64_t;
constexpr int64_t TICKS_PER_SECOND = 48000; // initial assumption (TBD)

TimeRational make_time(int64_t num, int32_t den) noexcept;

// Convert rational to ticks (rounded to nearest). Undefined if den==0.
Ticks to_ticks(const TimeRational& t) noexcept;

// Human readable string (e.g., 01:02:03.123)
std::string format_timecode(const TimeRational& t, int32_t frame_rate_num = 24, int32_t frame_rate_den = 1);

} // namespace ve
