#include "ui/timeline_panel.hpp"
#include "commands/timeline_commands.hpp"
#include "core/log.hpp"
#include <QPainter>
#include <QPolygon>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QApplication>
#include <QDebug>
#include <algorithm>

namespace ve::ui {

TimelinePanel::TimelinePanel(QWidget* parent)
    : QWidget(parent)
    , timeline_(nullptr)
    , current_time_(ve::TimePoint{0})
    , zoom_factor_(1.0)
    , scroll_x_(0)
    , dragging_(false)
    , dragged_segment_id_(0)
    , dragging_segment_(false)
    , resizing_segment_(false)
    , is_left_resize_(false)
    , show_drag_preview_(false)
    , selecting_range_(false)
    , selection_start_(ve::TimePoint{0})
    , selection_end_(ve::TimePoint{0})
{
    setMinimumHeight(200);
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus); // Only get focus when clicked, not interfering with drag
    
    // Initialize colors
    track_color_video_ = QColor(80, 120, 180);
    track_color_audio_ = QColor(120, 180, 80);
    segment_color_ = QColor(100, 140, 200);
    segment_selected_color_ = QColor(200, 140, 100);
    playhead_color_ = QColor(255, 0, 0);
    background_color_ = QColor(45, 45, 45);
    grid_color_ = QColor(70, 70, 70);
    
    // Add a QTimer for heartbeat debugging
    heartbeat_timer_ = new QTimer(this);
    connect(heartbeat_timer_, &QTimer::timeout, this, [this]() {
        static int heartbeat_count = 0;
        heartbeat_count++;
        if (heartbeat_count % 30 == 0) { // Every 3 seconds if 100ms intervals
            qDebug() << "HEARTBEAT #" << heartbeat_count << "- UI thread is responsive";
        }
    });
    heartbeat_timer_->start(100); // 100ms intervals
}

void TimelinePanel::set_timeline(ve::timeline::Timeline* timeline) {
    timeline_ = timeline;
    update();
}

void TimelinePanel::set_command_executor(CommandExecutor executor) {
    command_executor_ = executor;
}

void TimelinePanel::set_zoom(double zoom_factor) {
    zoom_factor_ = std::clamp(zoom_factor, 0.1, 10.0);
    update();
}

void TimelinePanel::set_current_time(ve::TimePoint time) {
    current_time_ = time;
    update();
}

void TimelinePanel::refresh() {
    update();
}

void TimelinePanel::zoom_in() {
    set_zoom(zoom_factor_ * 1.2);
}

void TimelinePanel::zoom_out() {
    set_zoom(zoom_factor_ / 1.2);
}

void TimelinePanel::zoom_fit() {
    if (!timeline_) return;
    
    auto duration = timeline_->duration();
    if (duration.to_rational().num <= 0) return;
    
    double duration_seconds = static_cast<double>(duration.to_rational().num) / duration.to_rational().den;
    double available_width = width() - 100; // Leave some margin
    double pixels_per_second = available_width / duration_seconds;
    
    zoom_factor_ = pixels_per_second / MIN_PIXELS_PER_SECOND;
    zoom_factor_ = std::clamp(zoom_factor_, 0.1, 10.0);
    
    scroll_x_ = 0;
    update();
}

void TimelinePanel::paintEvent(QPaintEvent* event) {
    static QElapsedTimer paint_timer;
    paint_timer.start();
    
    static int paint_count = 0;
    paint_count++;
    if (paint_count % 50 == 0) {  // Reduced frequency
        qDebug() << "PAINT EVENT #" << paint_count;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false); // Crisp lines for timeline
    
    // Aggressive clipping to only paint visible area
    QRect clipRect = event->rect();
    painter.setClipRect(clipRect);
    
    // Skip expensive operations if clip rect is very small (like resize handles)
    if (clipRect.width() < 5 || clipRect.height() < 5) {
        paint_timer.restart();
        return;
    }
    
    draw_background(painter);
    draw_timecode_ruler(painter);
    
    // Only draw tracks if they're in the visible area
    if (timeline_ && clipRect.intersects(QRect(0, TIMECODE_HEIGHT, width(), height() - TIMECODE_HEIGHT))) {
        draw_tracks(painter);
    }
    
    // Only draw overlays if they're in visible area
    if (show_drag_preview_) {
        draw_drag_preview(painter);
    }
    
    draw_playhead(painter);
    draw_selection(painter);
    
    auto elapsed = paint_timer.elapsed();
    if (elapsed > 16) { // Log slow paint events
        qDebug() << "SLOW PAINT EVENT took" << elapsed << "ms (clip rect:" << clipRect << ")";
    }
}

