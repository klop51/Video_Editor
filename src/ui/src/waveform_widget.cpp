/**
 * @file waveform_widget.cpp
 * @brief Implementation of High-Performance Qt Waveform Widge    // Setup refresh timer for smooth animation
    refresh_timer_->setInterval(1000 / refresh_rate_); // 60fps default
    connect(refresh_timer_.get(), &QTimer::timeout, this, &QWaveformWidget::update_rendering);
    
    // Setup background update timer for waveform data
    update_timer_->setInterval(100); // 10Hz background updates
    connect(update_timer_.get(), &QTimer::timeout, this, &QWaveformWidget::update_waveform_data);
    
    // Start timers
    if (auto_refresh_) {
        refresh_timer_->start();
        update_timer_->start();eek 8 Qt Timeline UI Integration - Complete implementation of professional
 * waveform visualization widget with real-time rendering, mouse interaction,
 * and integration with Week 7 waveform generation system.
 */

#include "ui/waveform_widget.hpp"
#include "core/time.hpp"
#include "core/log.hpp"

// Week 7 waveform system integration
#include "audio/waveform_generator.h"
#include "audio/waveform_cache.h"

#include <QtWidgets/QApplication>
#include <QtGui/QPainterPath>
#include <QtCore/QThread>
#include <QtCore/QDebug>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ve::ui {

//=============================================================================
// WaveformViewport Implementation
//=============================================================================

float WaveformViewport::pixels_per_second() const {
    return 48000.0f / samples_per_pixel; // Assuming 48kHz sample rate
}

ve::TimePoint WaveformViewport::end_time() const {
    return start_time + duration;
}

bool WaveformViewport::contains_time(const ve::TimePoint& time) const {
    return time >= start_time && time <= end_time();
}

QRect WaveformViewport::time_to_rect(const ve::TimePoint& start, const ve::TimePoint& duration, int height) const {
    // Convert timeline coordinates to widget pixel coordinates
    auto start_ratio = static_cast<double>(start.to_rational().num - start_time.to_rational().num) / 
                      static_cast<double>(this->duration.to_rational().num);
    auto duration_ratio = static_cast<double>(duration.to_rational().num) / 
                         static_cast<double>(this->duration.to_rational().num);
    
    int x = static_cast<int>(start_ratio * 1000); // Assuming 1000px width for calculation
    int width = static_cast<int>(duration_ratio * 1000);
    
    return QRect(x, 0, width, height);
}

//=============================================================================
// WaveformInteraction Implementation
//=============================================================================

ve::TimePoint WaveformInteraction::pixel_to_time(int pixel_x, const WaveformViewport& viewport) const {
    double ratio = static_cast<double>(pixel_x) / 1000.0; // Assuming widget width
    auto offset_rational = ve::TimeRational{
        static_cast<int64_t>(ratio * viewport.duration.to_rational().num),
        viewport.duration.to_rational().den
    };
    return ve::TimePoint(viewport.start_time.to_rational().num + offset_rational.num, offset_rational.den);
}

int WaveformInteraction::time_to_pixel(const ve::TimePoint& time, const WaveformViewport& viewport) const {
    auto time_offset = time.to_rational().num - viewport.start_time.to_rational().num;
    double ratio = static_cast<double>(time_offset) / static_cast<double>(viewport.duration.to_rational().num);
    return static_cast<int>(ratio * 1000); // Assuming widget width
}

//=============================================================================
// QWaveformWidget Implementation
//=============================================================================

