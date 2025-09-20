/**
 * @file waveform_generator.cpp
 * @brief Implementation of High-Performance Waveform Generator
 */

#include "audio/waveform_generator_impl.h"
#include "audio/audio_decoder.h"
#include "core/logging.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>

namespace ve::audio {

// Define common zoom levels
const ZoomLevel ZoomLevel::SAMPLE_VIEW{1, "Sample View"};
const ZoomLevel ZoomLevel::DETAILED_VIEW{10, "Detailed View"};
const ZoomLevel ZoomLevel::NORMAL_VIEW{100, "Normal View"};
const ZoomLevel ZoomLevel::OVERVIEW{1000, "Overview"};
const ZoomLevel ZoomLevel::TIMELINE_VIEW{10000, "Timeline View"};

//=============================================================================
// SIMD Waveform Processor Implementation
//=============================================================================

bool SIMDWaveformProcessor::is_simd_available() {
    #ifdef __AVX__
        return true;
    #elif defined(__SSE__)
        return true;
    #else
        return false;
    #endif
}

void SIMDWaveformProcessor::process_samples_simd(
    const float* samples,
    size_t sample_count,
    size_t channel_count,
    size_t samples_per_point,
    std::vector<std::vector<WaveformPoint>>& output) {
    
    if (!is_simd_available()) {
        process_samples_scalar(samples, sample_count, channel_count, samples_per_point, output);
        return;
    }
    
    const size_t points_needed = (sample_count / channel_count) / samples_per_point;
    output.resize(channel_count);
    for (auto& channel : output) {
        channel.resize(points_needed);
    }
    
    // Process each channel separately for better cache locality
    for (size_t ch = 0; ch < channel_count; ++ch) {
        const float* channel_samples = samples + ch;
        
        for (size_t point_idx = 0; point_idx < points_needed; ++point_idx) {
            float peak_pos = 0.0f;
            float peak_neg = 0.0f;
            float rms_sum = 0.0f;
            
            const size_t start_sample = point_idx * samples_per_point;
            const size_t end_sample = std::min(start_sample + samples_per_point, 
                                              sample_count / channel_count);
            
            #ifdef __AVX__
            // Process 8 samples at a time with AVX
            size_t simd_end = start_sample + ((end_sample - start_sample) & ~7ULL);
            for (size_t i = start_sample; i < simd_end; i += 8) {
                process_avx_chunk(channel_samples + i * channel_count, 
                                channel_count, peak_pos, peak_neg, rms_sum);
            }
            
            // Process remaining samples
            for (size_t i = simd_end; i < end_sample; ++i) {
                const float sample = channel_samples[i * channel_count];
                peak_pos = std::max(peak_pos, sample);
                peak_neg = std::min(peak_neg, sample);
                rms_sum += sample * sample;
            }
            #elif defined(__SSE__)
            // Process 4 samples at a time with SSE
            size_t simd_end = start_sample + ((end_sample - start_sample) & ~3ULL);
            for (size_t i = start_sample; i < simd_end; i += 4) {
                process_sse_chunk(channel_samples + i * channel_count, 
                                channel_count, peak_pos, peak_neg, rms_sum);
            }
            
            // Process remaining samples
            for (size_t i = simd_end; i < end_sample; ++i) {
                const float sample = channel_samples[i * channel_count];
                peak_pos = std::max(peak_pos, sample);
                peak_neg = std::min(peak_neg, sample);
                rms_sum += sample * sample;
            }
            #endif
            
            const float rms_value = std::sqrt(rms_sum / (end_sample - start_sample));
            output[ch][point_idx] = WaveformPoint(peak_pos, peak_neg, rms_value);
        }
    }
}

void SIMDWaveformProcessor::process_samples_scalar(
    const float* samples,
    size_t sample_count,
    size_t channel_count,
    size_t samples_per_point,
    std::vector<std::vector<WaveformPoint>>& output) {
    
    const size_t points_needed = (sample_count / channel_count) / samples_per_point;
    output.resize(channel_count);
    for (auto& channel : output) {
        channel.resize(points_needed);
    }
    
    // Process each channel
    for (size_t ch = 0; ch < channel_count; ++ch) {
        for (size_t point_idx = 0; point_idx < points_needed; ++point_idx) {
            float peak_pos = 0.0f;
            float peak_neg = 0.0f;
            float rms_sum = 0.0f;
            
            const size_t start_sample = point_idx * samples_per_point;
            const size_t end_sample = std::min(start_sample + samples_per_point, 
                                              sample_count / channel_count);
            
            for (size_t i = start_sample; i < end_sample; ++i) {
                const float sample = samples[i * channel_count + ch];
                peak_pos = std::max(peak_pos, sample);
                peak_neg = std::min(peak_neg, sample);
                rms_sum += sample * sample;
            }
            
            const float rms_value = std::sqrt(rms_sum / (end_sample - start_sample));
            output[ch][point_idx] = WaveformPoint(peak_pos, peak_neg, rms_value);
        }
    }
}

#ifdef __AVX__
void SIMDWaveformProcessor::process_avx_chunk(
    const float* samples,
    size_t stride,
    float& peak_pos,
    float& peak_neg,
    float& rms_sum) {
    
    // Load 8 samples with stride
    __m256 sample_vec = _mm256_set_ps(
        samples[7 * stride], samples[6 * stride], 
        samples[5 * stride], samples[4 * stride],
        samples[3 * stride], samples[2 * stride], 
        samples[1 * stride], samples[0 * stride]
    );
    
    // Update peaks
    __m256 current_max = _mm256_set1_ps(peak_pos);
    __m256 current_min = _mm256_set1_ps(peak_neg);
    
    current_max = _mm256_max_ps(current_max, sample_vec);
    current_min = _mm256_min_ps(current_min, sample_vec);
    
    // Extract maximum and minimum values
    float max_vals[8], min_vals[8];
    _mm256_storeu_ps(max_vals, current_max);
    _mm256_storeu_ps(min_vals, current_min);
    
    for (int i = 0; i < 8; ++i) {
        peak_pos = std::max(peak_pos, max_vals[i]);
        peak_neg = std::min(peak_neg, min_vals[i]);
    }
    
    // Update RMS sum
    __m256 squared = _mm256_mul_ps(sample_vec, sample_vec);
    float squared_vals[8];
    _mm256_storeu_ps(squared_vals, squared);
    
    for (int i = 0; i < 8; ++i) {
        rms_sum += squared_vals[i];
    }
}
#endif

#ifdef __SSE__
void SIMDWaveformProcessor::process_sse_chunk(
    const float* samples,
    size_t stride,
    float& peak_pos,
    float& peak_neg,
    float& rms_sum) {
    
    // Load 4 samples with stride
    __m128 sample_vec = _mm_set_ps(
        samples[3 * stride], samples[2 * stride], 
        samples[1 * stride], samples[0 * stride]
    );
    
    // Update peaks
    __m128 current_max = _mm_set1_ps(peak_pos);
    __m128 current_min = _mm_set1_ps(peak_neg);
    
    current_max = _mm_max_ps(current_max, sample_vec);
    current_min = _mm_min_ps(current_min, sample_vec);
    
    // Extract values
    float max_vals[4], min_vals[4];
    _mm_storeu_ps(max_vals, current_max);
    _mm_storeu_ps(min_vals, current_min);
    
    for (int i = 0; i < 4; ++i) {
        peak_pos = std::max(peak_pos, max_vals[i]);
        peak_neg = std::min(peak_neg, min_vals[i]);
    }
    
    // Update RMS sum
    __m128 squared = _mm_mul_ps(sample_vec, sample_vec);
    float squared_vals[4];
    _mm_storeu_ps(squared_vals, squared);
    
    for (int i = 0; i < 4; ++i) {
        rms_sum += squared_vals[i];
    }
}
#endif

//=============================================================================
// Memory Pool Implementation
//=============================================================================

WaveformMemoryPool::WaveformMemoryPool(size_t max_size_mb) 
    : max_size_(max_size_mb * 1024 * 1024) {
    // Initialize memory resource with initial buffer
    static char initial_buffer[64 * 1024]; // 64KB initial buffer
    memory_resource_ = std::pmr::monotonic_buffer_resource(initial_buffer, sizeof(initial_buffer));
}

WaveformMemoryPool::~WaveformMemoryPool() {
    clear();
}

void* WaveformMemoryPool::allocate(size_t size, size_t alignment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_usage_.load() + size > max_size_) {
        throw std::bad_alloc();
    }
    
