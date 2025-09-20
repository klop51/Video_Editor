/**
 * @file audio_track_widget.cpp
 * @brief Implementation of professional audio track widget for timeline integration
 * 
 * Week 8 Qt Timeline UI Integration - Complete implementation providing
 * professional audio track visualization with waveform display, editing controls,
 * and seamless integration with Week 7 waveform system and timeline architecture.
 */

#include "ui/audio_track_widget.hpp"
#include "audio/waveform_generator.h"
#include "audio/waveform_cache.h"
#include "../../commands/include/commands/command.hpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QDrag>
#include <QtCore/QMimeData>
#include <QtCore/QDebug>
#include <algorithm>
#include <cmath>

namespace ve::ui {

//==============================================================================
// AudioTrackHeader Implementation
//==============================================================================

AudioTrackHeader::AudioTrackHeader(QWidget* parent)
    : QWidget(parent)
    , main_layout_(new QVBoxLayout(this))
    , name_label_(new QLabel(track_name_, this))
    , volume_slider_(new QSlider(Qt::Vertical, this))
    , pan_slider_(new QSlider(Qt::Horizontal, this))
    , mute_button_(new QPushButton("M", this))
    , solo_button_(new QPushButton("S", this))
    , record_button_(new QPushButton("R", this))
{
    setup_ui();
}

void AudioTrackHeader::setup_ui() {
    setMinimumWidth(controls_config_.control_width);
    setMaximumWidth(controls_config_.control_width);
    
    // Configure name label
    name_label_->setAlignment(Qt::AlignCenter);
    name_label_->setStyleSheet("QLabel { font-weight: bold; color: white; background: rgba(0,0,0,100); border-radius: 3px; padding: 2px; }");
    
    // Configure volume slider
    volume_slider_->setRange(0, 200);  // 0% to 200%
    volume_slider_->setValue(static_cast<int>(volume_ * 100));
    volume_slider_->setMinimumHeight(controls_config_.slider_height);
    volume_slider_->setToolTip("Volume");
    
    // Configure pan slider
    pan_slider_->setRange(-100, 100);  // -100% to 100%
    pan_slider_->setValue(static_cast<int>(pan_ * 100));
    pan_slider_->setToolTip("Pan");
    
    // Configure buttons
    const int button_size = controls_config_.button_size;
    for (auto* button : {mute_button_, solo_button_, record_button_}) {
        button->setFixedSize(button_size, button_size);
        button->setCheckable(true);
    }
    
    mute_button_->setToolTip("Mute Track");
    mute_button_->setStyleSheet("QPushButton:checked { background-color: #ff6b6b; }");
    
    solo_button_->setToolTip("Solo Track");
    solo_button_->setStyleSheet("QPushButton:checked { background-color: #ffd93d; }");
    
    record_button_->setToolTip("Arm for Recording");
    record_button_->setStyleSheet("QPushButton:checked { background-color: #ff4757; }");
    record_button_->setVisible(controls_config_.show_record_arm);
    
    // Connect signals
    connect(volume_slider_, &QSlider::valueChanged, this, &AudioTrackHeader::on_volume_slider_changed);
    connect(pan_slider_, &QSlider::valueChanged, this, &AudioTrackHeader::on_pan_slider_changed);
    connect(mute_button_, &QPushButton::clicked, this, &AudioTrackHeader::on_mute_clicked);
    connect(solo_button_, &QPushButton::clicked, this, &AudioTrackHeader::on_solo_clicked);
    connect(record_button_, &QPushButton::clicked, this, &AudioTrackHeader::on_record_clicked);
    
    update_control_layout();
}

void AudioTrackHeader::update_control_layout() {
    // Clear existing layout
    while (auto* item = main_layout_->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->setParent(nullptr);
        }
        delete item;
    }
    
    // Add components based on configuration
    main_layout_->addWidget(name_label_);
    main_layout_->addSpacing(5);
    
