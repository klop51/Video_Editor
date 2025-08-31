// Week 8: Performance Adaptive Renderer
// Intelligent renderer with performance-based quality adaptation

#pragma once

#include "gfx/async_renderer.hpp"
#include "gfx/performance_monitor.hpp"
#include <atomic>
#include <mutex>
#include <chrono>

namespace ve::gfx {

/**
 * @brief Quality levels with detailed settings
 */
enum class DetailedQualityLevel {
    ULTRA_LOW,      // Emergency performance mode
    LOW,            // Minimum acceptable quality
    MEDIUM_LOW,     // Below standard quality
    MEDIUM,         // Standard quality
    MEDIUM_HIGH,    // Above standard quality
    HIGH,           // High quality
    ULTRA_HIGH      // Maximum quality
};

/**
 * @brief Quality adaptation strategy
 */
enum class AdaptationStrategy {
    CONSERVATIVE,   // Slow, careful adjustments
    BALANCED,       // Moderate adjustment speed
    AGGRESSIVE,     // Fast adjustments for responsiveness
    CUSTOM          // User-defined thresholds
};

/**
 * @brief Performance-adaptive render job
 */
struct AdaptiveRenderJob : public RenderJob {
    DetailedQualityLevel max_quality = DetailedQualityLevel::HIGH;
    DetailedQualityLevel min_quality = DetailedQualityLevel::LOW;
    bool enable_quality_adaptation = true;
    bool respect_performance_targets = true;
    float target_fps = 30.0f;                     // Target FPS for this job
    float max_frame_time_ms = 33.3f;              // Maximum acceptable frame time
    
    // Quality adaptation callbacks
    std::function<void(DetailedQualityLevel old_level, DetailedQualityLevel new_level)> quality_change_callback;
    std::function<void(const GPUPerformanceStats& stats)> performance_update_callback;
};

/**
 * @brief Quality adaptation statistics
 */
struct QualityAdaptationStats {
    // Adaptation events
    size_t total_quality_adaptations = 0;
    size_t quality_reductions = 0;
    size_t quality_increases = 0;
    size_t emergency_quality_drops = 0;
    
    // Performance tracking
    float average_fps_before_adaptation = 0.0f;
    float average_fps_after_adaptation = 0.0f;
    float performance_improvement_ratio = 0.0f;
    
    // Quality distribution
    std::array<size_t, 7> quality_level_usage{};  // Usage count for each quality level
    float average_quality_level = 0.0f;
    
    // Timing
    float average_adaptation_decision_time_ms = 0.0f;
    size_t adaptations_within_target_time = 0;
    
    // Effectiveness
    size_t successful_performance_recoveries = 0;
    size_t failed_performance_recoveries = 0;
    float adaptation_success_rate = 0.0f;
    
    void reset() {
        *this = QualityAdaptationStats{};
    }
};

/**
 * @brief Performance-adaptive renderer
 * 
 * Extends AsyncRenderer with intelligent quality adaptation based on real-time
 * performance monitoring. Automatically adjusts rendering quality to maintain
 * target frame rates and smooth user experience.
 */
class PerformanceAdaptiveRenderer {
public:
    /**
     * @brief Configuration for performance adaptation
     */
    struct Config {
        // Performance targets
        float target_fps = 30.0f;                  // Target FPS to maintain
        float min_acceptable_fps = 20.0f;          // Minimum acceptable FPS
        float max_frame_time_ms = 33.3f;           // Maximum frame time (30 FPS)
        float performance_check_interval_ms = 500.0f; // Check performance every 500ms
        
        // Adaptation thresholds
        float fps_reduction_threshold = 0.9f;      // Reduce quality if FPS drops 10%
        float fps_increase_threshold = 1.2f;       // Increase quality if FPS exceeds 20%
        float emergency_fps_threshold = 0.5f;      // Emergency reduction at 50% target FPS
        