    void* ptr = memory_resource_.allocate(size, alignment);
    current_usage_ += size;
    return ptr;
}

void WaveformMemoryPool::deallocate(void* ptr, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_resource_.deallocate(ptr, size);
    current_usage_ -= size;
}

void WaveformMemoryPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_resource_.release();
    current_usage_ = 0;
}

//=============================================================================
// Waveform Generator Implementation
//=============================================================================

WaveformGeneratorImpl::WaveformGeneratorImpl(const WaveformGeneratorConfig& config)
    : config_(config)
    , memory_pool_(std::make_unique<WaveformMemoryPool>(config.max_memory_usage_mb))
    , audio_decoder_(AudioDecoder::create()) {
    
    // Start worker threads
    for (size_t i = 0; i < config_.max_concurrent_workers; ++i) {
        worker_threads_.emplace_back(&WaveformGeneratorImpl::worker_thread, this);
    }
    
    ve::log_info("WaveformGenerator initialized with {} workers, {}MB memory limit, SIMD: {}",
                config_.max_concurrent_workers,
                config_.max_memory_usage_mb,
                config_.enable_simd_optimization && SIMDWaveformProcessor::is_simd_available() ? "enabled" : "disabled");
}

WaveformGeneratorImpl::~WaveformGeneratorImpl() {
    shutdown_ = true;
    queue_condition_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    ve::log_info("WaveformGenerator shutdown. Stats: {} waveforms generated, {} bytes processed, {}ms total time",
                stats_.total_generated.load(),
                stats_.total_bytes_processed.load(),
                stats_.total_generation_time.load().count());
}