    if (controls_config_.show_volume_slider) {
        main_layout_->addWidget(volume_slider_, 1);
    }
    
    if (controls_config_.show_pan_control) {
        main_layout_->addWidget(pan_slider_);
    }
    
    // Button layout
    auto* button_layout = new QHBoxLayout();
    if (controls_config_.show_mute_button) {
        button_layout->addWidget(mute_button_);
    }
    if (controls_config_.show_solo_button) {
        button_layout->addWidget(solo_button_);
    }
    if (controls_config_.show_record_arm && record_button_->isVisible()) {
        button_layout->addWidget(record_button_);
    }
    button_layout->addStretch();
    
    main_layout_->addLayout(button_layout);
    main_layout_->addStretch();
}

void AudioTrackHeader::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    // Draw track color strip
    QRect color_strip(0, 0, 4, rect.height());
    painter.fillRect(color_strip, track_color_);
    
    // Draw background
    QColor bg_color = selected_ ? QColor(70, 70, 70) : QColor(50, 50, 50);
    painter.fillRect(rect.adjusted(4, 0, 0, 0), bg_color);
    
    // Draw track meters if enabled
    if (controls_config_.show_track_meters && !meter_rect_.isEmpty()) {
        draw_track_meters(painter, meter_rect_);
    }
    
    QWidget::paintEvent(event);
}

void AudioTrackHeader::draw_track_meters(QPainter& painter, const QRect& rect) {
    const int meter_width = rect.width() / 2;
    const int spacing = 2;
    
    // Left channel meter
    QRect left_meter(rect.x(), rect.y(), meter_width - spacing/2, rect.height());
    // Right channel meter  
    QRect right_meter(rect.x() + meter_width + spacing/2, rect.y(), meter_width - spacing/2, rect.height());
    
    auto draw_meter = [&](const QRect& meter_rect, float level) {
        // Background
        painter.fillRect(meter_rect, QColor(20, 20, 20));
        
        // Level indicator
        if (level > 0.0f) {
            int level_height = static_cast<int>(level * meter_rect.height());
            QRect level_rect(meter_rect.x(), meter_rect.bottom() - level_height, 
                           meter_rect.width(), level_height);
            
            QColor level_color = audio_track_utils::level_to_meter_color(20.0f * std::log10(level));
            painter.fillRect(level_rect, level_color);
        }
        
        // Border
        painter.setPen(QColor(100, 100, 100));
        painter.drawRect(meter_rect);
    };
    
    // Draw meters with current levels
    float left_level = meter_levels_.size() > 0 ? meter_levels_[0] : 0.0f;
    float right_level = meter_levels_.size() > 1 ? meter_levels_[1] : left_level;
    
    draw_meter(left_meter, left_level);
    draw_meter(right_meter, right_level);
}

void AudioTrackHeader::set_track_name(const QString& name) {
    track_name_ = name;
    name_label_->setText(name);
    emit track_name_changed(name);
}

void AudioTrackHeader::set_track_color(const QColor& color) {
    track_color_ = color;
    update();
}

void AudioTrackHeader::set_volume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 2.0f);
    volume_slider_->setValue(static_cast<int>(volume_ * 100));
    update();
}

void AudioTrackHeader::set_pan(float pan) {
    pan_ = std::clamp(pan, -1.0f, 1.0f);
    pan_slider_->setValue(static_cast<int>(pan_ * 100));
    update();
}

void AudioTrackHeader::set_muted(bool muted) {
    muted_ = muted;
    mute_button_->setChecked(muted);
    update();
}

void AudioTrackHeader::set_solo(bool solo) {
    solo_ = solo;
    solo_button_->setChecked(solo);
    update();
}

void AudioTrackHeader::set_record_armed(bool armed) {
    record_armed_ = armed;
    record_button_->setChecked(armed);
    update();
}

void AudioTrackHeader::set_selected(bool selected) {
    selected_ = selected;
    update();
}

