#include "ui/timeline_panel.hpp"
#include "commands/timeline_commands.hpp"
#include "core/log.hpp"
#include "ui/minimal_audio_track_widget.hpp"
#include "ui/minimal_waveform_widget.hpp"
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

#include <atomic>
// Global variable to track timeline operations
std::atomic<bool> g_timeline_busy{false};


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
    , update_timer_(nullptr)  // Initialize update timer to null
    , throttle_timer_(nullptr) // Initialize throttle timer
    , pending_heavy_update_(false) // No pending updates initially
    , segments_being_added_(0) // No segments being added initially
    , heavy_operation_mode_(false) // Phase 1: Start in normal mode
    , paint_throttle_timer_(nullptr) // Phase 1: Initialize throttle timer
    , pending_paint_request_(false) // Phase 1: No pending paint initially
    , cache_zoom_level_(-1) // Invalid zoom level initially
    , cached_font_metrics_(QFont()) // Phase 1: Initialize with default font
    , paint_objects_initialized_(false) // Phase 1: Objects not initialized yet
    , has_dirty_regions_(false) // Phase 2: No dirty regions initially
    , cached_hit_segment_id_(0)
    , cached_hit_segment_index_(std::numeric_limits<size_t>::max())
    , cached_hit_track_index_(std::numeric_limits<size_t>::max())
    , cached_hit_timeline_version_(0)
{
    setMinimumHeight(200);
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus); // Only get focus when clicked, not interfering with drag
    
    // Enable hover events for proper mouse highlighting
    setAttribute(Qt::WA_Hover, true);
    
    // Initialize colors
    track_color_video_ = QColor(80, 120, 180);
    track_color_audio_ = QColor(120, 180, 80);
    segment_color_ = QColor(100, 140, 200);
    segment_selected_color_ = QColor(200, 140, 100);
    playhead_color_ = QColor(255, 0, 0);
    background_color_ = QColor(45, 45, 45);
    grid_color_ = QColor(70, 70, 70);
    
    // Initialize waveform systems - simplified approach for now
    // TODO: Implement full waveform caching system when ready
    // waveform_cache_ = ve::audio::WaveformCache::create(cache_config);
    // waveform_generator_ = ve::audio::WaveformGenerator::create(gen_config);
    
    // Add a QTimer for heartbeat debugging - reduced frequency for release
    heartbeat_timer_ = new QTimer(this);
    connect(heartbeat_timer_, &QTimer::timeout, this, [this]() {
        static int heartbeat_count = 0;
        heartbeat_count++;
        if (heartbeat_count % 300 == 0) { // Every 30 seconds instead of 3 seconds
            qDebug() << "HEARTBEAT #" << heartbeat_count << "- UI thread is responsive";
        }
    });
    heartbeat_timer_->start(100); // 100ms intervals
    
    // Phase 1: Setup paint throttling timer
    paint_throttle_timer_ = new QTimer(this);
    paint_throttle_timer_->setSingleShot(true);
    connect(paint_throttle_timer_, &QTimer::timeout, this, [this]() {
        pending_paint_request_ = false;
        QWidget::update(); // Perform the actual paint
    });
}

void TimelinePanel::set_timeline(ve::timeline::Timeline* timeline) {
    timeline_ = timeline;
    cached_hit_segment_id_ = 0;
    cached_hit_segment_index_ = std::numeric_limits<size_t>::max();
    cached_hit_track_index_ = std::numeric_limits<size_t>::max();
    cached_hit_timeline_version_ = 0;

    
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
    
    // Phase 1: Detect if we're in bulk operation mode (multiple set_timeline calls)
    segments_being_added_++;
    
    // Use heavy operation mode during bulk operations
    if (segments_being_added_ > 2) {  // Lower threshold for faster detection
        set_heavy_operation_mode(true);
        
        if (!pending_heavy_update_) {
            pending_heavy_update_ = true;
            throttle_timer_->start(); // Use heavy throttling
        }
        
        // Reset counter and exit heavy mode after some time
        QTimer::singleShot(500, this, [this]() { 
            segments_being_added_ = 0; 
            set_heavy_operation_mode(false);
            pending_heavy_update_ = false;
        });
        return;
    }
    
    // Phase 2: Invalidate full timeline when adding segments
    invalidate_region(QRect(0, 0, width(), height()), true);
    
    // Normal operation for single timeline updates
    if (!update_timer_->isActive()) {
        update_timer_->start();
    }
}

void TimelinePanel::set_command_executor(CommandExecutor executor) {
    command_executor_ = executor;
}

void TimelinePanel::set_zoom(double zoom_factor) {
    zoom_factor_ = std::clamp(zoom_factor, 0.1, 10.0);
    // Phase 2: Invalidate full timeline on zoom change
    invalidate_region(QRect(0, 0, width(), height()), true);
    update();
}

void TimelinePanel::set_current_time(ve::TimePoint time) {
    // Phase 2: Invalidate old and new playhead positions
    int old_x = time_to_pixel(current_time_);
    int new_x = time_to_pixel(time);
    invalidate_region(QRect(old_x - 5, 0, 10, height()));
    invalidate_region(QRect(new_x - 5, 0, 10, height()));
    
    current_time_ = time;
    update();
}

void TimelinePanel::refresh() {
    // Throttle refresh calls to prevent excessive repaints (30fps max for general refresh)
    static auto last_refresh = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refresh);
    
    if (time_since_last.count() >= 33) {  // ~30fps
        update();
        last_refresh = now;
    }
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

// Phase 1: Override update to use throttled painting
void TimelinePanel::update() {
    request_throttled_update();
}

