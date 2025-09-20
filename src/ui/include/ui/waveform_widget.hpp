/**
 * @file waveform_widget.hpp
 * @brief High-Performance Qt Waveform Widget for Professional Audio Timeline
 * 
 * Week 8 Qt Timeline UI Integration - Professional waveform visualization widget
 * that integrates with the Week 7 waveform generation system to provide real-time
 * audio visualization in the video editor timeline.
 * 
 * Features:
 * - Real-time waveform rendering at 60fps
 * - Multi-resolution zoom (sample-accurate to timeline overview)
 * - Efficient QPainter optimization with paint caching
 * - Mouse interaction for precise audio editing
 * - Integration with Week 6 A/V sync master clock
 * - Professional broadcast-quality visualization
 */

#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtCore/QTimer>
#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QPaintEvent>
#include <memory>
#include <functional>
#include <mutex>

// Forward declarations for audio engine integration
namespace ve::audio {
    class WaveformGenerator;
    class WaveformCache;
    struct WaveformData;
    struct ZoomLevel;
}

// Forward declaration for TimePoint
namespace ve {
    class TimePoint;
}

namespace ve::ui {

/**
 * @brief Waveform rendering style configuration
 */
struct WaveformStyle {
    // Colors
    QColor background_color{42, 42, 42};           // Dark timeline background
    QColor waveform_color{100, 200, 255};         // Primary waveform blue
    QColor waveform_fill_color{100, 200, 255, 80}; // Semi-transparent fill
    QColor peak_color{255, 150, 150};             // Peak indicator red
    QColor selection_color{255, 255, 100, 100};   // Selection highlight
    QColor playhead_color{255, 255, 255};         // Playhead line
    QColor grid_color{80, 80, 80};                // Grid lines
    
    // Dimensions
    int waveform_height{80};                      // Height in pixels
    int margin_top{4};                            // Top margin
    int margin_bottom{4};                         // Bottom margin
    float line_width{1.0f};                       // Waveform line width
    float peak_line_width{2.0f};                  // Peak indicator width
    
    // Behavior
    bool show_peaks{true};                        // Show peak indicators
    bool show_rms{true};                          // Show RMS envelope
    bool show_grid{true};                         // Show time grid
    bool anti_aliasing{true};                     // Enable antialiasing
    float peak_threshold{0.8f};                   // Peak detection threshold
};

/**
 * @brief Waveform zoom and navigation state
 */
struct WaveformViewport {
    ve::TimePoint start_time;                     // Visible timeline start
    ve::TimePoint duration;                       // Visible timeline duration
    float samples_per_pixel{100.0f};             // Current zoom level
    float min_samples_per_pixel{1.0f};           // Maximum zoom in
    float max_samples_per_pixel{10000.0f};       // Maximum zoom out
    
    // Calculated properties
    float pixels_per_second() const;
    ve::TimePoint end_time() const;
    bool contains_time(const ve::TimePoint& time) const;
    QRect time_to_rect(const ve::TimePoint& start, const ve::TimePoint& duration, int height) const;
};

/**
 * @brief Mouse interaction state for precise audio editing
 */
struct WaveformInteraction {
    enum class Mode {
        NONE,
        SELECTION,                                // Selecting audio range
        SCRUBBING,                               // Audio scrubbing/playback
        ZOOMING,                                 // Timeline zoom operation
        PANNING                                  // Timeline pan operation
    };
    
    Mode current_mode{Mode::NONE};
    QPoint drag_start;                           // Mouse drag start position
    QPoint drag_current;                         // Current mouse position
    ve::TimePoint selection_start;               // Audio selection start
    ve::TimePoint selection_end;                 // Audio selection end
    bool is_dragging{false};                     // Currently dragging
    