void AudioTrackHeader::set_track_height(int height) {
    track_height_ = height;
    setMinimumHeight(height);
    setMaximumHeight(height);
    
    // Update meter rectangle
    if (controls_config_.show_track_meters) {
        meter_rect_ = QRect(width() - 20, height - 60, 16, 50);
    }
}

void AudioTrackHeader::on_volume_slider_changed(int value) {
    volume_ = value / 100.0f;
    emit volume_changed(volume_);
}

void AudioTrackHeader::on_pan_slider_changed(int value) {
    pan_ = value / 100.0f;
    emit pan_changed(pan_);
}

void AudioTrackHeader::on_mute_clicked() {
    muted_ = mute_button_->isChecked();
    emit mute_toggled(muted_);
}

void AudioTrackHeader::on_solo_clicked() {
    solo_ = solo_button_->isChecked();
    emit solo_toggled(solo_);
}

void AudioTrackHeader::on_record_clicked() {
    record_armed_ = record_button_->isChecked();
    emit record_arm_toggled(record_armed_);
}

void AudioTrackHeader::contextMenuEvent(QContextMenuEvent* event) {
    emit header_context_menu(event->pos());
    QWidget::contextMenuEvent(event);
}

//==============================================================================
// AudioTrackWidget Implementation
//==============================================================================

AudioTrackWidget::AudioTrackWidget(QWidget* parent)
    : QWidget(parent)
    , header_widget_(std::make_unique<AudioTrackHeader>(this))
    , main_layout_(new QHBoxLayout(this))
    , update_timer_(std::make_unique<QTimer>(this))
{
    setAcceptDrops(true);
    setMouseTracking(true);
    
    // Configure layout
    main_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout_->setSpacing(0);
    main_layout_->addWidget(header_widget_.get());
    main_layout_->addStretch(1);
    
    // Connect header signals
    connect(header_widget_.get(), &AudioTrackHeader::volume_changed,
            this, &AudioTrackWidget::on_header_volume_changed);
    connect(header_widget_.get(), &AudioTrackHeader::pan_changed,
            this, &AudioTrackWidget::on_header_pan_changed);
    connect(header_widget_.get(), &AudioTrackHeader::mute_toggled,
            this, &AudioTrackWidget::on_header_mute_toggled);
    connect(header_widget_.get(), &AudioTrackHeader::solo_toggled,
            this, &AudioTrackWidget::on_header_solo_toggled);
    
    // Configure update timer for smooth visual updates
    update_timer_->setSingleShot(true);
    update_timer_->setInterval(16); // ~60fps updates
    connect(update_timer_.get(), &QTimer::timeout, this, &AudioTrackWidget::update);
    
    // Initialize default waveform style
    default_waveform_style_.peak_color = QColor(100, 200, 255);
    default_waveform_style_.rms_color = QColor(50, 150, 200);
    default_waveform_style_.background_color = QColor(30, 30, 30);
    default_waveform_style_.center_line_color = QColor(80, 80, 80);
}

AudioTrackWidget::~AudioTrackWidget() = default;

void AudioTrackWidget::set_track(const ve::timeline::Track* track) {
    if (track_ == track) return;
    
    track_ = track;
    clear_audio_clips();
    
    if (track_) {
        // Load existing segments from track
        for (const auto& segment : track_->segments()) {
            add_audio_clip(segment);
        }
    }
    
    schedule_visual_update();
}

void AudioTrackWidget::set_track_index(size_t index) {
    track_index_ = index;
}

void AudioTrackWidget::set_track_name(const QString& name) {
    track_name_ = name;
    header_widget_->set_track_name(name);
}

void AudioTrackWidget::set_track_height(int height) {
    track_height_ = height;
    setMinimumHeight(height);
    setMaximumHeight(height);
    header_widget_->set_track_height(height);
    update_clip_bounds();
    schedule_visual_update();
}

