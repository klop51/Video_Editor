#pragma once

#include "audio_frame.hpp"
#include "decoder.hpp"  // for AudioError enum  
#include <core/time.hpp>
#include <atomic>
#include <memory>
#include <vector>

namespace ve::audio {

/**
 * @brief Configuration for audio buffer pool
 */
struct AudioBufferConfig {
    uint32_t buffer_size_samples = 1024;    // Buffer size in samples per channel
    uint16_t channel_count = 2;              // Number of audio channels
    SampleFormat sample_format = SampleFormat::Float32;
    uint32_t pool_size = 8;                  // Number of buffers in pool
    uint32_t sample_rate = 48000;            // Sample rate for timing calculations
    
    // Real-time performance settings
    bool lock_free = true;                   // Use lock-free operations
    bool zero_on_acquire = true;             // Zero buffer data on acquisition
    uint32_t alignment_bytes = 64;           // Memory alignment for SIMD operations
};

/**
 * @brief Lock-free circular buffer for real-time audio streaming
 * 
 * This buffer provides:
 * - Lock-free read/write operations for real-time threads
 * - Configurable size (64-2048 samples)
 * - Memory alignment for SIMD operations
 * - Overflow and underflow detection
 */
class CircularAudioBuffer {
public:
    /**
     * @brief Create circular buffer
     * @param config Buffer configuration
     */
    explicit CircularAudioBuffer(const AudioBufferConfig& config);
    
    /**
     * @brief Destructor
     */
    ~CircularAudioBuffer();
    
    // Non-copyable but movable
    CircularAudioBuffer(const CircularAudioBuffer&) = delete;
    CircularAudioBuffer& operator=(const CircularAudioBuffer&) = delete;
    CircularAudioBuffer(CircularAudioBuffer&&) = delete;
    CircularAudioBuffer& operator=(CircularAudioBuffer&&) = delete;
    
    /**
     * @brief Write audio data to buffer (producer side)
     * @param data Audio data to write (interleaved samples)
     * @param sample_count Number of samples per channel to write
     * @return Number of samples per channel actually written
     */
    uint32_t write(const void* data, uint32_t sample_count);
    
    /**
     * @brief Read audio data from buffer (consumer side)
     * @param data Output buffer for audio data
     * @param sample_count Number of samples per channel to read
     * @return Number of samples per channel actually read
     */
    uint32_t read(void* data, uint32_t sample_count);
    
    /**
     * @brief Write audio frame to buffer
     * @param frame Audio frame to write
     * @return Number of samples per channel actually written
     */
    uint32_t write_frame(const std::shared_ptr<AudioFrame>& frame);
    
    /**
     * @brief Read audio frame from buffer
     * @param sample_count Number of samples per channel to read
     * @param timestamp Timestamp for the frame
     * @return Audio frame with read data, or nullptr if not enough data
     */
    std::shared_ptr<AudioFrame> read_frame(uint32_t sample_count, const TimePoint& timestamp);
    
    /**
     * @brief Get number of samples per channel available for reading
     */
    uint32_t available_read() const;
    
    /**
     * @brief Get number of samples per channel available for writing
     */
    uint32_t available_write() const;
    
    /**
     * @brief Check if buffer is empty
     */
    bool is_empty() const;
    
    /**
     * @brief Check if buffer is full
     */
    bool is_full() const;
    
    /**
     * @brief Clear buffer (reset read/write positions)
     */
    void clear();
    
    /**
     * @brief Get buffer capacity in samples per channel
     */
    uint32_t capacity() const { return config_.buffer_size_samples; }
    
    /**
     * @brief Get channel count
     */
    uint16_t channel_count() const { return config_.channel_count; }
    
    /**
     * @brief Get sample format
     */
    SampleFormat format() const { return config_.sample_format; }
    
    /**
     * @brief Get overflow count (diagnostic)
     */
    uint64_t overflow_count() const { return overflow_count_.load(); }
    
    /**
     * @brief Get underflow count (diagnostic)
     */
    uint64_t underflow_count() const { return underflow_count_.load(); }

private:
    AudioBufferConfig config_;
    std::vector<uint8_t> buffer_data_;
    uint32_t sample_size_bytes_;
    uint32_t buffer_size_bytes_;
    
    // Lock-free ring buffer indices
    std::atomic<uint32_t> write_index_{0};
    std::atomic<uint32_t> read_index_{0};
    
    // Diagnostic counters
    mutable std::atomic<uint64_t> overflow_count_{0};
    mutable std::atomic<uint64_t> underflow_count_{0};
    
    // Helper methods
    uint32_t get_bytes_per_sample() const;
    uint8_t* get_write_ptr(uint32_t offset_samples);
    const uint8_t* get_read_ptr(uint32_t offset_samples) const;
    void zero_memory(void* ptr, uint32_t sample_count);
};

/**
 * @brief Pre-allocated buffer pool for real-time audio processing
 * 
 * This pool provides:
 * - Pre-allocated buffers to avoid malloc in audio thread
 * - Lock-free acquisition and release
 * - Automatic buffer recycling
 * - Memory alignment for SIMD operations
 */
class AudioBufferPool {
public:
    /**
     * @brief Create buffer pool
     * @param config Buffer configuration
     */
    explicit AudioBufferPool(const AudioBufferConfig& config);
    
