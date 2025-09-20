/**
 * @file audio_meters_widget.hpp
 * @brief Professional Audio Level Meters for Broadcast Standards
 * 
 * Week 8 Qt Timeline UI Integration - Professional audio level monitoring
 * with VU meters, PPM (Peak Programme Meter), real-time level display,
 * and broadcast-standard compliance for professional video editing.
 */

#pragma once

#include "../../core/include/core/time.hpp"
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtGui/QPainter>
#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <memory>
#include <vector>
#include <array>
#include <deque>

namespace ve::audio {
    struct AudioBuffer;
    class AudioProcessor;
}

namespace ve::ui {

/**
 * @brief Audio metering standards and configurations
 */
enum class MeterType {
    VU_METER,           // Volume Unit meter (RMS-based, slow response)
    PPM_METER,          // Peak Programme Meter (peak-based, fast response)
    DIGITAL_PEAK,       // Digital peak meter with sample peak detection
    RMS_METER,          // RMS level meter
    LOUDNESS_LUFS       // LUFS (Loudness Units relative to Full Scale)
};

enum class MeterStandard {
    BROADCAST_EU,       // EBU R68 European broadcast standard
    BROADCAST_US,       // ANSI/SMPTE broadcast standard  
    DIGITAL_STUDIO,     // Digital studio standard (dBFS)
    CONSUMER,           // Consumer electronics standard
    CUSTOM              // Custom configuration
};

/**
 * @brief Meter scale configuration for different standards
 */
struct MeterScale {
    float min_db{-60.0f};           // Minimum level in dB
    float max_db{+12.0f};           // Maximum level in dB (headroom)
    float reference_db{-18.0f};     // Reference level (0 VU or digital reference)
    
    // Critical levels
    float yellow_threshold{-6.0f};  // Warning level (yellow)
    float orange_threshold{-3.0f};  // High level (orange)
    float red_threshold{0.0f};      // Critical/clipping level (red)
    
    // Scale markings
    std::vector<float> major_marks; // Major scale divisions
    std::vector<float> minor_marks; // Minor scale divisions
    bool show_numeric_labels{true}; // Show dB values
    
    static MeterScale for_standard(MeterStandard standard, MeterType type);
};

/**
 * @brief Meter visual style configuration
 */
struct MeterStyle {
    // Colors
    QColor background_color{20, 20, 20};
    QColor scale_color{120, 120, 120};
    QColor text_color{200, 200, 200};
    
    // Level colors
    QColor green_color{0, 200, 0};      // Normal level
    QColor yellow_color{255, 255, 0};   // Warning level
    QColor orange_color{255, 140, 0};   // High level
    QColor red_color{255, 0, 0};        // Critical level
    QColor peak_hold_color{255, 255, 255}; // Peak hold indicator
    
    // Dimensions
    int meter_width{20};                // Width of level bar
    int scale_width{40};                // Width of scale area
    int spacing{2};                     // Spacing between elements
    int peak_hold_height{2};            // Height of peak hold line
    
    // Visual options
    bool show_scale{true};              // Show dB scale
    bool show_peak_hold{true};          // Show peak hold indicators
    bool show_numeric_readout{true};    // Show numeric level display
    bool gradient_fill{true};           // Use gradient for level bars
    bool logarithmic_scale{true};       // Use logarithmic visual scale
    
    // Animation
    float peak_hold_time{2.0f};         // Peak hold duration (seconds)
    float meter_decay_rate{10.0f};      // Meter fall-back rate (dB/second)
    int update_rate_hz{30};             // Visual update rate
};

/**
 * @brief Real-time audio level data
 */
struct AudioLevelData {
    float peak_db{-std::numeric_limits<float>::infinity()};      // Instantaneous peak
    float rms_db{-std::numeric_limits<float>::infinity()};       // RMS level
    float peak_hold_db{-std::numeric_limits<float>::infinity()}; // Peak hold value
    float lufs_momentary{-std::numeric_limits<float>::infinity()}; // LUFS momentary
    float lufs_short_term{-std::numeric_limits<float>::infinity()}; // LUFS short-term
    float lufs_integrated{-std::numeric_limits<float>::infinity()}; // LUFS integrated
    
