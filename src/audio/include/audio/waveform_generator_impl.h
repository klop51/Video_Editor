/**
 * @file waveform_generator_impl.h
 * @brief High-Performance Waveform Generator Implementation
 * 
 * Implements efficient multi-threaded waveform generation with SIMD optimization
 * and memory-mapped file support for professional video editing workflows.
 */

#pragma once

#include "audio/waveform_generator.h"
#include "audio/audio_decoder.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <memory_resource>
#include <immintrin.h>  // For SIMD intrinsics

namespace ve::audio {

/**
 * @brief Work item for background waveform generation
 */
struct WaveformTask {
    std::string audio_source;
    std::pair<ve::TimePoint, ve::TimePoint> time_range;
    ZoomLevel zoom_level;
    uint32_t channel_mask;
    WaveformProgressCallback progress_callback;
    WaveformCompletionCallback completion_callback;
    std::promise<std::shared_ptr<WaveformData>> promise;
    std::atomic<bool> cancelled{false};
    
    WaveformTask(const std::string& source, 
                 const std::pair<ve::TimePoint, ve::TimePoint>& range,
                 const ZoomLevel& zoom,
                 uint32_t mask = 0)
        : audio_source(source), time_range(range), zoom_level(zoom), channel_mask(mask) {}
};

/**
 * @brief SIMD-optimized waveform calculation engine
 */
class SIMDWaveformProcessor {
public:
    /**
     * @brief Process audio samples to extract peak and RMS values using SIMD
     * @param samples Interleaved audio samples
     * @param sample_count Number of samples to process
     * @param channel_count Number of audio channels
     * @param samples_per_point Samples to aggregate per waveform point
     * @param output Output waveform points
     */
    static void process_samples_simd(
        const float* samples,
        size_t sample_count,
        size_t channel_count,
        size_t samples_per_point,
        std::vector<std::vector<WaveformPoint>>& output
    );
    
    /**
     * @brief Fallback non-SIMD processing for compatibility
     */
    static void process_samples_scalar(
        const float* samples,
        size_t sample_count,
        size_t channel_count,
        size_t samples_per_point,
        std::vector<std::vector<WaveformPoint>>& output
    );
    
    /**
     * @brief Check if SIMD instructions are available on current CPU
     */
    static bool is_simd_available();
    
private:
    /**
     * @brief Process 8 float samples at once using AVX
     */
    static void process_avx_chunk(
        const float* samples,
        size_t stride,
        float& peak_pos,
        float& peak_neg,
        float& rms_sum
    );
    
    /**
     * @brief Process 4 float samples at once using SSE
     */
    static void process_sse_chunk(
        const float* samples,
        size_t stride,
        float& peak_pos,
        float& peak_neg,
        float& rms_sum
    );
};

/**
 * @brief Memory pool for efficient waveform data allocation
 */
class WaveformMemoryPool {
public:
    explicit WaveformMemoryPool(size_t max_size_mb = 512);
    ~WaveformMemoryPool();
    
    /**
     * @brief Allocate memory for waveform data
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    
    /**
     * @brief Deallocate memory
     */
    void deallocate(void* ptr, size_t size);
    
    /**
     * @brief Get current memory usage
     */
    size_t get_usage() const { return current_usage_.load(); }
    
    /**
     * @brief Get maximum memory limit
     */
    size_t get_limit() const { return max_size_; }
    
    /**
     * @brief Clear all allocations (for cleanup)
     */
    void clear();

private:
    std::pmr::monotonic_buffer_resource memory_resource_;
    std::atomic<size_t> current_usage_{0};
    size_t max_size_;
    mutable std::mutex mutex_;
};

/**
 * @brief High-performance waveform generator implementation
 */
class WaveformGeneratorImpl : public WaveformGenerator {
public:
    explicit WaveformGeneratorImpl(const WaveformGeneratorConfig& config);
    ~WaveformGeneratorImpl() override;
    
    // WaveformGenerator interface implementation
    std::shared_ptr<WaveformData> generate_waveform(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const ZoomLevel& zoom_level,
        uint32_t channel_mask = 0
    ) override;
    
    std::future<std::shared_ptr<WaveformData>> generate_waveform_async(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const ZoomLevel& zoom_level,
        WaveformProgressCallback progress_callback = nullptr,
        WaveformCompletionCallback completion_callback = nullptr,
        uint32_t channel_mask = 0
    ) override;
    
    std::map<size_t, std::shared_ptr<WaveformData>> generate_multi_resolution(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const std::vector<ZoomLevel>& zoom_levels,
        WaveformProgressCallback progress_callback = nullptr
    ) override;
    
    std::shared_ptr<WaveformData> update_waveform(
        std::shared_ptr<WaveformData> existing_waveform,
        const std::vector<AudioFrame>& new_audio_data,
        const ve::TimePoint& insert_position
    ) override;
    
    bool cancel_generation(const std::string& audio_source) override;
    float get_generation_progress(const std::string& audio_source) const override;
    bool is_generating() const override;
    
    const WaveformGeneratorConfig& get_config() const override { return config_; }
    void set_config(const WaveformGeneratorConfig& config) override;

private:
    /**
     * @brief Worker thread function for background processing
     */
    void worker_thread();
    
    /**
     * @brief Process a single waveform generation task
     */
    std::shared_ptr<WaveformData> process_task(const WaveformTask& task);
    
    /**
     * @brief Load and decode audio data for processing
     */
    std::vector<AudioFrame> load_audio_data(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range
    );
    
    /**
     * @brief Convert audio frames to waveform points
     */
    std::shared_ptr<WaveformData> convert_to_waveform(
        const std::vector<AudioFrame>& audio_data,
        const ZoomLevel& zoom_level,
        uint32_t channel_mask,
        const ve::TimePoint& start_time
    );
    
    /**
     * @brief Update progress for a task
     */
    void update_progress(const std::string& audio_source, float progress, const std::string& status);
    
    /**
     * @brief Cleanup completed tasks
     */
    void cleanup_completed_tasks();
    
    // Configuration and state
    WaveformGeneratorConfig config_;
    std::unique_ptr<WaveformMemoryPool> memory_pool_;
    
    // Threading infrastructure
    std::vector<std::thread> worker_threads_;
    std::queue<std::shared_ptr<WaveformTask>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::atomic<bool> shutdown_{false};
    
    // Task tracking
    std::unordered_map<std::string, std::shared_ptr<WaveformTask>> active_tasks_;
    std::unordered_map<std::string, float> progress_map_;
    mutable std::mutex tracking_mutex_;
    
    // Audio decoder for file loading
    std::unique_ptr<AudioDecoder> audio_decoder_;
    
    // Performance statistics
    struct Stats {
        std::atomic<size_t> total_generated{0};
        std::atomic<size_t> total_bytes_processed{0};
        std::atomic<std::chrono::milliseconds> total_generation_time{0};
    } stats_;
};

} // namespace ve::audio