        // Adaptation behavior
        AdaptationStrategy strategy = AdaptationStrategy::BALANCED;
        uint32_t adaptation_cooldown_ms = 2000;    // Wait 2s between adaptations
        uint32_t emergency_cooldown_ms = 500;      // Wait 500ms for emergency adaptations
        bool enable_preemptive_adaptation = true;  // Adapt before performance drops
        
        // Quality level settings
        DetailedQualityLevel default_quality = DetailedQualityLevel::MEDIUM_HIGH;
        DetailedQualityLevel emergency_quality = DetailedQualityLevel::LOW;
        bool allow_ultra_low_quality = false;      // Allow emergency ultra-low quality
        
        // Advanced features
        bool enable_predictive_adaptation = true;  // Predict performance issues
        bool enable_workload_analysis = true;      // Analyze job complexity
        bool enable_thermal_awareness = false;     // Consider GPU temperature
        
        // Monitoring
        bool enable_detailed_logging = true;
        bool enable_adaptation_history = true;
        size_t adaptation_history_size = 100;      // Keep last 100 adaptations
    };
    
    /**
     * @brief Create performance-adaptive renderer
     * @param base_renderer Underlying async renderer
     * @param performance_monitor Performance monitor for metrics
     * @param config Configuration options
     */
    PerformanceAdaptiveRenderer(AsyncRenderer* base_renderer,
                               PerformanceMonitor* performance_monitor,
                               const Config& config = Config{});
    
    /**
     * @brief Destructor
     */
    ~PerformanceAdaptiveRenderer();
    
    // Non-copyable, non-movable
    PerformanceAdaptiveRenderer(const PerformanceAdaptiveRenderer&) = delete;
    PerformanceAdaptiveRenderer& operator=(const PerformanceAdaptiveRenderer&) = delete;
    
    /**
     * @brief Submit adaptive render job
     * @param job Adaptive render job with quality options
     * @return Future that resolves to output texture
     */
    std::future<TextureHandle> render_adaptive(AdaptiveRenderJob job);
    
    /**
     * @brief Submit single effect with adaptive quality
     * @param effect_type Effect type
     * @param parameters Effect parameters
     * @param param_size Parameter size
     * @param input_texture Source texture
     * @param target_quality Target quality level
     * @return Future that resolves to output texture
     */
    std::future<TextureHandle> apply_effect_adaptive(int effect_type, const void* parameters, size_t param_size,
                                                     TextureHandle input_texture,
                                                     DetailedQualityLevel target_quality = DetailedQualityLevel::HIGH);
    
    /**
     * @brief Set adaptive quality mode
     * @param enabled Whether to enable automatic quality adaptation
     */
    void set_adaptive_quality_mode(bool enabled);
    
    /**
     * @brief Check if adaptive quality mode is enabled
     */
    bool is_adaptive_quality_mode_enabled() const { return adaptive_quality_enabled_.load(); }
    
    /**
     * @brief Update quality based on current FPS
     * @param current_fps Current FPS measurement
     * @param force_update Force update even if within cooldown
     */
    void update_quality_based_on_fps(float current_fps, bool force_update = false);
    
    /**
     * @brief Calculate optimal quality level based on performance stats
     * @param stats Current performance statistics
     * @param job_complexity Complexity estimate for pending job (0.0-1.0)
     * @return Recommended quality level
     */
    DetailedQualityLevel calculate_optimal_quality(const GPUPerformanceStats& stats, float job_complexity = 0.5f) const;
    
    /**
     * @brief Set target performance parameters
     * @param target_fps Target FPS to maintain
     * @param max_frame_time_ms Maximum acceptable frame time
     */
    void set_performance_targets(float target_fps, float max_frame_time_ms);
    
    /**
     * @brief Get current quality level
     */
    DetailedQualityLevel get_current_quality_level() const { return current_quality_level_.load(); }
    
    /**
     * @brief Force set quality level (overrides adaptation)
     * @param quality Quality level to set
     * @param duration_ms Duration to maintain this quality (0 = permanent)
     */
    void force_quality_level(DetailedQualityLevel quality, uint32_t duration_ms = 0);
    