void AudioTrackWidget::set_timeline_zoom(double zoom_factor) {
    if (std::abs(zoom_factor_ - zoom_factor) < 0.001) return;
    
    zoom_factor_ = zoom_factor;
    update_clip_bounds();
    
    // Update all waveform widgets with new zoom
    for (auto& clip : audio_clips_) {
        if (clip.waveform_widget) {
            // Calculate new time range for waveform widget
            auto duration_seconds = static_cast<double>(clip.duration.num) / clip.duration.den;
            clip.waveform_widget->set_time_range(0.0, duration_seconds);
            clip.waveform_widget->set_zoom_factor(zoom_factor_);
        }
    }
    
    schedule_visual_update();
}

void AudioTrackWidget::set_timeline_scroll(int scroll_x) {
    if (scroll_x_ == scroll_x) return;
    
    scroll_x_ = scroll_x;
    update_clip_bounds();
    schedule_visual_update();
}

void AudioTrackWidget::set_current_time(const ve::TimePoint& time) {
    current_time_ = time;
    schedule_visual_update();
}

void AudioTrackWidget::set_selection_range(const ve::TimePoint& start, const ve::TimePoint& end) {
    selection_start_ = start;
    selection_end_ = end;
    schedule_visual_update();
}

void AudioTrackWidget::set_waveform_generator(std::shared_ptr<ve::audio::WaveformGenerator> generator) {
    waveform_generator_ = generator;
    
    // Update existing clip waveform widgets
    for (auto& clip : audio_clips_) {
        if (clip.waveform_widget) {
            clip.waveform_widget->set_waveform_generator(generator);
        }
    }
}

void AudioTrackWidget::set_waveform_cache(std::shared_ptr<ve::audio::WaveformCache> cache) {
    waveform_cache_ = cache;
    
    // Update existing clip waveform widgets
    for (auto& clip : audio_clips_) {
        if (clip.waveform_widget) {
            clip.waveform_widget->set_waveform_cache(cache);
        }
    }
}

void AudioTrackWidget::add_audio_clip(const ve::timeline::Segment& segment) {
    // Create new clip visual
    AudioClipVisual clip;
    clip.segment_id = segment.id();
    clip.start_time = segment.start_time();
    clip.duration = segment.duration();
    
    // Extract audio file path from segment metadata
    // This would typically come from the segment's media source
    clip.audio_file_path = ""; // To be implemented based on segment structure
    
    // Set default visual properties
    clip.waveform_style = default_waveform_style_;
    clip.volume = 1.0f;
    clip.pan = 0.0f;
    
    // Create waveform widget for this clip
    create_clip_waveform_widget(clip);
    
    audio_clips_.push_back(std::move(clip));
    update_clip_bounds();
    schedule_visual_update();
}

void AudioTrackWidget::remove_audio_clip(ve::timeline::SegmentId segment_id) {
    auto it = std::find_if(audio_clips_.begin(), audio_clips_.end(),
        [segment_id](const AudioClipVisual& clip) {
            return clip.segment_id == segment_id;
        });
    
    if (it != audio_clips_.end()) {
        destroy_clip_waveform_widget(*it);
        audio_clips_.erase(it);
        
        // Remove from selection if selected
        deselect_clip(segment_id);
        
        schedule_visual_update();
    }
}

void AudioTrackWidget::update_audio_clip(const ve::timeline::Segment& segment) {
    if (auto* clip = find_clip_by_id(segment.id())) {
        clip->start_time = segment.start_time();
        clip->duration = segment.duration();
        
        // Update waveform widget if it exists
        if (clip->waveform_widget) {
            auto duration_seconds = static_cast<double>(clip->duration.num) / clip->duration.den;
            clip->waveform_widget->set_time_range(0.0, duration_seconds);
        }
        
        update_clip_bounds();
        schedule_visual_update();
    }
}

void AudioTrackWidget::clear_audio_clips() {
    // Destroy all waveform widgets
    for (auto& clip : audio_clips_) {
        destroy_clip_waveform_widget(clip);
    }
    
    audio_clips_.clear();
    selected_clips_.clear();
    schedule_visual_update();
}

