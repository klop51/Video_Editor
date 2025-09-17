#include "audio/audio_buffer_pool.hpp"
#include <core/log.hpp>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <new>

namespace ve::audio {

// Helper function to get size in bytes for sample format
static uint32_t get_sample_size_bytes(SampleFormat format) {
    switch (format) {
        case SampleFormat::Int16: return 2;
        case SampleFormat::Int32: return 4;
        case SampleFormat::Float32: return 4;
        default: return 4;
    }
}

// Align memory address to specified boundary
static void* align_pointer(void* ptr, size_t alignment) {
    auto addr = reinterpret_cast<uintptr_t>(ptr);
    auto aligned = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

//============================================================================
// CircularAudioBuffer Implementation
//============================================================================

CircularAudioBuffer::CircularAudioBuffer(const AudioBufferConfig& config)
    : config_(config) {
    
    sample_size_bytes_ = get_sample_size_bytes(config_.sample_format);
    buffer_size_bytes_ = config_.buffer_size_samples * config_.channel_count * sample_size_bytes_;
    
    // Allocate aligned buffer with extra space for alignment
    size_t total_size = buffer_size_bytes_ + config_.alignment_bytes;
    buffer_data_.resize(total_size);
    
    std::ostringstream oss;
    oss << "CircularAudioBuffer created: " << config_.buffer_size_samples 
        << " samples, " << config_.channel_count << " channels, " << sample_size_bytes_ << " bytes per sample";
    ve::log::info(oss.str());
}

CircularAudioBuffer::~CircularAudioBuffer() = default;

uint32_t CircularAudioBuffer::write(const void* data, uint32_t sample_count) {
    if (!data || sample_count == 0) return 0;
    
    uint32_t available_space = available_write();
    uint32_t samples_to_write = std::min(sample_count, available_space);
    
    if (samples_to_write == 0) {
        overflow_count_.fetch_add(1);
        return 0;
    }
    
    uint32_t write_pos = write_index_.load();
    uint32_t bytes_per_frame = config_.channel_count * sample_size_bytes_;
    
    // Handle wrap-around writes
    uint32_t samples_to_end = config_.buffer_size_samples - write_pos;
    
    if (samples_to_write <= samples_to_end) {
        // Single contiguous write
        uint8_t* dest = get_write_ptr(write_pos);
        std::memcpy(dest, data, samples_to_write * bytes_per_frame);
    } else {
        // Split write (wrap around)
        uint8_t* dest1 = get_write_ptr(write_pos);
        const uint8_t* src = static_cast<const uint8_t*>(data);
        
        // Write to end of buffer
        std::memcpy(dest1, src, samples_to_end * bytes_per_frame);
        
        // Write remaining at beginning
        uint32_t remaining_samples = samples_to_write - samples_to_end;
        uint8_t* dest2 = get_write_ptr(0);
        std::memcpy(dest2, src + samples_to_end * bytes_per_frame, 
                   remaining_samples * bytes_per_frame);
    }
    
    // Update write index (atomic)
    uint32_t new_write_pos = (write_pos + samples_to_write) % config_.buffer_size_samples;
    write_index_.store(new_write_pos);
    
    return samples_to_write;
}

uint32_t CircularAudioBuffer::read(void* data, uint32_t sample_count) {
    if (!data || sample_count == 0) return 0;
    
    uint32_t available_data = available_read();
    uint32_t samples_to_read = std::min(sample_count, available_data);
    
    if (samples_to_read == 0) {
        underflow_count_.fetch_add(1);
        // Zero fill the requested data
        if (config_.zero_on_acquire) {
            zero_memory(data, sample_count);
        }
        return 0;
    }
    
    uint32_t read_pos = read_index_.load();
    uint32_t bytes_per_frame = config_.channel_count * sample_size_bytes_;
    
    // Handle wrap-around reads
    uint32_t samples_to_end = config_.buffer_size_samples - read_pos;
    
    if (samples_to_read <= samples_to_end) {
        // Single contiguous read
        const uint8_t* src = get_read_ptr(read_pos);
        std::memcpy(data, src, samples_to_read * bytes_per_frame);
    } else {
        // Split read (wrap around)
        const uint8_t* src1 = get_read_ptr(read_pos);
        uint8_t* dest = static_cast<uint8_t*>(data);
        
        // Read from current position to end
        std::memcpy(dest, src1, samples_to_end * bytes_per_frame);
        
        // Read remaining from beginning
        uint32_t remaining_samples = samples_to_read - samples_to_end;
        const uint8_t* src2 = get_read_ptr(0);
        std::memcpy(dest + samples_to_end * bytes_per_frame, src2, 
                   remaining_samples * bytes_per_frame);
    }
    
    // Zero fill any remaining requested data
    if (samples_to_read < sample_count && config_.zero_on_acquire) {
        uint8_t* dest = static_cast<uint8_t*>(data);
        uint32_t zero_bytes_per_frame = config_.channel_count * sample_size_bytes_;
        std::memset(dest + samples_to_read * zero_bytes_per_frame, 0, 
                   (sample_count - samples_to_read) * zero_bytes_per_frame);
    }
    
    // Update read index (atomic)
    uint32_t new_read_pos = (read_pos + samples_to_read) % config_.buffer_size_samples;
    read_index_.store(new_read_pos);
    
    return samples_to_read;
}

uint32_t CircularAudioBuffer::write_frame(const std::shared_ptr<AudioFrame>& frame) {
    if (!frame) return 0;
    return write(frame->data(), frame->sample_count());
}

std::shared_ptr<AudioFrame> CircularAudioBuffer::read_frame(uint32_t sample_count, const TimePoint& timestamp) {
    auto frame = AudioFrame::create(
        config_.sample_rate,
        config_.channel_count,
        sample_count,
        config_.sample_format,
        timestamp
    );
    
    if (!frame) return nullptr;
    
    uint32_t samples_read = read(frame->data(), sample_count);
    if (samples_read == 0) {
        return nullptr;
    }
    
    return frame;
}

uint32_t CircularAudioBuffer::available_read() const {
    uint32_t write_pos = write_index_.load();
    uint32_t read_pos = read_index_.load();
    
    if (write_pos >= read_pos) {
        return write_pos - read_pos;
    } else {
        return config_.buffer_size_samples - read_pos + write_pos;
    }
}

uint32_t CircularAudioBuffer::available_write() const {
    uint32_t used = available_read();
    return config_.buffer_size_samples - used - 1; // Leave one sample gap to distinguish full/empty
}

bool CircularAudioBuffer::is_empty() const {
    return available_read() == 0;
}

bool CircularAudioBuffer::is_full() const {
    return available_write() == 0;
}

void CircularAudioBuffer::clear() {
    read_index_.store(0);
    write_index_.store(0);
}

uint32_t CircularAudioBuffer::get_bytes_per_sample() const {
    return sample_size_bytes_;
}

uint8_t* CircularAudioBuffer::get_write_ptr(uint32_t offset_samples) {
    uint8_t* aligned_ptr = static_cast<uint8_t*>(align_pointer(buffer_data_.data(), config_.alignment_bytes));
    return aligned_ptr + offset_samples * config_.channel_count * sample_size_bytes_;
}

const uint8_t* CircularAudioBuffer::get_read_ptr(uint32_t offset_samples) const {
    const uint8_t* aligned_ptr = static_cast<const uint8_t*>(align_pointer(
        const_cast<uint8_t*>(buffer_data_.data()), config_.alignment_bytes));
    return aligned_ptr + offset_samples * config_.channel_count * sample_size_bytes_;
}

void CircularAudioBuffer::zero_memory(void* ptr, uint32_t sample_count) {
    uint32_t bytes_to_zero = sample_count * config_.channel_count * sample_size_bytes_;
    std::memset(ptr, 0, bytes_to_zero);
}

//============================================================================
// AudioBufferPool Implementation
//============================================================================

AudioBufferPool::AudioBufferPool(const AudioBufferConfig& config)
    : config_(config) {
    
    buffer_pool_.reserve(config_.pool_size);
    buffer_available_.resize(config_.pool_size, true);
    available_count_.store(config_.pool_size);
    
    // Pre-allocate all buffers
    for (uint32_t i = 0; i < config_.pool_size; ++i) {
        auto buffer = create_buffer(i);
        buffer_pool_.push_back(buffer);
    }
    
    std::ostringstream oss;
    oss << "AudioBufferPool created: " << config_.pool_size 
        << " buffers, " << config_.buffer_size_samples << " samples each";
    ve::log::info(oss.str());
}

AudioBufferPool::~AudioBufferPool() = default;

std::shared_ptr<AudioFrame> AudioBufferPool::acquire_buffer() {
    TimePoint default_timestamp(TimeRational(0, 1));
    return acquire_buffer(default_timestamp);
}

std::shared_ptr<AudioFrame> AudioBufferPool::acquire_buffer([[maybe_unused]] const TimePoint& timestamp) {
    uint32_t available = available_count_.load();
    if (available == 0) {
        allocation_failures_.fetch_add(1);
        return nullptr;
    }
    
    // Find next available buffer (lock-free)
    uint32_t start_index = next_index_.load();
    for (uint32_t i = 0; i < config_.pool_size; ++i) {
        uint32_t index = (start_index + i) % config_.pool_size;
        
        // Try to claim this buffer (simple approach without atomic CAS)
        if (buffer_available_[index]) {
            buffer_available_[index] = false;
            
            // Successfully claimed buffer
            available_count_.fetch_sub(1);
            next_index_.store((index + 1) % config_.pool_size);
            
            // Get the buffer and update its timestamp
            auto buffer = buffer_pool_[index];
            
            // Zero the buffer if requested
            if (config_.zero_on_acquire) {
                std::memset(buffer->data(), 0, 
                           buffer->sample_count() * buffer->channel_count() * 
                           get_sample_size_bytes(buffer->format()));
            }
            
            return buffer;
        }
    }
    
    // No buffer found (race condition)
    allocation_failures_.fetch_add(1);
    return nullptr;
}

void AudioBufferPool::release_buffer([[maybe_unused]] std::shared_ptr<AudioFrame> frame) {
    // This method is called by the custom deleter
    // The actual implementation is in return_buffer_to_pool
}

uint32_t AudioBufferPool::available_count() const {
    return available_count_.load();
}

bool AudioBufferPool::is_empty() const {
    return available_count() == 0;
}

bool AudioBufferPool::is_full() const {
    return available_count() == config_.pool_size;
}

std::shared_ptr<AudioFrame> AudioBufferPool::create_buffer([[maybe_unused]] uint32_t index) {
    // Create AudioFrame with custom deleter that returns to pool
    auto buffer = AudioFrame::create(
        config_.sample_rate,
        config_.channel_count,
        config_.buffer_size_samples,
        config_.sample_format,
        TimePoint(TimeRational(0, 1))
    );
    
    return buffer;
}

void AudioBufferPool::return_buffer_to_pool(uint32_t index) {
    if (index >= config_.pool_size) return;
    
    // Mark buffer as available
    buffer_available_[index] = true;
    available_count_.fetch_add(1);
}

void AudioBufferPool::BufferDeleter::operator()([[maybe_unused]] AudioFrame* frame) {
    if (pool_) {
        pool_->return_buffer_to_pool(index_);
    }
    // Don't delete the frame - it's managed by the pool
}

//============================================================================
// AudioStreamBuffer Implementation
//============================================================================

AudioStreamBuffer::AudioStreamBuffer(const AudioBufferConfig& config)
    : config_(config)
    , circular_buffer_(config)
    , buffer_pool_(config) {
}

bool AudioStreamBuffer::push_frame(const std::shared_ptr<AudioFrame>& frame) {
    if (!frame) return false;
    
    uint32_t samples_written = circular_buffer_.write_frame(frame);
    return samples_written == frame->sample_count();
}

std::shared_ptr<AudioFrame> AudioStreamBuffer::pop_frame(uint32_t sample_count, const TimePoint& timestamp) {
    return circular_buffer_.read_frame(sample_count, timestamp);
}

uint32_t AudioStreamBuffer::get_latency_samples() const {
    return circular_buffer_.available_read();
}

TimePoint AudioStreamBuffer::get_latency_time() const {
    uint32_t latency_samples = get_latency_samples();
    TimeRational latency_time(latency_samples, config_.sample_rate);
    return TimePoint(latency_time);
}

//============================================================================
// Utility Functions
//============================================================================

namespace buffer_utils {

uint32_t calculate_buffer_size(double target_latency_ms, uint32_t sample_rate) {
    double samples_exact = (target_latency_ms / 1000.0) * sample_rate;
    uint32_t samples = static_cast<uint32_t>(std::ceil(samples_exact));
    
    // Round to next power of 2 for efficiency
    uint32_t power_of_2 = 1;
    while (power_of_2 < samples) {
        power_of_2 *= 2;
    }
    
    // Clamp to reasonable range
    return std::clamp(power_of_2, 64u, 8192u);
}

double calculate_latency_ms(uint32_t buffer_size_samples, uint32_t sample_rate) {
    return (static_cast<double>(buffer_size_samples) / sample_rate) * 1000.0;
}

uint32_t recommend_pool_size(uint32_t buffer_size_samples) {
    // Recommend more buffers for smaller buffer sizes
    if (buffer_size_samples <= 128) return 16;
    if (buffer_size_samples <= 512) return 12;
    if (buffer_size_samples <= 1024) return 8;
    return 6;
}

uint32_t align_size(uint32_t size, uint32_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

} // namespace buffer_utils

} // namespace ve::audio