/**
 * @file audio_thumbnail.h
 * @brief Audio Thumbnail System for Project Browser Preview
 * 
 * Provides fast generation of small audio overview waveforms for file browsers,
 * project previews, and quick visual identification. Optimized for batch
 * processing and lightweight rendering.
 */

#pragma once

#include "audio/waveform_generator.h"
#include "audio/waveform_cache.h"
#include "core/time.hpp"
#include <memory>
#include <vector>
#include <future>
#include <functional>
#include <chrono>

namespace ve::audio {

/**
 * @brief Thumbnail size configuration
 */
enum class ThumbnailSize {
    TINY = 64,      // 64x32 pixels
    SMALL = 128,    // 128x64 pixels  
    MEDIUM = 256,   // 256x128 pixels
    LARGE = 512     // 512x256 pixels
};

/**
 * @brief Audio thumbnail data for visual rendering
 */
struct AudioThumbnail {
    std::string audio_source;                    // Source file path
    ThumbnailSize size;                          // Thumbnail dimensions
    ve::TimePoint total_duration;                // Total audio duration
    int32_t sample_rate;                         // Original sample rate
    size_t channel_count;                        // Number of audio channels
    
    // Visual data for rendering
    std::vector<std::vector<float>> peak_data;   // Per-channel peak values (0.0-1.0)
    std::vector<std::vector<float>> rms_data;    // Per-channel RMS values (0.0-1.0)
    
    // Thumbnail metadata
    std::chrono::system_clock::time_point generated_time;
    bool is_silent;                              // True if audio is mostly silent
    float max_amplitude;                         // Maximum amplitude in thumbnail
    float average_rms;                           // Average RMS level
    
    // Quality indicators
    bool is_clipped;                             // Contains clipping
    float dynamic_range_db;                      // Dynamic range in dB
    
    // Convenience methods
    size_t width() const { return static_cast<size_t>(size); }
    size_t height() const { return static_cast<size_t>(size) / 2; }
    bool is_valid() const { return !peak_data.empty() && channel_count > 0; }
    
    // Get peak value at specific x position (0.0-1.0)
    float get_peak_at_position(size_t channel, float position) const;
    
    // Get RMS value at specific x position (0.0-1.0)
    float get_rms_at_position(size_t channel, float position) const;
};

/**
 * @brief Thumbnail generation configuration
 */
struct ThumbnailConfig {
    // Visual settings
    ThumbnailSize default_size = ThumbnailSize::MEDIUM;
    bool generate_rms = true;                    // Include RMS data for visual depth
    bool detect_silence = true;                  // Mark silent regions
    float silence_threshold_db = -60.0f;         // Silence detection threshold
    
    // Quality analysis
    bool analyze_clipping = true;                // Detect audio clipping
    float clipping_threshold = 0.95f;            // Clipping detection threshold
    bool calculate_dynamic_range = true;         // Calculate dynamic range
    
    // Performance settings
    size_t max_concurrent_thumbnails = 8;       // Maximum concurrent generations
    std::chrono::milliseconds generation_timeout{10000}; // 10 second timeout
    bool enable_fast_mode = false;               // Sacrifice quality for speed
    
    // Caching
    bool enable_thumbnail_cache = true;          // Cache generated thumbnails
    std::chrono::hours cache_duration{168};     // Keep thumbnails for 7 days
    
    // Batch processing
    size_t batch_size = 50;                     // Files per batch
    bool prioritize_visible_thumbnails = true;   // Process visible items first
};

/**
 * @brief Progress callback for thumbnail generation
 */
using ThumbnailProgressCallback = std::function<void(const std::string& audio_source, float progress)>;

/**
 * @brief Completion callback for thumbnail generation
 */
using ThumbnailCompletionCallback = std::function<void(std::shared_ptr<AudioThumbnail> thumbnail, bool success)>;

/**
 * @brief Batch completion callback
 */
using BatchCompletionCallback = std::function<void(size_t completed_count, size_t total_count)>;

/**
 * @brief Audio thumbnail generator interface
 */
class AudioThumbnailGenerator {
public:
    virtual ~AudioThumbnailGenerator() = default;
    
    /**
     * @brief Generate single audio thumbnail
     * @param audio_source Path to audio file
     * @param size Desired thumbnail size
     * @param priority Generation priority (higher = process sooner)
     * @return Future containing generated thumbnail
     */
    virtual std::future<std::shared_ptr<AudioThumbnail>> generate_thumbnail(
        const std::string& audio_source,
        ThumbnailSize size = ThumbnailSize::MEDIUM,
        int priority = 0
    ) = 0;
    