QWaveformWidget::QWaveformWidget(QWidget* parent)
    : QWidget(parent)
    , playhead_position_(0, 1)
    , selection_start_(0, 1)
    , selection_end_(0, 1)
    , refresh_timer_(std::make_unique<QTimer>(this))
    , update_timer_(std::make_unique<QTimer>(this))
{
    // Widget configuration
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // Initialize viewport with sensible defaults
    viewport_.start_time = ve::TimePoint(0, 1);
    viewport_.duration = ve::TimePoint(60, 1); // 60 seconds default view
    viewport_.samples_per_pixel = 1000.0f;
    
    // Setup refresh timer for smooth animation
    refresh_timer_->setInterval(1000 / refresh_rate_); // 60fps default
    connect(refresh_timer_.get(), &QTimer::timeout, this, &QWaveformWidget::update_rendering);
    
    // Setup background update timer for waveform data
    update_timer_->setInterval(100); // 10Hz background updates
    connect(update_timer_.get(), &QTimer::timeout, this, &QWaveformWidget::update_waveform_data);
    
    // Start timers
    if (auto_refresh_enabled_) {
        refresh_timer_->start();
        update_timer_->start();
    }
    
    // Initialize performance tracking
    performance_metrics_.last_frame_time = std::chrono::high_resolution_clock::now();
    
    is_initialized_ = true;
    
    ve::log_info("QWaveformWidget initialized successfully");
}

QWaveformWidget::~QWaveformWidget() {
    if (refresh_timer_) {
        refresh_timer_->stop();
    }
    if (update_timer_) {
        update_timer_->stop();
    }
    
    ve::log_info("QWaveformWidget destroyed");
}

//=============================================================================
// Audio Source Configuration
//=============================================================================

void QWaveformWidget::set_audio_source(const std::string& audio_file_path) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (audio_source_path_ != audio_file_path) {
        audio_source_path_ = audio_file_path;
        current_waveform_data_.reset();
        invalidate_paint_cache();
        
        if (!audio_file_path.empty()) {
            // Request new waveform data for the audio source
            request_waveform_data(viewport_.start_time, viewport_.duration);
        }
        
        update();
        ve::log_info("Audio source set to: {}", audio_file_path);
    }
}

void QWaveformWidget::set_waveform_generator(std::shared_ptr<ve::audio::WaveformGenerator> generator) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    waveform_generator_ = generator;
    ve::log_info("Waveform generator connected to widget");
}

void QWaveformWidget::set_waveform_cache(std::shared_ptr<ve::audio::WaveformCache> cache) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    waveform_cache_ = cache;
    ve::log_info("Waveform cache connected to widget");
}

//=============================================================================
// Timeline Integration
//=============================================================================

void QWaveformWidget::set_timeline_range(const ve::TimePoint& start, const ve::TimePoint& duration) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (viewport_.start_time != start || viewport_.duration != duration) {
        viewport_.start_time = start;
        viewport_.duration = duration;
        
        invalidate_paint_cache();
        request_waveform_data(start, duration);
        update();
        
        emit zoom_changed(viewport_.samples_per_pixel);
    }
}

void QWaveformWidget::set_playhead_position(const ve::TimePoint& position) {
    if (playhead_position_ != position) {
        playhead_position_ = position;
        
        // Only update the playhead region for efficiency
        if (viewport_.contains_time(position)) {
            int pixel_x = interaction_.time_to_pixel(position, viewport_);
            QRect update_rect(pixel_x - 2, 0, 4, height());
            update(update_rect);
        }
    }
}

void QWaveformWidget::set_selection_range(const ve::TimePoint& start, const ve::TimePoint& end) {
    if (selection_start_ != start || selection_end_ != end) {
        selection_start_ = start;
        selection_end_ = end;
        
        invalidate_paint_cache();
        update();
        
        emit selection_changed(start, end);
    }
}

//=============================================================================
// Zoom and Navigation
//=============================================================================

void QWaveformWidget::zoom_to_fit() {
    if (audio_source_path_.empty()) return;
    
    // Set viewport to show entire audio file
    // For now, assume reasonable default duration
    viewport_.start_time = ve::TimePoint(0, 1);
    viewport_.duration = ve::TimePoint(300, 1); // 5 minutes default
    viewport_.samples_per_pixel = viewport_.max_samples_per_pixel;
    
    invalidate_paint_cache();
    request_waveform_data(viewport_.start_time, viewport_.duration);
    update();
    
    emit zoom_changed(viewport_.samples_per_pixel);
}