std::shared_ptr<WaveformData> WaveformGeneratorImpl::generate_waveform(
    const std::string& audio_source,
    const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
    const ZoomLevel& zoom_level,
    uint32_t channel_mask) {
    
    auto task = std::make_shared<WaveformTask>(audio_source, time_range, zoom_level, channel_mask);
    auto future = task->promise.get_future();
    
    // Process synchronously
    auto result = process_task(*task);
    
    if (result) {
        stats_.total_generated++;
        stats_.total_bytes_processed += waveform_utils::calculate_memory_usage(*result);
    }
    
    return result;
}

std::future<std::shared_ptr<WaveformData>> WaveformGeneratorImpl::generate_waveform_async(
    const std::string& audio_source,
    const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
    const ZoomLevel& zoom_level,
    WaveformProgressCallback progress_callback,
    WaveformCompletionCallback completion_callback,
    uint32_t channel_mask) {
    
    auto task = std::make_shared<WaveformTask>(audio_source, time_range, zoom_level, channel_mask);
    task->progress_callback = progress_callback;
    task->completion_callback = completion_callback;
    
    auto future = task->promise.get_future();
    
    // Add to queue for background processing
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
        
        std::lock_guard<std::mutex> tracking_lock(tracking_mutex_);
        active_tasks_[audio_source] = task;
        progress_map_[audio_source] = 0.0f;
    }
    
    queue_condition_.notify_one();
    return future;
}

std::map<size_t, std::shared_ptr<WaveformData>> WaveformGeneratorImpl::generate_multi_resolution(
    const std::string& audio_source,
    const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
    const std::vector<ZoomLevel>& zoom_levels,
    WaveformProgressCallback progress_callback) {
    
    std::map<size_t, std::shared_ptr<WaveformData>> results;
    
    // Load audio data once for all zoom levels
    auto audio_data = load_audio_data(audio_source, time_range);
    if (audio_data.empty()) {
        ve::log_error("Failed to load audio data for multi-resolution generation: {}", audio_source);
        return results;
    }
    
    // Generate each zoom level
    for (size_t i = 0; i < zoom_levels.size(); ++i) {
        const auto& zoom_level = zoom_levels[i];
        
        if (progress_callback) {
            float progress = static_cast<float>(i) / zoom_levels.size();
            progress_callback(progress, "Generating zoom level: " + zoom_level.name);
        }
        
        auto waveform_data = convert_to_waveform(audio_data, zoom_level, 0, time_range.first);
        if (waveform_data) {
            results[zoom_level.samples_per_point] = waveform_data;
        }
    }
    
    if (progress_callback) {
        progress_callback(1.0f, "Multi-resolution generation complete");
    }
    
    return results;
}