void TimelinePanel::mousePressEvent(QMouseEvent* event) {
    qDebug() << "MousePressEvent: button=" << event->button() << "pos=" << event->pos() << "timeline_=" << (timeline_ != nullptr);
    
    if (!timeline_) return;
    
    drag_start_ = event->pos();
    last_mouse_pos_ = event->pos();
    
    if (event->button() == Qt::LeftButton) {
        if (event->pos().y() < TIMECODE_HEIGHT) {
            // Clicked in timecode ruler - start scrubbing
            qDebug() << "Clicked in timecode ruler";
            ve::TimePoint time = pixel_to_time(event->pos().x());
            current_time_ = time;
            emit time_changed(time);
            dragging_ = true;
            update();
        } else {
            // Clicked in track area - check for segment interaction
            qDebug() << "Clicked in track area, searching for segment";
            ve::timeline::Segment* segment = find_segment_at_pos(event->pos());
            
            qDebug() << "Found segment:" << (segment != nullptr);
            if (segment) {
                qDebug() << "Segment ID:" << segment->id << "start:" << segment->start_time.to_rational().num;
                // Check if clicking on segment edge for resizing
                bool is_left_edge;
                if (is_on_segment_edge(event->pos(), *segment, is_left_edge)) {
                    // Start resizing operation
                    qDebug() << "Starting resize operation, left_edge=" << is_left_edge;
                    resizing_segment_ = true;
                    is_left_resize_ = is_left_edge;
                    dragged_segment_id_ = segment->id;
                    original_segment_start_ = segment->start_time;
                    original_segment_duration_ = segment->duration;
                    setCursor(Qt::SizeHorCursor);
                    // Don't grab mouse immediately - wait for actual mouse movement
                } else {
                    // Start moving operation
                    qDebug() << "Starting move operation for segment ID:" << segment->id;
                    dragging_segment_ = true;
                    dragged_segment_id_ = segment->id;
                    original_segment_start_ = segment->start_time;
                    original_segment_duration_ = segment->duration;
                    setCursor(Qt::ClosedHandCursor);
                    // Don't grab mouse immediately - wait for actual mouse movement
                    qDebug() << "Ready for dragging, dragging_segment_=" << dragging_segment_;
                    
                    // Select the segment if not already selected
                    if (std::find(selected_segments_.begin(), selected_segments_.end(), segment->id) == selected_segments_.end()) {
                        if (!(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
                            selected_segments_.clear();
                        }
                        selected_segments_.push_back(segment->id);
                        emit selection_changed();
                        update(); // Update display to show selection
                    }
                }
            } else {
                // Clicked empty space - handle selection
                qDebug() << "Clicked empty space";
                handle_click(event->pos());
            }
            
            dragging_ = true;
            qDebug() << "Final dragging_ state:" << dragging_;
        }
    } else if (event->button() == Qt::RightButton) {
        // Right click for context menu
        handle_context_menu(event->pos());
    }
}

void TimelinePanel::mouseMoveEvent(QMouseEvent* event) {
    static int mouse_move_count = 0;
    mouse_move_count++;
    if (mouse_move_count % 50 == 0) {
        qDebug() << "MOUSE MOVE #" << mouse_move_count << "- frequency check, pos=" << event->pos();
    }
    
    if (!timeline_) return;
    
    // Check if we need to start dragging (if segment operations are prepared but not yet dragging)
    if ((dragging_segment_ || resizing_segment_) && !dragging_) {
        // Start actual dragging if mouse moved enough
        QPoint diff = event->pos() - last_mouse_pos_;
        if (abs(diff.x()) > 3 || abs(diff.y()) > 3) { // Minimum movement threshold
            dragging_ = true;
            grabMouse(); // Now grab mouse for actual drag operation
            qDebug() << "Starting actual drag operation - mouse grabbed";
        }
    }
    
    if (dragging_) {
        qDebug() << "MouseMove: dragging=true, pos=" << event->pos() << "dragging_segment_=" << dragging_segment_ << "resizing_segment_=" << resizing_segment_;
        if (event->pos().y() < TIMECODE_HEIGHT) {
            // Scrubbing in timecode ruler
            qDebug() << "Scrubbing in timecode ruler";
            ve::TimePoint time = pixel_to_time(event->pos().x());
            current_time_ = time;
            emit time_changed(time);
            update();
        } else {
            // Dragging in track area
            qDebug() << "Dragging in track area, calling update_drag";
            update_drag(event->pos());
        }
    } else {
        // Throttle cursor updates - only update every 10 pixels movement to reduce calls
        QPoint diff = event->pos() - last_mouse_pos_;
        if (abs(diff.x()) > 10 || abs(diff.y()) > 10) {
            update_cursor(event->pos());
        }
    }
    
    last_mouse_pos_ = event->pos();
}

void TimelinePanel::mouseReleaseEvent(QMouseEvent* event) {
    qDebug() << "MouseReleaseEvent: dragging_=" << dragging_ << "dragging_segment_=" << dragging_segment_ << "resizing_segment_=" << resizing_segment_;
    
    if (dragging_) {
        // Complete any ongoing operations with command system
        if (command_executor_ && (dragging_segment_ || resizing_segment_)) {
            qDebug() << "Finishing segment edit";
            finish_segment_edit(event->pos());
        }
        
        qDebug() << "Ending drag operation";
        end_drag(event->pos());
        dragging_ = false;
        dragging_segment_ = false;
        resizing_segment_ = false;
        dragged_segment_id_ = 0;
        
        // Hide drag preview
        show_drag_preview_ = false;
        
        // Release mouse grab and restore cursor
        if (hasMouseTracking()) {
            releaseMouse();
        }
        
        // Reset cursor state completely
        setCursor(Qt::ArrowCursor);
        
        // Update cursor based on current mouse position
        QPoint current_pos = mapFromGlobal(QCursor::pos());
        update_cursor(current_pos);
        
        // Update cursor for new position
        update_cursor(event->pos());
    }
}

void TimelinePanel::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double delta = event->angleDelta().y() / 120.0; // Standard wheel step
        double zoom_change = 1.0 + (delta * 0.1);
        set_zoom(zoom_factor_ * zoom_change);
    } else {
        // Scroll horizontally
        scroll_x_ -= event->angleDelta().y() / 8; // Adjust scroll speed
        scroll_x_ = std::max(0, scroll_x_);
        update();
    }
}

void TimelinePanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void TimelinePanel::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            delete_selected_segments();
            break;
            
        case Qt::Key_X:
            if (event->modifiers() & Qt::ControlModifier) {
                cut_selected_segments();
            }
            break;
            
        case Qt::Key_C:
            if (event->modifiers() & Qt::ControlModifier) {
                copy_selected_segments();
            }
            break;
            
        case Qt::Key_V:
            if (event->modifiers() & Qt::ControlModifier) {
                paste_segments();
            }
            break;
            
        case Qt::Key_S:
            if (event->modifiers() & Qt::ControlModifier) {
                split_segment_at_playhead();
            }
            break;
            
        case Qt::Key_Left:
            // Move playhead left
            if (event->modifiers() & Qt::ControlModifier) {
                // Jump to previous clip
                jump_to_previous_clip();
            } else {
                // Move playhead by small increment
                seek_relative(-1.0); // 1 second back
            }
            break;
            
        case Qt::Key_Right:
            // Move playhead right
            if (event->modifiers() & Qt::ControlModifier) {
                // Jump to next clip
                jump_to_next_clip();
            } else {
                // Move playhead by small increment
                seek_relative(1.0); // 1 second forward
            }
            break;
            
        case Qt::Key_Home:
            // Jump to beginning
            current_time_ = ve::TimePoint{0};
            emit time_changed(current_time_);
            update();
            break;
            
        case Qt::Key_End:
            // Jump to end
            jump_to_end();
            break;
            
        case Qt::Key_Escape:
            // Escape key - cancel any ongoing drag operations
            if (dragging_ || dragging_segment_ || resizing_segment_) {
                qDebug() << "Escape pressed - canceling drag operations";
                cancel_drag_operations();
            }
            break;
            
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void TimelinePanel::draw_background(QPainter& painter) {
    painter.fillRect(rect(), background_color_);
}

void TimelinePanel::draw_timecode_ruler(QPainter& painter) {
    QRect ruler_rect(0, 0, width(), TIMECODE_HEIGHT);
    painter.fillRect(ruler_rect, QColor(60, 60, 60));
    
    painter.setPen(QColor(200, 200, 200));
    
    // Calculate time intervals for tick marks
    double pixels_per_second = MIN_PIXELS_PER_SECOND * zoom_factor_;
    double seconds_per_pixel = 1.0 / pixels_per_second;
    
    // Determine appropriate time interval for major ticks
    double time_interval = 1.0; // Start with 1 second
    while (time_interval * pixels_per_second < 50) {
        time_interval *= 2;
    }
    
    // Draw time ticks
    ve::TimePoint start_time = pixel_to_time(-scroll_x_);
    ve::TimePoint end_time = pixel_to_time(width() - scroll_x_);
    
    double start_seconds = static_cast<double>(start_time.to_rational().num) / start_time.to_rational().den;
    double end_seconds = static_cast<double>(end_time.to_rational().num) / end_time.to_rational().den;
    
    for (double t = 0; t <= end_seconds + time_interval; t += time_interval) {
        if (t < start_seconds) continue;
        
        int x = time_to_pixel(ve::TimePoint{static_cast<int64_t>(t * 1000000), 1000000});
        
        // Major tick
        painter.drawLine(x, 0, x, TIMECODE_HEIGHT);
        
        // Time label
        int minutes = static_cast<int>(t / 60);
        int seconds = static_cast<int>(t) % 60;
        QString time_str = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
        
        QRect text_rect(x + 5, 5, 100, TIMECODE_HEIGHT - 10);
        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, time_str);
    }
    
    // Draw ruler border
    painter.setPen(grid_color_);
    painter.drawLine(0, TIMECODE_HEIGHT, width(), TIMECODE_HEIGHT);
}

void TimelinePanel::draw_tracks(QPainter& painter) {
    const auto& tracks = timeline_->tracks();
    
    for (size_t i = 0; i < tracks.size(); ++i) {
        int track_y = track_y_position(i);
        draw_track(painter, *tracks[i], track_y);
    }
}

void TimelinePanel::draw_track(QPainter& painter, const ve::timeline::Track& track, int track_y) {
    QRect track_rect(0, track_y, width(), TRACK_HEIGHT);
    
    // Track background
    QColor track_bg = (track.type() == ve::timeline::Track::Video) ? 
                      track_color_video_.darker(150) : 
                      track_color_audio_.darker(150);
    painter.fillRect(track_rect, track_bg);
    
    // Track border
    painter.setPen(grid_color_);
    painter.drawRect(track_rect);
    
    // Track label
    painter.setPen(QColor(200, 200, 200));
    QRect label_rect(5, track_y + 5, 80, 20);
    QString track_name = QString::fromStdString(track.name());
    painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, track_name);
    
    // Draw segments
    draw_segments(painter, track, track_y);
}