void QWaveformWidget::zoom_in(float factor) {
    float new_samples_per_pixel = viewport_.samples_per_pixel / factor;
    new_samples_per_pixel = std::max(new_samples_per_pixel, viewport_.min_samples_per_pixel);
    
    if (new_samples_per_pixel != viewport_.samples_per_pixel) {
        // Adjust duration to maintain center point
        auto center_time = viewport_.start_time + 
                          ve::TimePoint(viewport_.duration.to_rational().num / 2, viewport_.duration.to_rational().den);
        
        viewport_.samples_per_pixel = new_samples_per_pixel;
        
        // Recalculate duration and start time
        auto new_duration = viewport_.duration;
        new_duration = ve::TimePoint(new_duration.to_rational().num / static_cast<int64_t>(factor), 
                                     new_duration.to_rational().den);
        viewport_.duration = new_duration;
        
        // Center on the same time point
        viewport_.start_time = center_time - 
                              ve::TimePoint(new_duration.to_rational().num / 2, new_duration.to_rational().den);
        
        invalidate_paint_cache();
        request_waveform_data(viewport_.start_time, viewport_.duration);
        update();
        
        emit zoom_changed(viewport_.samples_per_pixel);
    }
}

void QWaveformWidget::zoom_out(float factor) {
    float new_samples_per_pixel = viewport_.samples_per_pixel * factor;
    new_samples_per_pixel = std::min(new_samples_per_pixel, viewport_.max_samples_per_pixel);
    
    if (new_samples_per_pixel != viewport_.samples_per_pixel) {
        zoom_in(1.0f / factor); // Reuse zoom_in logic
    }
}

void QWaveformWidget::zoom_to_selection() {
    if (selection_start_ != selection_end_) {
        viewport_.start_time = selection_start_;
        viewport_.duration = selection_end_ - selection_start_;
        
        // Calculate appropriate samples per pixel
        auto duration_seconds = static_cast<double>(viewport_.duration.to_rational().num) / 
                               viewport_.duration.to_rational().den;
        viewport_.samples_per_pixel = static_cast<float>((duration_seconds * 48000.0) / width());
        
        viewport_.samples_per_pixel = std::clamp(viewport_.samples_per_pixel, 
                                                viewport_.min_samples_per_pixel,
                                                viewport_.max_samples_per_pixel);
        
        invalidate_paint_cache();
        request_waveform_data(viewport_.start_time, viewport_.duration);
        update();
        
        emit zoom_changed(viewport_.samples_per_pixel);
    }
}

void QWaveformWidget::pan_to_time(const ve::TimePoint& center_time) {
    auto half_duration = ve::TimePoint(viewport_.duration.to_rational().num / 2, 
                                      viewport_.duration.to_rational().den);
    viewport_.start_time = center_time - half_duration;
    
    invalidate_paint_cache();
    request_waveform_data(viewport_.start_time, viewport_.duration);
    update();
}

//=============================================================================
// Visual Customization
//=============================================================================

void QWaveformWidget::set_style(const WaveformStyle& style) {
    style_ = style;
    invalidate_paint_cache();
    update();
}

//=============================================================================
// Real-time Updates
//=============================================================================

void QWaveformWidget::refresh_waveform() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    current_waveform_data_.reset();
    invalidate_paint_cache();
    
    if (!audio_source_path_.empty()) {
        request_waveform_data(viewport_.start_time, viewport_.duration);
    }
    
    update();
}

void QWaveformWidget::set_auto_refresh(bool enabled) {
    auto_refresh_enabled_ = enabled;
    
    if (enabled) {
        if (!refresh_timer_->isActive()) {
            refresh_timer_->start();
        }
        if (!update_timer_->isActive()) {
            update_timer_->start();
        }
    } else {
        refresh_timer_->stop();
        update_timer_->stop();
    }
}

