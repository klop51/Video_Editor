#pragma once
#include <string>
#include <optional>
#include <vector>

namespace ve { namespace timeline { class Timeline; }}

namespace ve::persistence {

struct SaveResult { bool success=false; std::string error; };
struct LoadResult { bool success=false; std::string error; };

SaveResult save_timeline_json(const timeline::Timeline& tl, const std::string& path) noexcept;
LoadResult load_timeline_json(timeline::Timeline& tl, const std::string& path) noexcept;

} // namespace ve::persistence
