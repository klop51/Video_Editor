/**
 * @file waveform_generator.h
 * @brief Multi-Resolution Waveform Generation System for Professional Video Editor
 * 
 * Provides efficient waveform data extraction with multiple zoom levels for professional
 * audio visualization. Integrates with existing audio decode pipeline and supports
 * real-time waveform generation during playback and editing.
 */

#pragma once

#include "core/time.hpp"
#include "audio/audio_frame.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <future>
#include <chrono>
#include <map>

namespace ve::audio {

/**
 * @brief Waveform data point containing peak and RMS values
 */
struct WaveformPoint {
    float peak_positive = 0.0f;   // Maximum positive amplitude
    float peak_negative = 0.0f;   // Maximum negative amplitude (typically negative)
    float rms_value = 0.0f;       // RMS (Root Mean Square) value for average energy
    
    // Constructor for convenience
    WaveformPoint() = default;
    WaveformPoint(float peak_pos, float peak_neg, float rms) 
        : peak_positive(peak_pos), peak_negative(peak_neg), rms_value(rms) {}
    
    // Utility methods
    float peak_amplitude() const { return std::max(std::abs(peak_positive), std::abs(peak_negative)); }
    bool is_silent() const { return peak_amplitude() < 1e-6f; }
};

/**
 * @brief Multi-channel waveform data for a specific time range
 */
struct WaveformData {
    std::vector<std::vector<WaveformPoint>> channels;  // Per-channel waveform points
    ve::TimePoint start_time;                          // Start time of this waveform data
    ve::TimePoint duration;                            // Duration covered by this data
    int32_t sample_rate;                               // Sample rate of source audio
    size_t samples_per_point;                          // Audio samples represented per waveform point
    
    // Metadata
    size_t channel_count() const { return channels.size(); }
    size_t point_count() const { return channels.empty() ? 0 : channels[0].size(); }
    bool is_valid() const { return !channels.empty() && point_count() > 0; }
};

/**
 * @brief Zoom level configuration for multi-resolution waveforms
 */
struct ZoomLevel {
    size_t samples_per_point;     // Number of audio samples per waveform point
    std::string name;             // Human-readable name (e.g., "Sample View", "Overview")
    
    ZoomLevel(size_t samples, const std::string& level_name) 
        : samples_per_point(samples), name(level_name) {}
    
    // Common zoom levels for professional video editing
    static const ZoomLevel SAMPLE_VIEW;      // 1:1 ratio
    static const ZoomLevel DETAILED_VIEW;    // 1:10 ratio  
    static const ZoomLevel NORMAL_VIEW;      // 1:100 ratio
    static const ZoomLevel OVERVIEW;         // 1:1000 ratio
    static const ZoomLevel TIMELINE_VIEW;    // 1:10000 ratio
};

/**
 * @brief Progress callback for background waveform generation
 */
using WaveformProgressCallback = std::function<void(float progress, const std::string& status)>;

/**
 * @brief Completion callback for async waveform generation
 */
using WaveformCompletionCallback = std::function<void(std::shared_ptr<WaveformData> data, bool success)>;

/**
 * @brief Configuration for waveform generation
 */
struct WaveformGeneratorConfig {
    // Performance settings
    size_t max_concurrent_workers = 4;        // Number of worker threads
    size_t chunk_size_samples = 65536;        // Samples processed per chunk (64K)
    bool enable_simd_optimization = true;     // Use SIMD for processing
    
    // Quality settings
    bool generate_rms = true;                 // Include RMS calculation
    float silence_threshold = -60.0f;         // dB threshold for silence detection
    
    // Memory management
    size_t max_memory_usage_mb = 512;         // Maximum memory for waveform generation
    bool enable_memory_mapping = true;        // Use memory-mapped files for large sources
    
    // Progress reporting
    float progress_update_interval = 0.1f;   // Progress callback interval (10%)
    bool enable_progress_callbacks = true;   // Enable progress reporting
};

/**
 * @brief Abstract interface for waveform generation
 */
class WaveformGenerator {
public:
    virtual ~WaveformGenerator() = default;
    
    /**
     * @brief Generate waveform data synchronously
     * @param audio_source Path to audio file or audio stream identifier
     * @param time_range Time range to generate waveforms for
     * @param zoom_level Zoom level configuration
     * @param channel_mask Bitmask of channels to process (0 = all channels)
     * @return Generated waveform data or nullptr on failure
     */
    virtual std::shared_ptr<WaveformData> generate_waveform(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const ZoomLevel& zoom_level,
        uint32_t channel_mask = 0
    ) = 0;
    