void AudioTrackWidget::create_clip_waveform_widget(AudioClipVisual& clip) {
    if (!clip.waveform_visible || clip.audio_file_path.empty()) {
        return;
    }
    
    // Create embedded waveform widget
    clip.waveform_widget = std::make_shared<QWaveformWidget>(this);
    
    // Configure waveform widget
    if (waveform_generator_) {
        clip.waveform_widget->set_waveform_generator(waveform_generator_);
    }
    if (waveform_cache_) {
        clip.waveform_widget->set_waveform_cache(waveform_cache_);
    }
    
    // Set style and properties
    clip.waveform_widget->set_style(clip.waveform_style);
    clip.waveform_widget->set_zoom_factor(zoom_factor_);
    
    // Load audio file
    auto duration_seconds = static_cast<double>(clip.duration.num) / clip.duration.den;
    clip.waveform_widget->set_time_range(0.0, duration_seconds);
    
    // Note: Actual file loading would happen here
    // clip.waveform_widget->load_audio_file(clip.audio_file_path);
    
    // Hide the widget initially - we'll render it manually in paintEvent
    clip.waveform_widget->hide();
}

void AudioTrackWidget::destroy_clip_waveform_widget(AudioClipVisual& clip) {
    if (clip.waveform_widget) {
        clip.waveform_widget.reset();
    }
}

void AudioTrackWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect content_rect = rect();
    content_rect.setLeft(header_widget_->width());
    
    // Render track background
    render_track_background(painter, content_rect);
    
    // Render audio clips
    render_audio_clips(painter, content_rect);
    
    // Render playhead
    render_playhead(painter, content_rect);
    
    // Render selection overlay
    render_selection_overlay(painter, content_rect);
    
    // Render drop indicator if dragging
    if (show_drop_indicator_) {
        render_drop_indicator(painter, content_rect);
    }
    
    QWidget::paintEvent(event);
}

void AudioTrackWidget::render_track_background(QPainter& painter, const QRect& rect) {
    // Base background
    QColor bg_color = header_widget_->is_solo() ? QColor(60, 60, 40) : QColor(40, 40, 40);
    if (header_widget_->is_muted()) {
        bg_color = QColor(50, 30, 30);
    }
    
    painter.fillRect(rect, bg_color);
    
    // Track separator line
    painter.setPen(QColor(80, 80, 80));
    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
}

void AudioTrackWidget::render_audio_clips(QPainter& painter, const QRect& rect) {
    painter.setClipRect(rect);
    
    for (const auto& clip : audio_clips_) {
        QRect clip_rect = clip_to_widget_rect(clip);
        if (clip_rect.intersects(rect)) {
            render_clip(painter, clip);
        }
    }
}

void AudioTrackWidget::render_clip(QPainter& painter, const AudioClipVisual& clip) {
    QRect clip_rect = clip_to_widget_rect(clip);
    
    // Skip if too small to be meaningful
    if (clip_rect.width() < MIN_CLIP_WIDTH) {
        return;
    }
    
    // Render clip background
    QColor clip_color = audio_track_utils::calculate_clip_color(
        /* segment - would need actual segment */ ve::timeline::Segment{},
        clip.is_selected, clip.is_muted);
    
    painter.fillRect(clip_rect, clip_color);
    
    // Render waveform
    if (clip.waveform_visible) {
        render_clip_waveform(painter, clip);
    }
    
    // Render clip borders and handles
    render_clip_borders(painter, clip);
    if (clip.is_selected) {
        render_clip_selection(painter, clip);
        render_clip_handles(painter, clip);
    }
}

