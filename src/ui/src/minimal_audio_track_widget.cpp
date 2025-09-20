/**
 * @file minimal_audio_track_widget.cpp
 * @brief Enhanced Implementation of Professional Audio Track Widget
 */

#include "ui/minimal_audio_track_widget.hpp"
#include "ui/minimal_waveform_widget.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QApplication>
#include <QtGui/QAction>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QPainterPath>
#include <QtCore/QRect>
#include <QtCore/QTimer>
#include <algorithm>

namespace ve::ui {

MinimalAudioTrackWidget::MinimalAudioTrackWidget(QWidget* parent)
    : QWidget(parent)
    , waveform_widget_(nullptr)
    , layout_(nullptr)
    , context_menu_(nullptr)
    , update_timer_(new QTimer(this))
    , track_name_("Audio Track")
{
    setup_ui();
    setup_context_menu();
    
    // Set up update timer for smooth animations
    connect(update_timer_, &QTimer::timeout, this, &MinimalAudioTrackWidget::update_display);
    update_timer_->setInterval(33); // ~30 FPS
    update_timer_->start();
}

MinimalAudioTrackWidget::~MinimalAudioTrackWidget() = default;

void MinimalAudioTrackWidget::setup_ui() {
    // Create main layout
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(5, 2, 5, 2);
    layout_->setSpacing(5);
    
    // Create track header with controls
    QWidget* header_widget = new QWidget(this);
    header_widget->setFixedWidth(120);
    header_widget->setStyleSheet("background-color: #3a3a3a; border: 1px solid #555;");
    
    // Create waveform widget
    waveform_widget_ = new MinimalWaveformWidget(this);
    waveform_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    waveform_widget_->setFixedHeight(track_height_);
    
    // Add widgets to layout
    layout_->addWidget(header_widget);
    layout_->addWidget(waveform_widget_);
    
    setFixedHeight(track_height_ + 10); // Add margins
    setMinimumWidth(200);
}

void MinimalAudioTrackWidget::setup_context_menu() {
    context_menu_ = new QMenu(this);
    
    // Track actions
    QAction* mute_action = context_menu_->addAction("Toggle Mute");
    QAction* solo_action = context_menu_->addAction("Toggle Solo");
    context_menu_->addSeparator();
    
    // Clip actions
    QAction* delete_clip_action = context_menu_->addAction("Delete Clip");
    QAction* split_clip_action = context_menu_->addAction("Split Clip");
    
    // Connect actions
    connect(mute_action, &QAction::triggered, this, &MinimalAudioTrackWidget::on_mute_action_triggered);
    connect(solo_action, &QAction::triggered, this, &MinimalAudioTrackWidget::on_solo_action_triggered);
    connect(delete_clip_action, &QAction::triggered, this, &MinimalAudioTrackWidget::on_delete_clip_action_triggered);
    connect(split_clip_action, &QAction::triggered, this, &MinimalAudioTrackWidget::on_split_clip_action_triggered);
}

// Track configuration methods
void MinimalAudioTrackWidget::set_track_name(const QString& name) {
    track_name_ = name;
    update();
}

void MinimalAudioTrackWidget::set_track_color(const QColor& color) {
    track_color_ = color;
    update();
}

void MinimalAudioTrackWidget::set_waveform_generator(std::shared_ptr<ve::audio::WaveformGenerator> generator) {
    if (waveform_widget_) {
        waveform_widget_->set_waveform_generator(generator);
    }
}

void MinimalAudioTrackWidget::set_audio_duration(double duration_seconds) {
    audio_duration_seconds_ = duration_seconds;
    update_waveform_display();
}

void MinimalAudioTrackWidget::set_track_height(int height) {
    track_height_ = height;
    if (waveform_widget_) {
        waveform_widget_->setFixedHeight(height);
    }
    setFixedHeight(height + 10);
    update();
}

// Audio clip management
void MinimalAudioTrackWidget::add_audio_clip(const AudioClip& clip) {
    audio_clips_.push_back(clip);
    update();
}

void MinimalAudioTrackWidget::remove_audio_clip(int clip_index) {
    if (clip_index >= 0 && clip_index < static_cast<int>(audio_clips_.size())) {
        audio_clips_.erase(audio_clips_.begin() + clip_index);
        if (selected_clip_index_ == clip_index) {
            selected_clip_index_ = -1;
        }
        update();
    }
}

void MinimalAudioTrackWidget::clear_audio_clips() {
    audio_clips_.clear();
    selected_clip_index_ = -1;
    update();
}

void MinimalAudioTrackWidget::select_clip(int clip_index) {
    if (clip_index >= 0 && clip_index < static_cast<int>(audio_clips_.size())) {
        // Deselect previous clip
        if (selected_clip_index_ >= 0 && selected_clip_index_ < static_cast<int>(audio_clips_.size())) {
            audio_clips_[selected_clip_index_].is_selected = false;
        }
        
        // Select new clip
        selected_clip_index_ = clip_index;
        audio_clips_[clip_index].is_selected = true;
        
        emit clip_selected(clip_index);
        update();
    }
}

void MinimalAudioTrackWidget::set_clip_fade(int clip_index, double fade_in, double fade_out) {
    if (clip_index >= 0 && clip_index < static_cast<int>(audio_clips_.size())) {
        audio_clips_[clip_index].fade_in = fade_in;
        audio_clips_[clip_index].fade_out = fade_out;
        update();
    }
}

// Timeline integration
void MinimalAudioTrackWidget::set_timeline_position(double position_seconds) {
    timeline_position_seconds_ = position_seconds;
    update();
}

void MinimalAudioTrackWidget::set_visible_time_range(double start_seconds, double duration_seconds) {
    visible_start_seconds_ = start_seconds;
    visible_duration_seconds_ = duration_seconds;
    update_waveform_display();
}

void MinimalAudioTrackWidget::set_timeline_zoom(double zoom_factor) {
    zoom_factor_ = zoom_factor;
    update_waveform_display();
}

// Track state
void MinimalAudioTrackWidget::set_track_muted(bool muted) {
    is_muted_ = muted;
    emit track_muted_changed(muted);
    update();
}

void MinimalAudioTrackWidget::set_track_solo(bool solo) {
    is_solo_ = solo;
    emit track_solo_changed(solo);
    update();
}

void MinimalAudioTrackWidget::set_track_selected(bool selected) {
    is_selected_ = selected;
    if (selected) {
        emit track_selected();
    }
    update();
}

bool MinimalAudioTrackWidget::is_track_selected() const {
    return is_selected_;
}

// Event handlers
void MinimalAudioTrackWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        last_mouse_pos_ = event->pos();
        