    ve::TimePoint timestamp{0, 1};     // When this measurement was taken
    bool clipping{false};               // Digital clipping detected
    bool over_threshold{false};         // Over broadcast threshold
    
    void reset() {
        peak_db = rms_db = peak_hold_db = -std::numeric_limits<float>::infinity();
        lufs_momentary = lufs_short_term = lufs_integrated = -std::numeric_limits<float>::infinity();
        clipping = over_threshold = false;
    }
};

/**
 * @brief Single audio channel meter widget
 */
class AudioMeterChannel : public QWidget {
    Q_OBJECT

public:
    explicit AudioMeterChannel(QWidget* parent = nullptr);
    ~AudioMeterChannel() override = default;
    
    /**
     * @brief Configuration
     */
    void set_meter_type(MeterType type);
    void set_meter_standard(MeterStandard standard);
    void set_meter_style(const MeterStyle& style);
    void set_channel_name(const QString& name);
    void set_orientation(Qt::Orientation orientation);
    
    /**
     * @brief Level updates
     */
    void update_level(const AudioLevelData& level_data);
    void update_peak(float peak_db);
    void update_rms(float rms_db);
    void reset_meters();
    void reset_peak_holds();
    
    /**
     * @brief Getters
     */
    MeterType meter_type() const { return meter_type_; }
    MeterStandard meter_standard() const { return meter_standard_; }
    const AudioLevelData& current_levels() const { return current_levels_; }
    
    /**
     * @brief Size hints for layout
     */
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void level_threshold_exceeded(float level_db);
    void clipping_detected();
    void levels_reset();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void update_display();
    void decay_peak_hold();

private:
    /**
     * @brief Rendering functions
     */
    void render_meter_background(QPainter& painter, const QRect& rect);
    void render_meter_scale(QPainter& painter, const QRect& scale_rect);
    void render_level_bar(QPainter& painter, const QRect& meter_rect);
    void render_peak_hold(QPainter& painter, const QRect& meter_rect);
    void render_numeric_display(QPainter& painter, const QRect& text_rect);
    void render_clipping_indicator(QPainter& painter, const QRect& rect);
    
    /**
     * @brief Coordinate conversion
     */
    float db_to_meter_position(float db_level) const;
    float meter_position_to_db(float position) const;
    QRect calculate_meter_rect() const;
    QRect calculate_scale_rect() const;
    QRect calculate_text_rect() const;
    
    /**
     * @brief Level processing
     */
    void process_level_update(const AudioLevelData& new_data);
    void update_peak_hold(float current_peak);
    void apply_meter_ballistics();
    QColor level_to_color(float db_level) const;
    
    /**
     * @brief Configuration
     */
    MeterType meter_type_{MeterType::PPM_METER};
    MeterStandard meter_standard_{MeterStandard::BROADCAST_EU};
    MeterScale meter_scale_;
    MeterStyle meter_style_;
    Qt::Orientation orientation_{Qt::Vertical};
    QString channel_name_{"Ch"};
    
    /**
     * @brief Level data
     */
    AudioLevelData current_levels_;
    AudioLevelData display_levels_;     // Smoothed for display
    std::deque<AudioLevelData> level_history_;
    
    /**
     * @brief Peak hold state
     */
    float peak_hold_level_{-std::numeric_limits<float>::infinity()};
    ve::TimePoint peak_hold_time_{0, 1};
    
    /**
     * @brief Visual state
     */
    std::unique_ptr<QTimer> display_timer_;
    std::unique_ptr<QTimer> peak_hold_timer_;
    mutable QMutex level_mutex_;
    