void QWaveformWidget::set_refresh_rate(int fps) {
    refresh_rate_ = std::clamp(fps, 10, 120); // Reasonable range
    refresh_timer_->setInterval(1000 / refresh_rate_);
}

//=============================================================================
// Qt Widget Interface
//=============================================================================

QSize QWaveformWidget::sizeHint() const {
    return QSize(800, height_hint_);
}

QSize QWaveformWidget::minimumSizeHint() const {
    return QSize(200, 60);
}

//=============================================================================
// Qt Event Handling
//=============================================================================

void QWaveformWidget::paintEvent(QPaintEvent* event) {
    auto render_start = std::chrono::high_resolution_clock::now();
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, style_.anti_aliasing);
    
    // Clear background
    painter.fillRect(event->rect(), style_.background_color);
    
    // Render waveform components
    render_waveform(painter, event->rect());
    
    // Track rendering performance
    auto render_end = std::chrono::high_resolution_clock::now();
    auto render_duration = std::chrono::duration<double, std::milli>(render_end - render_start);
    
    {
        std::lock_guard<std::mutex> lock(performance_mutex_);
        performance_metrics_.last_render_time_ms = render_duration.count();
        performance_metrics_.frame_count++;
        
        // Update average
        if (performance_metrics_.frame_count > 1) {
            performance_metrics_.average_render_time_ms = 
                (performance_metrics_.average_render_time_ms * 0.9) + 
                (render_duration.count() * 0.1);
        } else {
            performance_metrics_.average_render_time_ms = render_duration.count();
        }
    }
    
    check_performance_thresholds();
}

void QWaveformWidget::mousePressEvent(QMouseEvent* event) {
    if (!is_initialized_) return;
    
    setFocus();
    interaction_.drag_start = event->pos();
    interaction_.drag_current = event->pos();
    interaction_.is_dragging = true;
    
    ve::TimePoint click_time = widget_to_timeline_position(event->pos());
    
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ShiftModifier) {
            // Extend selection
            interaction_.current_mode = WaveformInteraction::Mode::SELECTION;
            if (selection_start_ == selection_end_) {
                selection_start_ = click_time;
            }
            selection_end_ = click_time;
            set_selection_range(selection_start_, selection_end_);
        } else if (event->modifiers() & Qt::ControlModifier) {
            // Start zoom operation
            interaction_.current_mode = WaveformInteraction::Mode::ZOOMING;
        } else {
            // Start new selection or scrubbing
            interaction_.current_mode = WaveformInteraction::Mode::SELECTION;
            selection_start_ = click_time;
            selection_end_ = click_time;
            set_selection_range(selection_start_, selection_end_);
            
            // Also update playhead
            set_playhead_position(click_time);
            emit playhead_position_changed(click_time);
        }
    } else if (event->button() == Qt::MiddleButton) {
        // Pan operation
        interaction_.current_mode = WaveformInteraction::Mode::PANNING;
    }
    
    emit waveform_clicked(click_time, event->button());
}

void QWaveformWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!is_initialized_ || !interaction_.is_dragging) return;
    
    interaction_.drag_current = event->pos();
    ve::TimePoint current_time = widget_to_timeline_position(event->pos());
    
    switch (interaction_.current_mode) {
        case WaveformInteraction::Mode::SELECTION: {
            selection_end_ = current_time;
            set_selection_range(selection_start_, selection_end_);
            
            // Audio scrubbing during selection
            emit audio_scrubbing(current_time);
            break;
        }
        
        case WaveformInteraction::Mode::PANNING: {
            // Calculate pan delta
            int pixel_delta = interaction_.drag_current.x() - interaction_.drag_start.x();
            auto time_delta = pixel_delta * (viewport_.duration.to_rational().num / width());
            
            viewport_.start_time = ve::TimePoint(
                viewport_.start_time.to_rational().num - time_delta,
                viewport_.start_time.to_rational().den
            );
            
            invalidate_paint_cache();
            request_waveform_data(viewport_.start_time, viewport_.duration);
            update();
            break;
        }
        
        case WaveformInteraction::Mode::ZOOMING: {
            // Zoom based on vertical mouse movement
            int y_delta = interaction_.drag_current.y() - interaction_.drag_start.y();
            float zoom_factor = 1.0f + (y_delta * 0.01f);
            
            if (zoom_factor > 1.05f) {
                zoom_in(1.1f);
                interaction_.drag_start = interaction_.drag_current;
            } else if (zoom_factor < 0.95f) {
                zoom_out(1.1f);
                interaction_.drag_start = interaction_.drag_current;
            }
            break;
        }
        
        default:
            break;
    }
}

void QWaveformWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!is_initialized_) return;
    
    interaction_.is_dragging = false;
    interaction_.current_mode = WaveformInteraction::Mode::NONE;
    
    // Finalize selection if needed
    if (selection_start_ != selection_end_) {
        // Ensure proper order
        if (selection_start_ > selection_end_) {
            std::swap(selection_start_, selection_end_);
        }
        set_selection_range(selection_start_, selection_end_);
    }
}

void QWaveformWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    ve::TimePoint click_time = widget_to_timeline_position(event->pos());
    
    if (event->button() == Qt::LeftButton) {
        // Double-click to zoom to selection or fit
        if (selection_start_ != selection_end_) {
            zoom_to_selection();
        } else {
            zoom_to_fit();
        }
    }
    
    emit waveform_double_clicked(click_time);
}

void QWaveformWidget::wheelEvent(QWheelEvent* event) {
    if (!is_initialized_) return;
    
    float zoom_factor = 1.2f;
    
    if (event->angleDelta().y() > 0) {
        // Zoom in
        zoom_in(zoom_factor);
    } else {
        // Zoom out
        zoom_out(zoom_factor);
    }
    
    event->accept();
}

void QWaveformWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    // Invalidate cache on resize
    invalidate_paint_cache();
    
    // Adjust viewport if needed
    if (is_initialized_) {
        request_waveform_data(viewport_.start_time, viewport_.duration);
    }
}

void QWaveformWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    
    // Reset interaction state when mouse leaves widget
    interaction_.current_mode = WaveformInteraction::Mode::NONE;
    interaction_.is_dragging = false;
}

void QWaveformWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Home:
            pan_to_time(ve::TimePoint(0, 1));
            break;
            
        case Qt::Key_End:
            // Pan to end (would need audio duration info)
            break;
            
        case Qt::Key_Plus:
            zoom_in();
            break;
            
        case Qt::Key_Minus:
            zoom_out();
            break;
            
        case Qt::Key_Space:
            // Toggle playback (emit signal for parent to handle)
            emit playhead_position_changed(playhead_position_);
            break;
            
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void QWaveformWidget::focusInEvent(QFocusEvent* event) {
    QWidget::focusInEvent(event);
    update(); // Redraw with focus indication
}

void QWaveformWidget::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    update(); // Redraw without focus indication
}

//=============================================================================
// Slot Implementations
//=============================================================================

void QWaveformWidget::update_waveform_data() {
    if (!waveform_generator_ || audio_source_path_.empty()) {
        return;
    }
    
    // Check if we need to request new waveform data
    if (!is_waveform_data_valid()) {
        request_waveform_data(viewport_.start_time, viewport_.duration);
    }
}

void QWaveformWidget::update_rendering() {
    // Only update if we're visible and have valid data
    if (isVisible() && is_waveform_data_valid()) {
        update();
    }
}

void QWaveformWidget::handle_cache_update() {
    // React to cache updates
    if (waveform_cache_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // Check if our current view might have new cached data
        invalidate_paint_cache();
        update();
    }
}

