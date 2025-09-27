#include "ui/timeline_panel.hpp"
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

#include <atomic>
// Global variable to track timeline operations
std::atomic<bool> g_timeline_busy{false};


namespace ve::ui {

TimelinePanel::TimelinePanel(QWidget* parent)
    : QWidget(parent)
    , timeline_(nullptr)
    , current_time_(ve::TimePoint{0})
    , zoom_factor_(1.0)
    , playback_controller_(nullptr)
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
    // Advanced Timeline Features (Phase 2)
    , snap_enabled_(true)
    , grid_size_(ve::TimeDuration{1, 10}) // 0.1 second grid by default
    , ripple_mode_(false)
    , rubber_band_selecting_(false)
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
    // Professional timeline dimensions - take significant portion of application
    setMinimumHeight(600);    // Professional minimum height for multi-track editing
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus); // Only get focus when clicked, not interfering with drag
    
    // Enable hover events for proper mouse highlighting
    setAttribute(Qt::WA_Hover, true);
    
    // Professional track colors matching industry standards
    track_color_video_ = QColor(45, 65, 95);     // Dark blue-gray for video tracks
    track_color_audio_ = QColor(55, 75, 45);     // Dark olive-green for audio tracks
    segment_color_ = QColor(100, 140, 200);
    segment_selected_color_ = QColor(200, 140, 100);
    playhead_color_ = QColor(255, 0, 0);
    background_color_ = QColor(30, 30, 30);      // Darker professional background
    grid_color_ = QColor(60, 60, 60);            // Lighter grid lines
    snap_guide_color_ = QColor(255, 255, 0, 180);  // Semi-transparent yellow
    rubber_band_color_ = QColor(100, 150, 255, 80); // Semi-transparent blue
    
    // WaveformGenerator and WaveformCache integration placeholder
    // TODO: Implement actual WaveformGenerator when audio engine is complete
    // Future waveform system initialization will go here
    
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
    
    // Phase 4: Initialize advanced optimizations
    initialize_phase4_optimizations();
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

