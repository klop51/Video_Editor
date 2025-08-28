#include "core/time.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>

namespace ve {

TimeRational make_time(int64_t num, int32_t den) noexcept {
    if(den == 0) den = 1;
    return normalize(TimeRational{num, den});
}

Ticks to_ticks(const TimeRational& t) noexcept {
    // Convert to ticks: ticks = num * (TICKS_PER_SECOND / den)
    // Use long double for intermediate to reduce overflow risk; simple rounding.
    long double v = static_cast<long double>(t.num) * static_cast<long double>(TICKS_PER_SECOND) / static_cast<long double>(t.den);
    return static_cast<Ticks>(std::llround(v));
}

std::string format_timecode(const TimeRational& t, int32_t frame_rate_num, int32_t frame_rate_den) {
    // Convert to seconds as long double for formatting only.
    long double seconds = static_cast<long double>(t.num) / static_cast<long double>(t.den);
    if(frame_rate_den <= 0) frame_rate_den = 1;
    long double frame_rate = static_cast<long double>(frame_rate_num) / static_cast<long double>(frame_rate_den);
    if(frame_rate <= 0.0L) frame_rate = 24.0L;

    auto total_seconds = static_cast<int64_t>(seconds);
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds % 3600) / 60;
    int64_t secs = total_seconds % 60;
    long double fractional = seconds - static_cast<long double>(total_seconds);
    int64_t frame = static_cast<int64_t>(std::floor(fractional * frame_rate + 0.5L));

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ':'
        << std::setw(2) << minutes << ':'
        << std::setw(2) << secs << '.'
        << std::setw(2) << frame;
    return oss.str();
}

TimeRational normalize(const TimeRational& in) noexcept {
    if(in.den == 0) return TimeRational{0,1};
    int64_t num = in.num;
    int32_t den = in.den;
    if(den < 0) { den = -den; num = -num; }
    if(num == 0) return TimeRational{0,1};
    auto g = std::gcd(num < 0 ? -num : num, static_cast<int64_t>(den));
    if(g <= 1) return TimeRational{num, den};
    num /= g;
    den = static_cast<int32_t>(den / g);
    return TimeRational{num, den};
}

} // namespace ve
