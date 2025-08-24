#pragma once
#include <string>
#include <functional>

namespace ve::log {

enum class Level { Trace, Debug, Info, Warn, Error, Critical };

using SinkFn = std::function<void(Level, const std::string&)>;

void set_sink(SinkFn sink) noexcept;

void write(Level lvl, const std::string& msg) noexcept;

	// Thin wrapper over spdlog allowing future pluggable sinks & JSON structured mode.
	void set_json_mode(bool enabled) noexcept;
	bool json_mode() noexcept;
	void trace(const std::string& msg) noexcept;
	void debug(const std::string& msg) noexcept;
	void info(const std::string& msg) noexcept;
	void warn(const std::string& msg) noexcept;
	void error(const std::string& msg) noexcept;
	void critical(const std::string& msg) noexcept;

} // namespace ve::log