//=============================================================================
// Private Implementation Methods
//=============================================================================

void QWaveformWidget::render_waveform(QPainter& painter, const QRect& rect) {
    // Check for cached rendering first
    if (is_paint_cache_valid() && paint_cache_rect_.contains(rect)) {
        painter.drawPixmap(rect, paint_cache_, rect);
        render_playhead(painter, rect); // Always render playhead on top
        return;
    }
    
    // Full rendering pipeline
    render_background(painter, rect);
    render_grid(painter, rect);
    render_waveform_data(painter, rect);
    render_selection(painter, rect);
    render_peaks(painter, rect);
    render_playhead(painter, rect);
    
    // Update paint cache
    update_paint_cache();
}

void QWaveformWidget::render_background(QPainter& painter, const QRect& rect) {
    painter.fillRect(rect, style_.background_color);
}

void QWaveformWidget::render_grid(QPainter& painter, const QRect& rect) {
    if (!style_.show_grid) return;
    
    painter.setPen(QPen(style_.grid_color, 1));
    
    // Calculate grid intervals
    auto grid_times = waveform_utils::calculate_grid_intervals(
        viewport_.start_time, viewport_.duration, width()
    );
    
    for (const auto& time : grid_times) {
        if (viewport_.contains_time(time)) {
            int x = interaction_.time_to_pixel(time, viewport_);
            painter.drawLine(x, rect.top(), x, rect.bottom());
        }
    }
}

void QWaveformWidget::render_waveform_data(QPainter& painter, const QRect& rect) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!current_waveform_data_) {
        // Render placeholder
        painter.setPen(QPen(style_.waveform_color.darker(), 1));
        painter.drawText(rect.center(), "Loading waveform...");
        return;
    }
    
    // Render actual waveform data
    painter.setPen(QPen(style_.waveform_color, style_.line_width));
    
    // For now, render a simplified waveform representation
    // In a full implementation, this would render the actual WaveformData
    int center_y = rect.center().y();
    int amplitude_height = style_.waveform_height / 2;
    
    // Draw a mock waveform pattern
    QPolygonF waveform_polygon;
    for (int x = rect.left(); x <= rect.right(); x += 2) {
        float progress = static_cast<float>(x - rect.left()) / rect.width();
        float amplitude = 0.5f * std::sin(progress * 20.0f) * std::sin(progress * 3.0f);
        
        int y = center_y + static_cast<int>(amplitude * amplitude_height);
        waveform_polygon << QPointF(x, y);
    }
    
    painter.drawPolyline(waveform_polygon);
    
    // Fill area if enabled
    if (style_.show_rms) {
        QBrush fill_brush(style_.waveform_fill_color);
        painter.setBrush(fill_brush);
        
        // Create filled polygon
        QPolygonF fill_polygon = waveform_polygon;
        fill_polygon << QPointF(rect.right(), center_y);
        fill_polygon << QPointF(rect.left(), center_y);
        
        painter.drawPolygon(fill_polygon);
    }
}

void QWaveformWidget::render_selection(QPainter& painter, const QRect& rect) {
    if (selection_start_ == selection_end_) return;
    
    int start_x = interaction_.time_to_pixel(selection_start_, viewport_);
    int end_x = interaction_.time_to_pixel(selection_end_, viewport_);
    
    if (start_x > end_x) std::swap(start_x, end_x);
    
    QRect selection_rect(start_x, rect.top(), end_x - start_x, rect.height());
    selection_rect = selection_rect.intersected(rect);
    
    if (!selection_rect.isEmpty()) {
        painter.fillRect(selection_rect, style_.selection_color);
    }
}