    /**
     * @brief Generate thumbnails for multiple files
     * @param audio_sources List of audio file paths
     * @param size Desired thumbnail size for all files
     * @param progress_callback Progress reporting callback
     * @param completion_callback Individual completion callback
     * @param batch_callback Batch progress callback
     * @return Future containing vector of generated thumbnails
     */
    virtual std::future<std::vector<std::shared_ptr<AudioThumbnail>>> generate_batch(
        const std::vector<std::string>& audio_sources,
        ThumbnailSize size = ThumbnailSize::MEDIUM,
        ThumbnailProgressCallback progress_callback = nullptr,
        ThumbnailCompletionCallback completion_callback = nullptr,
        BatchCompletionCallback batch_callback = nullptr
    ) = 0;
    
    /**
     * @brief Get cached thumbnail if available
     * @param audio_source Path to audio file
     * @param size Desired thumbnail size
     * @return Cached thumbnail or nullptr if not available
     */
    virtual std::shared_ptr<AudioThumbnail> get_cached_thumbnail(
        const std::string& audio_source,
        ThumbnailSize size = ThumbnailSize::MEDIUM
    ) = 0;
    
    /**
     * @brief Check if thumbnail is available (cached or can be generated quickly)
     * @param audio_source Path to audio file
     * @param size Desired thumbnail size
     * @return True if thumbnail is immediately available
     */
    virtual bool is_thumbnail_available(
        const std::string& audio_source,
        ThumbnailSize size = ThumbnailSize::MEDIUM
    ) = 0;
    
    /**
     * @brief Cancel thumbnail generation for specific file
     * @param audio_source Path to audio file
     * @return True if cancellation was successful
     */
    virtual bool cancel_generation(const std::string& audio_source) = 0;
    
    /**
     * @brief Cancel all pending thumbnail generations
     * @return Number of cancelled generations
     */
    virtual size_t cancel_all_generations() = 0;
    
    /**
     * @brief Get current generation progress for a file
     * @param audio_source Path to audio file
     * @return Progress (0.0-1.0) or -1.0 if not generating
     */
    virtual float get_generation_progress(const std::string& audio_source) = 0;
    
    /**
     * @brief Clear thumbnail cache
     * @param older_than_hours Only clear thumbnails older than specified hours (0 = all)
     * @return Number of thumbnails cleared
     */
    virtual size_t clear_cache(size_t older_than_hours = 0) = 0;
    
    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t total_thumbnails;
        size_t memory_usage_bytes;
        size_t disk_usage_bytes;
        float hit_ratio;
        std::chrono::system_clock::time_point oldest_thumbnail;
        std::chrono::system_clock::time_point newest_thumbnail;
    };
    
    virtual CacheStats get_cache_statistics() = 0;
    
    /**
     * @brief Get current configuration
     */
    virtual const ThumbnailConfig& get_config() const = 0;
    
    /**
     * @brief Update configuration
     */
    virtual void set_config(const ThumbnailConfig& config) = 0;
    
    /**
     * @brief Create thumbnail generator instance
     * @param waveform_generator Waveform generator for audio processing
     * @param waveform_cache Optional waveform cache for optimization
     * @param config Thumbnail generation configuration
     * @return Unique pointer to generator instance
     */
    static std::unique_ptr<AudioThumbnailGenerator> create(
        std::shared_ptr<WaveformGenerator> waveform_generator,
        std::shared_ptr<WaveformCache> waveform_cache = nullptr,
        const ThumbnailConfig& config = {}
    );
};

/**
 * @brief Thumbnail batch processor for high-volume operations
 */
class ThumbnailBatchProcessor {
public:
    /**
     * @brief Process directory of audio files
     * @param directory_path Path to directory containing audio files
     * @param recursive Process subdirectories recursively
     * @param file_extensions Audio file extensions to process
     * @param size Thumbnail size to generate
     * @param progress_callback Progress reporting
     * @return Future containing processing results
     */
    static std::future<std::vector<std::shared_ptr<AudioThumbnail>>> process_directory(
        const std::filesystem::path& directory_path,
        bool recursive = false,
        const std::vector<std::string>& file_extensions = {".wav", ".mp3", ".m4a", ".flac", ".ogg"},
        ThumbnailSize size = ThumbnailSize::MEDIUM,
        ThumbnailProgressCallback progress_callback = nullptr
    );
    