std::shared_ptr<WaveformData> WaveformGeneratorImpl::update_waveform(
    std::shared_ptr<WaveformData> existing_waveform,
    const std::vector<AudioFrame>& new_audio_data,
    const ve::TimePoint& insert_position) {
    
    // Implement incremental updates for better performance
    if (!existing_waveform || new_audio_data.empty()) {
        ve::log_warning("Invalid parameters for waveform update");
        return existing_waveform;
    }
    
    // Convert insert position to waveform point index
    auto samples_per_point = existing_waveform->samples_per_point;
    auto sample_rate = existing_waveform->sample_rate;
    
    // Calculate insert position in samples and waveform points
    auto insert_sample = static_cast<size_t>(insert_position.to_seconds() * sample_rate);
    auto insert_point = insert_sample / samples_per_point;
    
    // Create new waveform data for the inserted audio
    ZoomLevel insert_zoom_level(samples_per_point, "Insert Level");
    auto new_waveform_data = convert_to_waveform(new_audio_data, insert_zoom_level, 0, insert_position);
    if (!new_waveform_data) {
        ve::log_warning("Failed to convert new audio frames to waveform data");
        return existing_waveform;
    }
    
    // Create updated waveform by inserting new data
    auto updated_waveform = std::make_shared<WaveformData>();
    updated_waveform->sample_rate = existing_waveform->sample_rate;
    updated_waveform->samples_per_point = samples_per_point;
    updated_waveform->duration = existing_waveform->duration + 
        ve::TimePoint(new_audio_data.size() * samples_per_point, sample_rate);
    
    // Resize channels for the updated waveform
    updated_waveform->channels.resize(existing_waveform->channels.size());
    
    for (size_t ch = 0; ch < existing_waveform->channels.size(); ++ch) {
        auto& existing_channel = existing_waveform->channels[ch];
        auto& new_channel = new_waveform_data->channels[ch];
        auto& updated_channel = updated_waveform->channels[ch];
        
        // Reserve space for efficiency
        updated_channel.reserve(existing_channel.size() + new_channel.size());
        
        // Copy existing data up to insert point
        updated_channel.insert(updated_channel.end(), 
                             existing_channel.begin(), 
                             existing_channel.begin() + std::min(insert_point, existing_channel.size()));
        
        // Insert new data
        updated_channel.insert(updated_channel.end(), new_channel.begin(), new_channel.end());
        
        // Copy remaining existing data after insert point
        if (insert_point < existing_channel.size()) {
            updated_channel.insert(updated_channel.end(),
                                 existing_channel.begin() + insert_point,
                                 existing_channel.end());
        }
    }
    
    ve::log_info("Successfully updated waveform with {} new points at position {}", 
                 new_waveform_data->point_count(), insert_position.to_seconds());
                 
    return updated_waveform;
}

bool WaveformGeneratorImpl::cancel_generation(const std::string& audio_source) {
    std::lock_guard<std::mutex> lock(tracking_mutex_);
    
    auto it = active_tasks_.find(audio_source);
    if (it != active_tasks_.end()) {
        it->second->cancelled = true;
        active_tasks_.erase(it);
        progress_map_.erase(audio_source);
        return true;
    }
    
    return false;
}

float WaveformGeneratorImpl::get_generation_progress(const std::string& audio_source) const {
    std::lock_guard<std::mutex> lock(tracking_mutex_);
    
    auto it = progress_map_.find(audio_source);
    return (it != progress_map_.end()) ? it->second : -1.0f;
}

bool WaveformGeneratorImpl::is_generating() const {
    std::lock_guard<std::mutex> lock(tracking_mutex_);
    return !active_tasks_.empty();
}

void WaveformGeneratorImpl::set_config(const WaveformGeneratorConfig& config) {
    config_ = config;
    
    // Update memory pool if limit changed
    if (memory_pool_->get_limit() != config.max_memory_usage_mb * 1024 * 1024) {
        memory_pool_ = std::make_unique<WaveformMemoryPool>(config.max_memory_usage_mb);
    }
}