void TimelinePanel::draw_segments(QPainter& painter, const ve::timeline::Track& track, int track_y) {
    const auto& segments = track.segments();
    
    // Get visible viewport for aggressive culling
    int viewport_left = -scroll_x_;
    int viewport_right = viewport_left + width();
    
    // Quick exit if no segments
    if (segments.empty()) return;
    
    // Pre-calculate common values
    static QColor cached_video_color(100, 150, 200);
    static QColor cached_audio_color(100, 200, 150);
    static QColor cached_text_color(255, 255, 255);
    static QFont cached_small_font;
    static QFont cached_name_font;
    static bool fonts_initialized = false;
    
    if (!fonts_initialized) {
        cached_small_font = painter.font();
        cached_small_font.setPointSize(8);
        cached_name_font = painter.font();
        cached_name_font.setPointSize(9);
        cached_name_font.setBold(true);
        fonts_initialized = true;
    }
    
    bool is_video_track = (track.type() == ve::timeline::Track::Video);
    QColor base_color = is_video_track ? cached_video_color : cached_audio_color;
    
    for (const auto& segment : segments) {
        // Skip drawing the segment being dragged to show preview instead
        if (show_drag_preview_ && segment.id == dragged_segment_id_) {
            continue;
        }
        
        int start_x = time_to_pixel(segment.start_time);
        int end_x = time_to_pixel(segment.end_time());
        int segment_width = end_x - start_x;
        
        // Aggressive culling
        if (segment_width < 1) continue;
        if (end_x < viewport_left || start_x > viewport_right) continue;
        
        QRect segment_rect(start_x, track_y + 5, segment_width, TRACK_HEIGHT - 10);
        
        // Check if segment is selected (cache this lookup)
        bool is_selected = std::find(selected_segments_.begin(), selected_segments_.end(), 
                                   segment.id) != selected_segments_.end();
        
        QColor segment_color = is_selected ? base_color.lighter(130) : base_color;
        
        // For very small segments, use simple solid fill
        if (segment_width < 20) {
            painter.fillRect(segment_rect, segment_color);
            // Skip all other details for tiny segments
            continue;
        }
        
        // Draw segment background with gradient for larger segments
        QLinearGradient gradient(segment_rect.topLeft(), segment_rect.bottomLeft());
        gradient.setColorAt(0, segment_color.lighter(110));
        gradient.setColorAt(1, segment_color.darker(110));
        painter.fillRect(segment_rect, gradient);
        
        // Enhanced border
        painter.setPen(QPen(is_selected ? cached_text_color : segment_color.darker(150), 
                           is_selected ? 2 : 1));
        painter.drawRect(segment_rect);
        
        // Only draw details for segments larger than minimum threshold
        if (segment_width < 30) continue;
        
        // Draw clip duration indicator
        auto duration = segment.duration.to_rational();
        double duration_seconds = static_cast<double>(duration.num) / duration.den;
        QString duration_text = QString("%1s").arg(duration_seconds, 0, 'f', 1);
        
        painter.setPen(cached_text_color);
        painter.setFont(cached_small_font);
        
        QRect duration_rect = segment_rect.adjusted(3, segment_rect.height() - 15, -3, -2);
        painter.drawText(duration_rect, Qt::AlignRight | Qt::AlignBottom, duration_text);
        
        // Segment name (if wide enough)
        if (segment_width > 50) {
            painter.setFont(cached_name_font);
            
            QRect text_rect = segment_rect.adjusted(5, 2, -5, -18);
            QString segment_name = QString::fromStdString(segment.name);
            
            // Truncate name if too long (cache font metrics)
            static QFontMetrics cached_fm(cached_name_font);
            if (cached_fm.horizontalAdvance(segment_name) > text_rect.width()) {
                segment_name = cached_fm.elidedText(segment_name, Qt::ElideRight, text_rect.width());
            }
            
            painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignTop, segment_name);
        }
        
        // Only draw waveforms/thumbnails for larger segments to reduce overhead
        if (segment_width > 60) {  // Increased threshold
            if (is_video_track) {
                draw_video_thumbnail(painter, segment_rect, segment);
            } else {
                draw_audio_waveform(painter, segment_rect, segment);
            }
        }
        
        // Draw segment handles for resizing (if selected)
        if (is_selected && segment_width > 20) {
            draw_segment_handles(painter, segment_rect);
        }
    }
}

void TimelinePanel::draw_playhead(QPainter& painter) {
    int x = time_to_pixel(current_time_);
    
    painter.setPen(QPen(playhead_color_, PLAYHEAD_WIDTH));
    painter.drawLine(x, 0, x, height());
    
    // Playhead handle
    QPolygon handle;
    handle << QPoint(x - 5, 0) << QPoint(x + 5, 0) << QPoint(x, 10);
    painter.setBrush(playhead_color_);
    painter.drawPolygon(handle);
}

void TimelinePanel::draw_selection(QPainter& painter) {
    if (selecting_range_) {
        int start_x = time_to_pixel(selection_start_);
        int end_x = time_to_pixel(selection_end_);
        
        if (start_x > end_x) std::swap(start_x, end_x);
        
        QRect selection_rect(start_x, TIMECODE_HEIGHT, end_x - start_x, height() - TIMECODE_HEIGHT);
        painter.fillRect(selection_rect, QColor(255, 255, 255, 50));
        
        painter.setPen(QPen(QColor(255, 255, 255), 1, Qt::DashLine));
        painter.drawRect(selection_rect);
    }
}

