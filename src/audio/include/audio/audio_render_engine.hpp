/**
 * @file audio_render_engine.hpp
 * @brief Phase 2 Week 3: Advanced Audio Rendering Engine
 * 
 * Professional audio rendering system providing:
 * - Real-time and offline audio rendering
 * - Multi-format export support (WAV, MP3, FLAC, AAC)
 * - Multi-track mix-down with effects processing
 * - Quality control and monitoring systems
 * - High-performance processing pipeline
 * - Professional metadata handling
 */

#pragma once

#include "core/time.hpp"
#include "core/log.hpp"
#include "audio/audio_frame.hpp"
#include "audio/audio_clock.hpp"
#include "audio/mixing_graph.hpp"
#include "audio/audio_effects.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <future>
#include <unordered_map>

namespace ve::audio {

/**
 * @brief Audio export format specifications
 */
enum class ExportFormat {
    WAV,        ///< Uncompressed WAV format
    MP3,        ///< MPEG-1 Audio Layer III
    FLAC,       ///< Free Lossless Audio Codec
    AAC,        ///< Advanced Audio Codec
    OGG,        ///< Ogg Vorbis
    AIFF        ///< Audio Interchange File Format
};

/**
 * @brief Audio quality preset levels
 */
enum class QualityPreset {
    Draft,      ///< Fast rendering, lower quality
    Standard,   ///< Balanced quality and speed
    High,       ///< High quality, slower rendering
    Maximum,    ///< Maximum quality, slowest rendering
    Custom      ///< User-defined custom settings
};

/**
 * @brief Audio rendering mode
 */
enum class RenderMode {
    Realtime,   ///< Real-time rendering (for monitoring)
    Offline,    ///< Offline rendering (for export)
    Preview     ///< Low-latency preview rendering
};

/**
 * @brief Export format configuration
 */
struct ExportConfig {
    ExportFormat format = ExportFormat::WAV;
    uint32_t sample_rate = 48000;           ///< Target sample rate
    uint16_t channel_count = 2;             ///< Target channel count
    uint32_t bit_depth = 24;                ///< Bit depth (8, 16, 24, 32)
    QualityPreset quality = QualityPreset::High;
    
    // Format-specific settings
    struct {
        uint32_t bitrate = 320;             ///< MP3/AAC bitrate (kbps)
        bool vbr = true;                    ///< Variable bitrate
        uint32_t compression_level = 5;      ///< FLAC compression (0-8)
        bool joint_stereo = true;           ///< Joint stereo encoding
    } codec_settings;
    
    // Metadata
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string comment;
    uint32_t year = 0;
    uint32_t track_number = 0;
    
    bool normalize_output = false;          ///< Apply output normalization
    double target_lufs = -23.0;            ///< Target LUFS for normalization
    bool apply_dithering = true;           ///< Apply dithering for bit depth reduction
};

/**
 * @brief Multi-track mix-down configuration
 */
struct MixdownConfig {
    struct TrackConfig {
        uint32_t track_id;
        double volume = 1.0;                ///< Track volume multiplier
        double pan = 0.0;                   ///< Pan position (-1.0 to 1.0)
        bool muted = false;                 ///< Track mute state
        bool solo = false;                  ///< Track solo state
        std::vector<NodeID> effect_chain;   ///< Applied effects
    };
    
    std::vector<TrackConfig> tracks;
    double master_volume = 1.0;             ///< Master output volume
    bool enable_master_effects = true;     ///< Apply master effect chain
    std::vector<NodeID> master_effect_chain; ///< Master effects
    
    // Mix settings
    bool enable_side_chain = false;        ///< Enable side-chain compression
    bool enable_bus_sends = false;         ///< Enable auxiliary bus sends
    uint32_t max_polyphony = 128;          ///< Maximum simultaneous voices
};

/**
 * @brief Quality control metrics
 */
struct QualityMetrics {
    // Level analysis
    double peak_level_db = -std::numeric_limits<double>::infinity();
    double rms_level_db = -std::numeric_limits<double>::infinity();
    double lufs_momentary = -std::numeric_limits<double>::infinity();
    double lufs_short_term = -std::numeric_limits<double>::infinity();
    double lufs_integrated = -std::numeric_limits<double>::infinity();
    
    // Dynamic range
    double dynamic_range_db = 0.0;
    double crest_factor = 0.0;
    