    /**
     * @brief Cached rendering data
     */
    mutable QRect cached_meter_rect_;
    mutable QRect cached_scale_rect_;
    mutable QRect cached_text_rect_;
    bool geometry_cache_valid_{false};
};

/**
 * @brief Multi-channel audio meter widget
 */
class AudioMetersWidget : public QWidget {
    Q_OBJECT

public:
    explicit AudioMetersWidget(QWidget* parent = nullptr);
    ~AudioMetersWidget() override = default;
    
    /**
     * @brief Channel configuration
     */
    void set_channel_count(size_t channel_count);
    void set_channel_names(const std::vector<QString>& names);
    void add_channel(const QString& name = "");
    void remove_channel(size_t channel_index);
    void clear_channels();
    
    /**
     * @brief Meter configuration
     */
    void set_meter_type(MeterType type);
    void set_meter_standard(MeterStandard standard);
    void set_meter_style(const MeterStyle& style);
    void set_layout_direction(Qt::Orientation orientation);
    
    /**
     * @brief Level updates
     */
    void update_levels(const std::vector<AudioLevelData>& channel_levels);
    void update_channel_level(size_t channel_index, const AudioLevelData& level_data);
    void update_stereo_levels(const AudioLevelData& left, const AudioLevelData& right);
    void reset_all_meters();
    void reset_all_peak_holds();
    
    /**
     * @brief Audio processing integration
     */
    void connect_audio_processor(std::shared_ptr<ve::audio::AudioProcessor> processor);
    void set_auto_update_enabled(bool enabled);
    void set_update_rate(int hz);
    
    /**
     * @brief Professional features
     */
    void enable_loudness_monitoring(bool enabled);
    void set_broadcast_standard(MeterStandard standard);
    void enable_clipping_detection(bool enabled);
    void enable_over_threshold_detection(bool enabled, float threshold_db = -6.0f);
    
    /**
     * @brief Getters
     */
    size_t channel_count() const { return meter_channels_.size(); }
    MeterType meter_type() const { return meter_type_; }
    const std::vector<AudioLevelData>& current_levels() const { return current_levels_; }
    
    /**
     * @brief Size management
     */
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /**
     * @brief Alert signals
     */
    void channel_clipping(size_t channel_index);
    void channel_over_threshold(size_t channel_index, float level_db);
    void loudness_over_limit(float lufs_level);
    void meters_reset();
    
    /**
     * @brief Monitoring signals
     */
    void levels_updated(const std::vector<AudioLevelData>& levels);
    void peak_levels_changed(const std::vector<float>& peak_levels);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void on_channel_clipping();
    void on_channel_threshold_exceeded(float level);
    void on_auto_update_timer();
    void process_audio_buffer(const ve::audio::AudioBuffer& buffer);

private:
    /**
     * @brief Channel management
     */
    void create_channel_meter(const QString& name);
    void update_layout();
    void connect_channel_signals(AudioMeterChannel* channel, size_t index);
    
    /**
     * @brief Audio processing
     */
    void analyze_audio_buffer(const ve::audio::AudioBuffer& buffer);
    AudioLevelData calculate_channel_levels(const float* channel_data, size_t sample_count);
    void update_loudness_measurements(const std::vector<AudioLevelData>& levels);
    
    /**
     * @brief Configuration
     */
    MeterType meter_type_{MeterType::PPM_METER};
    MeterStandard meter_standard_{MeterStandard::BROADCAST_EU};
    MeterStyle meter_style_;
    Qt::Orientation layout_orientation_{Qt::Horizontal};
    
    /**
     * @brief Channel meters
     */
    std::vector<std::unique_ptr<AudioMeterChannel>> meter_channels_;
    std::vector<QString> channel_names_;
    std::vector<AudioLevelData> current_levels_;
    
    /**
     * @brief Layout
     */
    std::unique_ptr<QHBoxLayout> horizontal_layout_;
    std::unique_ptr<QVBoxLayout> vertical_layout_;
    
    /**
     * @brief Audio processing integration
     */
    std::shared_ptr<ve::audio::AudioProcessor> audio_processor_;
    std::unique_ptr<QTimer> auto_update_timer_;
    bool auto_update_enabled_{false};
    int update_rate_hz_{30};
    