void TimelinePanel::draw_drag_preview(QPainter& painter) {
    if (!show_drag_preview_ || dragged_segment_id_ == 0) return;
    
    // Find the track containing the dragged segment
    const auto& tracks = timeline_->tracks();
    size_t track_index = 0;
    ve::timeline::Track* track = nullptr;
    
    for (size_t i = 0; i < tracks.size(); ++i) {
        auto* segment = tracks[i]->find_segment(dragged_segment_id_);
        if (segment) {
            track_index = i;
            track = tracks[i].get();
            break;
        }
    }
    
    if (!track) return;
    
    // Calculate preview rectangle
    int start_x = time_to_pixel(preview_start_time_);
    double pixels_per_second = MIN_PIXELS_PER_SECOND * zoom_factor_;
    int width = static_cast<int>(preview_duration_.to_rational().num * pixels_per_second / preview_duration_.to_rational().den);
    int track_y = track_y_position(track_index);
    
    QRect preview_rect(start_x, track_y + 5, width, TRACK_HEIGHT - 10);
    
    // Draw semi-transparent preview
    painter.save();
    painter.setOpacity(0.7);
    
    // Use different color based on track type
    QColor preview_color;
    if (track->type() == ve::timeline::Track::Video) {
        preview_color = QColor(100, 150, 255); // Light blue for video
    } else {
        preview_color = QColor(100, 255, 150); // Light green for audio
    }
    
    painter.fillRect(preview_rect, preview_color);
    
    // Draw preview border
    painter.setPen(QPen(preview_color.darker(150), 2, Qt::DashLine));
    painter.drawRect(preview_rect);
    
    painter.restore();
}

ve::TimePoint TimelinePanel::pixel_to_time(int x) const {
    double pixels_per_second = MIN_PIXELS_PER_SECOND * zoom_factor_;
    double seconds = (x + scroll_x_) / pixels_per_second;
    return ve::TimePoint{static_cast<int64_t>(seconds * 1000000), 1000000};
}

int TimelinePanel::time_to_pixel(ve::TimePoint time) const {
    double pixels_per_second = MIN_PIXELS_PER_SECOND * zoom_factor_;
    double seconds = static_cast<double>(time.to_rational().num) / time.to_rational().den;
    return static_cast<int>(seconds * pixels_per_second) - scroll_x_;
}

int TimelinePanel::track_at_y(int y) const {
    if (y < TIMECODE_HEIGHT) return -1;
    return (y - TIMECODE_HEIGHT) / (TRACK_HEIGHT + TRACK_SPACING);
}

int TimelinePanel::track_y_position(size_t track_index) const {
    return TIMECODE_HEIGHT + static_cast<int>(track_index) * (TRACK_HEIGHT + TRACK_SPACING);
}

void TimelinePanel::start_drag(const QPoint& pos) {
    // Implementation for drag operations
}

void TimelinePanel::update_drag(const QPoint& pos) {
    qDebug() << "update_drag called: pos=" << pos << "dragged_segment_id_=" << dragged_segment_id_;
    
    if (pos.y() < TIMECODE_HEIGHT) {
        // Scrubbing - already handled in mouseMoveEvent
        qDebug() << "Returning early - scrubbing";
        return;
    }
    
    // Handle segment dragging/resizing with live preview (without modifying actual data)
    if (!timeline_ || dragged_segment_id_ == 0) {
        qDebug() << "Returning early - no timeline or no dragged segment";
        return;
    }
    
    qDebug() << "Processing drag: dragging_segment_=" << dragging_segment_ << "resizing_segment_=" << resizing_segment_;
    
    if (dragging_segment_) {
        qDebug() << "Processing segment drag";
        // Calculate new position for preview
        ve::TimePoint new_time = pixel_to_time(pos.x());
        
        // Snap to reasonable boundaries
        if (new_time.to_rational().num < 0) {
            new_time = ve::TimePoint{0};
        }
        
        // Store preview position
        preview_start_time_ = new_time;
        preview_duration_ = original_segment_duration_;
        show_drag_preview_ = true;
        qDebug() << "Set preview: start_time=" << new_time.to_rational().num << "show_preview=" << show_drag_preview_;
        update();
        
    } else if (resizing_segment_) {
        // Calculate new size for preview
        ve::TimePoint new_time = pixel_to_time(pos.x());
        
        if (is_left_resize_) {
            // Resize from left edge
            ve::TimePoint original_end = ve::TimePoint{
                original_segment_start_.to_rational().num + original_segment_duration_.to_rational().num,
                original_segment_start_.to_rational().den
            };
            
            if (new_time < original_end && new_time.to_rational().num >= 0) {
                preview_start_time_ = new_time;
                preview_duration_ = ve::TimeDuration{
                    original_end.to_rational().num - new_time.to_rational().num,
                    new_time.to_rational().den
                };
                show_drag_preview_ = true;
                update();
            }
        } else {
            // Resize from right edge
            if (new_time > original_segment_start_) {
                preview_start_time_ = original_segment_start_;
                preview_duration_ = ve::TimeDuration{
                    new_time.to_rational().num - original_segment_start_.to_rational().num,
                    original_segment_start_.to_rational().den
                };
                show_drag_preview_ = true;
                update();
            }
        }
    }
}

void TimelinePanel::end_drag(const QPoint& pos) {
    Q_UNUSED(pos);
    // Complete any ongoing drag operations
    update();
}

