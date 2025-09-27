# RealTimeLoudnessMonitor Stack Corruption Analysis

## Root Cause Investigation Results

### 1. **Stack Alignment Issues Identified**

**Problem**: Mixed data type alignment in class members:
```cpp
class RealTimeLoudnessMonitor {
private:
    double sample_rate_ = 48000.0;           // 8 bytes, aligned to 8
    uint16_t channels_ = 2;                  // 2 bytes, creates padding
    // [6 bytes padding inserted by compiler]
    
    size_t momentary_window_samples_;        // 8 bytes on 64-bit
    size_t short_term_window_samples_;       // 8 bytes
    size_t momentary_write_pos_ = 0;         // 8 bytes
    size_t short_term_write_pos_ = 0;        // 8 bytes
    
    // 4 AudioLevelMeter objects - each contains:
    AudioLevelMeter peak_meter_left_;        // ~64 bytes each
    AudioLevelMeter peak_meter_right_;       // Multiple std::chrono::time_point objects
    AudioLevelMeter rms_meter_left_;         // with potential alignment issues
    AudioLevelMeter rms_meter_right_;
    
    std::atomic<uint64_t> samples_processed_{0};  // Atomic requires alignment
    double integrated_sum_squares_ = 0.0;    // 8 bytes
    uint64_t integrated_sample_count_ = 0;   // 8 bytes
    
    mutable std::mutex measurement_mutex_;   // OS-dependent alignment requirements
    LoudnessMeasurement current_measurement_; // Large struct with mixed alignment
};
```

**Issue**: The `uint16_t channels_` between two `double` values creates padding and potential alignment corruption.

### 2. **Calling Convention Problems**

**Problem**: Method signature mismatch potential:
```cpp
void process_samples(const AudioFrame& frame);  // Reference parameter
```

The crash happens on **return instruction**, suggesting:
- Stack frame corruption during parameter passing
- Reference parameter causing ABI issues
- Return address corruption during stack unwinding

### 3. **Memory Corruption During Processing**

**Identified Issues**:

#### A. **Atomic Variable Corruption**
```cpp
std::atomic<uint64_t> samples_processed_{0};
samples_processed_ += static_cast<uint64_t>(frame_sample_count);  // This line works fine
// But corruption may occur during atomic destructor
```

#### B. **AudioLevelMeter Constructor/Destructor Issues**
```cpp
AudioLevelMeter peak_meter_left_;   // 4 instances in the class
AudioLevelMeter peak_meter_right_;  // Each contains std::chrono::time_point
AudioLevelMeter rms_meter_left_;    // which may have OS-specific alignment
AudioLevelMeter rms_meter_right_;
```

The `AudioLevelMeter` contains:
- `std::chrono::steady_clock::time_point` objects (platform-specific size/alignment)
- `MeterBallistics` struct with multiple `double` values
- Potential constructor/destructor stack corruption

#### C. **Mutex Destructor Corruption**
```cpp
mutable std::mutex measurement_mutex_;  // OS mutex has specific alignment requirements
```
Mutex destruction during stack unwinding could corrupt stack frame.

### 4. **Alternative Implementation Approaches**

#### **Approach A: Stack Alignment Fix**
```cpp
class alignas(64) RealTimeLoudnessMonitor {  // Force 64-byte alignment
private:
    // Reorder members for optimal alignment
    double sample_rate_ = 48000.0;                    // 8 bytes
    double integrated_sum_squares_ = 0.0;             // 8 bytes  
    double relative_threshold_lufs_;                  // 8 bytes
    
    uint64_t integrated_sample_count_ = 0;            // 8 bytes
    std::atomic<uint64_t> samples_processed_{0};      // 8 bytes, atomic-aligned
    
    size_t momentary_window_samples_;                 // 8 bytes
    size_t short_term_window_samples_;                // 8 bytes
    size_t momentary_write_pos_ = 0;                  // 8 bytes
    size_t short_term_write_pos_ = 0;                 // 8 bytes
    
    uint16_t channels_ = 2;                           // 2 bytes
    // [6 bytes padding to next 8-byte boundary]
    
    // Move meters to end to avoid alignment conflicts
    AudioLevelMeter peak_meter_left_;
    AudioLevelMeter peak_meter_right_;
    AudioLevelMeter rms_meter_left_;
    AudioLevelMeter rms_meter_right_;
    
    // Heap-allocated to avoid stack issues
    std::unique_ptr<std::mutex> measurement_mutex_;   // Heap-allocated mutex
    std::unique_ptr<LoudnessMeasurement> current_measurement_;
};
```

#### **Approach B: Interface Segregation**
```cpp
// Split into smaller, safer interfaces
class SimpleLoudnessProcessor {
public:
    // Simple function interface instead of complex method
    static LoudnessResult process_audio_samples(
        const float* samples,           // Raw pointer instead of reference
        size_t sample_count,           // Explicit size
        size_t channel_count,          // No complex objects
        double sample_rate
    );
};

// Separate state management
class LoudnessState {
private:
    // All members heap-allocated to avoid stack corruption
    std::unique_ptr<InternalState> state_;
public:
    void update(const LoudnessResult& result);
    LoudnessMeasurement get_measurement() const;
};
```

#### **Approach C: C-Style Interface**
```cpp
// Eliminate C++ complexity entirely for this critical path
extern "C" {
    typedef struct loudness_result {
        double momentary_lufs;
        double short_term_lufs;
        int error_code;
    } loudness_result_t;
    
    // Simple C function - no stack corruption risk
    loudness_result_t process_loudness_samples(
        const float* samples,
        size_t sample_count,
        size_t channels,
        double sample_rate
    );
}
```

### 5. **Immediate Fix Recommendations**

#### **Option 1: Stack Alignment Fix (Safest)**
```cpp
class alignas(32) RealTimeLoudnessMonitor {
    // Reorder all members by size (largest to smallest)
    // Use heap allocation for all complex objects
    // Eliminate std::atomic if possible
};
```

#### **Option 2: Interface Simplification (Recommended)**
```cpp
// Replace the complex method with simple function
void process_samples_safe(
    const float* left_channel,      // Raw pointers instead of AudioFrame&
    const float* right_channel,
    size_t sample_count            // No complex objects on stack
);
```

#### **Option 3: Disable Problematic Features (Current)**
```cpp
// Current Phase P solution works but loses functionality
if (loudness_monitor_) {  
    ve::log::info("PHASE P: Loudness monitor temporarily DISABLED");
    // Skip the problematic call entirely
}
```

### 6. **Recommended Solution Path**

1. **Immediate**: Keep Phase P solution active (stability first)
2. **Short-term**: Implement Interface Simplification (Option 2)
3. **Long-term**: Full stack alignment rewrite (Option 1)

The stack corruption occurs during the **return instruction** because:
- Complex object destructors (AudioLevelMeter, std::mutex) run during stack unwinding
- Mixed alignment causes stack frame corruption
- Atomic variables may have platform-specific destruction issues

**The safest fix is to simplify the interface and eliminate complex stack objects.**