void TimelinePanel::paintEvent(QPaintEvent* event) {
    // Performance monitoring - track paint timing
    auto paint_start = std::chrono::high_resolution_clock::now();
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false); // Crisp lines for timeline
    
    // Phase 1: Check if we should use minimal timeline rendering
    if (should_skip_expensive_features()) {
        draw_minimal_timeline(painter);
        return;
    }
    
    // Initialize pre-allocated paint objects
    init_paint_objects();
    
    // Throttle paint events for heavy operations - but do minimal painting to avoid white screen
    static auto last_paint = std::chrono::high_resolution_clock::now();
    auto time_since_last_paint = std::chrono::duration_cast<std::chrono::milliseconds>(paint_start - last_paint);
    bool is_throttled = heavy_operation_mode_ && time_since_last_paint.count() < (1000 / HEAVY_OPERATION_FPS);
    
    if (is_throttled) {
        // During heavy operations, show simplified view
        draw_minimal_timeline(painter);
        return;
    }
    
    // Full detailed timeline rendering
    QRect paint_rect = event->rect();
    painter.setClipRect(paint_rect);
    
    // Phase 2: Smart dirty region rendering
    bool use_dirty_regions = has_dirty_regions_ && !heavy_operation_mode_;
    QRect actual_paint_rect = paint_rect;
    
    if (use_dirty_regions) {
        // Only paint areas that actually need updating
        actual_paint_rect = total_dirty_rect_.intersected(paint_rect);
        if (actual_paint_rect.isEmpty()) {
            // Nothing dirty in the paint area - skip expensive rendering
            clear_dirty_regions();
            return;
        }
        painter.setClipRect(actual_paint_rect);
        ve::log::info("Dirty region paint: " + std::to_string(actual_paint_rect.width()) + "x" + 
                     std::to_string(actual_paint_rect.height()) + " vs full " + 
                     std::to_string(paint_rect.width()) + "x" + std::to_string(paint_rect.height()));
    }
    
    // Draw all timeline components in order
    auto after_background = std::chrono::high_resolution_clock::now();
    draw_background(painter);
    
    auto after_timecode = std::chrono::high_resolution_clock::now();
    // Only draw timecode if it intersects the paint area
    if (actual_paint_rect.top() <= TIMECODE_HEIGHT) {
        draw_timecode_ruler(painter);
    }
    
    auto after_tracks = std::chrono::high_resolution_clock::now();
    // Only draw tracks if they intersect the paint area
    if (actual_paint_rect.bottom() > TIMECODE_HEIGHT) {
        draw_tracks(painter);
    }
    
    // Only draw overlays if they intersect the paint area
    draw_playhead(painter);
    draw_selection(painter);
    
    // Clear dirty regions after successful paint
    if (use_dirty_regions) {
        clear_dirty_regions();
    }
    
    // Performance monitoring - detailed timing breakdown
    auto paint_end = std::chrono::high_resolution_clock::now();
    auto paint_duration = std::chrono::duration<double, std::milli>(paint_end - paint_start);
    auto background_time = std::chrono::duration<double, std::milli>(after_background - paint_start);
    auto timecode_time = std::chrono::duration<double, std::milli>(after_timecode - after_background);
    auto tracks_time = std::chrono::duration<double, std::milli>(after_tracks - after_timecode);
    
    last_paint = paint_start;
    
    static auto last_warning = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (paint_duration.count() > 16.0 && 
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_warning).count() > 1000) {
        ve::log::warn("Timeline paint slow: " + std::to_string(paint_duration.count()) + 
                     "ms (background: " + std::to_string(background_time.count()) + 
                     "ms, timecode: " + std::to_string(timecode_time.count()) + 
                     "ms, tracks: " + std::to_string(tracks_time.count()) + 
                     "ms, rect: " + std::to_string(event->rect().width()) + "x" + std::to_string(event->rect().height()) + ")");
        last_warning = now;
    }
}

