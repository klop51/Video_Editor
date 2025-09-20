/**
 * @file audio_monitor_widget.hpp
 * @brief Real-Time Audio Level Monitoring Widget
 * 
 * Task 7: Real Audio Monitoring     // Audio pipeline integration
    void set_audio_pipeline(ve::core::AudioPipeline* pipeline);
    void start_monitoring();
    void stop_monitoring();
    bool is_monitoring() const { return monitoring_active_; }
    
    // Real-time level updates
    void set_master_levels(float left_db, float right_db);
    void update_channel_levels(const std::vector<std::pair<float, float>>& channel_levels);fessional audio monitoring interface
 * providing VU meters, peak detection, and real-time level visualization
 * for live monitoring during playback and recording.
 */

#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QProgressBar>
#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

namespace ve::audio {
    class AudioPipeline;
}

namespace ve::ui {

/**
 * @brief Audio channel level data for monitoring
 */
struct AudioLevelData {
    float peak_left = 0.0f;        // Peak level (0.0 to 1.0)
    float peak_right = 0.0f;       // Peak level (0.0 to 1.0)
    float rms_left = 0.0f;         // RMS level (0.0 to 1.0)
    float rms_right = 0.0f;        // RMS level (0.0 to 1.0)
    bool clipping = false;         // True if clipping detected
    uint32_t channel_id = 0;       // Audio channel ID
    std::string channel_name;      // Channel name
};

/**
 * @brief VU Meter Widget for displaying audio levels
 */
class VUMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit VUMeterWidget(QWidget* parent = nullptr);
    ~VUMeterWidget() override;

    // Level control
    void set_level(float level_db);
    void set_stereo_levels(float left_db, float right_db);
    void set_peak_hold_time(int ms);
    void set_decay_rate(float db_per_second);
    
    // Visual configuration
    void set_meter_type(bool is_peak_meter);
    void set_scale_range(float min_db, float max_db);
    void set_orientation(Qt::Orientation orientation);
    
    // Status
    bool is_clipping() const { return clipping_detected_; }
    float get_current_level() const { return current_level_db_; }

signals:
    void clipping_detected();
    void level_changed(float level_db);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void update_meter();
    void reset_peak_hold();

private:
    void draw_meter(QPainter& painter);
    void draw_scale(QPainter& painter);
    void draw_level_bar(QPainter& painter, const QRect& rect, float level_db);
    QColor get_level_color(float level_db) const;
    int db_to_pixel(float db) const;
    
    // Configuration
    Qt::Orientation orientation_;
    bool is_stereo_;
    bool is_peak_meter_;
    float min_db_;
    float max_db_;
    
    // Level data
    std::atomic<float> current_level_db_;
    std::atomic<float> left_level_db_;
    std::atomic<float> right_level_db_;
    std::atomic<float> peak_level_db_;
    std::atomic<bool> clipping_detected_;
    
    // Peak hold
    float peak_hold_level_;
    int peak_hold_time_ms_;
    float decay_rate_;
    
    // Timers
    QTimer* update_timer_;
    QTimer* peak_timer_;
    
    // Visual elements
    QRect meter_rect_;
    static constexpr int SCALE_WIDTH = 40;
    static constexpr int METER_WIDTH = 20;
    static constexpr float CLIPPING_THRESHOLD = -3.0f; // dB
};

/**
 * @brief Multi-Channel Audio Monitor Panel
 */
class AudioMonitorPanel : public QWidget {
    Q_OBJECT

public:
    explicit AudioMonitorPanel(QWidget* parent = nullptr);
    ~AudioMonitorPanel() override;

    // Audio pipeline integration
    void set_audio_pipeline(ve::audio::AudioPipeline* pipeline);
    void start_monitoring();
    void stop_monitoring();
    bool is_monitoring() const { return monitoring_active_; }
    
    // Channel management
    void add_channel_monitor(uint32_t channel_id, const std::string& name);
    void remove_channel_monitor(uint32_t channel_id);
    void update_channel_levels(const std::vector<AudioLevelData>& levels);
    
    // Master section
    void set_master_levels(float left_db, float right_db);
    void set_master_volume(float volume_db);
    void set_master_mute(bool muted);

signals:
    void master_volume_changed(float volume_db);
    void master_mute_toggled(bool muted);
    void channel_clipping(uint32_t channel_id);
    void monitoring_started();
    void monitoring_stopped();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void update_audio_levels();
    void on_master_volume_changed(int value);
    void on_master_mute_clicked();

private:
    void setup_ui();
    void setup_master_section();
    void create_channel_monitor(uint32_t channel_id, const std::string& name);
    void update_pipeline_stats();
    
    // UI components
    QVBoxLayout* main_layout_;
    QWidget* master_section_;
    QWidget* channels_section_;
    QVBoxLayout* channels_layout_;
    
    // Master controls
    VUMeterWidget* master_meter_;
    QSlider* master_volume_slider_;
    QPushButton* master_mute_button_;
    QLabel* master_volume_label_;
    QLabel* master_db_label_;
    
    // Channel monitors
    struct ChannelMonitor {
        uint32_t channel_id;
        std::string name;
        VUMeterWidget* meter;
        QLabel* name_label;
        QLabel* level_label;
        QWidget* container;
    };
    std::vector<std::unique_ptr<ChannelMonitor>> channel_monitors_;
    
    // Audio pipeline integration
    ve::audio::AudioPipeline* audio_pipeline_;
    QTimer* monitoring_timer_;
    std::atomic<bool> monitoring_active_;
    
    // Level data storage
    mutable std::mutex levels_mutex_;
    std::vector<AudioLevelData> current_levels_;
    float master_left_db_;
    float master_right_db_;
    
    static constexpr int UPDATE_INTERVAL_MS = 33; // ~30 FPS
    static constexpr int MAX_CHANNELS = 16;
};

/**
 * @brief Compact audio level indicator for toolbars
 */
class CompactAudioMeter : public QWidget {
    Q_OBJECT

public:
    explicit CompactAudioMeter(QWidget* parent = nullptr);
    ~CompactAudioMeter() override;

    void set_levels(float left_db, float right_db);
    void set_clipping(bool clipping);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    std::atomic<float> left_level_db_;
    std::atomic<float> right_level_db_;
    std::atomic<bool> clipping_;
    
    static constexpr int METER_WIDTH = 60;
    static constexpr int METER_HEIGHT = 8;
    static constexpr float MIN_DB = -60.0f;
    static constexpr float MAX_DB = 0.0f;
};

} // namespace ve::ui