void TimelinePanel::finish_segment_edit(const QPoint& pos) {
    if (!timeline_ || !command_executor_ || dragged_segment_id_ == 0) return;
    
    // Find the current segment data
    ve::timeline::Segment* segment = nullptr;
    ve::timeline::TrackId track_id = 0;
    
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        segment = track->find_segment(dragged_segment_id_);
        if (segment) {
            track_id = track->id();
            break;
        }
    }
    
    if (!segment) return;
    
    if (resizing_segment_) {
        // Create trim command if segment dimensions changed
        if (segment->start_time != original_segment_start_ || 
            segment->duration != original_segment_duration_) {
            
            auto trim_cmd = std::make_unique<ve::commands::TrimSegmentCommand>(
                dragged_segment_id_, segment->start_time, segment->duration
            );
            
            if (command_executor_(std::move(trim_cmd))) {
                ve::log::info("Completed segment resize using command system");
            } else {
                ve::log::warn("Failed to execute segment resize command");
            }
        }
    } else if (dragging_segment_) {
        // Create move command if segment position changed
        if (segment->start_time != original_segment_start_) {
            // For now, we'll create a simple move within the same track
            // In the future, we could detect cross-track moves
            auto move_cmd = std::make_unique<ve::commands::MoveSegmentCommand>(
                dragged_segment_id_, track_id, track_id, 
                original_segment_start_, segment->start_time
            );
            
            if (command_executor_(std::move(move_cmd))) {
                ve::log::info("Completed segment move using command system");
            } else {
                ve::log::warn("Failed to execute segment move command");
            }
        }
    }
}

void TimelinePanel::handle_click(const QPoint& pos) {
    if (pos.y() < TIMECODE_HEIGHT) {
        // Timeline scrubbing - already handled in mousePressEvent
        return;
    }
    
    // Find segment at click position
    ve::timeline::Segment* clicked_segment = find_segment_at_pos(pos);
    
    if (clicked_segment) {
        // Toggle segment selection
        auto it = std::find(selected_segments_.begin(), selected_segments_.end(), clicked_segment->id);
        
        if (it != selected_segments_.end()) {
            // Deselect if already selected
            selected_segments_.erase(it);
        } else {
            // Select segment (clear previous selection if not holding Ctrl)
            if (!(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
                selected_segments_.clear();
            }
            selected_segments_.push_back(clicked_segment->id);
        }
        
        emit selection_changed();
        update();
    } else {
        // Clicked empty space - seek timeline
        ve::TimePoint clicked_time = pixel_to_time(pos.x());
        current_time_ = clicked_time;
        emit time_changed(clicked_time);
        
        // Clear selection if not holding Ctrl
        if (!(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
            selected_segments_.clear();
            emit selection_changed();
        }
        
        update();
    }
}

void TimelinePanel::handle_context_menu(const QPoint& pos) {
    // Future implementation for right-click context menus
    Q_UNUSED(pos);
    ve::log::debug("Context menu requested at: " + std::to_string(pos.x()) + ", " + std::to_string(pos.y()));
}

void TimelinePanel::update_cursor(const QPoint& pos) {
    if (pos.y() < TIMECODE_HEIGHT) {
        // In timecode ruler - use I-beam cursor for scrubbing
        setCursor(Qt::IBeamCursor);
        return;
    }
    
    // Check if we're over a segment edge for resizing
    ve::timeline::Segment* segment = find_segment_at_pos(pos);
    if (segment) {
        bool is_left_edge;
        if (is_on_segment_edge(pos, *segment, is_left_edge)) {
            setCursor(Qt::SizeHorCursor);
            return;
        }
    }
    
    // Default cursor
    setCursor(Qt::ArrowCursor);
}

ve::timeline::Segment* TimelinePanel::find_segment_at_pos(const QPoint& pos) {
    if (!timeline_ || pos.y() < TIMECODE_HEIGHT) {
        return nullptr;
    }
    
    int track_index = track_at_y(pos.y());
    
    if (track_index < 0) {
        return nullptr;
    }
    
    const auto& tracks = timeline_->tracks();
    
    if (track_index >= static_cast<int>(tracks.size())) {
        return nullptr;
    }
    
    ve::TimePoint click_time = pixel_to_time(pos.x());
    
    // Search through segments in the track
    auto& segments = const_cast<std::vector<ve::timeline::Segment>&>(tracks[track_index]->segments());
    qDebug() << "Segments in track" << track_index << ":" << segments.size();
    
    for (auto& segment : segments) {
        qDebug() << "Checking segment ID:" << segment.id << "start:" << segment.start_time.to_rational().num << "end:" << segment.end_time().to_rational().num;
        if (click_time >= segment.start_time && click_time <= segment.end_time()) {
            qDebug() << "Found segment at position!";
            return &segment;
        }
    }
    
    qDebug() << "No segment found at position";
    return nullptr;
}

bool TimelinePanel::is_on_segment_edge(const QPoint& pos, const ve::timeline::Segment& segment, bool& is_left_edge) {
    int start_x = time_to_pixel(segment.start_time);
    int end_x = time_to_pixel(segment.end_time());
    
    const int edge_threshold = 5; // pixels
    
    if (std::abs(pos.x() - start_x) <= edge_threshold) {
        is_left_edge = true;
        return true;
    }
    
    if (std::abs(pos.x() - end_x) <= edge_threshold) {
        is_left_edge = false;
        return true;
    }
    
    return false;
}

// Editing operations implementation
void TimelinePanel::cut_selected_segments() {
    if (selected_segments_.empty()) return;
    
    copy_selected_segments();
    delete_selected_segments();
}

void TimelinePanel::copy_selected_segments() {
    if (selected_segments_.empty()) return;
    
    // Store selected segments for pasting (future enhancement)
    ve::log::info("Copied " + std::to_string(selected_segments_.size()) + " segments");
}

void TimelinePanel::paste_segments() {
    // Paste segments at playhead position (future enhancement)
    ve::log::info("Paste segments at playhead position");
}

void TimelinePanel::delete_selected_segments() {
    if (!timeline_ || selected_segments_.empty() || !command_executor_) return;
    
    // Create a macro command to delete all selected segments
    auto macro_command = std::make_unique<ve::commands::MacroCommand>("Delete segments");
    
    // Find the track for each selected segment and create remove commands
    for (auto segment_id : selected_segments_) {
        const auto& tracks = timeline_->tracks();
        for (size_t track_idx = 0; track_idx < tracks.size(); ++track_idx) {
            auto* track = tracks[track_idx].get();
            if (track->find_segment(segment_id)) {
                auto remove_cmd = std::make_unique<ve::commands::RemoveSegmentCommand>(
                    track->id(), segment_id
                );
                macro_command->add_command(std::move(remove_cmd));
                break;
            }
        }
    }
    
    // Execute the macro command through the command system
    if (command_executor_(std::move(macro_command))) {
        selected_segments_.clear();
        emit segments_deleted();
        emit selection_changed();
        ve::log::info("Deleted selected segments using command system");
    } else {
        ve::log::warn("Failed to delete segments through command system");
    }
}

void TimelinePanel::split_segment_at_playhead() {
    if (!timeline_ || !command_executor_) return;
    
    // Find segment under playhead
    const auto& tracks = timeline_->tracks();
    
    for (auto& track : tracks) {
        auto& segments = track->segments();
        for (auto& segment : segments) {
            if (current_time_ > segment.start_time && current_time_ < segment.end_time()) {
                // Create split command
                auto split_cmd = std::make_unique<ve::commands::SplitSegmentCommand>(
                    segment.id, current_time_
                );
                
                // Execute through command system
                if (command_executor_(std::move(split_cmd))) {
                    emit segment_split(segment.id, current_time_);
                    ve::log::info("Split segment at playhead using command system");
                } else {
                    ve::log::warn("Failed to split segment through command system");
                }
                return;
            }
        }
    }
    
    ve::log::debug("No segment found at playhead position for splitting");
}

// Navigation helpers implementation
void TimelinePanel::seek_relative(double seconds) {
    auto current_rational = current_time_.to_rational();
    double current_seconds = static_cast<double>(current_rational.num) / current_rational.den;
    
    double new_seconds = std::max(0.0, current_seconds + seconds);
    current_time_ = ve::TimePoint{static_cast<int64_t>(new_seconds * 1000000), 1000000};
    
    emit time_changed(current_time_);
    update();
}

void TimelinePanel::jump_to_previous_clip() {
    if (!timeline_) return;
    
    ve::TimePoint nearest_time{0};
    bool found = false;
    
    // Find the latest clip start time that's before current time
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        for (const auto& segment : track->segments()) {
            if (segment.start_time < current_time_ && segment.start_time > nearest_time) {
                nearest_time = segment.start_time;
                found = true;
            }
        }
    }
    
    if (found) {
        current_time_ = nearest_time;
        emit time_changed(current_time_);
        update();
    }
}

void TimelinePanel::jump_to_next_clip() {
    if (!timeline_) return;
    
    ve::TimePoint nearest_time{INT64_MAX, 1};
    bool found = false;
    
    // Find the earliest clip start time that's after current time
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        for (const auto& segment : track->segments()) {
            if (segment.start_time > current_time_ && segment.start_time < nearest_time) {
                nearest_time = segment.start_time;
                found = true;
            }
        }
    }
    
    if (found) {
        current_time_ = nearest_time;
        emit time_changed(current_time_);
        update();
    }
}

