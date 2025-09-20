/**
 * @file minimal_audio_meters_widget.hpp
 * @brief Professional Audio Level Meters Widget
 * 
 * Broadcast-standard audio level monitoring widget with professional features:
 * - Multiple meter types (VU, PPM, Digital)
 * - Professional ballistics (IEC 60268-10)
 * - Peak hold and clipping detection
 * - Stereo correlation monitoring
 * - Configurable scales and color coding
 */

#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QContextMenuEvent>
#include <vector>
#include <cmath>

namespace ve::ui {

/**
 * @brief Professional broadcast-standard audio level meters widget
 * 
 * Advanced implementation providing:
 * - Multiple meter types (VU, PPM, Digital)
 * - Professional ballistics and peak hold
 * - Multi-channel support with labeling
 * - Clipping detection and stereo correlation
 * - Broadcast-standard color coding
 */
class MinimalAudioMetersWidget : public QWidget {
    Q_OBJECT

public:
    enum class MeterType {
        VU,         // Volume Unit meter (slower ballistics)
        PPM,        // Peak Programme Meter (faster ballistics)  
        Digital     // Digital peak meter (instantaneous)
    };
    
    enum class ScaleType {
        Linear,     // Linear dB scale
        Broadcast,  // EBU R 128 broadcast scale
        Music       // Music production scale
    };
    
    struct ChannelMeter {
        QString name;
        double current_level = -60.0;    // Current level in dB
        double peak_level = -60.0;       // Peak hold level in dB
        double rms_level = -60.0;        // RMS level in dB
        int peak_hold_time = 0;          // Peak hold timer
        bool is_clipping = false;        // Clipping indicator
        bool is_muted = false;           // Mute status
        QColor meter_color = QColor(0, 255, 0); // Meter color
    };

    explicit MinimalAudioMetersWidget(QWidget* parent = nullptr);
    ~MinimalAudioMetersWidget() override;

    // Configuration
    void set_meter_type(MeterType type);
    void set_scale_type(ScaleType type);
    void set_channel_count(int count);
    void set_update_rate(int fps);
    void set_peak_hold_time(int milliseconds);
    
    // Channel management
    void set_channel_name(int channel, const QString& name);
    void set_channel_mute(int channel, bool muted);
    QString get_channel_name(int channel) const;
    bool is_channel_muted(int channel) const;
    
    // Level updates
    void update_levels(const std::vector<double>& levels);
    void update_level(int channel, double level_db);
    void reset_peak_holds();
    void reset_clipping_indicators();
    
    // Stereo correlation (for stereo pairs)
    void enable_stereo_correlation(bool enabled);
    double get_stereo_correlation() const;
    
    // Visual configuration
    void set_meter_width(int width);
    void set_meter_spacing(int spacing);
    void set_show_labels(bool show);
    void set_show_peak_values(bool show);
    void set_show_rms_values(bool show);
    
    // Getters
    MeterType get_meter_type() const { return meter_type_; }
    ScaleType get_scale_type() const { return scale_type_; }
    int get_channel_count() const { return static_cast<int>(channels_.size()); }

signals:
    void level_changed(int channel, double level_db);
    void peak_detected(int channel, double peak_db);
    void clipping_detected(int channel);
    void stereo_correlation_changed(double correlation);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void update_meters();
    void update_peak_hold();

private:
    // Setup and initialization
    void setup_ui();
    void setup_timers();
    void calculate_layout();
    
    // Drawing methods
    void draw_meter_background(QPainter& painter, const QRect& meter_rect, int channel);
    void draw_meter_scale(QPainter& painter, const QRect& meter_rect);
    void draw_meter_level(QPainter& painter, const QRect& meter_rect, int channel);
    void draw_peak_indicator(QPainter& painter, const QRect& meter_rect, int channel);
    void draw_clipping_indicator(QPainter& painter, const QRect& meter_rect, int channel);
    void draw_channel_label(QPainter& painter, const QRect& meter_rect, int channel);
    void draw_level_values(QPainter& painter, const QRect& meter_rect, int channel);
    void draw_stereo_correlation(QPainter& painter, const QRect& correlation_rect);
    
    // Utility methods
    double db_to_linear(double db) const;
    double linear_to_db(double linear) const;
    int db_to_pixel(double db, const QRect& meter_rect) const;
    double pixel_to_db(int pixel, const QRect& meter_rect) const;
    QColor get_level_color(double db) const;
    QColor get_peak_color(double db) const;
    QString format_db_value(double db) const;
    
    // Level processing
    void apply_ballistics(int channel, double input_level);
    void update_rms_level(int channel, double level);
    void update_peak_level(int channel, double level);
    void calculate_stereo_correlation();
    
    // Layout calculations
    QRect get_meter_rect(int channel) const;
    QRect get_correlation_rect() const;
    int get_meter_channel_at_position(const QPoint& pos) const;
    
    // Configuration
    MeterType meter_type_;
    ScaleType scale_type_;
    int update_rate_fps_;
    int peak_hold_time_ms_;
    int meter_width_;
    int meter_spacing_;
    bool show_labels_;
    bool show_peak_values_;
    bool show_rms_values_;
    bool stereo_correlation_enabled_;
    
    // Channel data
    std::vector<ChannelMeter> channels_;
    double stereo_correlation_;
    
    // Visual properties
    QRect meters_area_;
    int total_width_;
    int total_height_;
    
    // Timers
    QTimer* update_timer_;
    QTimer* peak_hold_timer_;
    
    // Constants for professional meters
    static constexpr double MIN_DB = -60.0;
    static constexpr double MAX_DB = 6.0;
    static constexpr double CLIPPING_THRESHOLD = -0.1;
    static constexpr double WARNING_THRESHOLD = -6.0;
    static constexpr double NORMAL_THRESHOLD = -20.0;
    
    // Ballistics constants (IEC 60268-10)
    static constexpr double VU_ATTACK_TIME = 0.3;   // 300ms attack
    static constexpr double VU_RELEASE_TIME = 1.5;  // 1.5s release
    static constexpr double PPM_ATTACK_TIME = 0.01; // 10ms attack  
    static constexpr double PPM_RELEASE_TIME = 1.7; // 1.7s release
    static constexpr double DIGITAL_ATTACK_TIME = 0.001; // 1ms attack
    static constexpr double DIGITAL_RELEASE_TIME = 0.1;  // 100ms release
};

} // namespace ve::ui