// ChatGPT Stop Token System: One-shot stop coordination for all producers
#pragma once
#include <atomic>

namespace ve::playback {

struct StopToken {
    std::atomic<int> state{0}; // 0=running, 1=stopping
    
    // Returns true only the first time it's called (edge-triggered)
    bool trip() {
        int expected = 0;
        return state.compare_exchange_strong(expected, 1, std::memory_order_acq_rel);
    }
    
    bool is_set() const { 
        return state.load(std::memory_order_acquire) != 0; 
    }
};

} // namespace ve::playback