    // Convert pixel position to timeline position
    ve::TimePoint pixel_to_time(int pixel_x, const WaveformViewport& viewport) const;
    int time_to_pixel(const ve::TimePoint& time, const WaveformViewport& viewport) const;
};

/**
 * @brief High-performance Qt Waveform Widget
 * 
 * Professional-grade waveform visualization widget designed for real-time
 * audio timeline display in professional video editing workflows.
 */
class QWaveformWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Widget construction and configuration
     */
    explicit QWaveformWidget(QWidget* parent = nullptr);
    ~QWaveformWidget() override;
    
    // Disable copy semantics for Qt widget
    QWaveformWidget(const QWaveformWidget&) = delete;
    QWaveformWidget& operator=(const QWaveformWidget&) = delete;

    /**
     * @brief Audio source configuration
     */
    void set_audio_source(const std::string& audio_file_path);
    void set_waveform_generator(std::shared_ptr<ve::audio::WaveformGenerator> generator);
    void set_waveform_cache(std::shared_ptr<ve::audio::WaveformCache> cache);
    
    /**
     * @brief Timeline integration
     */
    void set_timeline_range(const ve::TimePoint& start, const ve::TimePoint& duration);
    void set_playhead_position(const ve::TimePoint& position);
    void set_selection_range(const ve::TimePoint& start, const ve::TimePoint& end);
    
    /**
     * @brief Zoom and navigation
     */
    void zoom_to_fit();                          // Fit entire audio to widget
    void zoom_in(float factor = 2.0f);           // Zoom in by factor
    void zoom_out(float factor = 2.0f);          // Zoom out by factor
    void zoom_to_selection();                    // Zoom to current selection
    void pan_to_time(const ve::TimePoint& center_time); // Pan to center time
    
    /**
     * @brief Visual customization
     */
    void set_style(const WaveformStyle& style);
    const WaveformStyle& get_style() const { return style_; }
    void set_height_hint(int height) { height_hint_ = height; update(); }
    
    /**
     * @brief Real-time updates
     */
    void refresh_waveform();                     // Force waveform regeneration
    void set_auto_refresh(bool enabled);         // Enable automatic refresh
    void set_refresh_rate(int fps);              // Set refresh rate (default 60fps)
    
    /**
     * @brief Qt widget interface
     */
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /**
     * @brief User interaction signals
     */
    void playhead_position_changed(const ve::TimePoint& position);
    void selection_changed(const ve::TimePoint& start, const ve::TimePoint& end);
    void zoom_changed(float samples_per_pixel);
    void audio_scrubbing(const ve::TimePoint& position);
    void waveform_clicked(const ve::TimePoint& position, Qt::MouseButton button);
    void waveform_double_clicked(const ve::TimePoint& position);
    
    /**
     * @brief Performance and status signals
     */
    void waveform_generation_progress(float progress);
    void waveform_generation_complete();
    void rendering_performance_warning(const QString& message);

protected:
    /**
     * @brief Qt event handling
     */
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
    /**
     * @brief Keyboard interaction
     */
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    /**
     * @brief Internal update mechanisms
     */
    void update_waveform_data();                 // Background waveform update
    void update_rendering();                     // Refresh visual rendering
    void handle_cache_update();                  // Handle cache changes

private:
    /**
     * @brief Core rendering system
     */
    void render_waveform(QPainter& painter, const QRect& rect);
    void render_background(QPainter& painter, const QRect& rect);
    void render_grid(QPainter& painter, const QRect& rect);
    void render_waveform_data(QPainter& painter, const QRect& rect);
    void render_selection(QPainter& painter, const QRect& rect);
    void render_playhead(QPainter& painter, const QRect& rect);
    void render_peaks(QPainter& painter, const QRect& rect);
    
    /**
     * @brief Paint optimization system
     */
    void invalidate_paint_cache();
    void update_paint_cache();
    bool is_paint_cache_valid() const;
    QPixmap get_cached_waveform(const QRect& rect);
    void cache_waveform_region(const QRect& rect, const QPixmap& pixmap);
    