void TimelinePanel::jump_to_end() {
    if (!timeline_) return;
    
    ve::TimePoint end_time{0};
    
    // Find the latest end time of all segments
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        for (const auto& segment : track->segments()) {
            ve::TimePoint segment_end = segment.end_time();
            if (segment_end > end_time) {
                end_time = segment_end;
            }
        }
    }
    
    current_time_ = end_time;
    emit time_changed(current_time_);
    update();
}

void TimelinePanel::dragEnterEvent(QDragEnterEvent* event) {
    // Accept drops from media browser
    if (event->mimeData()->hasUrls()) {
        // Check if any URLs are media files
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                QString suffix = QFileInfo(filePath).suffix().toLower();
                
                // Accept common video/audio formats
                if (suffix == "mp4" || suffix == "avi" || suffix == "mov" || 
                    suffix == "mkv" || suffix == "wmv" || suffix == "flv" ||
                    suffix == "webm" || suffix == "m4v" || suffix == "mp3" ||
                    suffix == "wav" || suffix == "aac" || suffix == "flac") {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
    
    // Also accept internal drag operations (for moving clips)
    if (event->mimeData()->hasFormat("application/x-timeline-clip")) {
        event->acceptProposedAction();
        return;
    }
    
    event->ignore();
}

void TimelinePanel::dragMoveEvent(QDragMoveEvent* event) {
    // Show visual feedback for drop location
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat("application/x-timeline-clip")) {
        event->acceptProposedAction();
        
        // Update cursor to show drop location
        update();
    } else {
        event->ignore();
    }
}

void TimelinePanel::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        // Handle media file drops
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                QString suffix = QFileInfo(filePath).suffix().toLower();
                
                // Check if it's a supported media format
                bool isVideo = (suffix == "mp4" || suffix == "avi" || suffix == "mov" || 
                               suffix == "mkv" || suffix == "wmv" || suffix == "flv" ||
                               suffix == "webm" || suffix == "m4v");
                bool isAudio = (suffix == "mp3" || suffix == "wav" || suffix == "aac" || 
                               suffix == "flac" || suffix == "ogg");
                
                if (isVideo || isAudio) {
                    // Calculate drop position
                    ve::TimePoint drop_time = pixel_to_time(event->position().x());
                    int track_index = track_at_y(event->position().y() - TIMECODE_HEIGHT);
                    
                    // Ensure we have a valid track (create if needed)
                    if (track_index < 0) {
                        track_index = 0; // Default to first track
                    }
                    
                    ve::log::info("Dropping media on timeline: " + filePath.toStdString() + 
                                " at time " + std::to_string(drop_time.to_rational().num) + 
                                " on track " + std::to_string(track_index));
                    
                    // Emit signal to add clip to timeline (existing signal)
                    emit clip_added(filePath, drop_time, track_index);
                    
                    event->acceptProposedAction();
                    update();
                    return; // Only handle the first valid file
                }
            }
        }
    }
    
    event->ignore();
}