        // Check if clicking on a clip
        double click_time = pixel_to_time(event->x());
        int clip_index = find_clip_at_position(click_time);
        
        if (clip_index >= 0) {
            select_clip(clip_index);
            interaction_mode_ = InteractionMode::SelectingClip;
        } else {
            // Clicking on empty area - set playhead position
            timeline_position_seconds_ = click_time;
            emit playhead_position_changed(timeline_position_seconds_);
            interaction_mode_ = InteractionMode::DraggingPlayhead;
        }
        
        // Update selection state
        set_track_selected(true);
    }
    
    QWidget::mousePressEvent(event);
}

void MinimalAudioTrackWidget::mouseMoveEvent(QMouseEvent* event) {
    if (interaction_mode_ == InteractionMode::DraggingPlayhead) {
        double new_time = pixel_to_time(event->x());
        timeline_position_seconds_ = qMax(0.0, new_time);
        emit playhead_position_changed(timeline_position_seconds_);
        update();
    } else if (interaction_mode_ == InteractionMode::SelectingClip && selected_clip_index_ >= 0) {
        // Handle clip dragging (future enhancement)
        interaction_mode_ = InteractionMode::DraggingClip;
    }
    
    QWidget::mouseMoveEvent(event);
}

void MinimalAudioTrackWidget::mouseReleaseEvent(QMouseEvent* event) {
    interaction_mode_ = InteractionMode::None;
    QWidget::mouseReleaseEvent(event);
}

