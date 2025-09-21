#include "ui/timeline_dock.hpp"
#include "commands/timeline_commands.hpp"
#include "core/log.hpp"
#include "../../playback/include/playback/controller.hpp"
#include "ui/minimal_audio_track_widget.hpp"
#include "ui/minimal_waveform_widget.hpp"

// Fix Windows macro conflicts
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <QPainter>
#include <QPolygon>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QTimer>
#include <QUrl>
#include <QFileInfo>
#include <QFileDialog>
#include <QApplication>
#include <QDebug>
#include <QEnterEvent>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Timeline constants
static constexpr int MIN_PIXELS_PER_SECOND = 10;
static constexpr int PLAYHEAD_WIDTH = 2;

namespace ve::ui {

// TimelineWidget implementation (internal widget for rendering)
TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
    , timeline_(nullptr)
    , current_time_(ve::TimePoint{0})
    , zoom_factor_(1.0)
    , track_height_(60)
    , header_width_(150)
    , timeline_height_(600)
    , update_timer_(nullptr)
    , throttle_timer_(nullptr) 
    , pending_heavy_update_(false)
    , segments_being_added_(0)
    , playback_controller_(nullptr)
{
    // Professional timeline dimensions
    setMinimumHeight(600);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus);
    
    // Enable hover events for proper mouse highlighting
    setAttribute(Qt::WA_Hover, true);
}

void TimelineWidget::set_timeline(ve::timeline::Timeline* timeline) {
    timeline_ = timeline;
    
    // Enhanced throttling system for better performance during bulk operations
    if (!update_timer_) {
        update_timer_ = new QTimer(this);
        update_timer_->setSingleShot(true);
        update_timer_->setInterval(16); // ~60 FPS max update rate
        connect(update_timer_, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    }
    
    // Add heavy operation throttling timer for bulk segment additions
    if (!throttle_timer_) {
        throttle_timer_ = new QTimer(this);
        throttle_timer_->setSingleShot(true);
        throttle_timer_->setInterval(100); // 100ms for heavy operations
        connect(throttle_timer_, &QTimer::timeout, this, [this]() {
            pending_heavy_update_ = false;
            QWidget::update(); // Force full redraw after throttle period
        });
    }
    
    segments_being_added_++;
    
    if (segments_being_added_ > 2) {
        if (!pending_heavy_update_) {
            pending_heavy_update_ = true;
            throttle_timer_->start();
        }
    } else {
        if (!update_timer_->isActive()) {
            update_timer_->start();
        }
    }
    
    ve::log::info("Timeline widget set with " + std::to_string(timeline ? timeline->tracks().size() : 0) + " tracks");
}

void TimelineWidget::set_zoom(double zoom_factor) {
    zoom_factor_ = zoom_factor;
    update();
}

void TimelineWidget::set_current_time(ve::TimePoint time) {
    current_time_ = time;
    // Debug: Log time changes occasionally
    static int update_count = 0;
    update_count++;
    if (update_count % 30 == 0) { // Log every 30th update to avoid spam
        double seconds = static_cast<double>(time.to_rational().num) / time.to_rational().den;
        int pixel_pos = time_to_pixel(time);
        ve::log::info("Timeline playhead: " + std::to_string(seconds) + "s at pixel " + std::to_string(pixel_pos));
    }
    update(); // Trigger repaint for playhead position
}

void TimelineWidget::set_playback_controller(ve::playback::PlaybackController* controller) {
    playback_controller_ = controller;
}

void TimelineWidget::set_command_executor(CommandExecutor executor) {
    command_executor_ = executor;
}

void TimelineWidget::refresh() {
    update();
}

void TimelineWidget::zoom_in() {
    set_zoom(zoom_factor_ * 1.2);
}

void TimelineWidget::zoom_out() {
    set_zoom(zoom_factor_ / 1.2);
}

void TimelineWidget::zoom_fit() {
    set_zoom(1.0);
}

void TimelineWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(40, 40, 40)); // Dark background
    
    if (!timeline_) {
        draw_default_empty_tracks(painter, rect());
        draw_playhead(painter); // Draw playhead even with no timeline
        return;
    }
    
    if (timeline_->tracks().empty()) {
        draw_default_empty_tracks(painter, rect());
        draw_playhead(painter); // Draw playhead with empty tracks
        return;
    }
    
    draw_tracks(painter, rect());
    draw_playhead(painter); // Draw playhead on top of tracks
}

void TimelineWidget::draw_default_empty_tracks(QPainter& painter, const QRect& rect) {
    // Professional track layout matching industry standards
    QStringList default_tracks = {"V3", "V2", "V1", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8"};
    
    painter.setFont(QFont("Arial", 10));
    
    int y_offset = 30; // Leave space for timeline ruler
    
    for (int i = 0; i < default_tracks.size(); ++i) {
        QRect track_rect(0, y_offset + i * track_height_, rect.width(), track_height_);
        
        // Track background
        QColor track_color = default_tracks[i].startsWith('V') ? 
            QColor(45, 65, 95, 180) :    // Video tracks - dark blue
            QColor(55, 75, 45, 180);     // Audio tracks - dark olive
            
        painter.fillRect(track_rect, track_color);
        
        // Track border
        painter.setPen(QColor(100, 100, 100));
        painter.drawRect(track_rect);
        
        // Track label
        QRect label_rect(5, track_rect.y() + 5, header_width_ - 10, track_height_ - 10);
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, default_tracks[i]);
    }
    
    // Timeline ruler
    painter.fillRect(0, 0, rect.width(), 30, QColor(60, 60, 60));
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(10, 20, "Timeline (Empty - Import media to add content)");
}