void WaveformGeneratorImpl::worker_thread() {
    while (!shutdown_) {
        std::shared_ptr<WaveformTask> task;
        
        // Wait for task
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] { return !task_queue_.empty() || shutdown_; });
            
            if (shutdown_) break;
            
            task = task_queue_.front();
            task_queue_.pop();
        }
        
        // Process task
        if (task && !task->cancelled) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            auto result = process_task(*task);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            stats_.total_generation_time += duration;
            
            // Complete the task
            if (result) {
                stats_.total_generated++;
                stats_.total_bytes_processed += waveform_utils::calculate_memory_usage(*result);
            }
            
            task->promise.set_value(result);
            
            if (task->completion_callback) {
                task->completion_callback(result, result != nullptr);
            }
            
            // Remove from active tasks
            {
                std::lock_guard<std::mutex> lock(tracking_mutex_);
                active_tasks_.erase(task->audio_source);
                progress_map_.erase(task->audio_source);
            }
        }
    }
}

std::shared_ptr<WaveformData> WaveformGeneratorImpl::process_task(const WaveformTask& task) {
    try {
        update_progress(task.audio_source, 0.1f, "Loading audio data");
        
        auto audio_data = load_audio_data(task.audio_source, task.time_range);
        if (audio_data.empty() || task.cancelled) {
            return nullptr;
        }
        
        update_progress(task.audio_source, 0.3f, "Converting to waveform");
        
        auto result = convert_to_waveform(audio_data, task.zoom_level, task.channel_mask, task.time_range.first);
        
        update_progress(task.audio_source, 1.0f, "Generation complete");
        
        return result;
        
    } catch (const std::exception& e) {
        ve::log_error("Waveform generation failed for {}: {}", task.audio_source, e.what());
        return nullptr;
    }
}

std::vector<AudioFrame> WaveformGeneratorImpl::load_audio_data(
    const std::string& audio_source,
    const std::pair<ve::TimePoint, ve::TimePoint>& time_range) {
    
    if (!audio_decoder_) {
        ve::log_error("Audio decoder not available");
        return {};
    }
    
    // Load audio file and extract samples for the specified time range
    if (!audio_decoder_->open(audio_source)) {
        ve::log_error("Failed to open audio source: {}", audio_source);
        return {};
    }
    
    std::vector<AudioFrame> frames;
    AudioFrame frame;
    
    while (audio_decoder_->decode_frame(frame)) {
        // Check if frame is within our time range
        if (frame.timestamp >= time_range.first && frame.timestamp <= time_range.second) {
            frames.push_back(std::move(frame));
        }
        
        if (frame.timestamp > time_range.second) {
            break;
        }
    }
    
    audio_decoder_->close();
    return frames;
}

std::shared_ptr<WaveformData> WaveformGeneratorImpl::convert_to_waveform(
    const std::vector<AudioFrame>& audio_data,
    const ZoomLevel& zoom_level,
    uint32_t channel_mask,
    const ve::TimePoint& start_time) {
    
    if (audio_data.empty()) {
        return nullptr;
    }
    
    // Calculate total sample count and prepare sample buffer
    size_t total_samples = 0;
    size_t channel_count = 0;
    int32_t sample_rate = 0;
    
    for (const auto& frame : audio_data) {
        total_samples += frame.sample_count;
        channel_count = std::max(channel_count, static_cast<size_t>(frame.channel_count));
        sample_rate = frame.sample_rate;
    }
    
    if (total_samples == 0 || channel_count == 0) {
        return nullptr;
    }
    
    // Create continuous sample buffer
    std::vector<float> sample_buffer;
    sample_buffer.reserve(total_samples * channel_count);
    
    for (const auto& frame : audio_data) {
        const float* samples = reinterpret_cast<const float*>(frame.data.data());
        sample_buffer.insert(sample_buffer.end(), samples, samples + frame.sample_count * frame.channel_count);
    }
    
    // Process samples to generate waveform points
    std::vector<std::vector<WaveformPoint>> waveform_points;
    
    if (config_.enable_simd_optimization) {
        SIMDWaveformProcessor::process_samples_simd(
            sample_buffer.data(),
            sample_buffer.size(),
            channel_count,
            zoom_level.samples_per_point,
            waveform_points
        );
    } else {
        SIMDWaveformProcessor::process_samples_scalar(
            sample_buffer.data(),
            sample_buffer.size(),
            channel_count,
            zoom_level.samples_per_point,
            waveform_points
        );
    }
    
    // Create waveform data structure
    auto waveform_data = std::make_shared<WaveformData>();
    waveform_data->channels = std::move(waveform_points);
    waveform_data->start_time = start_time;
    waveform_data->duration = ve::TimePoint(total_samples, sample_rate);
    waveform_data->sample_rate = sample_rate;
    waveform_data->samples_per_point = zoom_level.samples_per_point;
    
    return waveform_data;
}