void MinimalAudioTrackWidget::contextMenuEvent(QContextMenuEvent* event) {
    double click_time = pixel_to_time(event->x());
    int clip_index = find_clip_at_position(click_time);
    
    if (clip_index >= 0) {
        emit clip_context_menu_requested(clip_index, event->globalPos());
    } else {
        emit track_context_menu_requested(event->globalPos());
    }
    
    if (context_menu_) {
        context_menu_->exec(event->globalPos());
    }
}

void MinimalAudioTrackWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    painter.fillRect(rect(), background_color_);
    
    // Draw selection highlight
    if (is_selected_) {
        painter.setPen(QPen(selected_color_, 2));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
    
    // Draw audio clips
    for (int i = 0; i < static_cast<int>(audio_clips_.size()); ++i) {
        draw_audio_clip(painter, audio_clips_[i], i);
    }
    
    // Draw playhead if within visible range
    if (timeline_position_seconds_ >= visible_start_seconds_ && 
        timeline_position_seconds_ <= visible_start_seconds_ + visible_duration_seconds_) {
        
        const int playhead_x = time_to_pixel(timeline_position_seconds_);
        painter.setPen(QPen(playhead_color_, 2));
        painter.drawLine(playhead_x, 0, playhead_x, height());
    }
}

void MinimalAudioTrackWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update_waveform_display();
}

// Helper methods
void MinimalAudioTrackWidget::update_waveform_display() {
    if (waveform_widget_) {
        waveform_widget_->set_time_range(visible_start_seconds_, visible_duration_seconds_);
        waveform_widget_->set_zoom_level(zoom_factor_);
        waveform_widget_->update();
    }
}

int MinimalAudioTrackWidget::find_clip_at_position(double time_seconds) const {
    for (int i = 0; i < static_cast<int>(audio_clips_.size()); ++i) {
        const AudioClip& clip = audio_clips_[i];
        if (time_seconds >= clip.start_time && time_seconds <= (clip.start_time + clip.duration)) {
            return i;
        }
    }
    return -1;
}

QRect MinimalAudioTrackWidget::get_clip_rect(const AudioClip& clip) const {
    QRect waveform_rect = get_waveform_rect();
    
    int start_x = time_to_pixel(clip.start_time);
    int end_x = time_to_pixel(clip.start_time + clip.duration);
    
    return QRect(start_x, waveform_rect.top(), end_x - start_x, waveform_rect.height());
}

void MinimalAudioTrackWidget::draw_audio_clip(QPainter& painter, const AudioClip& clip, int clip_index) {
    QRect clip_rect = get_clip_rect(clip);
    
    if (clip_rect.width() <= 0) return;
    
    // Use clip_index to avoid warning (could be used for debugging or future features)
    Q_UNUSED(clip_index);
    
    // Draw clip background
    QColor clip_color = clip.color;
    if (clip.is_muted) {
        clip_color = muted_color_;
    } else if (clip.is_selected) {
        clip_color = clip_color.lighter(120);
    }
    
    painter.fillRect(clip_rect, clip_color);
    
    // Draw clip border
    painter.setPen(QPen(clip_color.darker(150), 1));
    painter.drawRect(clip_rect);
    
    // Draw fade in/out
    if (clip.fade_in > 0.0 || clip.fade_out > 0.0) {
        draw_clip_fade(painter, clip_rect, clip.fade_in, clip.fade_out);
    }
    
    // Draw clip name
    if (clip_rect.width() > 50) {
        painter.setPen(QColor(255, 255, 255));
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);
        
        QRect text_rect = clip_rect.adjusted(5, 2, -5, -2);
        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignTop, clip.name);
    }
}