    /**
     * @brief Destructor
     */
    ~AudioBufferPool();
    
    // Non-copyable
    AudioBufferPool(const AudioBufferPool&) = delete;
    AudioBufferPool& operator=(const AudioBufferPool&) = delete;
    
    /**
     * @brief Acquire buffer from pool
     * @return Audio frame buffer, or nullptr if pool is empty
     */
    std::shared_ptr<AudioFrame> acquire_buffer();
    
    /**
     * @brief Acquire buffer with specific timestamp
     * @param timestamp Timestamp for the buffer
     * @return Audio frame buffer, or nullptr if pool is empty
     */
    std::shared_ptr<AudioFrame> acquire_buffer(const TimePoint& timestamp);
    
    /**
     * @brief Release buffer back to pool (automatic via shared_ptr deleter)
     * @param frame Buffer to release
     */
    void release_buffer(std::shared_ptr<AudioFrame> frame);
    
    /**
     * @brief Get number of available buffers in pool
     */
    uint32_t available_count() const;
    
    /**
     * @brief Get total pool size
     */
    uint32_t pool_size() const { return config_.pool_size; }
    
    /**
     * @brief Get buffer configuration
     */
    const AudioBufferConfig& config() const { return config_; }
    
    /**
     * @brief Check if pool is empty
     */
    bool is_empty() const;
    
    /**
     * @brief Check if pool is full
     */
    bool is_full() const;
    
    /**
     * @brief Get allocation failure count (diagnostic)
     */
    uint64_t allocation_failures() const { return allocation_failures_.load(); }

private:
    AudioBufferConfig config_;
    
    // Buffer storage
    std::vector<std::shared_ptr<AudioFrame>> buffer_pool_;
    std::vector<bool> buffer_available_;
    
    // Lock-free pool management
    std::atomic<uint32_t> available_count_{0};
    std::atomic<uint32_t> next_index_{0};
    
    // Diagnostic counters
    mutable std::atomic<uint64_t> allocation_failures_{0};
    
    // Custom deleter for returning buffers to pool
    class BufferDeleter {
    public:
        explicit BufferDeleter(AudioBufferPool* pool, uint32_t index) 
            : pool_(pool), index_(index) {}
        
        void operator()(AudioFrame* frame);
        
    private:
        AudioBufferPool* pool_;
        uint32_t index_;
    };
    
    // Buffer creation and management
    std::shared_ptr<AudioFrame> create_buffer(uint32_t index);
    void return_buffer_to_pool(uint32_t index);
};

/**
 * @brief Multi-buffer audio streaming system
 * 
 * Combines circular buffers and buffer pools for complete real-time audio management
 */
class AudioStreamBuffer {
public:
    /**
     * @brief Create stream buffer
     * @param config Buffer configuration  
     */
    explicit AudioStreamBuffer(const AudioBufferConfig& config);
    
    /**
     * @brief Get circular buffer for streaming
     */
    CircularAudioBuffer& circular_buffer() { return circular_buffer_; }
    
    /**
     * @brief Get buffer pool for frame management
     */
    AudioBufferPool& buffer_pool() { return buffer_pool_; }
    
    /**
     * @brief Push audio frame to stream
     * @param frame Audio frame to push
     * @return True if successful, false if buffer full
     */
    bool push_frame(const std::shared_ptr<AudioFrame>& frame);
    
    /**
     * @brief Pop audio frame from stream
     * @param sample_count Number of samples per channel to pop
     * @param timestamp Timestamp for the frame
     * @return Audio frame, or nullptr if not enough data
     */
    std::shared_ptr<AudioFrame> pop_frame(uint32_t sample_count, const TimePoint& timestamp);
    
    /**
     * @brief Get stream latency in samples
     */
    uint32_t get_latency_samples() const;
    
    /**
     * @brief Get stream latency as time
     */
    TimePoint get_latency_time() const;

private:
    AudioBufferConfig config_;
    CircularAudioBuffer circular_buffer_;
    AudioBufferPool buffer_pool_;
};

/**
 * @brief Utility functions for audio buffer management
 */
namespace buffer_utils {
    /**
     * @brief Calculate optimal buffer size for given latency target
     * @param target_latency_ms Target latency in milliseconds
     * @param sample_rate Sample rate in Hz
     * @return Recommended buffer size in samples
     */
    uint32_t calculate_buffer_size(double target_latency_ms, uint32_t sample_rate);
    
    /**
     * @brief Calculate buffer latency in milliseconds
     * @param buffer_size_samples Buffer size in samples
     * @param sample_rate Sample rate in Hz
     * @return Latency in milliseconds
     */
    double calculate_latency_ms(uint32_t buffer_size_samples, uint32_t sample_rate);
    
    /**
     * @brief Get recommended pool size for buffer size
     */
    uint32_t recommend_pool_size(uint32_t buffer_size_samples);
    
    /**
     * @brief Align size to SIMD boundary
     */
    uint32_t align_size(uint32_t size, uint32_t alignment = 64);
}

} // namespace ve::audio