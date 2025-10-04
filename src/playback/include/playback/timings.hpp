#pragma once

#include <chrono>
#include <string>

#include "../../core/include/core/log.hpp"

namespace ve::playback {

struct StageTimer {
    using clock = std::chrono::steady_clock;

    clock::time_point t0{};
    clock::time_point t1{};
    clock::time_point t2{};
    clock::time_point t3{};

    int sample_count{0};
    double decode_sum_us{0.0};
    double sws_sum_us{0.0};
    double upload_sum_us{0.0};
    double draw_sum_us{0.0};

    void begin() { t0 = clock::now(); }
    void afterDecode() { t1 = clock::now(); }
    void afterSws() { t2 = clock::now(); }
    void afterUpload() { t3 = clock::now(); }

    void endAndMaybeLog(const char* tag = "TIMING", int log_every = 120) {
        using std::chrono::duration_cast;
        using std::chrono::microseconds;

        const auto now = clock::now();

        const double dec_us = static_cast<double>(duration_cast<microseconds>(t1 - t0).count());
        const double sws_us = static_cast<double>(duration_cast<microseconds>(t2 - t1).count());
        const double upload_us = static_cast<double>(duration_cast<microseconds>(t3 - t2).count());
        const double draw_us = static_cast<double>(duration_cast<microseconds>(now - t3).count());

        decode_sum_us += dec_us;
        sws_sum_us += sws_us;
        upload_sum_us += upload_us;
        draw_sum_us += draw_us;
        ++sample_count;

        if (sample_count >= log_every) {
            const double inv = 1.0 / static_cast<double>(sample_count);
            ve::log::info(std::string(tag) + " avg_us: decode=" + std::to_string(decode_sum_us * inv) +
                          " sws=" + std::to_string(sws_sum_us * inv) +
                          " upload=" + std::to_string(upload_sum_us * inv) +
                          " draw=" + std::to_string(draw_sum_us * inv));
            decode_sum_us = sws_sum_us = upload_sum_us = draw_sum_us = 0.0;
            sample_count = 0;
        }
    }
};

} // namespace ve::playback