void QWaveformWidget::render_playhead(QPainter& painter, const QRect& rect) {
    if (!viewport_.contains_time(playhead_position_)) return;
    
    int x = interaction_.time_to_pixel(playhead_position_, viewport_);
    
    painter.setPen(QPen(style_.playhead_color, 2));
    painter.drawLine(x, rect.top(), x, rect.bottom());
    
    // Draw playhead indicator at top
    QPolygonF indicator;
    indicator << QPointF(x - 5, rect.top())
              << QPointF(x + 5, rect.top())
              << QPointF(x, rect.top() + 8);
    
    painter.setBrush(style_.playhead_color);
    painter.drawPolygon(indicator);
}

void QWaveformWidget::render_peaks(QPainter& painter, const QRect& rect) {
    if (!style_.show_peaks) return;
    
    // Peak rendering would be implemented here
    // For now, this is a placeholder
}

//=============================================================================
// Paint Cache Management
//=============================================================================

void QWaveformWidget::invalidate_paint_cache() {
    paint_cache_valid_ = false;
}

void QWaveformWidget::update_paint_cache() {
    if (width() > 0 && height() > 0) {
        paint_cache_ = QPixmap(size());
        paint_cache_.fill(Qt::transparent);
        
        QPainter cache_painter(&paint_cache_);
        cache_painter.setRenderHint(QPainter::Antialiasing, style_.anti_aliasing);
        
        QRect full_rect(0, 0, width(), height());
        render_background(cache_painter, full_rect);
        render_grid(cache_painter, full_rect);
        render_waveform_data(cache_painter, full_rect);
        render_selection(cache_painter, full_rect);
        render_peaks(cache_painter, full_rect);
        
        paint_cache_rect_ = full_rect;
        paint_cache_valid_ = true;
    }
}

bool QWaveformWidget::is_paint_cache_valid() const {
    return paint_cache_valid_ && !paint_cache_.isNull();
}

//=============================================================================
// Waveform Data Management
//=============================================================================

void QWaveformWidget::request_waveform_data(const ve::TimePoint& start, const ve::TimePoint& duration) {
    if (!waveform_generator_ || audio_source_path_.empty()) {
        return;
    }
    
    // This would integrate with the Week 7 waveform generation system
    // For now, simulate data request
    ve::log_debug("Requesting waveform data for time range: {} to {}", 
                  start.to_rational().num, 
                  (start + duration).to_rational().num);
    
    // In a full implementation, this would:
    // 1. Calculate optimal zoom level
    // 2. Check cache first
    // 3. Request generation if needed
    // 4. Update current_waveform_data_ when ready
}

void QWaveformWidget::process_waveform_data(std::shared_ptr<ve::audio::WaveformData> data) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    current_waveform_data_ = data;
    invalidate_paint_cache();
    update();
    
    emit waveform_generation_complete();
}

ve::audio::ZoomLevel QWaveformWidget::calculate_optimal_zoom_level() const {
    // This would integrate with Week 7 zoom level system
    // Return appropriate zoom level based on current viewport
    return ve::audio::ZoomLevel{}; // Placeholder
}

bool QWaveformWidget::is_waveform_data_valid() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_waveform_data_ != nullptr;
}

//=============================================================================
// Performance Monitoring
//=============================================================================

void QWaveformWidget::check_performance_thresholds() {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    
    const double target_frame_time = 1000.0 / refresh_rate_; // ms per frame
    
    if (performance_metrics_.last_render_time_ms > target_frame_time * 1.5) {
        performance_metrics_.dropped_frames++;
        
        if (performance_metrics_.dropped_frames % 10 == 0) {
            QString warning = QString("Waveform rendering performance degraded: %1ms (target: %2ms)")
                             .arg(performance_metrics_.last_render_time_ms, 0, 'f', 2)
                             .arg(target_frame_time, 0, 'f', 2);
            emit rendering_performance_warning(warning);
        }
    }
}

//=============================================================================
// Coordinate Conversion
//=============================================================================

ve::TimePoint QWaveformWidget::widget_to_timeline_position(const QPoint& widget_pos) const {
    double ratio = static_cast<double>(widget_pos.x()) / width();
    auto offset_samples = static_cast<int64_t>(ratio * viewport_.duration.to_rational().num);
    
    return ve::TimePoint(
        viewport_.start_time.to_rational().num + offset_samples,
        viewport_.start_time.to_rational().den
    );
}