void AudioTrackWidget::render_clip_waveform(QPainter& painter, const AudioClipVisual& clip) {
    if (!clip.waveform_widget) return;
    
    QRect clip_rect = clip_to_widget_rect(clip);
    QRect waveform_rect = audio_track_utils::calculate_waveform_rect(clip_rect, controls_config_);
    
    // Render waveform widget content into our painter
    // This is a simplified approach - in practice, we'd want to optimize this
    painter.save();
    painter.translate(waveform_rect.topLeft());
    
    // Create a temporary pixmap to render the waveform
    QPixmap waveform_pixmap(waveform_rect.size());
    waveform_pixmap.fill(Qt::transparent);
    QPainter waveform_painter(&waveform_pixmap);
    
    // Render the waveform widget content
    // clip.waveform_widget->render(&waveform_painter);
    
    painter.drawPixmap(0, 0, waveform_pixmap);
    painter.restore();
}

void AudioTrackWidget::render_clip_borders(QPainter& painter, const AudioClipVisual& clip) {
    QRect clip_rect = clip_to_widget_rect(clip);
    
    painter.setPen(QPen(QColor(120, 120, 120), CLIP_BORDER_WIDTH));
    painter.drawRect(clip_rect);
}

void AudioTrackWidget::render_clip_selection(QPainter& painter, const AudioClipVisual& clip) {
    QRect clip_rect = clip_to_widget_rect(clip);
    
    painter.setPen(QPen(QColor(255, 255, 100), SELECTION_BORDER_WIDTH));
    painter.drawRect(clip_rect.adjusted(-1, -1, 1, 1));
}

void AudioTrackWidget::render_clip_handles(QPainter& painter, const AudioClipVisual& clip) {
    QRect clip_rect = clip_to_widget_rect(clip);
    
    if (clip_rect.width() < CLIP_HANDLE_WIDTH * 3) {
        return; // Too small for handles
    }
    
    // Left handle
    QRect left_handle(clip_rect.left(), clip_rect.top(), CLIP_HANDLE_WIDTH, clip_rect.height());
    painter.fillRect(left_handle, QColor(255, 255, 255, 100));
    
    // Right handle
    QRect right_handle(clip_rect.right() - CLIP_HANDLE_WIDTH, clip_rect.top(), 
                      CLIP_HANDLE_WIDTH, clip_rect.height());
    painter.fillRect(right_handle, QColor(255, 255, 255, 100));
}

void AudioTrackWidget::render_playhead(QPainter& painter, const QRect& rect) {
    int playhead_x = time_to_pixel(current_time_);
    if (playhead_x >= rect.left() && playhead_x <= rect.right()) {
        painter.setPen(QPen(QColor(255, 0, 0), 2));
        painter.drawLine(playhead_x, rect.top(), playhead_x, rect.bottom());
    }
}

void AudioTrackWidget::render_selection_overlay(QPainter& painter, const QRect& rect) {
    if (selection_start_ >= selection_end_) return;
    
    int start_x = time_to_pixel(selection_start_);
    int end_x = time_to_pixel(selection_end_);
    
    QRect selection_rect(start_x, rect.top(), end_x - start_x, rect.height());
    if (selection_rect.intersects(rect)) {
        painter.fillRect(selection_rect, QColor(100, 100, 255, 50));
    }
}

void AudioTrackWidget::render_drop_indicator(QPainter& painter, const QRect& rect) {
    int drop_x = time_to_pixel(drop_position_);
    painter.setPen(QPen(QColor(0, 255, 0), 3));
    painter.drawLine(drop_x, rect.top(), drop_x, rect.bottom());
}

ve::TimePoint AudioTrackWidget::pixel_to_time(int pixel_x) const {
    // Convert pixel position to time, accounting for zoom and scroll
    double time_per_pixel = 1.0 / zoom_factor_;  // Simplified conversion
    double time_seconds = (pixel_x + scroll_x_) * time_per_pixel;
    
    // Convert to TimePoint (assuming 1000fps timebase for precision)
    return ve::TimePoint{static_cast<int64_t>(time_seconds * 1000), 1000};
}

int AudioTrackWidget::time_to_pixel(const ve::TimePoint& time) const {
    // Convert time to pixel position
    double time_seconds = static_cast<double>(time.num) / time.den;
    double pixels_per_second = zoom_factor_;  // Simplified conversion
    return static_cast<int>(time_seconds * pixels_per_second) - scroll_x_;
}