    /**
     * @brief Update thumbnails for modified files
     * @param audio_sources List of audio file paths to check
     * @param generator Thumbnail generator to use
     * @return Number of thumbnails updated
     */
    static std::future<size_t> update_modified_thumbnails(
        const std::vector<std::string>& audio_sources,
        std::shared_ptr<AudioThumbnailGenerator> generator
    );
    
    /**
     * @brief Export thumbnails to image files for external use
     * @param thumbnails Vector of thumbnails to export
     * @param output_directory Directory to save image files
     * @param format Image format ("png", "jpg", "bmp")
     * @return Number of successfully exported thumbnails
     */
    static size_t export_thumbnail_images(
        const std::vector<std::shared_ptr<AudioThumbnail>>& thumbnails,
        const std::filesystem::path& output_directory,
        const std::string& format = "png"
    );
};

/**
 * @brief Utility functions for thumbnail management
 */
namespace thumbnail_utils {
    
    /**
     * @brief Calculate optimal thumbnail size for display area
     * @param display_width Available display width in pixels
     * @param display_height Available display height in pixels
     * @return Recommended thumbnail size
     */
    ThumbnailSize calculate_optimal_size(size_t display_width, size_t display_height);
    
    /**
     * @brief Convert waveform data to thumbnail format
     * @param waveform_data High-resolution waveform data
     * @param target_width Target thumbnail width
     * @param include_rms Include RMS data in thumbnail
     * @return Generated thumbnail
     */
    std::shared_ptr<AudioThumbnail> convert_waveform_to_thumbnail(
        const WaveformData& waveform_data,
        size_t target_width,
        bool include_rms = true
    );
    
    /**
     * @brief Downsample thumbnail to smaller size
     * @param source_thumbnail Source thumbnail
     * @param target_size Target size
     * @return Downsampled thumbnail
     */
    std::shared_ptr<AudioThumbnail> downsample_thumbnail(
        const AudioThumbnail& source_thumbnail,
        ThumbnailSize target_size
    );
    
    /**
     * @brief Analyze audio characteristics for thumbnail metadata
     * @param waveform_data Waveform data to analyze
     * @param silence_threshold_db Silence threshold in dB
     * @param clipping_threshold Clipping detection threshold (0.0-1.0)
     * @return Analysis results (is_silent, is_clipped, dynamic_range_db, max_amplitude, average_rms)
     */
    struct AudioAnalysis {
        bool is_silent;
        bool is_clipped;
        float dynamic_range_db;
        float max_amplitude;
        float average_rms;
    };
    
    AudioAnalysis analyze_audio_characteristics(
        const WaveformData& waveform_data,
        float silence_threshold_db = -60.0f,
        float clipping_threshold = 0.95f
    );
    
    /**
     * @brief Generate thumbnail cache key
     * @param audio_source Audio file path
     * @param size Thumbnail size
     * @param file_modification_time File modification time for cache invalidation
     * @return Cache key string
     */
    std::string generate_thumbnail_cache_key(
        const std::string& audio_source,
        ThumbnailSize size,
        std::chrono::system_clock::time_point file_modification_time
    );
    
    /**
     * @brief Validate thumbnail data integrity
     * @param thumbnail Thumbnail to validate
     * @return True if thumbnail is valid and consistent
     */
    bool validate_thumbnail(const AudioThumbnail& thumbnail);
    
    /**
     * @brief Calculate memory usage for thumbnail
     * @param thumbnail Thumbnail to measure
     * @return Memory usage in bytes
     */
    size_t calculate_thumbnail_memory_usage(const AudioThumbnail& thumbnail);
    
    /**
     * @brief Serialize thumbnail to binary format
     * @param thumbnail Thumbnail to serialize
     * @return Serialized binary data
     */
    std::vector<uint8_t> serialize_thumbnail(const AudioThumbnail& thumbnail);
    
    /**
     * @brief Deserialize thumbnail from binary format
     * @param data Serialized binary data
     * @return Deserialized thumbnail or nullptr on failure
     */
    std::shared_ptr<AudioThumbnail> deserialize_thumbnail(const std::vector<uint8_t>& data);
    
    /**
     * @brief Get supported audio file extensions
     * @return Vector of supported file extensions
     */
    std::vector<std::string> get_supported_audio_extensions();
    
    /**
     * @brief Check if file is a supported audio format
     * @param file_path Path to check
     * @return True if file is supported
     */
    bool is_supported_audio_file(const std::filesystem::path& file_path);
}

} // namespace ve::audio