    /**
     * @brief Professional monitoring
     */
    bool loudness_monitoring_enabled_{false};
    bool clipping_detection_enabled_{true};
    bool over_threshold_detection_enabled_{true};
    float over_threshold_db_{-6.0f};
    
    /**
     * @brief LUFS measurement state
     */
    struct LoudnessState {
        std::deque<float> momentary_buffer;
        std::deque<float> short_term_buffer;
        std::vector<float> integrated_buffer;
        float integrated_lufs{-std::numeric_limits<float>::infinity()};
        ve::TimePoint measurement_start{0, 1};
    } loudness_state_;
    
    /**
     * @brief Thread safety
     */
    mutable QMutex meters_mutex_;
};

/**
 * @brief Professional master section meters widget
 */
class MasterMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit MasterMeterWidget(QWidget* parent = nullptr);
    ~MasterMeterWidget() override = default;
    
    /**
     * @brief Configuration
     */
    void set_channel_configuration(const QString& config); // "Stereo", "5.1", "7.1", etc.
    void enable_correlation_meter(bool enabled);
    void enable_phase_scope(bool enabled);
    void enable_spectrum_analyzer(bool enabled);
    
    /**
     * @brief Professional features
     */
    void set_broadcast_monitoring(bool enabled, MeterStandard standard);
    void enable_loudness_compliance(bool enabled);
    void set_target_lufs(float target_lufs);
    void enable_gating(bool enabled);
    
    /**
     * @brief Level monitoring
     */
    void update_master_levels(const std::vector<AudioLevelData>& levels);
    void update_correlation(float correlation);
    void update_phase_data(const std::vector<float>& left_channel, const std::vector<float>& right_channel);
    void update_spectrum(const std::vector<float>& frequency_bins);

signals:
    void loudness_compliance_changed(bool compliant);
    void correlation_warning(float correlation);
    void phase_issue_detected();
    void master_clipping();

private:
    std::unique_ptr<AudioMetersWidget> level_meters_;
    std::unique_ptr<QWidget> correlation_meter_;
    std::unique_ptr<QWidget> phase_scope_;
    std::unique_ptr<QWidget> spectrum_analyzer_;
    std::unique_ptr<QLabel> loudness_display_;
    
    // Configuration
    QString channel_config_{"Stereo"};
    bool broadcast_monitoring_{false};
    bool loudness_compliance_{false};
    float target_lufs_{-23.0f}; // EBU R128 standard
    
    void setup_ui();
    void update_loudness_display(float lufs_integrated, float lufs_short_term);
};

/**
 * @brief Utility functions for audio metering
 */
namespace audio_meter_utils {
    /**
     * @brief Level conversion utilities
     */
    float linear_to_db(float linear_level);
    float db_to_linear(float db_level);
    float rms_to_db(const float* samples, size_t count);
    float peak_to_db(const float* samples, size_t count);
    
    /**
     * @brief LUFS calculation (simplified)
     */
    float calculate_lufs_momentary(const std::vector<float>& samples, float sample_rate);
    float calculate_lufs_short_term(const std::deque<float>& momentary_values);
    float calculate_lufs_integrated(const std::vector<float>& short_term_values);
    
    /**
     * @brief Broadcast compliance
     */
    bool check_ebu_r128_compliance(float lufs_integrated, float lufs_range);
    bool check_atsc_a85_compliance(float lufs_integrated);
    
    /**
     * @brief Meter scale utilities
     */
    std::vector<float> generate_scale_marks(float min_db, float max_db, float step_db);
    QString format_db_value(float db_value, int precision = 1);
    QString format_lufs_value(float lufs_value);
    
    /**
     * @brief Color utilities
     */
    QColor interpolate_meter_color(float level_db, const MeterStyle& style);
    QLinearGradient create_meter_gradient(const MeterStyle& style, Qt::Orientation orientation);
}

} // namespace ve::ui