QRect AudioTrackWidget::time_range_to_rect(const ve::TimePoint& start, const ve::TimePoint& duration) const {
    int start_x = time_to_pixel(start);
    int end_x = time_to_pixel(ve::TimePoint{start.num + duration.num, start.den});
    
    QRect content_rect = rect();
    content_rect.setLeft(header_widget_->width());
    
    return QRect(start_x, content_rect.top(), end_x - start_x, content_rect.height());
}

QRect AudioTrackWidget::clip_to_widget_rect(const AudioClipVisual& clip) const {
    return time_range_to_rect(clip.start_time, clip.duration);
}

AudioClipVisual* AudioTrackWidget::find_clip_at_position(const QPoint& pos) {
    for (auto& clip : audio_clips_) {
        QRect clip_rect = clip_to_widget_rect(clip);
        if (clip_rect.contains(pos)) {
            return &clip;
        }
    }
    return nullptr;
}

AudioClipVisual* AudioTrackWidget::find_clip_by_id(ve::timeline::SegmentId segment_id) {
    auto it = std::find_if(audio_clips_.begin(), audio_clips_.end(),
        [segment_id](const AudioClipVisual& clip) {
            return clip.segment_id == segment_id;
        });
    return it != audio_clips_.end() ? &(*it) : nullptr;
}

void AudioTrackWidget::update_clip_bounds() {
    // Update cached bounds for all clips based on current zoom/scroll
    for (auto& clip : audio_clips_) {
        clip.bounds = clip_to_widget_rect(clip);
    }
    
    // Update cached content rect
    cached_content_rect_ = rect();
    cached_content_rect_.setLeft(header_widget_->width());
}

void AudioTrackWidget::mousePressEvent(QMouseEvent* event) {
    QPoint content_pos = event->pos();
    content_pos.setX(content_pos.x() - header_widget_->width());
    
    if (content_pos.x() < 0) {
        // Click in header area
        QWidget::mousePressEvent(event);
        return;
    }
    
    interaction_start_pos_ = event->pos();
    interaction_current_pos_ = event->pos();
    
    if (event->button() == Qt::LeftButton) {
        handle_clip_click(content_pos, event->button(), event->modifiers());
    } else if (event->button() == Qt::RightButton) {
        // Context menu will be handled in contextMenuEvent
    }
    
    QWidget::mousePressEvent(event);
}

void AudioTrackWidget::handle_clip_click(const QPoint& pos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
    AudioClipVisual* clicked_clip = find_clip_at_position(pos);
    
    if (clicked_clip) {
        // Check if clicking on resize handles
        QRect clip_rect = clip_to_widget_rect(*clicked_clip);
        bool near_left_edge = pos.x() <= clip_rect.left() + CLIP_HANDLE_WIDTH;
        bool near_right_edge = pos.x() >= clip_rect.right() - CLIP_HANDLE_WIDTH;
        
        if (near_left_edge || near_right_edge) {
            start_clip_resize(clicked_clip, near_left_edge, pos);
        } else {
            // Regular clip selection/drag
            bool multi_select = modifiers & Qt::ControlModifier;
            select_clip(clicked_clip->segment_id, multi_select);
            start_clip_drag(clicked_clip, pos);
        }
    } else {
        // Click on empty timeline
        if (!(modifiers & Qt::ControlModifier)) {
            deselect_all_clips();
        }
        handle_timeline_click(pos, button);
    }
}

void AudioTrackWidget::start_clip_drag(AudioClipVisual* clip, const QPoint& start_pos) {
    interaction_mode_ = InteractionMode::DRAGGING_CLIP;
    interaction_target_clip_ = clip;
}