    /**
     * @brief Get quality adaptation statistics
     */
    QualityAdaptationStats get_adaptation_stats() const;
    
    /**
     * @brief Reset adaptation statistics
     */
    void reset_adaptation_stats();
    
    /**
     * @brief Update configuration
     * @param new_config New configuration options
     */
    void update_config(const Config& new_config);
    
    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }
    
    /**
     * @brief Get adaptation history (if enabled)
     * @return Vector of recent quality adaptations with timestamps
     */
    std::vector<std::pair<std::chrono::steady_clock::time_point, DetailedQualityLevel>> get_adaptation_history() const;
    
    /**
     * @brief Check if performance is currently degraded
     */
    bool is_performance_degraded() const;
    
    /**
     * @brief Get performance improvement recommendations
     */
    std::vector<std::string> get_performance_recommendations() const;

private:
    // Core adaptation logic
    void performance_monitoring_thread();
    void analyze_current_performance();
    void make_adaptation_decision(const GPUPerformanceStats& stats);
    void apply_quality_adaptation(DetailedQualityLevel new_quality, const std::string& reason);
    
    // Quality calculation
    DetailedQualityLevel calculate_quality_for_fps(float fps) const;
    DetailedQualityLevel calculate_quality_for_frame_time(float frame_time_ms) const;
    DetailedQualityLevel calculate_emergency_quality(const GPUPerformanceStats& stats) const;
    float estimate_job_complexity(const AdaptiveRenderJob& job) const;
    
    // Adaptation strategies
    DetailedQualityLevel apply_conservative_strategy(DetailedQualityLevel current, const GPUPerformanceStats& stats) const;
    DetailedQualityLevel apply_balanced_strategy(DetailedQualityLevel current, const GPUPerformanceStats& stats) const;
    DetailedQualityLevel apply_aggressive_strategy(DetailedQualityLevel current, const GPUPerformanceStats& stats) const;
    
    // Prediction and analysis
    bool predict_performance_degradation(const GPUPerformanceStats& stats) const;
    float analyze_performance_trend() const;
    bool should_preemptively_adapt() const;
    
    // Timing and cooldowns
    bool is_adaptation_allowed() const;
    bool is_emergency_adaptation_allowed() const;
    void update_adaptation_cooldown();
    
    // Statistics and history
    void update_adaptation_statistics(DetailedQualityLevel old_quality, DetailedQualityLevel new_quality,
                                     const GPUPerformanceStats& before_stats, const GPUPerformanceStats& after_stats);
    void record_adaptation_in_history(DetailedQualityLevel quality);
    void cleanup_adaptation_history();
    
    // Configuration and dependencies
    Config config_;
    AsyncRenderer* base_renderer_;
    PerformanceMonitor* performance_monitor_;
    
    // State management
    std::atomic<bool> adaptive_quality_enabled_{true};
    std::atomic<DetailedQualityLevel> current_quality_level_{DetailedQualityLevel::MEDIUM_HIGH};
    std::atomic<bool> forced_quality_active_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    // Performance monitoring thread
    std::thread monitoring_thread_;
    
    // Timing and cooldowns
    mutable std::mutex timing_mutex_;
    std::chrono::steady_clock::time_point last_adaptation_time_;
    std::chrono::steady_clock::time_point last_emergency_adaptation_time_;
    std::chrono::steady_clock::time_point forced_quality_end_time_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    QualityAdaptationStats adaptation_stats_;
    
    // Adaptation history
    mutable std::mutex history_mutex_;
    std::vector<std::pair<std::chrono::steady_clock::time_point, DetailedQualityLevel>> adaptation_history_;
    
    // Performance tracking
    mutable std::mutex performance_tracking_mutex_;
    std::vector<float> recent_fps_measurements_;
    std::vector<float> recent_frame_times_;
    static constexpr size_t MAX_PERFORMANCE_SAMPLES = 20;
};

} // namespace ve::gfx