void MinimalAudioTrackWidget::draw_clip_fade(QPainter& painter, const QRect& clip_rect, double fade_in, double fade_out) {
    // Draw fade in
    if (fade_in > 0.0) {
        int fade_width = time_to_pixel(fade_in) - time_to_pixel(0.0);
        if (fade_width > 0 && fade_width < clip_rect.width()) {
            QRect fade_rect(clip_rect.left(), clip_rect.top(), fade_width, clip_rect.height());
            
            QPainterPath fade_path;
            fade_path.moveTo(fade_rect.bottomLeft());
            fade_path.lineTo(fade_rect.topRight());
            fade_path.lineTo(fade_rect.bottomRight());
            fade_path.closeSubpath();
            
            painter.fillPath(fade_path, QColor(0, 0, 0, 60));
        }
    }
    
    // Draw fade out
    if (fade_out > 0.0) {
        int fade_width = time_to_pixel(fade_out) - time_to_pixel(0.0);
        if (fade_width > 0 && fade_width < clip_rect.width()) {
            QRect fade_rect(clip_rect.right() - fade_width, clip_rect.top(), fade_width, clip_rect.height());
            
            QPainterPath fade_path;
            fade_path.moveTo(fade_rect.topLeft());
            fade_path.lineTo(fade_rect.bottomRight());
            fade_path.lineTo(fade_rect.bottomLeft());
            fade_path.closeSubpath();
            
            painter.fillPath(fade_path, QColor(0, 0, 0, 60));
        }
    }
}

// Coordinate conversion
double MinimalAudioTrackWidget::pixel_to_time(int pixel_x) const {
    QRect waveform_rect = get_waveform_rect();
    if (waveform_rect.width() <= 0) return 0.0;
    
    double relative_x = static_cast<double>(pixel_x - waveform_rect.left()) / waveform_rect.width();
    return visible_start_seconds_ + relative_x * visible_duration_seconds_;
}

int MinimalAudioTrackWidget::time_to_pixel(double time_seconds) const {
    QRect waveform_rect = get_waveform_rect();
    if (visible_duration_seconds_ <= 0.0) return waveform_rect.left();
    
    double relative_time = (time_seconds - visible_start_seconds_) / visible_duration_seconds_;
    return waveform_rect.left() + static_cast<int>(relative_time * waveform_rect.width());
}

QRect MinimalAudioTrackWidget::get_waveform_rect() const {
    QRect full_rect = rect();
    
    // Exclude header area
    if (layout_) {
        QLayoutItem* header_item = layout_->itemAt(0);
        if (header_item && header_item->widget()) {
            int header_width = header_item->widget()->width() + layout_->spacing();
            full_rect.setLeft(full_rect.left() + header_width);
        }
    }
    
    return full_rect;
}

QRect MinimalAudioTrackWidget::get_track_header_rect() const {
    if (layout_) {
        QLayoutItem* header_item = layout_->itemAt(0);
        if (header_item && header_item->widget()) {
            return header_item->widget()->geometry();
        }
    }
    
    return QRect(0, 0, 120, height());
}

// Context menu methods
void MinimalAudioTrackWidget::show_track_context_menu(const QPoint& position) {
    if (context_menu_) {
        context_menu_->exec(position);
    }
}

void MinimalAudioTrackWidget::show_clip_context_menu(int clip_index, const QPoint& position) {
    selected_clip_index_ = clip_index;
    if (context_menu_) {
        context_menu_->exec(position);
    }
}

// Context menu action slots
void MinimalAudioTrackWidget::on_mute_action_triggered() {
    set_track_muted(!is_muted_);
}

void MinimalAudioTrackWidget::on_solo_action_triggered() {
    set_track_solo(!is_solo_);
}

void MinimalAudioTrackWidget::on_delete_clip_action_triggered() {
    if (selected_clip_index_ >= 0) {
        remove_audio_clip(selected_clip_index_);
    }
}

void MinimalAudioTrackWidget::on_split_clip_action_triggered() {
    if (selected_clip_index_ >= 0 && selected_clip_index_ < static_cast<int>(audio_clips_.size())) {
        // Split clip at current playhead position (future enhancement)
        // For now, just emit a signal
        emit clip_context_menu_requested(selected_clip_index_, QPoint());
    }
}

void MinimalAudioTrackWidget::update_display() {
    // This slot is called by the update timer for smooth animations
    // Currently just triggers a repaint, but could be used for animations
    update();
}

} // namespace ve::ui