float QWaveformWidget::calculate_pixel_to_time_ratio() const {
    return static_cast<float>(viewport_.duration.to_rational().num) / width();
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

namespace waveform_utils {

QColor interpolate_color(const QColor& start, const QColor& end, float ratio) {
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    
    int r = static_cast<int>(start.red() * (1.0f - ratio) + end.red() * ratio);
    int g = static_cast<int>(start.green() * (1.0f - ratio) + end.green() * ratio);
    int b = static_cast<int>(start.blue() * (1.0f - ratio) + end.blue() * ratio);
    int a = static_cast<int>(start.alpha() * (1.0f - ratio) + end.alpha() * ratio);
    
    return QColor(r, g, b, a);
}

void render_waveform_points(QPainter& painter, 
                           const std::vector<QPointF>& points,
                           const WaveformStyle& style) {
    if (points.empty()) return;
    
    painter.setPen(QPen(style.waveform_color, style.line_width));
    
    // Use efficient polyline rendering for large point sets
    if (points.size() > 1000) {
        painter.drawPolyline(points.data(), static_cast<int>(points.size()));
    } else {
        QPolygonF polygon(points);
        painter.drawPolyline(polygon);
    }
}

std::vector<ve::TimePoint> calculate_grid_intervals(
    const ve::TimePoint& start,
    const ve::TimePoint& duration,
    int widget_width,
    int min_pixel_spacing) {
    
    std::vector<ve::TimePoint> intervals;
    
    if (widget_width <= 0 || min_pixel_spacing <= 0) {
        return intervals;
    }
    
    // Calculate appropriate time interval for grid lines
    double duration_seconds = static_cast<double>(duration.to_rational().num) / duration.to_rational().den;
    double seconds_per_pixel = duration_seconds / widget_width;
    double min_interval_seconds = seconds_per_pixel * min_pixel_spacing;
    
    // Choose nice round intervals
    double interval_seconds = 1.0;
    if (min_interval_seconds > 60.0) {
        interval_seconds = 60.0; // 1 minute
    } else if (min_interval_seconds > 10.0) {
        interval_seconds = 10.0; // 10 seconds
    } else if (min_interval_seconds > 5.0) {
        interval_seconds = 5.0; // 5 seconds
    } else if (min_interval_seconds > 1.0) {
        interval_seconds = 1.0; // 1 second
    } else {
        interval_seconds = 0.1; // 100ms
    }
    
    // Generate interval points
    double start_seconds = static_cast<double>(start.to_rational().num) / start.to_rational().den;
    double end_seconds = start_seconds + duration_seconds;
    
    double first_interval = std::ceil(start_seconds / interval_seconds) * interval_seconds;
    
    for (double time = first_interval; time <= end_seconds; time += interval_seconds) {
        int64_t time_num = static_cast<int64_t>(time * start.to_rational().den);
        intervals.emplace_back(time_num, start.to_rational().den);
    }
    
    return intervals;
}

bool should_skip_rendering(const QRect& widget_rect, const QRect& damage_rect) {
    return !widget_rect.intersects(damage_rect);
}

QRect calculate_minimal_update_region(const QRect& old_rect, const QRect& new_rect) {
    return old_rect.united(new_rect);
}

float db_to_linear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

float linear_to_db(float linear) {
    return 20.0f * std::log10(std::max(linear, 1e-6f));
}

QColor amplitude_to_color(float amplitude, const WaveformStyle& style) {
    if (amplitude > style.peak_threshold) {
        return style.peak_color;
    }
    
    float intensity = std::clamp(amplitude, 0.0f, 1.0f);
    return interpolate_color(style.background_color, style.waveform_color, intensity);
}

} // namespace waveform_utils

} // namespace ve::ui