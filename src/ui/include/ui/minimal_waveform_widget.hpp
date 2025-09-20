/**
 * @file minimal_waveform_widget.hpp
 * @brief Enhanced Qt Waveform Widget for Week 8 Qt Timeline UI Integration
 * 
 * Professional implementation with proper integration to Week 7 waveform
 * generation system. Features real-time waveform display, zoom controls,
 * and professional audio visualization.
 */

#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QPainter>
#include <QtCore/QTimer>
#include <QtCore/QFutureWatcher>
#include <QtCore/QMutex>
#include <memory>
#include <vector>

// Forward declarations for Week 7 audio engine integration
namespace ve::audio {
    class WaveformGenerator;
    class WaveformCache;
    struct WaveformData;
}

namespace ve {
    struct AudioFrame;
}

namespace ve::ui {

/**
 * @brief Professional Qt widget for displaying audio waveforms
 * 
 * Enhanced implementation featuring:
 * - Real Week 7 WaveformGenerator integration
 * - Multi-resolution waveform display
 * - Professional zoom and pan controls
 * - 60fps rendering optimization
 * - Interactive timeline navigation
 */
class MinimalWaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit MinimalWaveformWidget(QWidget* parent = nullptr);
    ~MinimalWaveformWidget() override;

    // Week 7 WaveformGenerator integration
    void set_waveform_generator(std::shared_ptr<ve::audio::WaveformGenerator> generator);
    void set_waveform_cache(std::shared_ptr<ve::audio::WaveformCache> cache);
    
    // Audio content management
    void set_audio_file(const QString& file_path);
    void set_audio_duration(double duration_seconds);
    void clear_waveform_data();
    
    // Professional zoom and navigation
    void set_zoom_level(double zoom_factor);
    void set_time_range(double start_seconds, double duration_seconds);
    void zoom_to_selection(double start_seconds, double end_seconds);
    void zoom_fit_all();
    
    // Timeline integration
    void set_playhead_position(double position_seconds);
    void set_selection(double start_seconds, double end_seconds);
    void clear_selection();
    
    // Visual configuration
    void set_waveform_color(const QColor& color);
    void set_background_color(const QColor& color);
    void set_grid_enabled(bool enabled);
    void set_time_rulers_enabled(bool enabled);

signals:
    void playhead_position_changed(double position_seconds);
    void selection_changed(double start_seconds, double end_seconds);
    void waveform_clicked(double time_seconds);
    void zoom_level_changed(double zoom_factor);
    void waveform_generation_progress(int percentage);

public slots:
    void update_waveform();
    void refresh_display();
    void handle_waveform_ready();

protected:
    // Qt widget overrides
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // Week 7 integration
    std::shared_ptr<ve::audio::WaveformGenerator> waveform_generator_;
    std::shared_ptr<ve::audio::WaveformCache> waveform_cache_;
    
    // Waveform data management
    struct WaveformDisplayData {
        std::vector<float> peaks;
        std::vector<float> rms_values;
        double samples_per_pixel = 1.0;
        bool is_valid = false;
    };
    WaveformDisplayData current_waveform_;
    QMutex waveform_mutex_;
    
    // Audio file state
    QString current_audio_file_;
    double audio_duration_seconds_ = 0.0;
    
    // Display state
    double zoom_factor_ = 1.0;
    double display_start_seconds_ = 0.0;
    double display_duration_seconds_ = 10.0;
    double playhead_position_seconds_ = 0.0;
    
    // Selection state
    bool has_selection_ = false;
    double selection_start_seconds_ = 0.0;
    double selection_end_seconds_ = 0.0;
    
    // Mouse interaction state
    enum class InteractionMode {
        None,
        Dragging,
        Selecting,
        MovingPlayhead
    };
    InteractionMode interaction_mode_ = InteractionMode::None;
    QPoint last_mouse_pos_;
    double mouse_drag_start_time_ = 0.0;
    
    // Visual configuration
    QColor waveform_color_ = QColor(100, 150, 255);
    QColor background_color_ = QColor(30, 30, 30);
    QColor playhead_color_ = QColor(255, 100, 100);
    QColor selection_color_ = QColor(255, 255, 100, 60);
    QColor grid_color_ = QColor(80, 80, 80);
    bool grid_enabled_ = true;
    bool time_rulers_enabled_ = true;
    
    // Qt components and optimization
    QTimer* update_timer_;
    QTimer* render_throttle_timer_;
    QFutureWatcher<void>* waveform_loader_;
    bool needs_repaint_ = true;
    
    // Helper methods for waveform generation
    void load_waveform_async();
    void generate_display_waveform();
    bool is_waveform_cached() const;
    
    // Drawing methods
    void draw_waveform(QPainter& painter, const QRect& rect);
    void draw_placeholder(QPainter& painter, const QRect& rect);
    void draw_playhead(QPainter& painter, const QRect& rect);
    void draw_selection(QPainter& painter, const QRect& rect);
    void draw_grid(QPainter& painter, const QRect& rect);
    void draw_time_rulers(QPainter& painter, const QRect& rect);
    
    // Coordinate conversion
    double pixel_to_time(int pixel_x) const;
    int time_to_pixel(double time_seconds) const;
    double get_samples_per_pixel() const;
    
    // Helper methods for time formatting and grid
    double calculate_time_step() const;
    QString format_time(double seconds) const;
    
    // Optimization helpers
    void throttle_repaint();
    bool should_regenerate_waveform() const;
    void update_display_parameters();
};

} // namespace ve::ui