void TimelinePanel::set_playback_controller(ve::playback::PlaybackController* controller) {
    playback_controller_ = controller;
    // Video playback integration is now available
    ve::log::info("Timeline panel connected to playback controller for video scrubbing");
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
    
    // Patch 5: Use queued update to prevent painting conflicts during timeline updates
    if (painting_) {
        repaint_pending_ = true;
    } else {
        QTimer::singleShot(0, this, [this]() { update(); });
    }
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
// Phase 1: Override update to use throttled painting with re-entrancy protection
void TimelinePanel::update() {
    // If a paint is in progress, just mark pending; the queued invoke below will coalesce.
    if (painting_.load()) {
        repaint_pending_.store(true);
        return;
    }
    request_throttled_update();
}

void TimelinePanel::paintEvent(QPaintEvent* event) {
    // Re-entrancy protection: prevent overlapping paint operations
    if (painting_.exchange(true)) {
        // Should never happen, but bail out defensively.
        return;
    }
    
    // Performance monitoring - track paint timing
    auto paint_start = std::chrono::high_resolution_clock::now();
    
    // Phase 4: Initialize memory containers and optimizations
    update_memory_containers_for_paint();
    
    // Phase 4: Reset paint object pools for this paint session
    paint_object_pool_.reset_pools();
    
    // Phase 4: Cache painter state for optimization
    QPainter painter(this);
    advanced_paint_state_.cache_current_state(painter);
    
    painter.setRenderHint(QPainter::Antialiasing, false); // Crisp lines for timeline
    
    // Phase 1: Check if we should use minimal timeline rendering
    if (should_skip_expensive_features()) {
        draw_minimal_timeline(painter);
        
        // Phase 4: Record performance metrics
        auto paint_end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(paint_end - paint_start);
        performance_analytics_.record_paint_time("total", total_time);
        
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
    
    // Phase 4: Cleanup resources and record performance metrics
    auto paint_end = std::chrono::high_resolution_clock::now();
    auto paint_duration = std::chrono::duration<double, std::milli>(paint_end - paint_start);
    auto background_time = std::chrono::duration<double, std::milli>(after_background - paint_start);
    auto timecode_time = std::chrono::duration<double, std::milli>(after_timecode - after_background);
    auto tracks_time = std::chrono::duration<double, std::milli>(after_tracks - after_timecode);
    
    // Phase 4: Record detailed performance analytics
    auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(paint_end - paint_start);
    auto bg_time = std::chrono::duration_cast<std::chrono::microseconds>(after_background - paint_start);
    auto tc_time = std::chrono::duration_cast<std::chrono::microseconds>(after_timecode - after_background);
    auto tr_time = std::chrono::duration_cast<std::chrono::microseconds>(after_tracks - after_timecode);
    
    performance_analytics_.record_paint_time("total", total_time);
    performance_analytics_.record_paint_time("background", bg_time);
    performance_analytics_.record_paint_time("timecode", tc_time);
    performance_analytics_.record_paint_time("segments", tr_time);
    
    // Phase 4: Cleanup resources for next paint
    cleanup_phase4_resources();
    
    last_paint = paint_start;
    
    static auto last_warning = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // Enhanced warning with Phase 4 statistics
    if (paint_duration.count() > 16.0 && 
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_warning).count() > 1000) {
        
        // Print comprehensive performance report every second if slow
        performance_analytics_.print_analytics();
        paint_object_pool_.print_pool_statistics();
        advanced_paint_state_.print_state_optimization_stats();
        memory_optimizations_.print_memory_stats();
        
        std::string warning_msg = "Timeline paint slow: " + std::to_string(paint_duration.count()) + 
                                "ms (background: " + std::to_string(background_time.count()) + 
                                "ms, timecode: " + std::to_string(timecode_time.count()) + 
                                "ms, tracks: " + std::to_string(tracks_time.count()) + 
                                "ms) Paint rect: " + std::to_string(event->rect().width()) + "x" + 
                                std::to_string(event->rect().height()) + 
                                ", Cache hit rate: " + std::to_string(performance_analytics_.get_overall_cache_hit_rate()) + 
                                "%, State changes avoided: " + std::to_string(advanced_paint_state_.total_state_changes_avoided_);
        ve::log::warn(warning_msg);
        
        last_warning = now;
    }
    
    // Draw rubber band selection if active
    if (rubber_band_selecting_ && !rubber_band_rect_.isEmpty()) {
        QPainter painter(this);
        painter.setPen(QPen(rubber_band_color_, 1, Qt::SolidLine));
        painter.setBrush(QBrush(rubber_band_color_));
        painter.drawRect(rubber_band_rect_);
    }
    
    // Clear re-entrancy guards at method end
    painting_ = false;
    repaint_pending_ = false;
}

void TimelinePanel::mousePressEvent(QMouseEvent* event) {
    // Removed debug logging for performance - mouse press events can be frequent
    
    if (!timeline_) return;
    
    drag_start_ = event->pos();
    last_mouse_pos_ = event->pos();
    
    if (event->button() == Qt::LeftButton) {
        if (event->pos().y() < TIMECODE_HEIGHT) {
            // Clicked in timecode ruler - ONLY move playhead on click (not drag)
            ve::TimePoint time = pixel_to_time(event->pos().x());
            current_time_ = time;
            emit time_changed(time);
            
            // Immediate video preview for timeline scrubbing (without debounce)
            if (playback_controller_) {
                auto r = time.to_rational();
                int64_t time_us = (r.num * 1000000) / r.den;
                playback_controller_->seek(time_us);
                // This provides immediate video frame preview during timeline scrubbing
            }
            
            update();
            // Don't enable dragging for playhead - user wants click-only movement
            return;
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
    
    // Handle rubber band selection
    if (rubber_band_selecting_) {
        rubber_band_rect_ = QRect(rubber_band_start_, event->pos()).normalized();
        
        // Throttle rubber band updates
        if (time_since_last.count() >= 16) {  // ~60fps for rubber band
            update();
            last_update = now;
        }
        
        last_mouse_pos_ = event->pos();
        return;
    }
    
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
        // NOTE: Playhead scrubbing removed - user wants click-only playhead movement
        // Only handle segment dragging in track area
        if (event->pos().y() >= TIMECODE_HEIGHT) {
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
    
    // Handle rubber band selection completion
    if (rubber_band_selecting_) {
        rubber_band_selecting_ = false;
        
        // Select all segments within rubber band rectangle
        rubber_band_rect_ = QRect(rubber_band_start_, event->pos()).normalized();
        
        if (!(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
            selected_segments_.clear();
        }
        
        select_segments_in_range(
            pixel_to_time(rubber_band_rect_.left()),
            pixel_to_time(rubber_band_rect_.right())
        );
        
        rubber_band_rect_ = QRect(); // Clear rectangle
        emit selection_changed();
        update();
        return;
    }
    
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
    // Enhanced viewport culling: Calculate viewport info once
    ViewportInfo viewport = calculate_viewport_info();
    
    if (timeline_) {
        // Draw actual timeline tracks
        const auto& tracks = timeline_->tracks();
        
        for (size_t i = 0; i < tracks.size(); ++i) {
            int track_y = track_y_position(i);
            
            // Enhanced viewport culling: Skip tracks completely outside viewport
            if (!is_track_visible(track_y, viewport)) {
                continue;
            }
            
            draw_track(painter, *tracks[i], track_y);
        }
    } else {
        // Draw default empty timeline structure (V1-V3, A1-A8) when no timeline is loaded
        draw_default_empty_tracks(painter, viewport);
    }
}

void TimelinePanel::draw_track(QPainter& painter, const ve::timeline::Track& track, int track_y) {
    QRect track_rect(0, track_y, width(), TRACK_HEIGHT);
    
    // Professional track background with type-specific styling
    QColor track_bg;
    QColor track_border;
    QString track_icon;
    
    if (track.type() == ve::timeline::Track::Video) {
        track_bg = QColor(25, 35, 55);       // Dark blue-gray for video tracks
        track_border = QColor(65, 85, 125);  // Lighter blue border
        track_icon = "V";                    // Video icon placeholder
    } else {
        track_bg = QColor(35, 45, 25);       // Dark olive-green for audio tracks
        track_border = QColor(75, 95, 55);   // Lighter green border
        track_icon = "A";                    // Audio icon placeholder
    }
    
    painter.fillRect(track_rect, track_bg);
    
    // Professional track border with subtle gradient
    painter.setPen(QPen(track_border, 1));
    painter.drawRect(track_rect);
    
    // Track header area with professional styling
    QRect header_rect(0, track_y, 120, TRACK_HEIGHT);
    painter.fillRect(header_rect, track_bg.lighter(115));
    
    // Track type icon in a small square
    QRect icon_rect(8, track_y + 8, 20, 20);
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawRect(icon_rect);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    painter.drawText(icon_rect, Qt::AlignCenter, track_icon);
    
    // Track name with professional typography
    painter.setPen(QColor(220, 220, 220));
    painter.setFont(QFont("Arial", 9));
    QRect label_rect(35, track_y + 5, 80, 20);
    QString track_name = QString::fromStdString(track.name());
    painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, track_name);
    
    // Track controls area indicator (mute/solo placeholder)
    QRect controls_rect(35, track_y + 25, 80, 15);
    painter.setPen(QColor(150, 150, 150));
    painter.setFont(QFont("Arial", 7));
    if (track.is_muted()) {
        painter.drawText(controls_rect, Qt::AlignLeft | Qt::AlignVCenter, "MUTED");
    }
    
    // Separator line between header and content
    painter.setPen(QPen(track_border, 1));
    painter.drawLine(120, track_y, 120, track_y + TRACK_HEIGHT);
    
    // Draw segments
    draw_segments(painter, track, track_y);
}

void TimelinePanel::draw_default_empty_tracks(QPainter& painter, const ViewportInfo& viewport) {
    // Draw default professional track layout: V1-V3 (video) and A1-A8 (audio)
    const int total_tracks = 11; // 3 video tracks + 8 audio tracks
    const std::vector<std::pair<std::string, bool>> default_tracks = {
        {"V3", true},  // Video tracks (top to bottom)
        {"V2", true},
        {"V1", true},
        {"A1", false}, // Audio tracks (top to bottom)
        {"A2", false},
        {"A3", false},
        {"A4", false},
        {"A5", false},
        {"A6", false},
        {"A7", false},
        {"A8", false}
    };
    
    for (int i = 0; i < total_tracks; ++i) {
        int track_y = track_y_position(i);
        
        // Enhanced viewport culling: Skip tracks completely outside viewport
        if (!is_track_visible(track_y, viewport)) {
            continue;
        }
        
        const auto& track_info = default_tracks[i];
        const std::string& track_name = track_info.first;
        bool is_video = track_info.second;
        
        draw_empty_track(painter, track_name, is_video, track_y);
    }
}

void TimelinePanel::draw_empty_track(QPainter& painter, const std::string& track_name, bool is_video, int track_y) {
    QRect track_rect(0, track_y, width(), TRACK_HEIGHT);
    
    // Professional track background with type-specific styling
    QColor track_bg;
    QColor track_border;
    QString track_icon;
    
    if (is_video) {
        track_bg = QColor(25, 35, 55);       // Dark blue-gray for video tracks
        track_border = QColor(65, 85, 125);  // Lighter blue border
        track_icon = "V";                    // Video icon placeholder
    } else {
        track_bg = QColor(35, 45, 25);       // Dark olive-green for audio tracks
        track_border = QColor(75, 95, 55);   // Lighter green border
        track_icon = "A";                    // Audio icon placeholder
    }
    
    // Draw track background
    painter.fillRect(track_rect, track_bg);
    
    // Draw track border
    painter.setPen(QPen(track_border, 1));
    painter.drawRect(track_rect);
    
    // Track header area (first 120 pixels)
    QRect header_rect(0, track_y, 120, TRACK_HEIGHT);
    painter.fillRect(header_rect, track_bg.darker(110));
    
    // Track icon
    QRect icon_rect(10, track_y + 5, 20, TRACK_HEIGHT - 10);
    painter.setPen(QPen(track_border.lighter(150), 1));
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(icon_rect, Qt::AlignCenter, track_icon);
    
    // Track name
    QRect label_rect(35, track_y, 80, TRACK_HEIGHT);
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.setFont(QFont("Arial", 9));
    painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(track_name));
    
    // Separator line between header and content
    painter.setPen(QPen(track_border, 1));
    painter.drawLine(120, track_y, 120, track_y + TRACK_HEIGHT);
    
    // No segments to draw for empty tracks
}

void TimelinePanel::draw_segments(QPainter& painter, const ve::timeline::Track& track, int track_y) {
    // Phase 3: Use batched rendering for better performance
    draw_segments_batched(painter, track, track_y);
}

void TimelinePanel::draw_segments_batched(QPainter& painter, const ve::timeline::Track& track, int track_y) {
    // Phase 4: Enhanced viewport culling with memory optimization
    ViewportInfo viewport = calculate_viewport_info();
    std::vector<const ve::timeline::Segment*> visible_segments = cull_segments_optimized(track.segments(), viewport);
    
    // Early exit if no visible segments
    if (visible_segments.empty()) return;
    
    // Phase 4: Use memory optimizations for segment batching
    batch_similar_segments(visible_segments);
    
    // Performance circuit breaker - limit number of segments processed per paint event
    static int max_segments_per_paint = 40;
    if (visible_segments.size() > static_cast<size_t>(max_segments_per_paint)) {
        visible_segments.resize(max_segments_per_paint);
        static auto last_warning = std::chrono::steady_clock::time_point{};
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_warning).count() > 1000) {
            ve::log::warn("Timeline: Segment limit reached, using Phase 4 optimized batched rendering");
            last_warning = now;
        }
    }
    
    // Phase 4: Draw optimized batches with memory pool and state caching
    for (const auto& batch : memory_optimizations_.segment_batches_) {
        draw_segment_batch_optimized(painter, batch, track_y);
    }
    
    // Phase 4: Record performance metrics
    performance_analytics_.record_memory_saved(visible_segments.size() * sizeof(ve::timeline::Segment*));
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
    
    // Phase 4: Optimized state management with caching
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
    
    // Mark paint state as valid for the session
    paint_state_cache_.has_valid_state = true;
    
    // Phase 4: Batch draw all rectangles with optimized state changes
    for (size_t i = 0; i < batch.segments.size(); ++i) {
        const auto& segment = *batch.segments[i];
        const QRect& rect = batch.rects[i];
        
        // Handle different detail levels with smart state caching
        switch (batch.detail_level) {
            case DetailLevel::MINIMAL:
                // Just fill with lighter color for minimal segments
                apply_brush_if_needed(painter, batch.color.lighter(120));
                painter.fillRect(rect, painter.brush());
                break;
                
            case DetailLevel::BASIC:
                // Fill + border for basic segments
                apply_brush_if_needed(painter, batch.color.lighter(110));
                painter.fillRect(rect, painter.brush());
                apply_pen_if_needed(painter, cached_border_pen_.color(), cached_border_pen_.widthF(), cached_border_pen_.style());
                painter.drawRect(rect);
                break;
                
            case DetailLevel::NORMAL:
            case DetailLevel::DETAILED:
                // Full rendering for normal/detailed segments
                apply_brush_if_needed(painter, batch.color);
                painter.fillRect(rect, painter.brush());
                apply_pen_if_needed(painter, cached_border_pen_.color(), cached_border_pen_.widthF(), cached_border_pen_.style());
                painter.drawRect(rect);
                
                // Add text/details if not in heavy operation mode
                if (!should_skip_expensive_features()) {
                    // Add segment name for NORMAL and above
                    if (batch.detail_level >= DetailLevel::NORMAL) {
                        apply_font_if_needed(painter, cached_name_font);
                        apply_pen_if_needed(painter, cached_text_color);
                        
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
                        
                        apply_font_if_needed(painter, cached_small_font);
                        apply_pen_if_needed(painter, cached_text_color);
                        
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
    
    // Phase 4: Use smart state caching for playhead
    apply_pen_if_needed(painter, playhead_color_, PLAYHEAD_WIDTH);
    painter.drawLine(x, 0, x, height());
    
    // Playhead handle
    QPolygon handle;
    handle << QPoint(x - 5, 0) << QPoint(x + 5, 0) << QPoint(x, 10);
    apply_brush_if_needed(painter, playhead_color_);
    painter.drawPolygon(handle);
}

void TimelinePanel::draw_selection(QPainter& painter) {
    if (selecting_range_) {
        int start_x = time_to_pixel(selection_start_);
        int end_x = time_to_pixel(selection_end_);
        
        if (start_x > end_x) std::swap(start_x, end_x);
        
        QRect selection_rect(start_x, TIMECODE_HEIGHT, end_x - start_x, height() - TIMECODE_HEIGHT);
        painter.fillRect(selection_rect, QColor(255, 255, 255, 50));
        
        // Phase 4: Use smart state caching for selection
        apply_pen_if_needed(painter, QColor(255, 255, 255), 1.0, Qt::DashLine);
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
    auto rational = time.to_rational();
    
    // CRITICAL FIX: Prevent division by zero SIGABRT
    if (rational.den == 0) {
        ve::log::error("TimelinePanel::time_to_pixel - Invalid time with denominator=0, using 0 position");
        return -scroll_x_;  // Return valid pixel position for invalid time
    }
    
    double seconds = static_cast<double>(rational.num) / rational.den;
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
        ve::TimePoint raw_time = pixel_to_time(pos.x());
        
        // Apply snapping (grid first, then segments)
        ve::TimePoint snapped_time = snap_to_grid(raw_time);
        snapped_time = snap_to_segments(snapped_time, dragged_segment_id_);
        
        // Snap to reasonable boundaries
        if (snapped_time.to_rational().num < 0) {
            snapped_time = ve::TimePoint{0};
        }
        
        // Store preview position
        preview_start_time_ = snapped_time;
        preview_duration_ = original_segment_duration_;
        show_drag_preview_ = true;
        
        // Update snap guides for visual feedback
        snap_points_.clear();
        if (snap_enabled_) {
            snap_points_.push_back(snapped_time);
        }
        
        update();
        
    } else if (resizing_segment_) {
        // Calculate new size for preview with snapping
        ve::TimePoint raw_time = pixel_to_time(pos.x());
        ve::TimePoint snapped_time = snap_to_grid(raw_time);
        snapped_time = snap_to_segments(snapped_time, dragged_segment_id_);
        
        if (is_left_resize_) {
            // Resize from left edge
            ve::TimePoint original_end = ve::TimePoint{
                original_segment_start_.to_rational().num + original_segment_duration_.to_rational().num,
                original_segment_start_.to_rational().den
            };
            
            if (snapped_time < original_end && snapped_time.to_rational().num >= 0) {
                preview_start_time_ = snapped_time;
                preview_duration_ = ve::TimeDuration{
                    original_end.to_rational().num - snapped_time.to_rational().num,
                    snapped_time.to_rational().den
                };
                show_drag_preview_ = true;
                
                // Update snap guides
                snap_points_.clear();
                if (snap_enabled_) {
                    snap_points_.push_back(snapped_time);
                }
                
                update();
            }
        } else {
            // Resize from right edge
            if (snapped_time > original_segment_start_) {
                preview_start_time_ = original_segment_start_;
                preview_duration_ = ve::TimeDuration{
                    snapped_time.to_rational().num - original_segment_start_.to_rational().num,
                    original_segment_start_.to_rational().den
                };
                show_drag_preview_ = true;
                
                // Update snap guides
                snap_points_.clear();
                if (snap_enabled_) {
                    snap_points_.push_back(snapped_time);
                }
                
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
        // Clicked empty space
        if (!(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
            selected_segments_.clear();
            emit selection_changed();
        }
        
        // Start rubber band selection if in track area
        if (pos.y() >= TIMECODE_HEIGHT) {
            rubber_band_selecting_ = true;
            rubber_band_start_ = pos;
            rubber_band_rect_ = QRect(pos, QSize(0, 0));
        } else {
            // Timeline seeking
            ve::TimePoint clicked_time = pixel_to_time(pos.x());
            current_time_ = clicked_time;
            emit time_changed(clicked_time);
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
    // Enhanced Waveform Integration - Simplified implementation using enhanced placeholder
    
    // For now, use enhanced placeholder until full WaveformGenerator is implemented
    draw_placeholder_waveform(painter, rect, segment);
}

void TimelinePanel::draw_cached_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment) {
    // Phase 2: Enhanced waveform rendering with file-aware patterns
    if (!timeline_) {
        draw_audio_waveform(painter, rect, segment);
        return;
    }
    
    // Get the clip for this segment to use file path for enhanced waveform generation
    const auto* clip = timeline_->get_clip(segment.clip_id);
    if (!clip || !clip->source || clip->source->path.empty()) {
        draw_audio_waveform(painter, rect, segment);
        return;
    }
    
    // Phase 2: Generate file-specific waveform pattern using path hash
    std::hash<std::string> hasher;
    size_t file_hash = hasher(clip->source->path);
    
    // Extract audio file characteristics from path for realistic patterns
    std::string path = clip->source->path;
    std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    
    // Phase 2: Determine waveform characteristics based on likely audio content
    float base_amplitude = 0.6f;
    float variation_factor = 0.8f;
    int sample_density = 8; // pixels between samples
    
    // Enhance pattern based on file type hints
    if (path.find("music") != std::string::npos || path.find("song") != std::string::npos) {
        base_amplitude = 0.8f; // Music tends to have higher dynamic range
        variation_factor = 1.2f;
        sample_density = 6; // Denser sampling for music
    } else if (path.find("voice") != std::string::npos || path.find("speech") != std::string::npos) {
        base_amplitude = 0.5f; // Speech typically has lower peaks
        variation_factor = 0.6f;
        sample_density = 10; // Less dense for speech
    } else if (path.find("drums") != std::string::npos || path.find("beat") != std::string::npos) {
        base_amplitude = 0.9f; // Drums have sharp peaks
        variation_factor = 1.5f;
        sample_density = 4; // Very dense for percussive content
    }
    
    // Phase 2: Render enhanced waveform with realistic amplitude variation
    painter.setPen(QPen(QColor(120, 200, 255, 160), 1)); // Slightly more visible
    
    int center_y = rect.center().y();
    int max_height = rect.height() * 0.7; // Use more of available height
    
    // Generate waveform samples across the segment width
    for (int x = rect.left(); x < rect.right(); x += sample_density) {
        // Create pseudo-random but deterministic waveform pattern
        int sample_seed = static_cast<int>(file_hash + x + segment.id * 37);
        
        // Generate multiple frequency components for realistic look
        float low_freq = std::sin(sample_seed * 0.02f) * 0.4f;
        float mid_freq = std::sin(sample_seed * 0.08f) * 0.3f;
        float high_freq = std::sin(sample_seed * 0.15f) * 0.2f;
        
        // Combine frequencies with amplitude envelope
        float amplitude = (low_freq + mid_freq + high_freq) * base_amplitude;
        amplitude *= (1.0f + (sample_seed % 100) / 100.0f * variation_factor);
        
        // Apply realistic amplitude clipping
        amplitude = std::max(-1.0f, std::min(1.0f, amplitude));
        
        int sample_height = static_cast<int>(std::abs(amplitude) * max_height);
        
        // Draw positive and negative waveform components
        if (amplitude >= 0) {
            painter.drawLine(x, center_y, x, center_y - sample_height);
        } else {
            painter.drawLine(x, center_y, x, center_y + sample_height);
        }
        
        // Add subtle stereo effect for wider segments
        if (rect.width() > 200 && sample_density <= 6) {
            int stereo_offset = static_cast<int>((sample_seed % 3) - 1);
            int stereo_height = sample_height * 0.7;
            painter.drawLine(x + 1, center_y - stereo_offset, x + 1, center_y - stereo_offset - stereo_height);
        }
    }
    
    // Phase 2: Add center line for reference
    if (rect.width() > 100) {
        painter.setPen(QPen(QColor(120, 200, 255, 40), 1));
        painter.drawLine(rect.left(), center_y, rect.right(), center_y);
    }
    
    std::hash<std::string> path_hasher;
    size_t path_hash = path_hasher(clip->source->path);
    
    painter.setPen(QPen(QColor(120, 200, 255, 120), 1));
    
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
    // Remove old cache entries if cache is too large
    segment_cache_.erase(std::remove_if(segment_cache_.begin(), segment_cache_.end(),
        [this](const SegmentCacheEntry& entry) { return segment_cache_.size() > 50; }), 
        segment_cache_.end());
    
    // Create new cache entry
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

// Paint object initialization
void TimelinePanel::init_paint_objects() const {
    if (paint_objects_initialized_) return;
    
    cached_video_color_ = QColor(70, 130, 180);  // Steel blue
    cached_audio_color_ = QColor(100, 149, 237); // Cornflower blue
    cached_selected_color_ = QColor(255, 215, 0); // Gold
    cached_text_color_ = QColor(255, 255, 255);  // White
    
    cached_border_pen_ = QPen(QColor(255, 255, 255, 100), 1);
    cached_grid_pen_ = QPen(QColor(255, 255, 255, 30), 1);
    
    cached_segment_brush_ = QBrush(cached_video_color_);
    
    cached_name_font_ = QFont("Arial", 9);
    cached_small_font_ = QFont("Arial", 7);
    cached_font_metrics_ = QFontMetrics(cached_name_font_);
    
    paint_objects_initialized_ = true;
}

// Performance mode management
void TimelinePanel::set_heavy_operation_mode(bool heavy_mode) {
    if (heavy_operation_mode_ == heavy_mode) return;
    
    heavy_operation_mode_ = heavy_mode;
    
    // Adjust paint throttling
    if (paint_throttle_timer_) {
        paint_throttle_timer_->stop();
    }
    
    pending_paint_request_ = false;
}

// ======= Phase 1 Performance Optimizations Implementation =======

void TimelinePanel::request_throttled_update() {
    if (pending_paint_request_) return; // Already have a pending request
    
    pending_paint_request_ = true;
    
    if (paint_throttle_timer_ && !paint_throttle_timer_->isActive()) {
        int throttle_interval = heavy_operation_mode_ ? (1000 / HEAVY_OPERATION_FPS) : (1000 / NORMAL_FPS);
        paint_throttle_timer_->start(throttle_interval);
    } else {
        QWidget::update();
    }
}

bool TimelinePanel::should_skip_expensive_features() const {
    return heavy_operation_mode_ || segments_being_added_ > 0;
}

void TimelinePanel::draw_minimal_timeline(QPainter& painter) {
    // Ultra-fast minimal timeline for heavy operations
    painter.fillRect(rect(), QColor(45, 45, 45));
    
    // Basic timecode ruler
    painter.setPen(QColor(100, 100, 100));
    painter.drawLine(0, TIMECODE_HEIGHT, width(), TIMECODE_HEIGHT);
    
    if (!timeline_) {
        // Draw default empty tracks even in minimal mode
        ViewportInfo viewport;
        viewport.left_x = 0;
        viewport.right_x = width();
        viewport.top_y = 0;
        viewport.bottom_y = height();
        viewport.start_time = ve::TimePoint{0};
        viewport.end_time = ve::TimePoint{10000}; // 10 seconds default
        viewport.time_to_pixel_ratio = 1.0;
        
        draw_default_empty_tracks(painter, viewport);
        return;
    }
    
    // Draw basic track structure only - no details
    const auto& tracks = timeline_->tracks();
    int y = TIMECODE_HEIGHT;
    
    for (size_t i = 0; i < tracks.size() && y < height(); ++i) {
        const auto& track = *tracks[i];
        
        // Track separator
        painter.setPen(QColor(70, 70, 70));
        painter.drawLine(0, y, width(), y);
        
        // Very basic segments - just colored rectangles
        const auto& segments = track.segments();
        QColor track_color = (track.type() == ve::timeline::Track::Video) ? 
                            QColor(80, 120, 180) : QColor(120, 180, 80);
        
        // Limit segments for performance (only first 5 per track)
        int segments_drawn = 0;
        for (const auto& segment : segments) {
            if (segments_drawn >= 5) break;
            
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

void TimelinePanel::invalidate_region(const QRect& rect, bool needs_full_redraw) {
    if (rect.isEmpty()) return;
    
    // Patch 6: Clamp dirty regions to widget bounds to prevent Qt painting conflicts
    QRect widget_bounds(0, 0, width(), height());
    QRect clamped_rect = rect.intersected(widget_bounds);
    if (clamped_rect.isEmpty()) return;
    
    // Phase 2: Smart dirty region tracking
    bool merged = false;
    
    // Try to merge with existing dirty regions
    for (auto& dirty : dirty_regions_) {
        if (dirty.rect.intersects(clamped_rect)) {
            // Merge overlapping regions
            dirty.rect = dirty.rect.united(clamped_rect);
            dirty.needs_full_redraw = dirty.needs_full_redraw || needs_full_redraw;
            merged = true;
            break;
        }
    }
    
    // If not merged, add as new dirty region
    if (!merged) {
        dirty_regions_.emplace_back(clamped_rect, needs_full_redraw);
    }
    
    // Update total dirty rect for efficient bounds checking
    if (has_dirty_regions_) {
        total_dirty_rect_ = total_dirty_rect_.united(clamped_rect);
    } else {
        total_dirty_rect_ = clamped_rect;
        has_dirty_regions_ = true;
    }
    
    // Limit dirty regions to prevent memory bloat (keep most recent)
    if (dirty_regions_.size() > 50) {
        dirty_regions_.erase(dirty_regions_.begin(), dirty_regions_.begin() + 25);
    }
    
    // Schedule actual Qt update for the combined region
    QWidget::update(total_dirty_rect_);
}

void TimelinePanel::clear_dirty_regions() {
    // Phase 2: Clear all dirty region tracking
    dirty_regions_.clear();
    has_dirty_regions_ = false;
    total_dirty_rect_ = QRect();
}

TimelinePanel::DetailLevel TimelinePanel::calculate_detail_level(int visible_width, double zoom_factor) const {
    // Simple detail level calculation for Phase 1
    if (heavy_operation_mode_) return DetailLevel::MINIMAL;
    if (zoom_factor < 0.1) return DetailLevel::MINIMAL;
    if (zoom_factor < 0.5) return DetailLevel::BASIC;
    if (zoom_factor < 2.0) return DetailLevel::NORMAL;
    return DetailLevel::DETAILED;
}

void TimelinePanel::apply_pen_if_needed(QPainter& painter, const QColor& color, double width, Qt::PenStyle style) const {
    painter.setPen(QPen(color, width, style));
}

void TimelinePanel::apply_brush_if_needed(QPainter& painter, const QColor& color) const {
    painter.setBrush(QBrush(color));
}

void TimelinePanel::apply_font_if_needed(QPainter& painter, const QFont& font) const {
    painter.setFont(font);
}

void TimelinePanel::reset_paint_state_cache() const {
    // Simple implementation for Phase 1
    // No paint state cache to reset yet
}

// ======= Phase 2 Additional Functions =======

void TimelinePanel::invalidate_track(size_t track_index) {
    if (!timeline_ || track_index >= timeline_->tracks().size()) return;
    
    // Calculate track bounds
    int track_y = TIMECODE_HEIGHT + static_cast<int>(track_index) * (TRACK_HEIGHT + TRACK_SPACING);
    QRect track_rect(0, track_y, width(), TRACK_HEIGHT);
    
    invalidate_region(track_rect, true);
}

void TimelinePanel::invalidate_segment(uint32_t segment_id) {
    if (!timeline_) return;
    
    // Find segment and calculate its bounds
    for (size_t track_idx = 0; track_idx < timeline_->tracks().size(); ++track_idx) {
        const auto& track = *timeline_->tracks()[track_idx];
        const auto& segments = track.segments();
        
        for (const auto& segment : segments) {
            if (segment.id == segment_id) {
                int start_x = time_to_pixel(segment.start_time);
                int end_x = time_to_pixel(segment.end_time());
                int track_y = TIMECODE_HEIGHT + static_cast<int>(track_idx) * (TRACK_HEIGHT + TRACK_SPACING);
                
                QRect segment_rect(start_x - 2, track_y, end_x - start_x + 4, TRACK_HEIGHT);
                invalidate_region(segment_rect, false);
                return;
            }
        }
    }
}

bool TimelinePanel::is_region_dirty(const QRect& rect) const {
    if (!has_dirty_regions_) return false;
    
    // Quick bounds check first
    if (!total_dirty_rect_.intersects(rect)) return false;
    
    // Check individual dirty regions
    for (const auto& dirty : dirty_regions_) {
        if (dirty.rect.intersects(rect)) return true;
    }
    
    return false;
}

// ======= Phase 3 Advanced Optimizations Implementation =======

void TimelinePanel::update_timeline_data_cache() const {
    if (!timeline_) return;
    
    auto now = std::chrono::steady_clock::now();
    uint64_t current_version = 1; // TODO: Get actual timeline version
    
    // Check if cache needs updating
    if (timeline_data_cache_.timeline_version == current_version &&
        !timeline_data_cache_.cached_tracks.empty()) {
        return; // Cache is valid
    }
    
    // Prevent concurrent updates
    if (timeline_data_cache_.is_updating) return;
    timeline_data_cache_.is_updating = true;
    
    timeline_data_cache_.cached_tracks.clear();
    timeline_data_cache_.cached_tracks.reserve(timeline_->tracks().size());
    
    const auto& tracks = timeline_->tracks();
    for (size_t track_idx = 0; track_idx < tracks.size(); ++track_idx) {
        const auto& track = *tracks[track_idx];
        
        CachedTrackData cached_track;
        cached_track.version = current_version;
        cached_track.zoom_level = zoom_factor_;
        cached_track.scroll_x = scroll_x_;
        cached_track.last_update = now;
        
        // Calculate track bounds
        int track_y = TIMECODE_HEIGHT + static_cast<int>(track_idx) * (TRACK_HEIGHT + TRACK_SPACING);
        cached_track.bounds = QRect(0, track_y, width(), TRACK_HEIGHT);
        
        // Cache visible segments for this track
        const auto& segments = track.segments();
        cached_track.visible_segments.reserve(segments.size());
        
        for (const auto& segment : segments) {
            int start_x = time_to_pixel(segment.start_time);
            int end_x = time_to_pixel(segment.end_time());
            
            // Only cache segments that might be visible
            if (end_x >= scroll_x_ && start_x <= scroll_x_ + width()) {
                cached_track.visible_segments.push_back(&segment);
            }
        }
        
        timeline_data_cache_.cached_tracks.push_back(std::move(cached_track));
    }
    
    timeline_data_cache_.timeline_version = current_version;
    timeline_data_cache_.last_full_update = now;
    timeline_data_cache_.is_updating = false;
}

const TimelinePanel::CachedTrackData* TimelinePanel::get_cached_track_data(size_t track_index) const {
    update_timeline_data_cache();
    
    if (track_index >= timeline_data_cache_.cached_tracks.size()) {
        return nullptr;
    }
    
    const auto& cached_track = timeline_data_cache_.cached_tracks[track_index];
    
    // Validate cache entry
    if (!cached_track.is_valid(timeline_data_cache_.timeline_version, zoom_factor_, scroll_x_)) {
        return nullptr;
    }
    
    return &cached_track;
}

void TimelinePanel::invalidate_background_cache() {
    background_cache_valid_ = false;
    cached_background_zoom_ = -1.0;
    cached_background_scroll_ = -1;
    
    // Phase 4: Record cache invalidation
    performance_analytics_.record_cache_miss("background");
}

void TimelinePanel::invalidate_timecode_cache() {
    timecode_cache_valid_ = false;
    
    // Phase 4: Record cache invalidation
    performance_analytics_.record_cache_miss("timecode");
}

void TimelinePanel::invalidate_segment_cache(uint32_t segment_id) {
    auto it = segment_pixmap_cache_.find(segment_id);
    if (it != segment_pixmap_cache_.end()) {
        segment_pixmap_cache_.erase(it);
        
        // Phase 4: Record cache invalidation
        performance_analytics_.record_cache_miss("segment");
    }
}

void TimelinePanel::ProgressiveRenderer::start_progressive_render(const QRect& region) {
    render_region = region;
    current_pass = RenderPass::BACKGROUND;
    is_active = true;
    pass_start_time = std::chrono::steady_clock::now();
    
    // Define pass order for complex timelines
    remaining_passes = {
        RenderPass::BACKGROUND,
        RenderPass::TIMECODE,
        RenderPass::TRACK_STRUCTURE,
        RenderPass::SEGMENTS_BASIC,
        RenderPass::SEGMENTS_DETAILED,
        RenderPass::WAVEFORMS,
        RenderPass::OVERLAYS
    };
}

bool TimelinePanel::ProgressiveRenderer::advance_to_next_pass() {
    if (remaining_passes.empty()) {
        is_active = false;
        return false;
    }
    
    remaining_passes.erase(remaining_passes.begin());
    if (!remaining_passes.empty()) {
        current_pass = remaining_passes[0];
        pass_start_time = std::chrono::steady_clock::now();
        return true;
    }
    
    is_active = false;
    return false;
}

bool TimelinePanel::ProgressiveRenderer::is_render_complete() const {
    return !is_active && remaining_passes.empty();
}

void TimelinePanel::ProgressiveRenderer::reset() {
    is_active = false;
    remaining_passes.clear();
    current_pass = RenderPass::BACKGROUND;
}

bool TimelinePanel::render_next_progressive_pass(QPainter& painter) {
    if (!progressive_renderer_.is_active) return false;
    
    auto pass_start = std::chrono::high_resolution_clock::now();
    constexpr int MAX_PASS_TIME_MS = 8; // 8ms per pass for 120fps capability
    
    switch (progressive_renderer_.current_pass) {
        case RenderPass::BACKGROUND:
            if (!background_cache_valid_ || 
                std::abs(cached_background_zoom_ - zoom_factor_) > 0.001 ||
                cached_background_scroll_ != scroll_x_) {
                
                background_cache_ = QPixmap(size());
                QPainter cache_painter(&background_cache_);
                draw_background(cache_painter);
                
                background_cache_valid_ = true;
                cached_background_zoom_ = zoom_factor_;
                cached_background_scroll_ = scroll_x_;
            }
            painter.drawPixmap(0, 0, background_cache_);
            break;
            
        case RenderPass::TIMECODE:
            if (!timecode_cache_valid_) {
                timecode_cache_ = QPixmap(width(), TIMECODE_HEIGHT);
                QPainter cache_painter(&timecode_cache_);
                draw_timecode_ruler(cache_painter);
                timecode_cache_valid_ = true;
            }
            painter.drawPixmap(0, 0, timecode_cache_);
            break;
            
        case RenderPass::TRACK_STRUCTURE:
            // Draw basic track structure quickly
            painter.setPen(QColor(70, 70, 70));
            for (size_t i = 0; timeline_ && i < timeline_->tracks().size(); ++i) {
                int track_y = TIMECODE_HEIGHT + static_cast<int>(i) * (TRACK_HEIGHT + TRACK_SPACING);
                painter.drawLine(0, track_y, width(), track_y);
            }
            break;
            
        case RenderPass::SEGMENTS_BASIC:
            // Draw basic segment rectangles only
            if (timeline_) {
                for (size_t track_idx = 0; track_idx < timeline_->tracks().size(); ++track_idx) {
                    const auto* cached_track = get_cached_track_data(track_idx);
                    if (!cached_track) continue;
                    
                    const auto& track = *timeline_->tracks()[track_idx];
                    QColor track_color = (track.type() == ve::timeline::Track::Video) ? 
                                        QColor(80, 120, 180) : QColor(120, 180, 80);
                    
                    painter.fillRect(cached_track->bounds, track_color.darker(150));
                }
            }
            break;
            
        case RenderPass::SEGMENTS_DETAILED:
        case RenderPass::WAVEFORMS:
        case RenderPass::OVERLAYS:
            // These passes would implement detailed rendering
            // For now, just complete quickly
            break;
    }
    
    auto pass_end = std::chrono::high_resolution_clock::now();
    auto pass_duration = std::chrono::duration_cast<std::chrono::milliseconds>(pass_end - pass_start);
    
    // Move to next pass
    bool has_more = progressive_renderer_.advance_to_next_pass();
    
    // If we took too much time, yield to next frame
    if (pass_duration.count() > MAX_PASS_TIME_MS) {
        return has_more;
    }
    
    return has_more;
}

// ======= Phase 4: Memory & Threading Optimizations Implementation =======

void TimelinePanel::initialize_phase4_optimizations() {
    // Initialize paint object pools
    paint_object_pool_.initialize_pools();
    
    // Initialize memory containers
    memory_optimizations_.reserve_containers(1000); // Initial capacity for 1000 segments
    
    // Reset performance analytics
    performance_analytics_.reset_statistics();
    
    // Initialize advanced paint state cache
    advanced_paint_state_.reset_state_cache();
    
    ve::log::info("Phase 4 optimizations initialized successfully");
}

// Phase 4.1: Paint Object Memory Pool Implementation
void TimelinePanel::PaintObjectPool::initialize_pools() {
    // Pre-allocate common objects to avoid allocations during paint
    color_pool_.reserve(100);
    pen_pool_.reserve(50);
    brush_pool_.reserve(50);
    font_pool_.reserve(10);
    rect_pool_.reserve(200);
    
    // Pre-populate with common colors
    color_pool_.emplace_back(70, 130, 180);    // Steel blue (video)
    color_pool_.emplace_back(100, 149, 237);   // Cornflower blue (audio)
    color_pool_.emplace_back(255, 215, 0);     // Gold (selected)
    color_pool_.emplace_back(255, 255, 255);   // White (text)
    color_pool_.emplace_back(45, 45, 45);      // Dark gray (background)
    color_pool_.emplace_back(255, 255, 255, 100); // Semi-transparent white
    color_pool_.emplace_back(255, 255, 255, 30);  // Very transparent white
    
    // Pre-populate with common pens
    pen_pool_.emplace_back(QColor(255, 255, 255, 100), 1);
    pen_pool_.emplace_back(QColor(255, 255, 255, 30), 1);
    pen_pool_.emplace_back(QColor(255, 215, 0), 2); // Selection border
    
    // Pre-populate with common brushes
    brush_pool_.emplace_back(QColor(70, 130, 180));
    brush_pool_.emplace_back(QColor(100, 149, 237));
    brush_pool_.emplace_back(QColor(255, 215, 0));
    
    // Pre-populate with common fonts
    font_pool_.emplace_back("Arial", 9);
    font_pool_.emplace_back("Arial", 7);
    font_pool_.emplace_back("Arial", 10, QFont::Bold);
}

QColor* TimelinePanel::PaintObjectPool::get_color(int r, int g, int b, int a) {
    // Try to find existing color in pool
    for (size_t i = 0; i < color_pool_.size(); ++i) {
        const QColor& color = color_pool_[i];
        if (color.red() == r && color.green() == g && color.blue() == b && color.alpha() == a) {
            total_allocations_saved_++;
            return &color_pool_[i];
        }
    }
    
    // Add new color to pool if not found
    if (color_pool_.size() < 200) { // Limit pool size
        color_pool_.emplace_back(r, g, b, a);
        max_colors_used_ = std::max(max_colors_used_, color_pool_.size());
        return &color_pool_.back();
    }
    
    // Pool is full, use round-robin
    color_index_ = (color_index_ + 1) % color_pool_.size();
    color_pool_[color_index_] = QColor(r, g, b, a);
    return &color_pool_[color_index_];
}

QPen* TimelinePanel::PaintObjectPool::get_pen(const QColor& color, qreal width, Qt::PenStyle style) {
    // Try to find existing pen in pool
    for (size_t i = 0; i < pen_pool_.size(); ++i) {
        const QPen& pen = pen_pool_[i];
        if (pen.color() == color && std::abs(pen.widthF() - width) < 0.001 && pen.style() == style) {
            total_allocations_saved_++;
            return &pen_pool_[i];
        }
    }
    
    // Add new pen to pool if not found
    if (pen_pool_.size() < 100) { // Limit pool size
        pen_pool_.emplace_back(color, width);
        pen_pool_.back().setStyle(style);
        max_pens_used_ = std::max(max_pens_used_, pen_pool_.size());
        return &pen_pool_.back();
    }
    
    // Pool is full, use round-robin
    pen_index_ = (pen_index_ + 1) % pen_pool_.size();
    pen_pool_[pen_index_] = QPen(color, width);
    pen_pool_[pen_index_].setStyle(style);
    return &pen_pool_[pen_index_];
}

QBrush* TimelinePanel::PaintObjectPool::get_brush(const QColor& color) {
    // Try to find existing brush in pool
    for (size_t i = 0; i < brush_pool_.size(); ++i) {
        const QBrush& brush = brush_pool_[i];
        if (brush.color() == color && brush.style() == Qt::SolidPattern) {
            total_allocations_saved_++;
            return &brush_pool_[i];
        }
    }
    
    // Add new brush to pool if not found
    if (brush_pool_.size() < 100) { // Limit pool size
        brush_pool_.emplace_back(color);
        max_brushes_used_ = std::max(max_brushes_used_, brush_pool_.size());
        return &brush_pool_.back();
    }
    
    // Pool is full, use round-robin
    brush_index_ = (brush_index_ + 1) % brush_pool_.size();
    brush_pool_[brush_index_] = QBrush(color);
    return &brush_pool_[brush_index_];
}

void TimelinePanel::PaintObjectPool::reset_pools() {
    color_index_ = pen_index_ = brush_index_ = font_index_ = rect_index_ = 0;
    // Don't clear the pools, just reset indices for reuse
}

void TimelinePanel::PaintObjectPool::print_pool_statistics() const {
    ve::log::info("Paint Object Pool Statistics:");
    ve::log::info("  Colors in pool: " + std::to_string(color_pool_.size()) + ", max used: " + std::to_string(max_colors_used_));
    ve::log::info("  Pens in pool: " + std::to_string(pen_pool_.size()) + ", max used: " + std::to_string(max_pens_used_));
    ve::log::info("  Brushes in pool: " + std::to_string(brush_pool_.size()) + ", max used: " + std::to_string(max_brushes_used_));
    ve::log::info("  Total allocations saved: " + std::to_string(total_allocations_saved_));
}

// Phase 4.2: Performance Analytics Implementation
void TimelinePanel::PerformanceAnalytics::reset_statistics() {
    background_cache_hits_ = background_cache_misses_ = 0;
    timecode_cache_hits_ = timecode_cache_misses_ = 0;
    segment_cache_hits_ = segment_cache_misses_ = 0;
    timeline_data_cache_hits_ = timeline_data_cache_misses_ = 0;
    
    total_paint_time_ = background_paint_time_ = timecode_paint_time_ = segments_paint_time_ = std::chrono::microseconds{0};
    paint_event_count_ = 0;
    
    peak_memory_usage_ = current_cache_memory_ = 0;
    memory_allocations_saved_ = 0;
    
    progressive_renders_started_ = progressive_renders_completed_ = 0;
    avg_pass_time_ = std::chrono::microseconds{0};
}

void TimelinePanel::PerformanceAnalytics::record_cache_hit(const std::string& cache_type) {
    if (cache_type == "background") background_cache_hits_++;
    else if (cache_type == "timecode") timecode_cache_hits_++;
    else if (cache_type == "segment") segment_cache_hits_++;
    else if (cache_type == "timeline_data") timeline_data_cache_hits_++;
}

void TimelinePanel::PerformanceAnalytics::record_cache_miss(const std::string& cache_type) {
    if (cache_type == "background") background_cache_misses_++;
    else if (cache_type == "timecode") timecode_cache_misses_++;
    else if (cache_type == "segment") segment_cache_misses_++;
    else if (cache_type == "timeline_data") timeline_data_cache_misses_++;
}

void TimelinePanel::PerformanceAnalytics::record_paint_time(const std::string& phase, std::chrono::microseconds time) {
    if (phase == "total") {
        total_paint_time_ += time;
        paint_event_count_++;
    } else if (phase == "background") {
        background_paint_time_ += time;
    } else if (phase == "timecode") {
        timecode_paint_time_ += time;
    } else if (phase == "segments") {
        segments_paint_time_ += time;
    }
}

void TimelinePanel::PerformanceAnalytics::record_memory_saved(size_t bytes) {
    memory_allocations_saved_++;
    // Estimate saved memory
}

double TimelinePanel::PerformanceAnalytics::get_overall_cache_hit_rate() const {
    int total_hits = background_cache_hits_ + timecode_cache_hits_ + segment_cache_hits_ + timeline_data_cache_hits_;
    int total_misses = background_cache_misses_ + timecode_cache_misses_ + segment_cache_misses_ + timeline_data_cache_misses_;
    int total_accesses = total_hits + total_misses;
    
    if (total_accesses == 0) return 0.0;
    return static_cast<double>(total_hits) / total_accesses * 100.0;
}

void TimelinePanel::PerformanceAnalytics::print_analytics() const {
    ve::log::info("=== Timeline Performance Analytics ===");
    
    // Cache hit rates
    ve::log::info("Cache Hit Rates:");
    if (background_cache_hits_ + background_cache_misses_ > 0) {
        double bg_rate = static_cast<double>(background_cache_hits_) / (background_cache_hits_ + background_cache_misses_) * 100.0;
        ve::log::info("  Background: " + std::to_string(bg_rate) + "% (" + std::to_string(background_cache_hits_) + "/" + std::to_string(background_cache_hits_ + background_cache_misses_) + " hits)");
    }
    
    if (timecode_cache_hits_ + timecode_cache_misses_ > 0) {
        double tc_rate = static_cast<double>(timecode_cache_hits_) / (timecode_cache_hits_ + timecode_cache_misses_) * 100.0;
        ve::log::info("  Timecode: " + std::to_string(tc_rate) + "% (" + std::to_string(timecode_cache_hits_) + "/" + std::to_string(timecode_cache_hits_ + timecode_cache_misses_) + " hits)");
    }
    
    if (segment_cache_hits_ + segment_cache_misses_ > 0) {
        double seg_rate = static_cast<double>(segment_cache_hits_) / (segment_cache_hits_ + segment_cache_misses_) * 100.0;
        ve::log::info("  Segments: " + std::to_string(seg_rate) + "% (" + std::to_string(segment_cache_hits_) + "/" + std::to_string(segment_cache_hits_ + segment_cache_misses_) + " hits)");
    }
    
    ve::log::info("  Overall: " + std::to_string(get_overall_cache_hit_rate()) + "%");
    
    // Paint performance
    if (paint_event_count_ > 0) {
        auto avg_paint = total_paint_time_.count() / paint_event_count_;
        ve::log::info("Paint Performance:");
        ve::log::info("  Average paint time: " + std::to_string(avg_paint) + " μs");
        ve::log::info("  Total paint events: " + std::to_string(paint_event_count_));
        
        if (background_paint_time_.count() > 0) {
            ve::log::info("  Background phase: " + std::to_string(background_paint_time_.count() / paint_event_count_) + " μs avg");
        }
        if (segments_paint_time_.count() > 0) {
            ve::log::info("  Segments phase: " + std::to_string(segments_paint_time_.count() / paint_event_count_) + " μs avg");
        }
    }
    
    // Memory optimizations
    ve::log::info("Memory Optimizations:");
    ve::log::info("  Allocations saved: " + std::to_string(memory_allocations_saved_));
    ve::log::info("  Current cache memory: " + std::to_string(current_cache_memory_ / 1024) + " KB");
    
    // Progressive rendering
    if (progressive_renders_started_ > 0) {
        ve::log::info("Progressive Rendering:");
        ve::log::info("  Renders started: " + std::to_string(progressive_renders_started_));
        ve::log::info("  Renders completed: " + std::to_string(progressive_renders_completed_));
        if (progressive_renders_completed_ > 0) {
            ve::log::info("  Average pass time: " + std::to_string(avg_pass_time_.count() / progressive_renders_completed_) + " μs");
        }
    }
}

// Phase 4.3: Memory Container Optimizations Implementation
void TimelinePanel::MemoryOptimizations::reserve_containers(size_t segment_count) {
    visible_segments_buffer_.reserve(segment_count);
    segment_rects_buffer_.reserve(segment_count);
    segment_names_buffer_.reserve(segment_count);
    segment_colors_buffer_.reserve(segment_count);
    segment_batches_.reserve(segment_count / 4); // Estimate 4 segments per batch
}

void TimelinePanel::MemoryOptimizations::clear_containers() {
    visible_segments_buffer_.clear();
    segment_rects_buffer_.clear();
    segment_names_buffer_.clear();
    segment_colors_buffer_.clear();
    segment_batches_.clear();
    // Don't clear string pool - it's beneficial to keep cached strings
}

const QString& TimelinePanel::MemoryOptimizations::get_cached_string(const std::string& str) {
    auto it = string_pool_.find(str);
    if (it != string_pool_.end()) {
        string_pool_hits_++;
        return it->second;
    }
    
    // Add new string to pool
    string_pool_misses_++;
    auto result = string_pool_.emplace(str, QString::fromStdString(str));
    return result.first->second;
}

void TimelinePanel::MemoryOptimizations::batch_segments_by_color(const std::vector<const ve::timeline::Segment*>& segments) {
    segment_batches_.clear();
    
    // Group segments by color for batched rendering
    std::unordered_map<uint32_t, size_t> color_to_batch;
    
    for (const auto* segment : segments) {
        // Determine segment color (simplified)
        QColor segment_color(70, 130, 180); // Default video color
        uint32_t color_key = segment_color.rgb();
        
        auto it = color_to_batch.find(color_key);
        if (it == color_to_batch.end()) {
            // Create new batch
            segment_batches_.emplace_back();
            auto& batch = segment_batches_.back();
            batch.color = segment_color;
            color_to_batch[color_key] = segment_batches_.size() - 1;
            it = color_to_batch.find(color_key);
        }
        
        // Add segment to existing batch
        size_t batch_index = it->second;
        auto& batch = segment_batches_[batch_index];
        
        // Calculate segment rect (simplified)
        QRect segment_rect(0, 0, 100, 60); // Placeholder
        batch.rects.push_back(segment_rect);
        
        // Add cached string name
        std::string segment_name = "Segment"; // Placeholder
        batch.names.push_back(get_cached_string(segment_name));
    }
}

void TimelinePanel::MemoryOptimizations::print_memory_stats() const {
    ve::log::info("Memory Container Optimizations:");
    ve::log::info("  String pool size: " + std::to_string(string_pool_.size()));
    ve::log::info("  String pool hits: " + std::to_string(string_pool_hits_) + ", misses: " + std::to_string(string_pool_misses_));
    if (string_pool_hits_ + string_pool_misses_ > 0) {
        double hit_rate = static_cast<double>(string_pool_hits_) / (string_pool_hits_ + string_pool_misses_) * 100.0;
        ve::log::info("  String pool hit rate: " + std::to_string(hit_rate) + "%");
    }
    ve::log::info("  Segment batches created: " + std::to_string(segment_batches_.size()));
}

// Phase 4.4: Advanced Paint State Management Implementation
void TimelinePanel::AdvancedPaintStateCache::cache_current_state(QPainter& painter) {
    cached_state_.pen_color = painter.pen().color();
    cached_state_.brush_color = painter.brush().color();
    cached_state_.pen_width = painter.pen().widthF();
    cached_state_.pen_style = painter.pen().style();
    cached_state_.font = painter.font();
    cached_state_.transform = painter.transform();
    cached_state_.composition_mode = painter.compositionMode();
    cached_state_.antialiasing_enabled = painter.renderHints() & QPainter::Antialiasing;
    state_is_cached_ = true;
}

bool TimelinePanel::AdvancedPaintStateCache::apply_pen_optimized(QPainter& painter, const QColor& color, qreal width, Qt::PenStyle style) {
    if (state_is_cached_ && 
        cached_state_.pen_color == color && 
        std::abs(cached_state_.pen_width - width) < 0.001 && 
        cached_state_.pen_style == style) {
        pen_changes_avoided_++;
        total_state_changes_avoided_++;
        return false; // No change needed
    }
    
    painter.setPen(QPen(color, width, style));
    if (state_is_cached_) {
        cached_state_.pen_color = color;
        cached_state_.pen_width = width;
        cached_state_.pen_style = style;
    }
    return true; // State was changed
}

bool TimelinePanel::AdvancedPaintStateCache::apply_brush_optimized(QPainter& painter, const QColor& color) {
    if (state_is_cached_ && cached_state_.brush_color == color) {
        brush_changes_avoided_++;
        total_state_changes_avoided_++;
        return false; // No change needed
    }
    
    painter.setBrush(QBrush(color));
    if (state_is_cached_) {
        cached_state_.brush_color = color;
    }
    return true; // State was changed
}

bool TimelinePanel::AdvancedPaintStateCache::apply_font_optimized(QPainter& painter, const QFont& font) {
    if (state_is_cached_ && cached_state_.font == font) {
        font_changes_avoided_++;
        total_state_changes_avoided_++;
        return false; // No change needed
    }
    
    painter.setFont(font);
    if (state_is_cached_) {
        cached_state_.font = font;
    }
    return true; // State was changed
}

void TimelinePanel::AdvancedPaintStateCache::reset_state_cache() {
    state_is_cached_ = false;
    pen_changes_avoided_ = brush_changes_avoided_ = font_changes_avoided_ = 0;
    transform_changes_avoided_ = total_state_changes_avoided_ = 0;
}

void TimelinePanel::AdvancedPaintStateCache::print_state_optimization_stats() const {
    ve::log::info("Advanced Paint State Optimizations:");
    ve::log::info("  Pen changes avoided: " + std::to_string(pen_changes_avoided_));
    ve::log::info("  Brush changes avoided: " + std::to_string(brush_changes_avoided_));
    ve::log::info("  Font changes avoided: " + std::to_string(font_changes_avoided_));
    ve::log::info("  Transform changes avoided: " + std::to_string(transform_changes_avoided_));
    ve::log::info("  Total state changes avoided: " + std::to_string(total_state_changes_avoided_));
}

// Phase 4 Main Implementation Functions
void TimelinePanel::update_memory_containers_for_paint() const {
    if (!timeline_) return;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Clear containers for reuse
    memory_optimizations_.clear_containers();
    
    // Estimate total segments for efficient allocation
    size_t total_segments = 0;
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        total_segments += track->segments().size();
    }
    
    // Reserve containers based on estimated visible segments (viewport culling)
    size_t estimated_visible = std::min(total_segments, static_cast<size_t>(width() / 20)); // Rough estimate
    memory_optimizations_.reserve_containers(estimated_visible);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    performance_analytics_.record_paint_time("memory_containers", duration);
}

void TimelinePanel::batch_similar_segments(const std::vector<const ve::timeline::Segment*>& segments) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Use memory optimizations to batch segments by color
    memory_optimizations_.batch_segments_by_color(segments);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    performance_analytics_.record_paint_time("segment_batching", duration);
}

void TimelinePanel::draw_segment_batch_optimized(QPainter& painter, const MemoryOptimizations::SegmentBatch& batch, int track_y) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Set paint state once for entire batch
    QColor* batch_color = paint_object_pool_.get_color(batch.color.red(), batch.color.green(), batch.color.blue());
    QPen* batch_pen = paint_object_pool_.get_pen(*batch_color, 1.0);
    QBrush* batch_brush = paint_object_pool_.get_brush(*batch_color);
    
    // Apply optimized state changes
    advanced_paint_state_.apply_pen_optimized(painter, *batch_color);
    advanced_paint_state_.apply_brush_optimized(painter, *batch_color);
    
    // Draw all segments in batch with minimal state changes
    for (size_t i = 0; i < batch.rects.size(); ++i) {
        const QRect& rect = batch.rects[i];
        
        // Draw segment rectangle
        painter.fillRect(rect, *batch_brush);
        painter.drawRect(rect);
        
        // Draw segment name if there's space
        if (rect.width() > 30 && i < batch.names.size()) {
            QRect text_rect = rect.adjusted(2, 2, -2, -2);
            painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, batch.names[i]);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    performance_analytics_.record_paint_time("batch_drawing", duration);
    performance_analytics_.record_memory_saved(batch.rects.size() * sizeof(QRect)); // Rough estimate
}

void TimelinePanel::record_performance_metrics(const std::string& operation, std::chrono::microseconds duration) const {
    performance_analytics_.record_paint_time(operation, duration);
}

void TimelinePanel::cleanup_phase4_resources() const {
    // Reset object pools for next paint cycle
    paint_object_pool_.reset_pools();
    
    // Clear memory containers
    memory_optimizations_.clear_containers();
    
    // Don't reset performance analytics - they accumulate over time
}

// Enhanced Waveform Integration Helper Functions

void TimelinePanel::draw_placeholder_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment) {
    // Enhanced placeholder waveform with realistic patterns (fallback for when real generation fails)
    painter.setPen(QPen(QColor(120, 180, 255, 120), 1));
    
    int center_y = rect.center().y();
    int max_height = rect.height() * 0.6;
    
    // Create deterministic pattern based on segment properties
    int base_seed = static_cast<int>(segment.id * 13 + segment.start_time.to_rational().num / 1000);
    
    // Generate realistic waveform pattern
    for (int x = rect.left(); x < rect.right(); x += 4) {
        int sample_index = (x - rect.left()) / 4;
        int sample_seed = base_seed + sample_index;
        
        // Multiple frequency components for realism
        float low_freq = std::sin(sample_seed * 0.05f) * 0.5f;
        float mid_freq = std::sin(sample_seed * 0.12f) * 0.3f;
        float high_freq = std::sin(sample_seed * 0.25f) * 0.15f;
        
        float amplitude = low_freq + mid_freq + high_freq;
        amplitude *= (0.7f + (sample_seed % 50) / 100.0f);
        
        int sample_height = static_cast<int>(std::abs(amplitude) * max_height);
        sample_height = std::max(2, std::min(sample_height, max_height));
        
        if (amplitude >= 0) {
            painter.drawLine(x, center_y, x, center_y - sample_height);
        } else {
            painter.drawLine(x, center_y, x, center_y + sample_height);
        }
    }
    
    // Add subtle center reference line
    if (rect.width() > 80) {
        painter.setPen(QPen(QColor(120, 180, 255, 30), 1));
        painter.drawLine(rect.left(), center_y, rect.right(), center_y);
    }
}

// ============================================================================
// Advanced Timeline Features (Phase 2) Implementation
// ============================================================================

ve::TimePoint TimelinePanel::snap_to_grid(ve::TimePoint time) const {
    if (!snap_enabled_) return time;
    
    // Calculate the nearest grid point
    int64_t grid_ticks = grid_size_.to_rational().num;
    int32_t grid_den = grid_size_.to_rational().den;
    
    // Convert time to same denominator as grid
    int64_t time_ticks = (time.to_rational().num * grid_den) / time.to_rational().den;
    
    // Snap to nearest grid line
    int64_t snapped_ticks = ((time_ticks + grid_ticks / 2) / grid_ticks) * grid_ticks;
    
    return ve::TimePoint{snapped_ticks, grid_den};
}

ve::TimePoint TimelinePanel::snap_to_segments(ve::TimePoint time, ve::timeline::SegmentId exclude_id) const {
    if (!snap_enabled_ || !timeline_) return time;
    
    const double snap_threshold_pixels = 10.0; // 10 pixel snap threshold
    ve::TimeDuration snap_threshold = pixel_to_time_delta(snap_threshold_pixels);
    
    ve::TimePoint closest_snap = time;
    ve::TimeDuration min_distance = snap_threshold;
    
    // Check all segments for snap points (start and end)
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        for (const auto& segment : track->segments()) {
            if (segment.id == exclude_id) continue;
            
            // Check segment start
            ve::TimeDuration distance_to_start = abs_time_difference(time, segment.start_time);
            if (distance_to_start < min_distance) {
                closest_snap = segment.start_time;
                min_distance = distance_to_start;
            }
            
            // Check segment end
            ve::TimePoint segment_end = ve::TimePoint{
                segment.start_time.to_rational().num + segment.duration.to_rational().num,
                segment.start_time.to_rational().den
            };
            ve::TimeDuration distance_to_end = abs_time_difference(time, segment_end);
            if (distance_to_end < min_distance) {
                closest_snap = segment_end;
                min_distance = distance_to_end;
            }
        }
    }
    
    return closest_snap;
}

void TimelinePanel::draw_snap_guides(QPainter& painter) const {
    if (!snap_enabled_ || snap_points_.empty()) return;
    
    painter.save();
    painter.setPen(QPen(snap_guide_color_, 2, Qt::DashLine));
    
    for (const auto& snap_point : snap_points_) {
        int x = time_to_pixel(snap_point);
        painter.drawLine(x, TIMECODE_HEIGHT, x, height());
    }
    
    painter.restore();
}

bool TimelinePanel::is_on_segment_edge(const QPoint& pos, const ve::timeline::Segment& segment, bool& is_left_edge) const {
    if (!timeline_) return false;
    
    // Find which track this segment belongs to
    int track_y = -1;
    int track_index = 0;
    const auto& tracks = timeline_->tracks();
    for (const auto& track : tracks) {
        if (track->find_segment(segment.id)) {
            track_y = TIMECODE_HEIGHT + track_index * TRACK_HEIGHT;
            break;
        }
        track_index++;
    }
    
    if (track_y == -1) return false;
    
    // Check if mouse is within the track vertically
    if (pos.y() < track_y || pos.y() > track_y + TRACK_HEIGHT) return false;
    
    // Calculate segment bounds
    int segment_left = time_to_pixel(segment.start_time);
    ve::TimePoint segment_end = ve::TimePoint{
        segment.start_time.to_rational().num + segment.duration.to_rational().num,
        segment.start_time.to_rational().den
    };
    int segment_right = time_to_pixel(segment_end);
    
    const int edge_threshold = 8; // 8 pixels for edge detection
    
    // Check left edge
    if (abs(pos.x() - segment_left) <= edge_threshold) {
        is_left_edge = true;
        return true;
    }
    
    // Check right edge
    if (abs(pos.x() - segment_right) <= edge_threshold) {
        is_left_edge = false;
        return true;
    }
    
    return false;
}

void TimelinePanel::draw_segment_resize_handles(QPainter& painter, const ve::timeline::Segment& segment, int track_y) const {
    painter.save();
    
    int segment_left = time_to_pixel(segment.start_time);
    ve::TimePoint segment_end = ve::TimePoint{
        segment.start_time.to_rational().num + segment.duration.to_rational().num,
        segment.start_time.to_rational().den
    };
    int segment_right = time_to_pixel(segment_end);
    
    // Draw resize handles as small rectangles
    const int handle_width = 6;
    const int handle_height = TRACK_HEIGHT - 4;
    
    painter.setPen(QPen(QColor(255, 255, 255, 200), 1));
    painter.setBrush(QBrush(QColor(100, 100, 100, 150)));
    
    // Left handle
    QRect left_handle(segment_left - handle_width/2, track_y + 2, handle_width, handle_height);
    painter.drawRect(left_handle);
    
    // Right handle
    QRect right_handle(segment_right - handle_width/2, track_y + 2, handle_width, handle_height);
    painter.drawRect(right_handle);
    
    painter.restore();
}

void TimelinePanel::select_segments_in_range(ve::TimePoint start, ve::TimePoint end, int track_index) {
    if (!timeline_) return;
    
    // Ensure start <= end
    if (start > end) std::swap(start, end);
    
    const auto& tracks = timeline_->tracks();
    
    // If track_index is -1, select from all tracks
    if (track_index == -1) {
        for (const auto& track : tracks) {
            for (const auto& segment : track->segments()) {
                ve::TimePoint segment_end = ve::TimePoint{
                    segment.start_time.to_rational().num + segment.duration.to_rational().num,
                    segment.start_time.to_rational().den
                };
                
                // Check if segment overlaps with selection range
                if (segment.start_time < end && segment_end > start) {
                    if (std::find(selected_segments_.begin(), selected_segments_.end(), segment.id) == selected_segments_.end()) {
                        selected_segments_.push_back(segment.id);
                    }
                }
            }
        }
    } else if (track_index < static_cast<int>(tracks.size())) {
        // Select from specific track
        const auto& track = tracks[track_index];
        for (const auto& segment : track->segments()) {
            ve::TimePoint segment_end = ve::TimePoint{
                segment.start_time.to_rational().num + segment.duration.to_rational().num,
                segment.start_time.to_rational().den
            };
            
            if (segment.start_time < end && segment_end > start) {
                if (std::find(selected_segments_.begin(), selected_segments_.end(), segment.id) == selected_segments_.end()) {
                    selected_segments_.push_back(segment.id);
                }
            }
        }
    }
    
    emit selection_changed();
    update();
}

void TimelinePanel::toggle_segment_selection(ve::timeline::SegmentId segment_id) {
    auto it = std::find(selected_segments_.begin(), selected_segments_.end(), segment_id);
    if (it != selected_segments_.end()) {
        selected_segments_.erase(it);
    } else {
        selected_segments_.push_back(segment_id);
    }
    
    emit selection_changed();
    update();
}

bool TimelinePanel::is_segment_selected(ve::timeline::SegmentId segment_id) const {
    return std::find(selected_segments_.begin(), selected_segments_.end(), segment_id) != selected_segments_.end();
}

void TimelinePanel::ripple_edit_segments(ve::TimePoint edit_point, ve::TimeDuration delta) {
    if (!timeline_ || !ripple_mode_) return;
    
    // TODO: Implement ripple editing logic
    // This would move all segments after edit_point by delta amount
    // Need to integrate with command system for undo/redo support
}

// Helper function for time difference calculation
ve::TimeDuration TimelinePanel::abs_time_difference(ve::TimePoint a, ve::TimePoint b) const {
    if (a > b) {
        return ve::TimeDuration{
            a.to_rational().num - b.to_rational().num,
            a.to_rational().den
        };
    } else {
        return ve::TimeDuration{
            b.to_rational().num - a.to_rational().num,
            b.to_rational().den
        };
    }
}

// Helper function to convert pixel distance to time
ve::TimeDuration TimelinePanel::pixel_to_time_delta(double pixels) const {
    double seconds_per_pixel = 1.0 / (zoom_factor_ * 100.0);
    double seconds = pixels * seconds_per_pixel;
    return ve::TimeDuration{static_cast<int64_t>(seconds * 1000), 1000}; // Convert to milliseconds
}

} // namespace ve::ui
