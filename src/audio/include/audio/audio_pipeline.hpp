/**
 * @file audio_pipeline.hpp
 * @brief Audio Processing Pipeline for Playback Controller Integration
 *
 * Phase 1C: Integrates SimpleMixer and AudioOutput with PlaybackController
 * to provide complete audio processing pipeline for video editor playback.
 */

#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>

#include "audio/audio_frame.hpp"
#include "audio/simple_mixer.hpp"
#include "audio/audio_output.hpp"
#include "audio/professional_monitoring.hpp"
#include "core/time.hpp"

namespace ve::audio {

/**
 * @brief Audio pipeline state
 */
enum class AudioPipelineState {
    Uninitialized,
    Initialized,
    Playing,
    Paused,
    Stopped,
    Error
};

/**
 * @brief Audio processing statistics
 */
struct AudioPipelineStats {
    uint64_t total_frames_processed = 0;
    uint64_t total_samples_processed = 0;
    uint32_t buffer_underruns = 0;
    uint32_t buffer_overruns = 0;
    float average_cpu_usage = 0.0f;
    float peak_cpu_usage = 0.0f;
    uint32_t active_channels = 0;
    float master_volume_db = 0.0f;
    bool master_muted = false;
};

/**
 * @brief Audio pipeline configuration
 */
struct AudioPipelineConfig {
    uint32_t sample_rate = 48000;
    uint16_t channel_count = 2;
    SampleFormat format = SampleFormat::Float32;
    uint32_t buffer_size = 1024;
    uint32_t max_channels = 16;
    bool enable_clipping_protection = true;
    bool enable_output = true;
    
    // Professional monitoring configuration
    bool enable_professional_monitoring = false;
    ProfessionalAudioMonitoringSystem::MonitoringConfig monitoring_config;
};

/**
 * @brief Audio processing pipeline for playback integration
 *
 * Coordinates audio flow: Decoder → Mixer → Output
 * Provides thread-safe audio processing with monitoring and control.
 */
class AudioPipeline {
public:
    /**
     * @brief Create audio pipeline instance
     */
    static std::unique_ptr<AudioPipeline> create(const AudioPipelineConfig& config = {});

    ~AudioPipeline();

    // Lifecycle management
    bool initialize();
    void shutdown();
    bool is_initialized() const { return state_.load() == AudioPipelineState::Initialized; }

    // Audio processing
    bool process_audio_frame(std::shared_ptr<AudioFrame> frame);
    bool start_output();
    bool stop_output();
    bool pause_output();
    bool resume_output();

    // Mixer integration
    uint32_t add_audio_channel(const std::string& name = "Playback",
                              float initial_gain_db = 0.0f,
                              float initial_pan = 0.0f);
    bool remove_audio_channel(uint32_t channel_id);
    bool set_channel_gain(uint32_t channel_id, float gain_db);
    bool set_channel_pan(uint32_t channel_id, float pan);
    bool set_channel_mute(uint32_t channel_id, bool muted);
    bool set_channel_solo(uint32_t channel_id, bool solo);

    // Master controls
    bool set_master_volume(float volume_db);
    bool set_master_mute(bool muted);
    float get_master_volume() const;
    bool is_master_muted() const;

    // State and monitoring
    AudioPipelineState get_state() const { return state_.load(); }
    AudioPipelineStats get_stats() const;
    void reset_stats();

    // Configuration
    const AudioPipelineConfig& get_config() const { return config_; }
    bool set_config(const AudioPipelineConfig& config);

    // Professional monitoring integration
    std::shared_ptr<ProfessionalAudioMonitoringSystem> get_monitoring_system() const { 
        return monitoring_system_; 
    }
    bool enable_professional_monitoring(const ProfessionalAudioMonitoringSystem::MonitoringConfig& config = {});
    void disable_professional_monitoring();
    bool is_monitoring_enabled() const { return monitoring_enabled_; }

    // Error handling
    std::string get_last_error() const;
    void clear_error();

private:
    AudioPipeline(const AudioPipelineConfig& config);

    // Internal components
    AudioPipelineConfig config_;
    std::unique_ptr<SimpleMixer> mixer_;
    std::unique_ptr<AudioOutput> output_;

    // State management
    std::atomic<AudioPipelineState> state_{AudioPipelineState::Uninitialized};
    mutable std::mutex state_mutex_;

    // Audio processing thread
    std::thread processing_thread_;
    std::atomic<bool> processing_thread_should_exit_{false};
    void processing_thread_main();

    // Audio buffering
    std::vector<std::shared_ptr<AudioFrame>> audio_buffer_;
    mutable std::mutex buffer_mutex_;
    std::atomic<size_t> buffer_read_pos_{0};
    std::atomic<size_t> buffer_write_pos_{0};
    static constexpr size_t BUFFER_SIZE = 8; // Audio buffer size

    // Statistics
    mutable std::mutex stats_mutex_;
    AudioPipelineStats stats_;

    // Error handling
    mutable std::mutex error_mutex_;
    std::string last_error_;

    // Professional monitoring system
    std::shared_ptr<ProfessionalAudioMonitoringSystem> monitoring_system_;
    std::atomic<bool> monitoring_enabled_{false};

    // Helper methods
    void set_state(AudioPipelineState new_state);
    void set_error(const std::string& error);
    bool initialize_mixer();
    bool initialize_output();
    void update_stats(const std::shared_ptr<AudioFrame>& frame);
    void process_audio_buffer();
    std::shared_ptr<AudioFrame> mix_buffered_audio(uint32_t frame_count);
};

} // namespace ve::audio