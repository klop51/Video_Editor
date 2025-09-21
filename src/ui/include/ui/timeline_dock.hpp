#pragma once
#include "../../timeline/include/timeline/timeline.hpp"
#include "../../core/include/core/time.hpp"
#include "../../audio/include/audio/waveform_cache.h"
#include "../../audio/include/audio/waveform_generator.h"
#include <QDockWidget>
#include <QWidget>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QMenu>
#include <memory>
#include <functional>
#include <limits>
#include <unordered_map>
#include <chrono>

// Forward declarations
namespace ve::commands { class Command; }
namespace ve::playback { class PlaybackController; }

namespace ve::ui {

// Custom timeline widget that will be placed inside the dock
class TimelineWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    
    void set_timeline(ve::timeline::Timeline* timeline);
    void set_zoom(double zoom_factor);
    void set_current_time(ve::TimePoint time);
    
    // Video playback integration
    void set_playback_controller(ve::playback::PlaybackController* controller);
    
    // Command system integration
    using CommandExecutor = std::function<bool(std::unique_ptr<ve::commands::Command>)>;
    void set_command_executor(CommandExecutor executor);
    
    // View parameters
    double zoom_factor() const { return zoom_factor_; }
    ve::TimePoint current_time() const { return current_time_; }
    
public slots:
    void refresh();
    void zoom_in();
    void zoom_out();
    void zoom_fit();
    
signals:
    void time_changed(ve::TimePoint time);
    void selection_changed();
    void track_height_changed();
    void clip_added(const QString& filePath, ve::TimePoint start_time, int track_index);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private:
    // Core timeline data
    ve::timeline::Timeline* timeline_;
    double zoom_factor_;
    ve::TimePoint current_time_;
    
    // Cached dimensions
    int track_height_;
    int header_width_;
    int timeline_height_;
    
    // Performance optimization
    QTimer* update_timer_;
    QTimer* throttle_timer_;
    bool pending_heavy_update_;
    int segments_being_added_;
    
    // Drawing methods
    void draw_minimal_timeline(QPainter& painter, const QRect& rect);
    void draw_tracks(QPainter& painter, const QRect& rect);
    void draw_default_empty_tracks(QPainter& painter, const QRect& rect);
    void draw_playhead(QPainter& painter);
    
    // Helper methods
    int time_to_pixel(ve::TimePoint time) const;
    
    // Command execution
    CommandExecutor command_executor_;
    ve::playback::PlaybackController* playback_controller_;
};

// Combined Timeline Dock - contains the timeline widget
class TimelineDock : public QDockWidget {
    Q_OBJECT
    
public:
    explicit TimelineDock(const QString& title, QWidget* parent = nullptr);
    
    // Direct timeline access (forwards to internal widget)
    void set_timeline(ve::timeline::Timeline* timeline);
    void set_zoom(double zoom_factor);
    void set_current_time(ve::TimePoint time);
    void set_playback_controller(ve::playback::PlaybackController* controller);
    void set_command_executor(TimelineWidget::CommandExecutor executor);
    
    // Timeline widget access
    TimelineWidget* timeline_widget() { return timeline_widget_; }
    
public slots:
    void refresh();
    void zoom_in();
    void zoom_out();
    void zoom_fit();
    
signals:
    void time_changed(ve::TimePoint time);
    void selection_changed();
    void track_height_changed();
    void clip_added(const QString& filePath, ve::TimePoint start_time, int track_index);
    
private:
    TimelineWidget* timeline_widget_;
};

} // namespace ve::ui