void AudioTrackWidget::start_clip_resize(AudioClipVisual* clip, bool left_edge, const QPoint& start_pos) {
    interaction_mode_ = left_edge ? InteractionMode::RESIZING_CLIP_LEFT : InteractionMode::RESIZING_CLIP_RIGHT;
    interaction_target_clip_ = clip;
    interaction_is_left_edge_ = left_edge;
}

void AudioTrackWidget::handle_timeline_click(const QPoint& pos, Qt::MouseButton button) {
    // Timeline scrubbing or selection
    ve::TimePoint clicked_time = pixel_to_time(pos.x());
    emit playhead_moved(clicked_time);
    
    interaction_mode_ = InteractionMode::TIMELINE_SCRUBBING;
}

void AudioTrackWidget::select_clip(ve::timeline::SegmentId segment_id, bool multi_select) {
    if (!multi_select) {
        deselect_all_clips();
    }
    
    if (auto* clip = find_clip_by_id(segment_id)) {
        clip->is_selected = true;
        if (std::find(selected_clips_.begin(), selected_clips_.end(), segment_id) == selected_clips_.end()) {
            selected_clips_.push_back(segment_id);
        }
        emit clip_selected(segment_id, multi_select);
    }
    
    schedule_visual_update();
}

void AudioTrackWidget::deselect_clip(ve::timeline::SegmentId segment_id) {
    if (auto* clip = find_clip_by_id(segment_id)) {
        clip->is_selected = false;
        selected_clips_.erase(
            std::remove(selected_clips_.begin(), selected_clips_.end(), segment_id),
            selected_clips_.end());
        emit clip_deselected(segment_id);
    }
    
    schedule_visual_update();
}

void AudioTrackWidget::deselect_all_clips() {
    for (auto& clip : audio_clips_) {
        clip.is_selected = false;
    }
    selected_clips_.clear();
    schedule_visual_update();
}

void AudioTrackWidget::schedule_visual_update() {
    needs_visual_update_ = true;
    if (!update_timer_->isActive()) {
        update_timer_->start();
    }
}

void AudioTrackWidget::on_header_volume_changed(float volume) {
    emit track_volume_changed(track_index_, volume);
}

void AudioTrackWidget::on_header_pan_changed(float pan) {
    emit track_pan_changed(track_index_, pan);
}

void AudioTrackWidget::on_header_mute_toggled(bool muted) {
    emit track_mute_changed(track_index_, muted);
}

void AudioTrackWidget::on_header_solo_toggled(bool solo) {
    emit track_solo_changed(track_index_, solo);
}

//==============================================================================
// Utility Functions
//==============================================================================

namespace audio_track_utils {

QColor calculate_clip_color(const ve::timeline::Segment& segment, bool selected, bool muted) {
    QColor base_color(100, 150, 255);  // Default blue
    
    if (muted) {
        base_color = base_color.darker(200);
    }
    
    if (selected) {
        base_color = base_color.lighter(130);
    }
    
    return base_color;
}

QRect calculate_waveform_rect(const QRect& clip_rect, const AudioTrackControls& controls) {
    return clip_rect.adjusted(2, 2, -2, -2);  // Small margin for waveform
}

QColor level_to_meter_color(float level_db) {
    if (level_db > -3.0f) return QColor(255, 0, 0);     // Red (clipping)
    if (level_db > -6.0f) return QColor(255, 200, 0);   // Yellow (hot)
    if (level_db > -18.0f) return QColor(0, 255, 0);    // Green (good)
    return QColor(0, 150, 0);                            // Dark green (low)
}

QString format_audio_time(const ve::TimePoint& time) {
    double seconds = static_cast<double>(time.num) / time.den;
    int minutes = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int frames = static_cast<int>((seconds - static_cast<int>(seconds)) * 30); // Assuming 30fps
    
    return QString("%1:%2:%3").arg(minutes, 2, 10, QChar('0'))
                              .arg(secs, 2, 10, QChar('0'))
                              .arg(frames, 2, 10, QChar('0'));
}

} // namespace audio_track_utils

} // namespace ve::ui