void WaveformGeneratorImpl::update_progress(const std::string& audio_source, float progress, const std::string& status) {
    if (!config_.enable_progress_callbacks) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(tracking_mutex_);
        progress_map_[audio_source] = progress;
    }
    
    // Find and call progress callback
    std::shared_ptr<WaveformTask> task;
    {
        std::lock_guard<std::mutex> lock(tracking_mutex_);
        auto it = active_tasks_.find(audio_source);
        if (it != active_tasks_.end()) {
            task = it->second;
        }
    }
    
    if (task && task->progress_callback) {
        task->progress_callback(progress, status);
    }
}

void WaveformGeneratorImpl::cleanup_completed_tasks() {
    std::lock_guard<std::mutex> lock(tracking_mutex_);
    
    auto it = active_tasks_.begin();
    while (it != active_tasks_.end()) {
        if (it->second->cancelled) {
            progress_map_.erase(it->first);
            it = active_tasks_.erase(it);
        } else {
            ++it;
        }
    }
}

//=============================================================================
// Factory Method
//=============================================================================

std::unique_ptr<WaveformGenerator> WaveformGenerator::create(const WaveformGeneratorConfig& config) {
    return std::make_unique<WaveformGeneratorImpl>(config);
}

//=============================================================================
// Utility Functions
//=============================================================================