void TimelinePanel::mousePressEvent(QMouseEvent* event) {
    // Removed debug logging for performance - mouse press events can be frequent
    
    if (!timeline_) return;
    
    drag_start_ = event->pos();
    last_mouse_pos_ = event->pos();
    
    if (event->button() == Qt::LeftButton) {
        if (event->pos().y() < TIMECODE_HEIGHT) {
            // Clicked in timecode ruler - start scrubbing
            ve::TimePoint time = pixel_to_time(event->pos().x());
            current_time_ = time;
            emit time_changed(time);
            dragging_ = true;
            update();
        } else {
            // Clicked in track area - check for segment interaction
            ve::timeline::Segment* segment = find_segment_at_pos(event->pos());
            
            if (segment) {
                // Check if clicking on segment edge for resizing
                bool is_left_edge;
                if (is_on_segment_edge(event->pos(), *segment, is_left_edge)) {
                    // Start resizing operation - removed debug logging for performance
                    resizing_segment_ = true;
                    is_left_resize_ = is_left_edge;
                    dragged_segment_id_ = segment->id;
                    original_segment_start_ = segment->start_time;
                    original_segment_duration_ = segment->duration;
                    setCursor(Qt::SizeHorCursor);
                    // Don't grab mouse immediately - wait for actual mouse movement
                } else {
                    // Start moving operation - removed debug logging for performance
                    dragging_segment_ = true;
                    dragged_segment_id_ = segment->id;
                    original_segment_start_ = segment->start_time;
                    original_segment_duration_ = segment->duration;
                    setCursor(Qt::ClosedHandCursor);
                    // Don't grab mouse immediately - wait for actual mouse movement
                    
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
                // Clicked empty space - handle selection - removed debug logging for performance
                handle_click(event->pos());
            }
            
            dragging_ = true;
            // qDebug() << "Final dragging_ state:" << dragging_; // Removed for performance
        }
    } else if (event->button() == Qt::RightButton) {
        // Right click for context menu
        handle_context_menu(event->pos());
    }
}

void TimelinePanel::mouseMoveEvent(QMouseEvent* event) {
    // Enhanced time-based throttling to prevent excessive updates
    static auto last_update = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
    
    if (!timeline_) return;
    
    // Check if we need to start dragging (if segment operations are prepared but not yet dragging)
    if ((dragging_segment_ || resizing_segment_) && !dragging_) {
        // Start actual dragging if mouse moved enough - higher threshold for stability
        QPoint diff = event->pos() - last_mouse_pos_;
        if (abs(diff.x()) > 5 || abs(diff.y()) > 5) { // Increased threshold for less sensitivity
            dragging_ = true;
            grabMouse(); // Now grab mouse for actual drag operation
        }
    }
    
    if (dragging_) {
        if (event->pos().y() < TIMECODE_HEIGHT) {
            // Scrubbing in timecode ruler - more aggressive throttling
            if (time_since_last.count() >= 33) {  // ~30fps for scrubbing
                ve::TimePoint time = pixel_to_time(event->pos().x());
                current_time_ = time;
                emit time_changed(time);
                update();
                last_update = now;
            }
        } else {
            // Dragging in track area - throttle heavily during drag
            if (time_since_last.count() >= 33) {  // ~30fps for dragging
                update_drag(event->pos());
                last_update = now;
            }
        }
    } else {
        // Throttle cursor updates heavily - only update every 15 pixels movement and reduce frequency
        QPoint diff = event->pos() - last_mouse_pos_;
        if ((abs(diff.x()) > 15 || abs(diff.y()) > 15) && time_since_last.count() >= 50) {  // ~20fps for cursor updates
            update_cursor(event->pos());
            last_update = now;
        }
    }
    
    last_mouse_pos_ = event->pos();
}

void TimelinePanel::mouseReleaseEvent(QMouseEvent* event) {
    // Removed debug logging for performance - mouse release events during active editing
    
    if (dragging_) {
        // Complete any ongoing operations with command system
        if (command_executor_ && (dragging_segment_ || resizing_segment_)) {
            finish_segment_edit(event->pos());
        }
        
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
            
        case Qt::Key_Z:
            if (event->modifiers() & Qt::ControlModifier) {
                if (event->modifiers() & Qt::ShiftModifier) {
                    // Ctrl+Shift+Z for redo
                    request_redo();
                } else {
                    // Ctrl+Z for undo
                    request_undo();
                }
            }
            break;
            
        case Qt::Key_Y:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+Y for redo (alternative)
                request_redo();
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
    // Adaptive tick sizing: grow interval so that major ticks are ~>=50px apart
    double time_interval = 1.0; // seconds
    while (time_interval * pixels_per_second < 50.0) time_interval *= 2.0;
    
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
    
    // Enhanced viewport culling: Calculate viewport info once
    ViewportInfo viewport = calculate_viewport_info();
    
    for (size_t i = 0; i < tracks.size(); ++i) {
        int track_y = track_y_position(i);
        
        // Enhanced viewport culling: Skip tracks completely outside viewport
        if (!is_track_visible(track_y, viewport)) {
            continue;
        }
        
        draw_track(painter, *tracks[i], track_y);
    }
}

void TimelinePanel::draw_track(QPainter& painter, const ve::timeline::Track& track, int track_y) {
    QRect track_rect(0, track_y, width(), TRACK_HEIGHT);
    
    // Track background - much lighter to show segments clearly
    QColor track_bg = (track.type() == ve::timeline::Track::Video) ? 
                      QColor(40, 60, 90) :    // Very dark blue background for video tracks
                      QColor(40, 90, 60);     // Very dark green background for audio tracks
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
    // Phase 3: Use batched rendering for better performance
    draw_segments_batched(painter, track, track_y);
}

void TimelinePanel::draw_segments_batched(QPainter& painter, const ve::timeline::Track& track, int track_y) {
    // Enhanced viewport culling: Get optimized segment list
    ViewportInfo viewport = calculate_viewport_info();
    std::vector<const ve::timeline::Segment*> visible_segments = cull_segments_optimized(track.segments(), viewport);
    
    // Early exit if no visible segments
    if (visible_segments.empty()) return;
    
    // Performance circuit breaker - limit number of segments processed per paint event
    static int max_segments_per_paint = 40;
    if (visible_segments.size() > static_cast<size_t>(max_segments_per_paint)) {
        visible_segments.resize(max_segments_per_paint);
        static auto last_warning = std::chrono::steady_clock::time_point{};
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_warning).count() > 1000) {
            ve::log::warn("Timeline: Segment limit reached, using batched rendering");
            last_warning = now;
        }
    }
    
    // Phase 3: Create batches for similar segments
    std::vector<SegmentBatch> batches;
    create_segment_batches(visible_segments, track, track_y, batches);
    
    // Phase 3: Draw each batch with minimal state changes
    for (const auto& batch : batches) {
        draw_segment_batch(painter, batch);
    }
}

// Phase 3: Create segment batches for optimized rendering
void TimelinePanel::create_segment_batches(const std::vector<const ve::timeline::Segment*>& visible_segments, 
                                           const ve::timeline::Track& track, int track_y,
                                           std::vector<SegmentBatch>& batches) {
    batches.clear();
    batches.reserve(8); // Reserve space for common batch count
    
    bool is_video_track = (track.type() == ve::timeline::Track::Video);
    static QColor cached_video_color(150, 200, 255);
    static QColor cached_audio_color(150, 255, 200);
    QColor base_color = is_video_track ? cached_video_color : cached_audio_color;
    
    // Group segments by rendering characteristics
    for (const auto* segment_ptr : visible_segments) {
        const auto& segment = *segment_ptr;
        
        // Skip segments being dragged (they show preview instead)
        if (show_drag_preview_ && segment.id == dragged_segment_id_) {
            continue;
        }
        
        int start_x = time_to_pixel(segment.start_time);
        int end_x = time_to_pixel(segment.end_time());
        int segment_width = end_x - start_x;
        
        // Skip tiny segments
        if (segment_width < 2) continue;
        
        QRect segment_rect(start_x, track_y + 5, segment_width, TRACK_HEIGHT - 10);
        
        // Calculate rendering characteristics for batching
        bool is_selected = std::find(selected_segments_.begin(), selected_segments_.end(), 
                                   segment.id) != selected_segments_.end();
        QColor segment_color = is_selected ? cached_selected_color_ : base_color;
        DetailLevel detail_level = calculate_detail_level(segment_width, zoom_factor_);
        
        // Find existing batch or create new one
        bool found_batch = false;
        for (auto& batch : batches) {
            if (batch.color == segment_color && 
                batch.detail_level == detail_level && 
                batch.is_selected == is_selected) {
                batch.segments.push_back(segment_ptr);
                batch.rects.push_back(segment_rect);
                found_batch = true;
                break;
            }
        }
        
        if (!found_batch) {
            SegmentBatch new_batch;
            new_batch.color = segment_color;
            new_batch.detail_level = detail_level;
            new_batch.is_selected = is_selected;
            new_batch.segments.push_back(segment_ptr);
            new_batch.rects.push_back(segment_rect);
            batches.push_back(std::move(new_batch));
        }
    }
}

