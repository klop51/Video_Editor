#include "core/log.hpp"
#include <mutex>
#include <iostream>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace ve::log {

static SinkFn g_sink;
static std::mutex g_mutex;
static std::atomic<bool> g_json{false};

static const char* level_str(Level lvl) {
    switch(lvl) {
        case Level::Trace: return "trace";
        case Level::Debug: return "debug";
        case Level::Info: return "info";
        case Level::Warn: return "warn";
        case Level::Error: return "error";
        case Level::Critical: return "critical";
    }
    return "unknown";
}

void set_sink(SinkFn sink) noexcept {
    std::scoped_lock lock(g_mutex);
    g_sink = std::move(sink);
}

void set_json_mode(bool enabled) noexcept { g_json.store(enabled, std::memory_order_relaxed); }
bool json_mode() noexcept { return g_json.load(std::memory_order_relaxed); }

static void default_emit(Level lvl, const std::string& msg) {
    if(json_mode()) {
        // Minimal JSON line: {"ts":"ISO8601","level":"info","msg":"..."}
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        ts << '.' << std::setw(3) << std::setfill('0') << ms.count();
        std::clog << '{' << "\"ts\":\"" << ts.str() << "\",\"level\":\"" << level_str(lvl) << "\",\"msg\":\"";
        for(char c: msg){ if(c=='"') std::clog << "\\\""; else if(c=='\\') std::clog << "\\\\"; else std::clog << c; }
        std::clog << "\"}" << '\n';
    } else {
        switch(lvl){
            case Level::Trace: spdlog::trace(msg); break;
            case Level::Debug: spdlog::debug(msg); break;
            case Level::Info: spdlog::info(msg); break;
            case Level::Warn: spdlog::warn(msg); break;
            case Level::Error: spdlog::error(msg); break;
            case Level::Critical: spdlog::critical(msg); break;
        }
    }
}

void write(Level lvl, const std::string& msg) noexcept {
    std::scoped_lock lock(g_mutex);
    if(g_sink) { g_sink(lvl, msg); return; }
    default_emit(lvl, msg);
}

void trace(const std::string& msg) noexcept { write(Level::Trace, msg); }
void debug(const std::string& msg) noexcept { write(Level::Debug, msg); }
void info(const std::string& msg) noexcept { write(Level::Info, msg); }
void warn(const std::string& msg) noexcept { write(Level::Warn, msg); }
void error(const std::string& msg) noexcept { write(Level::Error, msg); }
void critical(const std::string& msg) noexcept { write(Level::Critical, msg); }

} // namespace ve::log
