#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "core/log.hpp"

namespace ve::core {

class StageTimer {
public:
    using clock = std::chrono::steady_clock;

    void begin() { t0_ = clock::now(); }
    void afterDecode() { t1_ = clock::now(); }
    void afterConversion() { t2_ = clock::now(); }
    void afterUpload() { t3_ = clock::now(); }

    void endAndMaybeLog(const char* tag = "TIMING", int log_every = 120) {
        using std::chrono::duration_cast;
        using std::chrono::microseconds;

        const auto now = clock::now();

        const double decode_us = static_cast<double>(duration_cast<microseconds>(t1_ - t0_).count());
        const double convert_us = static_cast<double>(duration_cast<microseconds>(t2_ - t1_).count());
        const double upload_us = static_cast<double>(duration_cast<microseconds>(t3_ - t2_).count());
        const double draw_us = static_cast<double>(duration_cast<microseconds>(now - t3_).count());

        auto& agg = aggregation();
        std::scoped_lock lk(agg.mutex);
        agg.decode_sum += decode_us;
        agg.convert_sum += convert_us;
        agg.upload_sum += upload_us;
        agg.draw_sum += draw_us;
        ++agg.samples;
        if (agg.samples >= log_every) {
            const double inv = 1.0 / static_cast<double>(agg.samples);
            ve::log::info(std::string(tag) + " avg_us: decode=" + std::to_string(agg.decode_sum * inv) +
                          " sws=" + std::to_string(agg.convert_sum * inv) +
                          " upload=" + std::to_string(agg.upload_sum * inv) +
                          " draw=" + std::to_string(agg.draw_sum * inv));
            agg.reset();
        }
    }

private:
    struct Aggregation {
        std::mutex mutex;
        int samples{0};
        double decode_sum{0.0};
        double convert_sum{0.0};
        double upload_sum{0.0};
        double draw_sum{0.0};

        void reset() {
            samples = 0;
            decode_sum = convert_sum = upload_sum = draw_sum = 0.0;
        }
    };

    static Aggregation& aggregation() {
        static Aggregation agg;
        return agg;
    }

    clock::time_point t0_{};
    clock::time_point t1_{};
    clock::time_point t2_{};
    clock::time_point t3_{};
};

} // namespace ve::core