// Phase 3: Draw a batch of segments with shared characteristics
void TimelinePanel::draw_segment_batch(QPainter& painter, const SegmentBatch& batch) {
    if (batch.segments.empty()) return;
    
    // Set up painter state once for the entire batch
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
    
    // Phase 3: Batch draw all rectangles with same color
    for (size_t i = 0; i < batch.segments.size(); ++i) {
        const auto& segment = *batch.segments[i];
        const QRect& rect = batch.rects[i];
        
        // Handle different detail levels efficiently
        switch (batch.detail_level) {
            case DetailLevel::MINIMAL:
                // Just fill with lighter color for minimal segments
                painter.fillRect(rect, batch.color.lighter(120));
                break;
                
            case DetailLevel::BASIC:
                // Fill + border for basic segments
                painter.fillRect(rect, batch.color.lighter(110));
                painter.setPen(cached_border_pen_);
                painter.drawRect(rect);
                break;
                
            case DetailLevel::NORMAL:
            case DetailLevel::DETAILED:
                // Full rendering for normal/detailed segments
                painter.fillRect(rect, batch.color);
                painter.setPen(cached_border_pen_);
                painter.drawRect(rect);
                
                // Add text/details if not in heavy operation mode
                if (!should_skip_expensive_features()) {
                    // Add segment name for NORMAL and above
                    if (batch.detail_level >= DetailLevel::NORMAL) {
                        painter.setFont(cached_name_font);
                        painter.setPen(cached_text_color);
                        
                        QRect text_rect = rect.adjusted(5, 2, -5, -18);
                        QString segment_name = QString::fromStdString(segment.name);
                        
                        if (!heavy_operation_mode_ && cached_font_metrics_.horizontalAdvance(segment_name) > text_rect.width()) {
                            segment_name = cached_font_metrics_.elidedText(segment_name, Qt::ElideRight, text_rect.width());
                        }
                        
                        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignTop, segment_name);
                    }
                    
                    // Add duration for DETAILED level
                    if (batch.detail_level == DetailLevel::DETAILED) {
                        auto duration = segment.duration.to_rational();
                        double duration_seconds = static_cast<double>(duration.num) / duration.den;
                        QString duration_text = QString("%1s").arg(duration_seconds, 0, 'f', 1);
                        
                        painter.setFont(cached_small_font);
                        painter.setPen(cached_text_color);
                        
                        QRect duration_rect = rect.adjusted(3, rect.height() - 15, -3, -2);
                        painter.drawText(duration_rect, Qt::AlignRight | Qt::AlignBottom, duration_text);
                        
                        // Draw waveforms/thumbnails for largest segments
                        if (rect.width() > 120) {
                            bool is_video_track = (segment.name.find("video") != std::string::npos); // Simple heuristic
                            if (is_video_track) {
                                draw_video_thumbnail(painter, rect, segment);
                            } else {
                                draw_cached_waveform(painter, rect, segment);
                            }
                        }
                    }
                }
                
                // Draw segment handles for selected segments
                if (batch.is_selected && batch.detail_level >= DetailLevel::BASIC) {
                    draw_segment_handles(painter, rect);
                }
                break;
        }
    }
}

// Enhanced viewport culling: Calculate viewport information once per paint
TimelinePanel::ViewportInfo TimelinePanel::calculate_viewport_info() const {
    ViewportInfo viewport;
    
    // Pixel boundaries with margins for smooth scrolling
    const int margin = 50;
    viewport.left_x = -scroll_x_ - margin;
    viewport.right_x = -scroll_x_ + width() + margin;
    viewport.top_y = 0;
    viewport.bottom_y = height();
    
    // Time boundaries
    viewport.start_time = pixel_to_time(viewport.left_x);
    viewport.end_time = pixel_to_time(viewport.right_x);
    
    // Cached conversion ratio for performance
    viewport.time_to_pixel_ratio = zoom_factor_ * MIN_PIXELS_PER_SECOND;
    
    return viewport;
}

// Enhanced viewport culling: Check if track is visible
bool TimelinePanel::is_track_visible(int track_y, const ViewportInfo& viewport) const {
    return (track_y + TRACK_HEIGHT > viewport.top_y) && (track_y < viewport.bottom_y);
}