    // Spectral analysis
    std::vector<double> frequency_spectrum;  ///< FFT magnitude spectrum
    double spectral_centroid = 0.0;
    double spectral_rolloff = 0.0;
    
    // Quality indicators
    bool clipping_detected = false;
    uint32_t clipped_samples = 0;
    bool phase_issues = false;
    double correlation = 1.0;               ///< Stereo correlation
    
    // Processing stats
    double cpu_usage_percent = 0.0;
    uint32_t buffer_underruns = 0;
    TimePoint last_update_time;
};

/**
 * @brief Render progress information
 */
struct RenderProgress {
    TimePoint current_time;
    TimeDuration total_duration;
    double progress_percent = 0.0;
    uint64_t samples_processed = 0;
    uint64_t total_samples = 0;
    double real_time_factor = 1.0;         ///< Processing speed vs real-time
    std::string current_operation;
    bool is_complete = false;
    bool has_error = false;
    std::string error_message;
};

/**
 * @brief Render callback function types
 */
using ProgressCallback = std::function<void(const RenderProgress&)>;
using QualityCallback = std::function<void(const QualityMetrics&)>;
using CompletionCallback = std::function<void(bool success, const std::string& output_path)>;

/**
 * @brief Advanced Audio Rendering Engine
 * 
 * Provides comprehensive audio rendering capabilities for professional video editing:
 * - Multi-format export with configurable quality settings
 * - Real-time and offline rendering modes
 * - Multi-track mix-down with effects processing
 * - Quality control and monitoring systems
 * - High-performance processing pipeline
 * - Professional metadata handling
 */
class AudioRenderEngine {
public:
    /**
     * @brief Construct audio render engine
     * @param mixing_graph The mixing graph for audio processing
     * @param audio_clock Audio clock for synchronization
     */
    AudioRenderEngine(std::shared_ptr<MixingGraph> mixing_graph,
                     std::shared_ptr<AudioClock> audio_clock);
    
    /**
     * @brief Destructor
     */
    ~AudioRenderEngine();

    // Core rendering operations
    
    /**
     * @brief Initialize rendering engine
     * @param sample_rate Output sample rate
     * @param channel_count Output channel count
     * @param buffer_size Processing buffer size
     * @return true if initialization successful
     */
    bool initialize(uint32_t sample_rate = 48000, 
                   uint16_t channel_count = 2,
                   uint32_t buffer_size = 512);
    
    /**
     * @brief Shutdown rendering engine
     */
    void shutdown();
    
    /**
     * @brief Check if engine is initialized
     */
    bool is_initialized() const { return initialized_; }

    // Export operations
    
    /**
     * @brief Start audio export to file
     * @param output_path Output file path
     * @param config Export configuration
     * @param mixdown_config Mix-down configuration
     * @param start_time Start time for export
     * @param duration Duration to export
     * @param progress_callback Progress callback (optional)
     * @param completion_callback Completion callback (optional)
     * @return Export job ID for tracking
     */
    uint32_t start_export(const std::string& output_path,
                         const ExportConfig& config,
                         const MixdownConfig& mixdown_config,
                         const TimePoint& start_time,
                         const TimeDuration& duration,
                         ProgressCallback progress_callback = nullptr,
                         CompletionCallback completion_callback = nullptr);
    
    /**
     * @brief Cancel ongoing export
     * @param job_id Export job ID
     * @return true if cancellation successful
     */
    bool cancel_export(uint32_t job_id);
    
    /**
     * @brief Get export progress
     * @param job_id Export job ID
     * @return Current progress information
     */
    RenderProgress get_export_progress(uint32_t job_id) const;

    // Real-time rendering
    
    /**
     * @brief Start real-time rendering
     * @param mode Rendering mode
     * @param mixdown_config Mix-down configuration
     * @param quality_callback Quality monitoring callback (optional)
     * @return true if started successfully
     */
    bool start_realtime_render(RenderMode mode,
                              const MixdownConfig& mixdown_config,
                              QualityCallback quality_callback = nullptr);
    
    /**
     * @brief Stop real-time rendering
     */
    void stop_realtime_render();
    
    /**
     * @brief Check if real-time rendering is active
     */
    bool is_realtime_rendering() const { return realtime_active_; }

    // Multi-track mix-down
    