    /**
     * @brief Generate waveform data asynchronously  
     * @param audio_source Path to audio file or audio stream identifier
     * @param time_range Time range to generate waveforms for
     * @param zoom_level Zoom level configuration
     * @param progress_callback Progress reporting callback
     * @param completion_callback Completion callback with results
     * @param channel_mask Bitmask of channels to process (0 = all channels)
     * @return Future that can be used to cancel or wait for completion
     */
    virtual std::future<std::shared_ptr<WaveformData>> generate_waveform_async(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const ZoomLevel& zoom_level,
        WaveformProgressCallback progress_callback = nullptr,
        WaveformCompletionCallback completion_callback = nullptr,
        uint32_t channel_mask = 0
    ) = 0;
    
    /**
     * @brief Generate multiple zoom levels in one pass for efficiency
     * @param audio_source Path to audio file or audio stream identifier  
     * @param time_range Time range to generate waveforms for
     * @param zoom_levels Vector of zoom levels to generate
     * @param progress_callback Progress reporting callback
     * @return Map of zoom level to generated waveform data
     */
    virtual std::map<size_t, std::shared_ptr<WaveformData>> generate_multi_resolution(
        const std::string& audio_source,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range,
        const std::vector<ZoomLevel>& zoom_levels,
        WaveformProgressCallback progress_callback = nullptr
    ) = 0;
    
    /**
     * @brief Update existing waveform with new audio data (for real-time editing)
     * @param existing_waveform Existing waveform to update
     * @param new_audio_data New audio frames to incorporate
     * @param insert_position Position to insert new data
     * @return Updated waveform data
     */
    virtual std::shared_ptr<WaveformData> update_waveform(
        std::shared_ptr<WaveformData> existing_waveform,
        const std::vector<AudioFrame>& new_audio_data,
        const ve::TimePoint& insert_position
    ) = 0;
    
    /**
     * @brief Cancel ongoing waveform generation
     * @param audio_source Audio source to cancel generation for
     * @return True if cancellation was successful
     */
    virtual bool cancel_generation(const std::string& audio_source) = 0;
    
    /**
     * @brief Get current progress for ongoing generation
     * @param audio_source Audio source to query progress for
     * @return Progress value (0.0 to 1.0) or -1.0 if not generating
     */
    virtual float get_generation_progress(const std::string& audio_source) const = 0;
    
    /**
     * @brief Check if generator is currently processing
     * @return True if any waveform generation is in progress
     */
    virtual bool is_generating() const = 0;
    
    /**
     * @brief Get current configuration
     */
    virtual const WaveformGeneratorConfig& get_config() const = 0;
    
    /**
     * @brief Update configuration (may not affect ongoing generation)
     */
    virtual void set_config(const WaveformGeneratorConfig& config) = 0;
    
    /**
     * @brief Create a waveform generator instance
     * @param config Configuration for the generator
     * @return Unique pointer to generator instance
     */
    static std::unique_ptr<WaveformGenerator> create(const WaveformGeneratorConfig& config = {});
};

/**
 * @brief Utility functions for waveform processing
 */
namespace waveform_utils {
    
    /**
     * @brief Calculate optimal zoom levels for a given audio duration
     * @param duration Total duration of audio
     * @param target_pixels Target display width in pixels
     * @return Vector of recommended zoom levels
     */
    std::vector<ZoomLevel> calculate_optimal_zoom_levels(
        const ve::TimePoint& duration, 
        size_t target_pixels
    );
    
    /**
     * @brief Downsample waveform data to lower resolution
     * @param source_data High-resolution source data
     * @param target_zoom_level Target zoom level
     * @return Downsampled waveform data
     */
    std::shared_ptr<WaveformData> downsample_waveform(
        const WaveformData& source_data,
        const ZoomLevel& target_zoom_level
    );
    
    /**
     * @brief Merge multiple waveform data segments
     * @param segments Vector of waveform segments to merge
     * @return Merged waveform data
     */
    std::shared_ptr<WaveformData> merge_waveform_segments(
        const std::vector<std::shared_ptr<WaveformData>>& segments
    );
    
    /**
     * @brief Extract waveform subset for specific time range
     * @param source_data Source waveform data
     * @param time_range Desired time range
     * @return Extracted waveform subset
     */
    std::shared_ptr<WaveformData> extract_time_range(
        const WaveformData& source_data,
        const std::pair<ve::TimePoint, ve::TimePoint>& time_range
    );
    
    /**
     * @brief Calculate memory usage for waveform data
     * @param data Waveform data to analyze
     * @return Memory usage in bytes
     */
    size_t calculate_memory_usage(const WaveformData& data);
    
    /**
     * @brief Validate waveform data integrity
     * @param data Waveform data to validate
     * @return True if data is valid and consistent
     */
    bool validate_waveform_data(const WaveformData& data);
}

} // namespace ve::audio