void TimelinePanel::draw_audio_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment) {
    Q_UNUSED(segment); // For now, we'll draw a fast placeholder waveform
    
    // Fast simplified waveform representation - reduced detail for performance
    painter.setPen(QPen(QColor(255, 255, 255, 100), 1));
    
    int center_y = rect.center().y();
    
    // Draw fewer waveform "peaks" to reduce draw calls (every 12 pixels instead of 8)
    for (int x = rect.left() + 6; x < rect.right() - 6; x += 12) {
        int peak_height = (x % 24) + 3; // Simpler pattern
        painter.drawLine(x, center_y - peak_height/2, x, center_y + peak_height/2);
    }
    
    // Draw center line only for wider segments
    if (rect.width() > 50) {
        painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
        painter.drawLine(rect.left(), center_y, rect.right(), center_y);
    }
}

void TimelinePanel::draw_video_thumbnail(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment) {
    Q_UNUSED(segment); // For now, we'll draw a fast placeholder thumbnail
    
    // Fast video thumbnail placeholder - simplified for performance
    QRect thumb_rect = rect.adjusted(2, 2, -2, -20); // Leave space for duration text
    
    // Simple border instead of detailed film strip
    painter.setPen(QPen(QColor(255, 255, 255, 120), 1));
    painter.setBrush(QBrush(QColor(50, 50, 50)));
    painter.drawRect(thumb_rect);
    
    // Only draw film strip holes for larger thumbnails
    if (thumb_rect.width() > 60) {
        // Simplified film strip holes - fewer of them
        for (int x = thumb_rect.left() + 4; x < thumb_rect.right() - 4; x += 12) {
            painter.fillRect(x, thumb_rect.top() + 1, 2, 2, QColor(0, 0, 0));
            painter.fillRect(x, thumb_rect.bottom() - 3, 2, 2, QColor(0, 0, 0));
        }
    }
    
    // Draw play icon in center only for reasonably sized thumbnails
    if (thumb_rect.width() > 30 && thumb_rect.height() > 20) {
        QPolygon play_icon;
        QPoint center = thumb_rect.center();
        int icon_size = qMin(thumb_rect.width(), thumb_rect.height()) / 4;
        
        play_icon << QPoint(center.x() - icon_size/2, center.y() - icon_size/2)
                  << QPoint(center.x() - icon_size/2, center.y() + icon_size/2)
                  << QPoint(center.x() + icon_size/2, center.y());
        
        painter.setBrush(QColor(255, 255, 255, 160));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(play_icon);
    }
}

void TimelinePanel::draw_segment_handles(QPainter& painter, const QRect& rect) {
    // Draw resize handles on left and right edges
    int handle_width = 4;
    QColor handle_color(255, 255, 255, 200);
    
    // Left handle
    QRect left_handle(rect.left(), rect.top() + 2, handle_width, rect.height() - 4);
    painter.fillRect(left_handle, handle_color);
    
    // Right handle  
    QRect right_handle(rect.right() - handle_width, rect.top() + 2, handle_width, rect.height() - 4);
    painter.fillRect(right_handle, handle_color);
    
    // Draw handle grip lines
    painter.setPen(QPen(QColor(0, 0, 0, 100), 1));
    
    // Left handle grips
    for (int i = 1; i < handle_width; i++) {
        painter.drawLine(rect.left() + i, rect.top() + 4, 
                        rect.left() + i, rect.bottom() - 4);
    }
    
    // Right handle grips
    for (int i = 1; i < handle_width; i++) {
        painter.drawLine(rect.right() - handle_width + i, rect.top() + 4, 
                        rect.right() - handle_width + i, rect.bottom() - 4);
    }
}

void TimelinePanel::cancel_drag_operations() {
    qDebug() << "Canceling all drag operations";
    
    // Reset all drag states
    dragging_ = false;
    dragging_segment_ = false;
    resizing_segment_ = false;
    dragged_segment_id_ = 0;
    show_drag_preview_ = false;
    selecting_range_ = false;
    
    // Release mouse if grabbed
    if (hasMouseTracking()) {
        releaseMouse();
    }
    
    // Reset cursor
    setCursor(Qt::ArrowCursor);
    
    // Force update to clear any visual artifacts
    update();
    
    qDebug() << "Drag operations canceled";
}

} // namespace ve::ui

#include "timeline_panel.moc"