    /**
     * @brief Waveform data management
     */
    void request_waveform_data(const ve::TimePoint& start, const ve::TimePoint& duration);
    void process_waveform_data(std::shared_ptr<ve::audio::WaveformData> data);
    ve::audio::ZoomLevel calculate_optimal_zoom_level() const;
    bool is_waveform_data_valid() const;
    
    /**
     * @brief Performance monitoring
     */
    void track_rendering_performance();
    void check_performance_thresholds();
    void emit_performance_warnings();
    
    /**
     * @brief Coordinate system conversion
     */
    QRect timeline_to_widget_rect(const ve::TimePoint& start, const ve::TimePoint& duration) const;
    ve::TimePoint widget_to_timeline_position(const QPoint& widget_pos) const;
    float calculate_pixel_to_time_ratio() const;
    
    /**
     * @brief State management
     */
    std::string audio_source_path_;              // Current audio file
    WaveformStyle style_;                        // Visual appearance
    WaveformViewport viewport_;                  // Current view state
    WaveformInteraction interaction_;            // Mouse interaction state
    
    ve::TimePoint playhead_position_;            // Current playback position
    ve::TimePoint selection_start_;              // Selection start time
    ve::TimePoint selection_end_;                // Selection end time
    
    /**
     * @brief Week 7 waveform system integration
     */
    std::shared_ptr<ve::audio::WaveformGenerator> waveform_generator_;
    std::shared_ptr<ve::audio::WaveformCache> waveform_cache_;
    std::shared_ptr<ve::audio::WaveformData> current_waveform_data_;
    
    /**
     * @brief Qt-specific components
     */
    std::unique_ptr<QTimer> refresh_timer_;      // Automatic refresh timer
    std::unique_ptr<QTimer> update_timer_;       // Background update timer
    QPixmap paint_cache_;                        // Cached rendering
    QRect paint_cache_rect_;                     // Cache validity region
    bool paint_cache_valid_{false};              // Cache validity flag
    
    /**
     * @brief Configuration
     */
    int height_hint_{100};                       // Preferred widget height
    int refresh_rate_{60};                       // Target refresh rate (fps)
    bool auto_refresh_enabled_{true};            // Automatic refresh
    bool is_initialized_{false};                 // Widget initialization state
    
    /**
     * @brief Performance tracking
     */
    mutable std::mutex performance_mutex_;       // Thread safety for metrics
    struct PerformanceMetrics {
        double last_render_time_ms{0.0};
        double average_render_time_ms{0.0};
        int frame_count{0};
        int dropped_frames{0};
        std::chrono::high_resolution_clock::time_point last_frame_time;
    } performance_metrics_;
    
    /**
     * @brief Thread safety
     */
    mutable std::mutex data_mutex_;              // Protect waveform data access
    mutable std::mutex cache_mutex_;             // Protect cache operations
};

/**
 * @brief Utility functions for waveform widget integration
 */
namespace waveform_utils {
    /**
     * @brief Color interpolation for smooth gradients
     */
    QColor interpolate_color(const QColor& start, const QColor& end, float ratio);
    
    /**
     * @brief Optimized waveform point rendering
     */
    void render_waveform_points(QPainter& painter, 
                               const std::vector<QPointF>& points,
                               const WaveformStyle& style);
    
    /**
     * @brief Time grid calculation for professional timeline display
     */
    std::vector<ve::TimePoint> calculate_grid_intervals(
        const ve::TimePoint& start,
        const ve::TimePoint& duration,
        int widget_width,
        int min_pixel_spacing = 50
    );
    
    /**
     * @brief Performance optimization helpers
     */
    bool should_skip_rendering(const QRect& widget_rect, const QRect& damage_rect);
    QRect calculate_minimal_update_region(const QRect& old_rect, const QRect& new_rect);
    
    /**
     * @brief Professional audio visualization utilities
     */
    float db_to_linear(float db);               // dB to linear amplitude conversion
    float linear_to_db(float linear);           // Linear to dB conversion
    QColor amplitude_to_color(float amplitude, const WaveformStyle& style);
}

} // namespace ve::ui