    /**
     * @brief Create mix-down configuration template
     * @param track_count Number of tracks
     * @return Default mix-down configuration
     */
    MixdownConfig create_mixdown_template(uint32_t track_count);
    
    /**
     * @brief Validate mix-down configuration
     * @param config Configuration to validate
     * @return true if configuration is valid
     */
    bool validate_mixdown_config(const MixdownConfig& config) const;
    
    /**
     * @brief Apply mix-down configuration
     * @param config Mix-down configuration
     * @return true if applied successfully
     */
    bool apply_mixdown_config(const MixdownConfig& config);

    // Quality control
    
    /**
     * @brief Get current quality metrics
     * @return Real-time quality metrics
     */
    QualityMetrics get_quality_metrics() const;
    
    /**
     * @brief Enable/disable quality monitoring
     * @param enabled Quality monitoring state
     * @param update_rate_ms Update rate in milliseconds
     */
    void set_quality_monitoring(bool enabled, uint32_t update_rate_ms = 100);
    
    /**
     * @brief Set quality monitoring callback
     * @param callback Quality callback function
     */
    void set_quality_callback(QualityCallback callback);

    // Format support queries
    
    /**
     * @brief Get supported export formats
     * @return Vector of supported formats
     */
    std::vector<ExportFormat> get_supported_formats() const;
    
    /**
     * @brief Check if format is supported
     * @param format Format to check
     * @return true if format is supported
     */
    bool is_format_supported(ExportFormat format) const;
    
    /**
     * @brief Get default export configuration for format
     * @param format Target format
     * @return Default configuration
     */
    ExportConfig get_default_export_config(ExportFormat format) const;

    // Utility methods
    
    /**
     * @brief Get format file extension
     * @param format Audio format
     * @return File extension (including dot)
     */
    static std::string get_format_extension(ExportFormat format);
    
    /**
     * @brief Get format display name
     * @param format Audio format
     * @return Human-readable format name
     */
    static std::string get_format_name(ExportFormat format);
    
    /**
     * @brief Estimate export file size
     * @param config Export configuration
     * @param duration Export duration
     * @return Estimated file size in bytes
     */
    uint64_t estimate_export_size(const ExportConfig& config, 
                                 const TimeDuration& duration) const;

private:
    // Internal implementation
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Core components
    std::shared_ptr<MixingGraph> mixing_graph_;
    std::shared_ptr<AudioClock> audio_clock_;
    
    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> realtime_active_{false};
    
    // Thread safety
    mutable std::mutex state_mutex_;
    mutable std::mutex metrics_mutex_;
    
    // Quality monitoring
    std::atomic<bool> quality_monitoring_enabled_{false};
    QualityCallback quality_callback_;
    
    // Export job management
    std::unordered_map<uint32_t, std::future<bool>> export_jobs_;
    std::atomic<uint32_t> next_job_id_{1};
    
    // Configuration
    uint32_t sample_rate_ = 48000;
    uint16_t channel_count_ = 2;
    uint32_t buffer_size_ = 512;
    
    // Internal helper methods
    bool initialize_codecs();
    void cleanup_codecs();
    bool validate_export_config(const ExportConfig& config) const;
    bool setup_export_encoder(const ExportConfig& config);
    void update_quality_metrics();
    void process_audio_block(std::shared_ptr<AudioFrame>& frame, const MixdownConfig& config);
    bool write_audio_file(const std::string& path, 
                         const ExportConfig& config,
                         const std::vector<std::shared_ptr<AudioFrame>>& frames);
};

/**
 * @brief Audio render engine factory
 */
class AudioRenderEngineFactory {
public:
    /**
     * @brief Create audio render engine instance
     * @param mixing_graph Mixing graph for processing
     * @param audio_clock Audio clock for synchronization
     * @return Shared pointer to render engine
     */
    static std::shared_ptr<AudioRenderEngine> create(
        std::shared_ptr<MixingGraph> mixing_graph,
        std::shared_ptr<AudioClock> audio_clock);
    
    /**
     * @brief Get global render engine instance
     * @return Shared pointer to global instance
     */
    static std::shared_ptr<AudioRenderEngine> get_instance();
    
private:
    static std::weak_ptr<AudioRenderEngine> global_instance_;
    static std::mutex factory_mutex_;
};

} // namespace ve::audio