void TimelineWidget::draw_tracks(QPainter& painter, const QRect& rect) {
    if (!timeline_) return;
    
    const auto& tracks = timeline_->tracks();
    
    painter.setFont(QFont("Arial", 10));
    
    int y_offset = 30; // Leave space for timeline ruler
    
    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = *tracks[i];
        QRect track_rect(0, y_offset + i * track_height_, rect.width(), track_height_);
        
        // Track background based on type
        QColor track_color = (track.type() == ve::timeline::Track::Video) ? 
            QColor(45, 65, 95, 180) :    // Video tracks - dark blue
            QColor(55, 75, 45, 180);     // Audio tracks - dark olive
            
        painter.fillRect(track_rect, track_color);
        
        // Track border
        painter.setPen(QColor(100, 100, 100));
        painter.drawRect(track_rect);
        
        // Track label
        QRect label_rect(5, track_rect.y() + 5, header_width_ - 10, track_height_ - 10);
        painter.setPen(QColor(200, 200, 200));
        painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(track.name()));
        
        // Draw segments if any
        const auto& segments = track.segments();
        for (const auto& segment : segments) {
            // Simple segment visualization
            int segment_x = header_width_ + 10;
            int segment_width = 100; // Fixed width for now
            QRect segment_rect(segment_x, track_rect.y() + 5, segment_width, track_height_ - 10);
            
            painter.fillRect(segment_rect, QColor(100, 140, 200, 200));
            painter.setPen(QColor(200, 200, 200));
            painter.drawRect(segment_rect);
        }
    }
    
    // Timeline ruler
    painter.fillRect(0, 0, rect.width(), 30, QColor(60, 60, 60));
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(10, 20, QString("Timeline (%1 tracks)").arg(tracks.size()));
}

void TimelineWidget::draw_playhead(QPainter& painter) {
    int x = time_to_pixel(current_time_);
    
    // Make playhead more visible with bright red color and thicker line
    
    // Draw playhead line (bright red vertical line)
    painter.setPen(QPen(QColor(255, 50, 50), 3)); // Thicker, brighter red playhead
    painter.drawLine(x, 0, x, height());
    
    // Draw playhead handle (larger triangle at top)
    QPolygon handle;
    handle << QPoint(x - 8, 0) << QPoint(x + 8, 0) << QPoint(x, 16);
    painter.setBrush(QColor(255, 50, 50)); // Bright red handle
    painter.setPen(QPen(QColor(200, 0, 0), 1)); // Dark red border
    painter.drawPolygon(handle);
    
    // Draw playhead position indicator for debugging
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(x + 10, 20, QString("Playhead: %1px").arg(x));
}

int TimelineWidget::time_to_pixel(ve::TimePoint time) const {
    double pixels_per_second = MIN_PIXELS_PER_SECOND * zoom_factor_;
    double seconds = static_cast<double>(time.to_rational().num) / time.to_rational().den;
    int pixel_pos = header_width_ + static_cast<int>(seconds * pixels_per_second);
    
    // Ensure playhead is always visible, even at time 0
    if (pixel_pos < header_width_) {
        pixel_pos = header_width_;
    }
    
    return pixel_pos;
}

void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    // Basic mouse handling - can be expanded
    QWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    // Basic mouse handling - can be expanded
    QWidget::mouseMoveEvent(event);
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event) {
    // Basic mouse handling - can be expanded
    QWidget::mouseReleaseEvent(event);
}

void TimelineWidget::wheelEvent(QWheelEvent* event) {
    // Basic zoom handling
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoom_in();
        } else {
            zoom_out();
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent* event) {
    // Basic context menu - can be expanded
    QWidget::contextMenuEvent(event);
}

void TimelineWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

// TimelineDock implementation (the main dock widget)
TimelineDock::TimelineDock(const QString& title, QWidget* parent)
    : QDockWidget(title, parent)
    , timeline_widget_(new TimelineWidget(this))
{
    // Set the timeline widget as the dock's main widget
    setWidget(timeline_widget_);
    
    // Configure dock properties
    setAllowedAreas(Qt::BottomDockWidgetArea);
    setMinimumHeight(650);
    resize(width(), 650);
    
    // Prevent floating and set features
    setFloating(false);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    
    // Forward signals from timeline widget to dock
    connect(timeline_widget_, &TimelineWidget::time_changed, this, &TimelineDock::time_changed);
    connect(timeline_widget_, &TimelineWidget::selection_changed, this, &TimelineDock::selection_changed);
    connect(timeline_widget_, &TimelineWidget::track_height_changed, this, &TimelineDock::track_height_changed);
    connect(timeline_widget_, &TimelineWidget::clip_added, this, &TimelineDock::clip_added);
    
    ve::log::info("TimelineDock created with integrated timeline widget");
}

// Forward timeline methods to internal widget
void TimelineDock::set_timeline(ve::timeline::Timeline* timeline) {
    timeline_widget_->set_timeline(timeline);
}

void TimelineDock::set_zoom(double zoom_factor) {
    timeline_widget_->set_zoom(zoom_factor);
}

void TimelineDock::set_current_time(ve::TimePoint time) {
    timeline_widget_->set_current_time(time);
    timeline_widget_->update(); // Explicit update to ensure repaint
}

void TimelineDock::set_playback_controller(ve::playback::PlaybackController* controller) {
    timeline_widget_->set_playback_controller(controller);
}

void TimelineDock::set_command_executor(TimelineWidget::CommandExecutor executor) {
    timeline_widget_->set_command_executor(executor);
}

void TimelineDock::refresh() {
    timeline_widget_->refresh();
}

void TimelineDock::zoom_in() {
    timeline_widget_->zoom_in();
}

void TimelineDock::zoom_out() {
    timeline_widget_->zoom_out();
}

void TimelineDock::zoom_fit() {
    timeline_widget_->zoom_fit();
}

} // namespace ve::ui