namespace waveform_utils {

std::vector<ZoomLevel> calculate_optimal_zoom_levels(
    const ve::TimePoint& duration,
    size_t target_pixels) {
    
    std::vector<ZoomLevel> levels;
    
    const double duration_seconds = static_cast<double>(duration.numerator()) / duration.denominator();
    const double samples_per_pixel = (duration_seconds * 48000.0) / target_pixels; // Assume 48kHz
    
    // Generate zoom levels from detailed to overview
    for (size_t factor = 1; factor <= 100000; factor *= 10) {
        if (factor <= samples_per_pixel * 10) {
            std::string name = "Zoom " + std::to_string(factor);
            levels.emplace_back(factor, name);
        }
    }
    
    // Ensure we have at least the common zoom levels
    if (levels.empty()) {
        levels = {ZoomLevel::NORMAL_VIEW, ZoomLevel::OVERVIEW, ZoomLevel::TIMELINE_VIEW};
    }
    
    return levels;
}

std::shared_ptr<WaveformData> downsample_waveform(
    const WaveformData& source_data,
    const ZoomLevel& target_zoom_level) {
    
    if (target_zoom_level.samples_per_point <= source_data.samples_per_point) {
        // Cannot downsample to higher resolution
        return nullptr;
    }
    
    const size_t downsample_factor = target_zoom_level.samples_per_point / source_data.samples_per_point;
    
    auto result = std::make_shared<WaveformData>();
    result->start_time = source_data.start_time;
    result->duration = source_data.duration;
    result->sample_rate = source_data.sample_rate;
    result->samples_per_point = target_zoom_level.samples_per_point;
    
    result->channels.resize(source_data.channel_count());
    
    for (size_t ch = 0; ch < source_data.channel_count(); ++ch) {
        const auto& source_channel = source_data.channels[ch];
        auto& target_channel = result->channels[ch];
        
        const size_t target_points = source_channel.size() / downsample_factor;
        target_channel.reserve(target_points);
        
        for (size_t i = 0; i < target_points; ++i) {
            float peak_pos = 0.0f;
            float peak_neg = 0.0f;
            float rms_sum = 0.0f;
            
            for (size_t j = 0; j < downsample_factor; ++j) {
                const size_t source_idx = i * downsample_factor + j;
                if (source_idx < source_channel.size()) {
                    const auto& point = source_channel[source_idx];
                    peak_pos = std::max(peak_pos, point.peak_positive);
                    peak_neg = std::min(peak_neg, point.peak_negative);
                    rms_sum += point.rms_value * point.rms_value;
                }
            }
            
            const float rms_value = std::sqrt(rms_sum / downsample_factor);
            target_channel.emplace_back(peak_pos, peak_neg, rms_value);
        }
    }
    
    return result;
}

std::shared_ptr<WaveformData> merge_waveform_segments(
    const std::vector<std::shared_ptr<WaveformData>>& segments) {
    
    if (segments.empty()) {
        return nullptr;
    }
    
    if (segments.size() == 1) {
        return segments[0];
    }
    
    // Validate segments are compatible
    const auto& first = segments[0];
    for (const auto& segment : segments) {
        if (segment->channel_count() != first->channel_count() ||
            segment->sample_rate != first->sample_rate ||
            segment->samples_per_point != first->samples_per_point) {
            return nullptr;
        }
    }
    
    auto result = std::make_shared<WaveformData>();
    result->start_time = first->start_time;
    result->sample_rate = first->sample_rate;
    result->samples_per_point = first->samples_per_point;
    result->channels.resize(first->channel_count());
    
    // Calculate total duration and merge points
    ve::TimePoint total_duration(0, 1);
    for (const auto& segment : segments) {
        total_duration = total_duration + segment->duration;
        
        for (size_t ch = 0; ch < segment->channel_count(); ++ch) {
            const auto& source_channel = segment->channels[ch];
            result->channels[ch].insert(
                result->channels[ch].end(),
                source_channel.begin(),
                source_channel.end()
            );
        }
    }
    
    result->duration = total_duration;
    return result;
}

std::shared_ptr<WaveformData> extract_time_range(
    const WaveformData& source_data,
    const std::pair<ve::TimePoint, ve::TimePoint>& time_range) {
    
    // Calculate point indices for the time range
    const double samples_per_second = static_cast<double>(source_data.sample_rate);
    const double points_per_second = samples_per_second / source_data.samples_per_point;
    
    const double start_offset = static_cast<double>(time_range.first.numerator()) / time_range.first.denominator();
    const double end_offset = static_cast<double>(time_range.second.numerator()) / time_range.second.denominator();
    
    const size_t start_point = static_cast<size_t>(start_offset * points_per_second);
    const size_t end_point = static_cast<size_t>(end_offset * points_per_second);
    
    if (start_point >= source_data.point_count() || end_point <= start_point) {
        return nullptr;
    }
    
    auto result = std::make_shared<WaveformData>();
    result->start_time = time_range.first;
    result->duration = time_range.second - time_range.first;
    result->sample_rate = source_data.sample_rate;
    result->samples_per_point = source_data.samples_per_point;
    result->channels.resize(source_data.channel_count());
    
    const size_t actual_end = std::min(end_point, source_data.point_count());
    
    for (size_t ch = 0; ch < source_data.channel_count(); ++ch) {
        const auto& source_channel = source_data.channels[ch];
        auto& target_channel = result->channels[ch];
        
        target_channel.assign(
            source_channel.begin() + start_point,
            source_channel.begin() + actual_end
        );
    }
    
    return result;
}

size_t calculate_memory_usage(const WaveformData& data) {
    size_t total_size = sizeof(WaveformData);
    
    for (const auto& channel : data.channels) {
        total_size += channel.size() * sizeof(WaveformPoint);
    }
    
    return total_size;
}

bool validate_waveform_data(const WaveformData& data) {
    if (data.channels.empty() || data.sample_rate <= 0 || data.samples_per_point == 0) {
        return false;
    }
    
    const size_t expected_points = data.channels[0].size();
    for (const auto& channel : data.channels) {
        if (channel.size() != expected_points) {
            return false;
        }
        
        for (const auto& point : channel) {
            if (std::isnan(point.peak_positive) || std::isnan(point.peak_negative) || 
                std::isnan(point.rms_value) || point.rms_value < 0.0f) {
                return false;
            }
        }
    }
    
    return true;
}

} // namespace waveform_utils

} // namespace ve::audio