// Enhanced viewport culling: Optimized segment filtering
std::vector<const ve::timeline::Segment*> TimelinePanel::cull_segments_optimized(
    const std::vector<ve::timeline::Segment>& segments, const ViewportInfo& viewport) const {
    
    std::vector<const ve::timeline::Segment*> visible_segments;
    
    if (segments.empty()) {
        return visible_segments;
    }
    
    visible_segments.reserve(std::min(segments.size(), size_t(50))); // Reserve reasonable size
    
    // Use binary search to find first potentially visible segment
    size_t start_idx = 0;
    size_t end_idx = segments.size();
    
    // Find first segment that might be visible (binary search on start_time)
    auto first_visible = std::lower_bound(segments.begin(), segments.end(), viewport.start_time,
        [](const ve::timeline::Segment& segment, ve::TimePoint time) {
            return segment.end_time() <= time;
        });
    
    if (first_visible != segments.end()) {
        start_idx = static_cast<size_t>(std::distance(segments.begin(), first_visible));
    }
    
    // Process only segments that could be visible
    for (size_t i = start_idx; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        
        // Early exit: segments are sorted by start_time, so if this one starts after viewport, we're done
        if (segment.start_time >= viewport.end_time) {
            break;
        }
        
        // Check if segment overlaps with viewport time range
        ve::TimePoint segment_end = segment.end_time();
        if (segment_end > viewport.start_time && segment.start_time < viewport.end_time) {
            visible_segments.push_back(&segment);
        }
    }
    
    return visible_segments;
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

void TimelinePanel::start_drag(const QPoint&) {
    // Future: initiate drag preparation
}

void TimelinePanel::update_drag(const QPoint& pos) {
    // Removed debug logging for performance - called frequently during mouse drag
    
    if (pos.y() < TIMECODE_HEIGHT) {
        // Scrubbing - already handled in mouseMoveEvent
        return;
    }
    
    // Handle segment dragging/resizing with live preview (without modifying actual data)
    if (!timeline_ || dragged_segment_id_ == 0) {
        return;
    }
    
    if (dragging_segment_) {
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
        // Removed drag preview debug logging for performance - called frequently during drag
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

void TimelinePanel::end_drag(const QPoint&) {
    update();
}

void TimelinePanel::finish_segment_edit(const QPoint&) {
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
#if defined(QT_NO_MENU)
    Q_UNUSED(pos);
    // Menus disabled in this Qt build
    return;
#else
    // Force a repaint before showing context menu to avoid white screen
    update();
    
    // Create context menu with proper parent to avoid memory issues
    QMenu menu(this); // Use 'this' as parent for proper memory management
    
    // Find segment at position and store ID instead of pointer to avoid dangling references
    ve::timeline::SegmentId segment_id = 0;
    ve::timeline::Segment* seg = find_segment_at_pos(pos);
    if (seg) {
        segment_id = seg->id;
    }
    
    if (segment_id != 0) {
        // Segment-specific menu - use segment ID instead of pointer
        QAction* splitAct = menu.addAction("Split Segment");
        QAction* deleteAct = menu.addAction("Delete Segment");
        menu.addSeparator();
        QAction* copyAct = menu.addAction("Copy Segment");
        QAction* cutAct = menu.addAction("Cut Segment");
        
        // Enable paste if clipboard has content
        QAction* pasteAct = nullptr;
        if (!clipboard_segments_.empty()) {
            pasteAct = menu.addAction("Paste Here");
        }
        
        // Use segment ID in lambda to avoid dangling pointer issues
        connect(splitAct, &QAction::triggered, this, [this, segment_id, pos]() {
            ve::log::info("Split segment requested id=" + std::to_string(segment_id));
            
            if (!timeline_ || !command_executor_) return;
            
            // Convert mouse position to timeline time for split point
            ve::TimePoint split_time = pixel_to_time(pos.x());
            
            // Create and execute split command
            auto split_cmd = std::make_unique<ve::commands::SplitSegmentCommand>(
                segment_id, split_time
            );
            
            if (command_executor_(std::move(split_cmd))) {
                emit segment_split(segment_id, split_time);
                ve::log::info("Split segment at mouse position using command system");
            } else {
                ve::log::warn("Failed to split segment through command system");
            }
        });
        
        connect(deleteAct, &QAction::triggered, this, [this, segment_id]() {
            ve::log::info("Delete segment requested id=" + std::to_string(segment_id));
            
            if (!timeline_ || !command_executor_) return;
            
            // Find the track containing this segment
            const auto& tracks = timeline_->tracks();
            for (const auto& track : tracks) {
                if (track->find_segment(segment_id)) {
                    // Create and execute remove command
                    auto remove_cmd = std::make_unique<ve::commands::RemoveSegmentCommand>(
                        track->id(), segment_id
                    );
                    
                    if (command_executor_(std::move(remove_cmd))) {
                        emit segments_deleted();
                        ve::log::info("Deleted segment using command system");
                    } else {
                        ve::log::warn("Failed to delete segment through command system");
                    }
                    break;
                }
            }
        });
        
        connect(copyAct, &QAction::triggered, this, [this, segment_id]() {
            ve::log::info("Copy segment requested id=" + std::to_string(segment_id));
            
            // Select this segment if not already selected
            if (std::find(selected_segments_.begin(), selected_segments_.end(), segment_id) == selected_segments_.end()) {
                selected_segments_.clear();
                selected_segments_.push_back(segment_id);
                emit selection_changed();
            }
            
            copy_selected_segments();
        });
        
        connect(cutAct, &QAction::triggered, this, [this, segment_id]() {
            ve::log::info("Cut segment requested id=" + std::to_string(segment_id));
            
            // Select this segment if not already selected
            if (std::find(selected_segments_.begin(), selected_segments_.end(), segment_id) == selected_segments_.end()) {
                selected_segments_.clear();
                selected_segments_.push_back(segment_id);
                emit selection_changed();
            }
            
            cut_selected_segments();
        });
        
        if (pasteAct) {
            connect(pasteAct, &QAction::triggered, this, [this, pos]() {
                ve::log::info("Paste at position requested");
                
                // Set playhead to paste position for accurate pasting
                ve::TimePoint paste_time = pixel_to_time(pos.x());
                set_current_time(paste_time);
                
                paste_segments();
            });
        }
    } else {
        // Track-specific menu
        QAction* addClipAct = menu.addAction("Add Clip Here");
        
        // Add paste option if clipboard has content
        QAction* pasteTrackAct = nullptr;
        if (!clipboard_segments_.empty()) {
            pasteTrackAct = menu.addAction("Paste Here");
        }
        
        // Add separator and undo/redo options
        menu.addSeparator();
        QAction* undoAct = menu.addAction("Undo");
        QAction* redoAct = menu.addAction("Redo");
        
        connect(addClipAct, &QAction::triggered, this, [this, pos]() {
            ve::log::info("Add clip requested at position");
            
            // Convert position to time and track
            ve::TimePoint drop_time = pixel_to_time(pos.x());
            int track_index = track_at_y(pos.y());
            
            if (track_index < 0) {
                ve::log::warn("Invalid track position for adding clip");
                return;
            }
            
            // Open file dialog to select media file
            QString filePath = QFileDialog::getOpenFileName(
                this, 
                "Add Media Clip",
                QString(),
                "Media Files (*.mp4 *.avi *.mov *.mkv *.mp3 *.wav *.aac *.m4a);;All Files (*)"
            );
            
            if (!filePath.isEmpty()) {
                // Emit signal to add clip (follows existing pattern from drag/drop)
                emit clip_added(filePath, drop_time, track_index);
                ve::log::info("Add clip requested: " + filePath.toStdString() + 
                             " at time " + std::to_string(drop_time.to_rational().num) + 
                             " on track " + std::to_string(track_index));
            }
        });
        
        if (pasteTrackAct) {
            connect(pasteTrackAct, &QAction::triggered, this, [this, pos]() {
                ve::log::info("Paste on track requested");
                
                // Set playhead to paste position for accurate pasting
                ve::TimePoint paste_time = pixel_to_time(pos.x());
                set_current_time(paste_time);
                
                paste_segments();
            });
        }
        
        connect(undoAct, &QAction::triggered, this, [this]() {
            request_undo();
        });
        
        connect(redoAct, &QAction::triggered, this, [this]() {
            request_redo();
        });
    }
    
    // Show menu at mouse position using standard Qt approach
    QPoint globalPos = mapToGlobal(pos);
    menu.exec(globalPos);
    
    // Force repaint after context menu closes to ensure proper display
    update();
#endif
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

    auto* track = tracks[track_index].get();
    auto& segments = const_cast<std::vector<ve::timeline::Segment>&>(track->segments());
    if (segments.empty()) {
        cached_hit_segment_id_ = 0;
        cached_hit_segment_index_ = std::numeric_limits<size_t>::max();
        cached_hit_track_index_ = std::numeric_limits<size_t>::max();
        cached_hit_timeline_version_ = 0;
        return nullptr;
    }

    ve::TimePoint click_time = pixel_to_time(pos.x());
    const uint64_t current_version = timeline_->version();

    if (cached_hit_segment_id_ != 0 &&
        cached_hit_timeline_version_ == current_version &&
        cached_hit_track_index_ == static_cast<size_t>(track_index) &&
        cached_hit_segment_index_ < segments.size()) {
        auto& cached_segment = segments[cached_hit_segment_index_];
        if (cached_segment.id == cached_hit_segment_id_ &&
            click_time >= cached_segment.start_time &&
            click_time <= cached_segment.end_time()) {
            return &cached_segment;
        }
    }

    int left = 0;
    int right = static_cast<int>(segments.size()) - 1;

    if (cached_hit_segment_id_ != 0 &&
        cached_hit_timeline_version_ == current_version &&
        cached_hit_track_index_ == static_cast<size_t>(track_index) &&
        cached_hit_segment_index_ < segments.size() &&
        segments[cached_hit_segment_index_].id == cached_hit_segment_id_) {
        auto& cached_segment = segments[cached_hit_segment_index_];
        if (click_time < cached_segment.start_time) {
            right = static_cast<int>(cached_hit_segment_index_) - 1;
        } else if (click_time > cached_segment.end_time()) {
            left = static_cast<int>(cached_hit_segment_index_) + 1;
        } else {
            cached_hit_segment_id_ = cached_segment.id;
            cached_hit_track_index_ = static_cast<size_t>(track_index);
            cached_hit_timeline_version_ = current_version;
            return &cached_segment;
        }
    }

    while (left <= right) {
        int mid = left + (right - left) / 2;
        auto& segment = segments[mid];

        if (click_time < segment.start_time) {
            right = mid - 1;
            continue;
        }

        if (click_time > segment.end_time()) {
            left = mid + 1;
            continue;
        }

        cached_hit_segment_id_ = segment.id;
        cached_hit_segment_index_ = static_cast<size_t>(mid);
        cached_hit_track_index_ = static_cast<size_t>(track_index);
        cached_hit_timeline_version_ = current_version;
        return &segment;
    }

    cached_hit_segment_id_ = 0;
    cached_hit_segment_index_ = std::numeric_limits<size_t>::max();
    cached_hit_track_index_ = std::numeric_limits<size_t>::max();
    cached_hit_timeline_version_ = 0;
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
    if (selected_segments_.empty()) {
        ve::log::info("No segments selected for copying");
        return;
    }
    
    if (!timeline_) {
        ve::log::warn("No timeline available for copying segments");
        return;
    }
    
    // Clear existing clipboard data
    clipboard_segments_.clear();
    
    // Find and copy each selected segment's data
    for (auto segment_id : selected_segments_) {
        const auto& tracks = timeline_->tracks();
        for (size_t track_idx = 0; track_idx < tracks.size(); ++track_idx) {
            auto* track = tracks[track_idx].get();
            const auto& segments = track->segments();
            
            auto segment_it = std::find_if(segments.begin(), segments.end(),
                [segment_id](const ve::timeline::Segment& seg) { 
                    return seg.id == segment_id; 
                });
            
            if (segment_it != segments.end()) {
                // Store segment data for pasting with relative positioning
                ClipboardSegment clipboard_seg;
                clipboard_seg.segment = *segment_it;
                clipboard_seg.original_track_index = track_idx;
                clipboard_seg.relative_start_time = segment_it->start_time;
                
                clipboard_segments_.push_back(clipboard_seg);
                ve::log::debug("Copied segment '" + segment_it->name + "' from track " + std::to_string(track_idx));
                break;
            }
        }
    }
    
    if (!clipboard_segments_.empty()) {
        // Calculate relative positioning based on first segment
        ve::TimePoint earliest_time = clipboard_segments_[0].relative_start_time;
        for (const auto& clip_seg : clipboard_segments_) {
            if (clip_seg.relative_start_time < earliest_time) {
                earliest_time = clip_seg.relative_start_time;
            }
        }
        
        // Make all times relative to the earliest segment
        for (auto& clip_seg : clipboard_segments_) {
            clip_seg.relative_start_time = ve::TimePoint{
                clip_seg.relative_start_time.to_rational().num - earliest_time.to_rational().num,
                clip_seg.relative_start_time.to_rational().den
            };
        }
        
        ve::log::info("Copied " + std::to_string(clipboard_segments_.size()) + " segments to clipboard");
    } else {
        ve::log::warn("Failed to copy any segments - segments not found in timeline");
    }
}

void TimelinePanel::paste_segments() {
    if (clipboard_segments_.empty()) {
        ve::log::info("No segments in clipboard to paste");
        return;
    }
    
    if (!timeline_ || !command_executor_) {
        ve::log::warn("Timeline or command executor not available for pasting");
        return;
    }
    
    // Get current playhead position as paste location
    ve::TimePoint paste_time = current_time_;
    
    // Create macro command for pasting all segments
    auto macro_command = std::make_unique<ve::commands::MacroCommand>("Paste segments");
    
    // Get current tracks
    const auto& tracks = timeline_->tracks();
    
    for (const auto& clipboard_seg : clipboard_segments_) {
        // Calculate absolute paste position
        ve::TimePoint absolute_time = ve::TimePoint{
            paste_time.to_rational().num + clipboard_seg.relative_start_time.to_rational().num,
            paste_time.to_rational().den
        };
        
        // Determine target track - use original track if available, otherwise first track
        size_t target_track_index = clipboard_seg.original_track_index;
        if (target_track_index >= tracks.size()) {
            target_track_index = 0; // Fallback to first track
        }
        
        if (target_track_index < tracks.size()) {
            // Create a new segment with unique ID for pasting
            ve::timeline::Segment new_segment = clipboard_seg.segment;
            new_segment.id = tracks[target_track_index]->generate_segment_id(); // Generate new unique ID from target track
            new_segment.start_time = absolute_time;
            
            // Create insert command
            auto insert_cmd = std::make_unique<ve::commands::InsertSegmentCommand>(
                tracks[target_track_index]->id(),
                new_segment,
                absolute_time
            );
            
            macro_command->add_command(std::move(insert_cmd));
            
            ve::log::debug("Prepared paste for segment '" + new_segment.name + 
                          "' at time " + std::to_string(absolute_time.to_rational().num) +
                          " on track " + std::to_string(target_track_index));
        } else {
            ve::log::warn("No valid tracks available for pasting segment");
        }
    }
    
    // Execute the paste operation
    if (macro_command->empty()) {
        ve::log::warn("No segments prepared for pasting");
        return;
    }
    
    if (command_executor_(std::move(macro_command))) {
        ve::log::info("Successfully pasted " + std::to_string(clipboard_segments_.size()) + 
                      " segments at time " + std::to_string(paste_time.to_rational().num));
        
        // Refresh timeline display
        refresh();
        emit segments_added(); // New signal for paste operations
    } else {
        ve::log::warn("Failed to paste segments through command system");
    }
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

void TimelinePanel::enterEvent(QEnterEvent* event) {
    // Ensure proper hover behavior when mouse enters timeline
    QWidget::enterEvent(event);
    
    // Make sure widget accepts hover events and updates properly
    setAttribute(Qt::WA_Hover, true);
    update(); // Trigger repaint to ensure proper highlighting
}

void TimelinePanel::leaveEvent(QEvent* event) {
    // Clean up hover state when mouse leaves timeline
    QWidget::leaveEvent(event);
    
    // Reset cursor to default when leaving
    setCursor(Qt::ArrowCursor);
    update(); // Trigger repaint to clean up any hover highlighting
}

void TimelinePanel::draw_audio_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment) {
    // Highly optimized placeholder waveform - minimal draw calls
    painter.setPen(QPen(QColor(255, 255, 255, 80), 1)); // Lower opacity for subtlety
    
    int center_y = rect.center().y();
    int seed = static_cast<int>(segment.id % 5 + 2); // Reduced variation for consistency
    
    // Draw much fewer waveform "peaks" to reduce draw calls (every 16 pixels instead of 12)
    for (int x = rect.left() + 8; x < rect.right() - 8; x += 16) {
        int peak_height = ((x + seed) % 16) + 2; // Reduced variation
        painter.drawLine(x, center_y - peak_height/2, x, center_y + peak_height/2);
    }
}

void TimelinePanel::draw_cached_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment) {
    // Ultra-fast waveform placeholder - optimized for performance over visual detail
    if (!timeline_) {
        draw_audio_waveform(painter, rect, segment);
        return;
    }
    
    // Get the clip for this segment to use file path for deterministic waveform
    const auto* clip = timeline_->get_clip(segment.clip_id);
    if (!clip || !clip->source) {
        draw_audio_waveform(painter, rect, segment);
        return;
    }
    
    // Use simple hash for deterministic pattern
    std::hash<std::string> hasher;
    size_t path_hash = hasher(clip->source->path);
    
    painter.setPen(QPen(QColor(120, 200, 255, 120), 1));
    
    int center_y = rect.center().y();
    int max_height = rect.height() / 2 - 4;
    
    // Ultra-fast block-based waveform - no expensive math
    int block_width = std::max(2, rect.width() / 50); // Much fewer blocks
    int num_blocks = rect.width() / block_width;
    
    for (int i = 0; i < num_blocks; ++i) {
        int x = rect.left() + i * block_width;
        
        // Simple deterministic pattern using bit operations (very fast)
        unsigned int pattern = (path_hash ^ (i * 17)) + (segment.id * 23);
        int height = (pattern % max_height) + 1;
        
        // Vary height with simple pattern
        if ((i % 7) == 0) height = height / 3;  // Quiet moments
        if ((i % 11) == 0) height = height * 2; // Loud moments
        height = std::min(height, max_height);
        
        // Draw simple vertical line (much faster than individual lines)
        painter.drawLine(x, center_y - height, x, center_y + height);
    }
    
    // Only draw center line for wider segments
    if (rect.width() > 100) {
        painter.setPen(QPen(QColor(120, 200, 255, 30), 1));
        painter.drawLine(rect.left(), center_y, rect.right(), center_y);
    }
}

void TimelinePanel::draw_video_thumbnail(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment) {
    // Use segment.id to introduce mild variation in placeholder visuals
    int variation = static_cast<int>((segment.id * 37) % 50);
    
    // Fast video thumbnail placeholder - simplified for performance
    QRect thumb_rect = rect.adjusted(2, 2, -2, -20); // Leave space for duration text
    
    // Simple border instead of detailed film strip
    painter.setPen(QPen(QColor(255, 255, 255, 120), 1));
    painter.setBrush(QBrush(QColor(50 + variation/5, 50, 50)));
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

void TimelinePanel::request_undo() {
    ve::log::info("Undo requested from timeline");
    emit undo_requested();
}

void TimelinePanel::request_redo() {
    ve::log::info("Redo requested from timeline");
    emit redo_requested();
}

// Segment rendering cache implementation
QPixmap* TimelinePanel::get_cached_segment(uint32_t segment_id, const QRect& rect) const {
    // Check if zoom level changed - clear cache if so
    int current_zoom = static_cast<int>(zoom_factor_ * 100);
    if (cache_zoom_level_ != current_zoom) {
        clear_segment_cache();
        cache_zoom_level_ = current_zoom;
        return nullptr;
    }
    
    // Find cached entry
    for (auto& entry : segment_cache_) {
        if (entry.segment_id == segment_id && entry.rect == rect && entry.zoom_level == current_zoom) {
            return &entry.cached_pixmap;
        }
    }
    return nullptr;
}

void TimelinePanel::cache_segment(uint32_t segment_id, const QRect& rect, const QPixmap& pixmap) const {
    // Limit cache size to prevent memory issues
    if (segment_cache_.size() > 100) {
        segment_cache_.erase(segment_cache_.begin(), segment_cache_.begin() + 50); // Remove first half
    }
    
    int current_zoom = static_cast<int>(zoom_factor_ * 100);
    SegmentCacheEntry entry;
    entry.segment_id = segment_id;
    entry.rect = rect;
    entry.cached_pixmap = pixmap;
    entry.zoom_level = current_zoom;
    
    segment_cache_.push_back(entry);
}

void TimelinePanel::clear_segment_cache() const {
    segment_cache_.clear();
}

// ======= Phase 1 Performance Optimizations =======

void TimelinePanel::init_paint_objects() const {
    if (paint_objects_initialized_) return;
    
    // Pre-allocate all paint objects to avoid allocation during paint events
    cached_video_color_ = QColor(80, 120, 180);
    cached_audio_color_ = QColor(120, 180, 80);
    cached_selected_color_ = QColor(200, 140, 100);
    cached_text_color_ = QColor(220, 220, 220);
    
    cached_border_pen_ = QPen(Qt::white, 1);
    cached_grid_pen_ = QPen(QColor(70, 70, 70), 1);
    
    cached_segment_brush_ = QBrush(cached_video_color_);
    
    cached_name_font_ = QFont("Arial", 8);
    cached_small_font_ = QFont("Arial", 7);
    cached_font_metrics_ = QFontMetrics(cached_name_font_);
    
    paint_objects_initialized_ = true;
}

void TimelinePanel::set_heavy_operation_mode(bool enabled) {
    if (heavy_operation_mode_ == enabled) return;
    
    heavy_operation_mode_ = enabled;
    ve::log::info(enabled ? "Timeline entering heavy operation mode (15fps)" : "Timeline returning to normal mode (60fps)");
    
    // Clear any pending paint requests when switching modes
    if (paint_throttle_timer_->isActive()) {
        paint_throttle_timer_->stop();
    }
    pending_paint_request_ = false;
    
    // Force immediate update when exiting heavy mode to show final state
    if (!enabled) {
        QWidget::update();
    }
}

void TimelinePanel::request_throttled_update() {
    if (pending_paint_request_) return; // Already have a pending request
    
    int throttle_interval = heavy_operation_mode_ ? (1000 / HEAVY_OPERATION_FPS) : (1000 / NORMAL_FPS);
    
    pending_paint_request_ = true;
    paint_throttle_timer_->start(throttle_interval);
}

bool TimelinePanel::should_skip_expensive_features() const {
    return heavy_operation_mode_ || segments_being_added_ > 0 || g_timeline_busy.load(std::memory_order_acquire);
}

void TimelinePanel::draw_minimal_timeline(QPainter& painter) {
    // Ultra-fast minimal timeline for heavy operations
    init_paint_objects();
    
    // Simple background
    painter.fillRect(rect(), QColor(45, 45, 45));
    
    // Basic timecode ruler
    painter.setPen(cached_grid_pen_);
    painter.drawLine(0, TIMECODE_HEIGHT, width(), TIMECODE_HEIGHT);
    
    if (!timeline_) return;
    
    // Draw basic track structure only
    const auto& tracks = timeline_->tracks();
    int y = TIMECODE_HEIGHT;
    
    for (size_t i = 0; i < tracks.size() && y < height(); ++i) {
        const auto& track = *tracks[i];
        
        // Track separator
        painter.setPen(cached_grid_pen_);
        painter.drawLine(0, y, width(), y);
        
        // Very basic segments - just colored rectangles, no details
        const auto& segments = track.segments();
        QColor track_color = (track.type() == ve::timeline::Track::Video) ? 
                            cached_video_color_ : cached_audio_color_;
        
        // Limit segments for performance
        int segments_drawn = 0;
        for (const auto& segment : segments) {
            if (segments_drawn >= 5) break; // Only draw first 5 segments per track
            
            int start_x = time_to_pixel(segment.start_time);
            int end_x = time_to_pixel(segment.end_time());
            int segment_width = end_x - start_x;
            
            if (segment_width > 2) { // Only draw visible segments
                QRect segment_rect(start_x, y + 5, segment_width, TRACK_HEIGHT - 10);
                painter.fillRect(segment_rect, track_color);
                segments_drawn++;
            }
        }
        
        y += TRACK_HEIGHT;
    }
    
    // Simple playhead
    int playhead_x = time_to_pixel(current_time_);
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(playhead_x, 0, playhead_x, height());
}

// ======= Phase 2 Performance Optimizations =======

void TimelinePanel::invalidate_region(const QRect& rect, bool needs_full_redraw) {
    if (rect.isEmpty()) return;
    
    // Check if this region overlaps with existing dirty regions
    bool merged = false;
    for (auto& dirty : dirty_regions_) {
        if (dirty.rect.intersects(rect)) {
            // Merge overlapping regions
            dirty.rect = dirty.rect.united(rect);
            dirty.needs_full_redraw = dirty.needs_full_redraw || needs_full_redraw;
            merged = true;
            break;
        }
    }
    
    if (!merged) {
        dirty_regions_.emplace_back(rect, needs_full_redraw);
    }
    
    // Update total dirty rect
    if (has_dirty_regions_) {
        total_dirty_rect_ = total_dirty_rect_.united(rect);
    } else {
        total_dirty_rect_ = rect;
        has_dirty_regions_ = true;
    }
    
    // Limit dirty regions to prevent memory bloat
    if (dirty_regions_.size() > 20) {
        // Merge all regions into one large region
        QRect combined;
        for (const auto& dirty : dirty_regions_) {
            combined = combined.united(dirty.rect);
        }
        dirty_regions_.clear();
        dirty_regions_.emplace_back(combined, true);
    }
}

void TimelinePanel::invalidate_track(size_t track_index) {
    if (!timeline_ || track_index >= timeline_->tracks().size()) return;
    
    int track_y = track_y_position(track_index);
    QRect track_rect(0, track_y, width(), TRACK_HEIGHT);
    invalidate_region(track_rect, true);
}

void TimelinePanel::invalidate_segment(uint32_t segment_id) {
    if (!timeline_) return;
    
    // Find the segment and invalidate its visual area
    const auto& tracks = timeline_->tracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = *tracks[i];
        for (const auto& segment : track.segments()) {
            if (segment.id == segment_id) {
                int start_x = time_to_pixel(segment.start_time);
                int end_x = time_to_pixel(segment.end_time());
                int track_y = track_y_position(i);
                
                QRect segment_rect(start_x - 5, track_y, end_x - start_x + 10, TRACK_HEIGHT);
                invalidate_region(segment_rect, false);
                return;
            }
        }
    }
}

void TimelinePanel::clear_dirty_regions() {
    dirty_regions_.clear();
    has_dirty_regions_ = false;
    total_dirty_rect_ = QRect();
}

bool TimelinePanel::is_region_dirty(const QRect& rect) const {
    if (!has_dirty_regions_) return false;
    
    for (const auto& dirty : dirty_regions_) {
        if (dirty.rect.intersects(rect)) {
            return true;
        }
    }
    return false;
}

// Phase 2: Level-of-detail rendering
TimelinePanel::DetailLevel TimelinePanel::calculate_detail_level(int segment_width, double zoom_factor) const {
    if (segment_width < 5) return DetailLevel::MINIMAL;     // Just a colored line
    if (segment_width < 20) return DetailLevel::BASIC;      // Colored rectangle with border
    if (segment_width < 60) return DetailLevel::NORMAL;     // + segment name
    return DetailLevel::DETAILED;                           // + duration, waveforms, etc.
}

} // namespace ve::ui

// Removed explicit moc include; handled by AUTOMOC
