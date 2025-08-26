#pragma once
#include <cstdint>

namespace ve::audio {

class AudioEngine {
public:
    bool initialize();
    void shutdown();
};

} // namespace ve::audio
