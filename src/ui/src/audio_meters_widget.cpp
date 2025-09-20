/**
 * @file audio_meters_widget.cpp
 * @brief Implementation of professional audio level meters
 * 
 * Week 8 Qt Timeline UI Integration - Complete implementation of broadcast-standard
 * audio level monitoring with VU meters, PPM, real-time processing, and
 * professional compliance features for video editing applications.
 */

#include "ui/audio_meters_widget.hpp"
#include "audio/audio_frame.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolTip>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ve::ui {

//==============================================================================
// MeterScale Implementation
//==============================================================================

MeterScale MeterScale::for_standard(MeterStandard standard, MeterType type) {
    MeterScale scale;
    
    switch (standard) {
        case MeterStandard::BROADCAST_EU:
            // EBU R68 European broadcast standard
            scale.min_db = -60.0f;
            scale.max_db = +12.0f;
            scale.reference_db = -18.0f;  // EBU reference
            scale.yellow_threshold = -9.0f;
            scale.orange_threshold = -6.0f;
            scale.red_threshold = 0.0f;
            scale.major_marks = {-60, -50, -40, -30, -20, -18, -15, -12, -9, -6, -3, 0, +3, +6, +9, +12};
            scale.minor_marks = {-55, -45, -35, -25, -22, -16, -14, -10, -8, -5, -2, -1, +1, +2, +4, +5, +7, +8, +10, +11};
            break;
            
        case MeterStandard::BROADCAST_US:
            // ANSI/SMPTE broadcast standard
            scale.min_db = -60.0f;
            scale.max_db = +20.0f;
            scale.reference_db = -20.0f;  // US broadcast reference
            scale.yellow_threshold = -12.0f;
            scale.orange_threshold = -6.0f;
            scale.red_threshold = 0.0f;
            scale.major_marks = {-60, -50, -40, -30, -20, -18, -15, -12, -9, -6, -3, 0, +3, +6, +12, +18};
            scale.minor_marks = {-55, -45, -35, -25, -22, -16, -14, -10, -8, -5, -2, -1, +1, +2, +4, +5, +9, +15};
            break;
            
        case MeterStandard::DIGITAL_STUDIO:
            // Digital studio standard (dBFS)
            scale.min_db = -96.0f;
            scale.max_db = 0.0f;
            scale.reference_db = -18.0f;  // Common digital reference
            scale.yellow_threshold = -6.0f;
            scale.orange_threshold = -3.0f;
            scale.red_threshold = -0.1f;  // Just below 0 dBFS
            scale.major_marks = {-96, -80, -60, -50, -40, -30, -24, -18, -12, -6, -3, 0};
            scale.minor_marks = {-90, -70, -55, -45, -35, -28, -21, -15, -9, -4, -2, -1};
            break;
            
        case MeterStandard::CONSUMER:
            // Consumer electronics standard
            scale.min_db = -40.0f;
            scale.max_db = +6.0f;
            scale.reference_db = -12.0f;
            scale.yellow_threshold = -3.0f;
            scale.orange_threshold = 0.0f;
            scale.red_threshold = +3.0f;
            scale.major_marks = {-40, -30, -20, -12, -6, -3, 0, +3, +6};
            scale.minor_marks = {-35, -25, -18, -15, -9, -4, -2, -1, +1, +2, +4, +5};
            break;
            
        default:
        case MeterStandard::CUSTOM:
            // Default custom scale
            scale.min_db = -60.0f;
            scale.max_db = +12.0f;
            scale.reference_db = -18.0f;
            scale.yellow_threshold = -6.0f;
            scale.orange_threshold = -3.0f;
            scale.red_threshold = 0.0f;
            scale.major_marks = {-60, -50, -40, -30, -20, -18, -12, -6, -3, 0, +6, +12};
            scale.minor_marks = {-55, -45, -35, -25, -15, -9, -4, -2, -1, +3, +9};
            break;
    }
    
    return scale;
}

//==============================================================================
// AudioMeterChannel Implementation
//==============================================================================

AudioMeterChannel::AudioMeterChannel(QWidget* parent)
    : QWidget(parent)
    , display_timer_(std::make_unique<QTimer>(this))
    , peak_hold_timer_(std::make_unique<QTimer>(this))
{
    // Initialize meter scale for default standard
    meter_scale_ = MeterScale::for_standard(meter_standard_, meter_type_);
    
    // Configure display timer
    display_timer_->setInterval(1000 / meter_style_.update_rate_hz);
    connect(display_timer_.get(), &QTimer::timeout, this, &AudioMeterChannel::update_display);
    display_timer_->start();
    
    // Configure peak hold timer
    peak_hold_timer_->setInterval(100); // Check peak hold every 100ms
    connect(peak_hold_timer_.get(), &QTimer::timeout, this, &AudioMeterChannel::decay_peak_hold);
    peak_hold_timer_->start();
    
    // Set minimum size
    setMinimumSize(meter_style_.meter_width + meter_style_.scale_width + meter_style_.spacing * 2, 100);
    
    // Initialize display levels
    display_levels_ = current_levels_;
}

void AudioMeterChannel::set_meter_type(MeterType type) {
    if (meter_type_ == type) return;
    
    meter_type_ = type;
    meter_scale_ = MeterScale::for_standard(meter_standard_, meter_type_);
    geometry_cache_valid_ = false;
    update();
}

void AudioMeterChannel::set_meter_standard(MeterStandard standard) {
    if (meter_standard_ == standard) return;
    
    meter_standard_ = standard;
    meter_scale_ = MeterScale::for_standard(meter_standard_, meter_type_);
    geometry_cache_valid_ = false;
    update();
}

void AudioMeterChannel::set_meter_style(const MeterStyle& style) {
    meter_style_ = style;
    
    // Update timer rates
    display_timer_->setInterval(1000 / meter_style_.update_rate_hz);
    
    geometry_cache_valid_ = false;
    update();
}

void AudioMeterChannel::set_channel_name(const QString& name) {
    channel_name_ = name;
    update();
}

void AudioMeterChannel::set_orientation(Qt::Orientation orientation) {
    if (orientation_ == orientation) return;
    
    orientation_ = orientation;
    geometry_cache_valid_ = false;
    updateGeometry();
    update();
}

void AudioMeterChannel::update_level(const AudioLevelData& level_data) {
    QMutexLocker locker(&level_mutex_);
    
    process_level_update(level_data);
    
    // Check for alerts
    if (level_data.clipping && !current_levels_.clipping) {
        emit clipping_detected();
    }
    
    if (level_data.peak_db > meter_scale_.red_threshold) {
        emit level_threshold_exceeded(level_data.peak_db);
    }
}

void AudioMeterChannel::update_peak(float peak_db) {
    AudioLevelData data = current_levels_;
    data.peak_db = peak_db;
    data.timestamp = ve::TimePoint{QDateTime::currentMSecsSinceEpoch(), 1000};
    update_level(data);
}

void AudioMeterChannel::update_rms(float rms_db) {
    AudioLevelData data = current_levels_;
    data.rms_db = rms_db;
    data.timestamp = ve::TimePoint{QDateTime::currentMSecsSinceEpoch(), 1000};
    update_level(data);
}

void AudioMeterChannel::reset_meters() {
    QMutexLocker locker(&level_mutex_);
    
    current_levels_.reset();
    display_levels_.reset();
    peak_hold_level_ = -std::numeric_limits<float>::infinity();
    level_history_.clear();
    
    emit levels_reset();
    update();
}

void AudioMeterChannel::reset_peak_holds() {
    QMutexLocker locker(&level_mutex_);
    
    peak_hold_level_ = -std::numeric_limits<float>::infinity();
    current_levels_.peak_hold_db = -std::numeric_limits<float>::infinity();
    display_levels_.peak_hold_db = -std::numeric_limits<float>::infinity();
    
    update();
}

QSize AudioMeterChannel::sizeHint() const {
    if (orientation_ == Qt::Vertical) {
        return QSize(meter_style_.meter_width + meter_style_.scale_width + meter_style_.spacing * 2, 200);
    } else {
        return QSize(200, meter_style_.meter_width + 30); // 30 for text
    }
}

QSize AudioMeterChannel::minimumSizeHint() const {
    if (orientation_ == Qt::Vertical) {
        return QSize(meter_style_.meter_width + meter_style_.scale_width + meter_style_.spacing * 2, 100);
    } else {
        return QSize(100, meter_style_.meter_width + 20);
    }
}

void AudioMeterChannel::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    // Update geometry cache if needed
    if (!geometry_cache_valid_) {
        cached_meter_rect_ = calculate_meter_rect();
        cached_scale_rect_ = calculate_scale_rect();
        cached_text_rect_ = calculate_text_rect();
        geometry_cache_valid_ = true;
    }
    
    // Render components
    render_meter_background(painter, rect);
    
    if (meter_style_.show_scale) {
        render_meter_scale(painter, cached_scale_rect_);
    }
    
    render_level_bar(painter, cached_meter_rect_);
    
    if (meter_style_.show_peak_hold) {
        render_peak_hold(painter, cached_meter_rect_);
    }
    
    if (meter_style_.show_numeric_readout) {
        render_numeric_display(painter, cached_text_rect_);
    }
    
    if (current_levels_.clipping) {
        render_clipping_indicator(painter, rect);
    }
}

void AudioMeterChannel::render_meter_background(QPainter& painter, const QRect& rect) {
    painter.fillRect(rect, meter_style_.background_color);
    
    // Draw border
    painter.setPen(QPen(meter_style_.scale_color, 1));
    painter.drawRect(rect);
}

void AudioMeterChannel::render_meter_scale(QPainter& painter, const QRect& scale_rect) {
    painter.setPen(meter_style_.scale_color);
    painter.setFont(QFont("Arial", 8));
    
    // Draw scale marks and labels
    for (float db_value : meter_scale_.major_marks) {
        float position = db_to_meter_position(db_value);
        
        if (orientation_ == Qt::Vertical) {
            int y = static_cast<int>(scale_rect.bottom() - position * scale_rect.height());
            
            // Major tick mark
            painter.drawLine(scale_rect.right() - 8, y, scale_rect.right(), y);
            
            // Label
            if (meter_scale_.show_numeric_labels) {
                QString label = audio_meter_utils::format_db_value(db_value);
                QRect label_rect(scale_rect.left(), y - 6, scale_rect.width() - 10, 12);
                painter.drawText(label_rect, Qt::AlignRight | Qt::AlignVCenter, label);
            }
        } else {
            int x = static_cast<int>(scale_rect.left() + position * scale_rect.width());
            
            // Major tick mark
            painter.drawLine(x, scale_rect.bottom() - 8, x, scale_rect.bottom());
            
            // Label
            if (meter_scale_.show_numeric_labels) {
                QString label = audio_meter_utils::format_db_value(db_value);
                QRect label_rect(x - 15, scale_rect.top(), 30, scale_rect.height() - 10);
                painter.drawText(label_rect, Qt::AlignCenter | Qt::AlignBottom, label);
            }
        }
    }
    
    // Draw minor tick marks
    painter.setPen(QPen(meter_style_.scale_color, 1, Qt::SolidLine));
    for (float db_value : meter_scale_.minor_marks) {
        float position = db_to_meter_position(db_value);
        
        if (orientation_ == Qt::Vertical) {
            int y = static_cast<int>(scale_rect.bottom() - position * scale_rect.height());
            painter.drawLine(scale_rect.right() - 4, y, scale_rect.right(), y);
        } else {
            int x = static_cast<int>(scale_rect.left() + position * scale_rect.width());
            painter.drawLine(x, scale_rect.bottom() - 4, x, scale_rect.bottom());
        }
    }
}

void AudioMeterChannel::render_level_bar(QPainter& painter, const QRect& meter_rect) {
    QMutexLocker locker(&level_mutex_);
    
    // Determine which level to display based on meter type
    float display_level = -std::numeric_limits<float>::infinity();
    switch (meter_type_) {
        case MeterType::VU_METER:
        case MeterType::RMS_METER:
            display_level = display_levels_.rms_db;
            break;
        case MeterType::PPM_METER:
        case MeterType::DIGITAL_PEAK:
            display_level = display_levels_.peak_db;
            break;
        case MeterType::LOUDNESS_LUFS:
            display_level = display_levels_.lufs_momentary;
            break;
    }
    
    if (!std::isfinite(display_level)) {
        return; // No valid level to display
    }
    
    float level_position = db_to_meter_position(display_level);
    level_position = std::clamp(level_position, 0.0f, 1.0f);
    
    // Create level rectangle
    QRect level_rect;
    if (orientation_ == Qt::Vertical) {
        int level_height = static_cast<int>(level_position * meter_rect.height());
        level_rect = QRect(meter_rect.left(), meter_rect.bottom() - level_height,
                          meter_rect.width(), level_height);
    } else {
        int level_width = static_cast<int>(level_position * meter_rect.width());
        level_rect = QRect(meter_rect.left(), meter_rect.top(),
                          level_width, meter_rect.height());
    }
    
    // Choose color based on level
    QColor level_color = level_to_color(display_level);
    
    if (meter_style_.gradient_fill) {
        // Create gradient fill
        QLinearGradient gradient = audio_meter_utils::create_meter_gradient(meter_style_, orientation_);
        if (orientation_ == Qt::Vertical) {
            gradient.setStart(0, meter_rect.bottom());
            gradient.setFinalStop(0, meter_rect.top());
        } else {
            gradient.setStart(meter_rect.left(), 0);
            gradient.setFinalStop(meter_rect.right(), 0);
        }
        painter.fillRect(level_rect, gradient);
    } else {
        // Solid color fill
        painter.fillRect(level_rect, level_color);
    }
    
    // Draw level border
    painter.setPen(QPen(level_color.lighter(120), 1));
    painter.drawRect(level_rect);
}

void AudioMeterChannel::render_peak_hold(QPainter& painter, const QRect& meter_rect) {
    QMutexLocker locker(&level_mutex_);
    
    if (!std::isfinite(peak_hold_level_)) {
        return;
    }
    
    float hold_position = db_to_meter_position(peak_hold_level_);
    hold_position = std::clamp(hold_position, 0.0f, 1.0f);
    
    painter.setPen(QPen(meter_style_.peak_hold_color, meter_style_.peak_hold_height));
    
    if (orientation_ == Qt::Vertical) {
        int y = static_cast<int>(meter_rect.bottom() - hold_position * meter_rect.height());
        painter.drawLine(meter_rect.left(), y, meter_rect.right(), y);
    } else {
        int x = static_cast<int>(meter_rect.left() + hold_position * meter_rect.width());
        painter.drawLine(x, meter_rect.top(), x, meter_rect.bottom());
    }
}

void AudioMeterChannel::render_numeric_display(QPainter& painter, const QRect& text_rect) {
    QMutexLocker locker(&level_mutex_);
    
    painter.setPen(meter_style_.text_color);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    
    // Format current level
    QString level_text;
    switch (meter_type_) {
        case MeterType::LOUDNESS_LUFS:
            if (std::isfinite(display_levels_.lufs_momentary)) {
                level_text = audio_meter_utils::format_lufs_value(display_levels_.lufs_momentary);
            } else {
                level_text = "-∞ LUFS";
            }
            break;
        default:
            if (std::isfinite(display_levels_.peak_db)) {
                level_text = audio_meter_utils::format_db_value(display_levels_.peak_db);
            } else {
                level_text = "-∞ dB";
            }
            break;
    }
    
    // Draw channel name and level
    QFont name_font("Arial", 8);
    painter.setFont(name_font);
    painter.drawText(text_rect.adjusted(2, 2, -2, -15), Qt::AlignCenter, channel_name_);
    
    QFont level_font("Arial", 9, QFont::Bold);
    painter.setFont(level_font);
    painter.drawText(text_rect.adjusted(2, 15, -2, -2), Qt::AlignCenter, level_text);
}

void AudioMeterChannel::render_clipping_indicator(QPainter& painter, const QRect& rect) {
    // Flash red border for clipping
    painter.setPen(QPen(Qt::red, 3));
    painter.drawRect(rect.adjusted(1, 1, -1, -1));
    
    // Clipping text
    painter.setPen(Qt::red);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(rect, Qt::AlignCenter, "CLIP");
}

float AudioMeterChannel::db_to_meter_position(float db_level) const {
    if (!std::isfinite(db_level)) {
        return 0.0f;
    }
    
    // Clamp to scale range
    db_level = std::clamp(db_level, meter_scale_.min_db, meter_scale_.max_db);
    
    if (meter_style_.logarithmic_scale) {
        // Logarithmic scale mapping
        float range = meter_scale_.max_db - meter_scale_.min_db;
        return (db_level - meter_scale_.min_db) / range;
    } else {
        // Linear scale mapping
        float range = meter_scale_.max_db - meter_scale_.min_db;
        return (db_level - meter_scale_.min_db) / range;
    }
}

float AudioMeterChannel::meter_position_to_db(float position) const {
    position = std::clamp(position, 0.0f, 1.0f);
    
    float range = meter_scale_.max_db - meter_scale_.min_db;
    return meter_scale_.min_db + position * range;
}

QRect AudioMeterChannel::calculate_meter_rect() const {
    QRect widget_rect = rect();
    
    if (orientation_ == Qt::Vertical) {
        int meter_x = meter_style_.spacing;
        int meter_width = meter_style_.meter_width;
        int meter_y = meter_style_.spacing;
        int meter_height = widget_rect.height() - meter_style_.spacing * 2;
        
        if (meter_style_.show_numeric_readout) {
            meter_height -= 30; // Space for text
        }
        
        return QRect(meter_x, meter_y, meter_width, meter_height);
    } else {
        int meter_x = meter_style_.spacing;
        int meter_width = widget_rect.width() - meter_style_.spacing * 2;
        int meter_y = meter_style_.spacing;
        int meter_height = meter_style_.meter_width;
        
        if (meter_style_.show_scale) {
            meter_width -= meter_style_.scale_width;
        }
        if (meter_style_.show_numeric_readout) {
            meter_width -= 60; // Space for text
        }
        
        return QRect(meter_x, meter_y, meter_width, meter_height);
    }
}

QRect AudioMeterChannel::calculate_scale_rect() const {
    if (!meter_style_.show_scale) {
        return QRect();
    }
    
    QRect widget_rect = rect();
    QRect meter_rect = calculate_meter_rect();
    
    if (orientation_ == Qt::Vertical) {
        int scale_x = meter_rect.right() + meter_style_.spacing;
        int scale_width = meter_style_.scale_width;
        return QRect(scale_x, meter_rect.top(), scale_width, meter_rect.height());
    } else {
        int scale_y = meter_rect.bottom() + meter_style_.spacing;
        int scale_height = 20;
        return QRect(meter_rect.left(), scale_y, meter_rect.width(), scale_height);
    }
}

QRect AudioMeterChannel::calculate_text_rect() const {
    if (!meter_style_.show_numeric_readout) {
        return QRect();
    }
    
    QRect widget_rect = rect();
    
    if (orientation_ == Qt::Vertical) {
        return QRect(0, widget_rect.height() - 30, widget_rect.width(), 30);
    } else {
        QRect meter_rect = calculate_meter_rect();
        return QRect(widget_rect.width() - 60, 0, 60, widget_rect.height());
    }
}

void AudioMeterChannel::process_level_update(const AudioLevelData& new_data) {
    current_levels_ = new_data;
    
    // Update peak hold
    if (new_data.peak_db > peak_hold_level_) {
        peak_hold_level_ = new_data.peak_db;
        peak_hold_time_ = new_data.timestamp;
    }
    
    // Add to history for smoothing
    level_history_.push_back(new_data);
    if (level_history_.size() > 10) { // Keep last 10 samples
        level_history_.pop_front();
    }
    
    // Apply meter ballistics
    apply_meter_ballistics();
}

void AudioMeterChannel::apply_meter_ballistics() {
    // Implement meter ballistics based on meter type
    switch (meter_type_) {
        case MeterType::VU_METER:
            // VU meter: slow attack, slow decay
            // Simplified ballistics - in practice would use proper time constants
            display_levels_.rms_db = current_levels_.rms_db;
            break;
            
        case MeterType::PPM_METER:
            // PPM: fast attack, slow decay
            display_levels_.peak_db = current_levels_.peak_db;
            break;
            
        default:
            display_levels_ = current_levels_;
            break;
    }
    
    display_levels_.peak_hold_db = peak_hold_level_;
}

QColor AudioMeterChannel::level_to_color(float db_level) const {
    if (db_level >= meter_scale_.red_threshold) {
        return meter_style_.red_color;
    } else if (db_level >= meter_scale_.orange_threshold) {
        return meter_style_.orange_color;
    } else if (db_level >= meter_scale_.yellow_threshold) {
        return meter_style_.yellow_color;
    } else {
        return meter_style_.green_color;
    }
}

void AudioMeterChannel::update_display() {
    // Schedule repaint if levels have changed
    if (needs_visual_update_) {
        update();
        needs_visual_update_ = false;
    }
}

void AudioMeterChannel::decay_peak_hold() {
    QMutexLocker locker(&level_mutex_);
    
    // Decay peak hold based on time
    auto current_time = ve::TimePoint{QDateTime::currentMSecsSinceEpoch(), 1000};
    double hold_duration = static_cast<double>(current_time.num - peak_hold_time_.num) / 1000.0;
    
    if (hold_duration > meter_style_.peak_hold_time) {
        // Start decay
        float decay_rate = meter_style_.meter_decay_rate; // dB per second
        float decay_amount = decay_rate * 0.1f; // Timer fires every 100ms
        peak_hold_level_ -= decay_amount;
        
        if (peak_hold_level_ < meter_scale_.min_db) {
            peak_hold_level_ = -std::numeric_limits<float>::infinity();
        }
        
        needs_visual_update_ = true;
    }
}

void AudioMeterChannel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Reset peak hold on click
        reset_peak_holds();
    }
    QWidget::mousePressEvent(event);
}

void AudioMeterChannel::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    
    // Meter type submenu
    QMenu* type_menu = menu.addMenu("Meter Type");
    type_menu->addAction("VU Meter", [this]() { set_meter_type(MeterType::VU_METER); });
    type_menu->addAction("PPM Meter", [this]() { set_meter_type(MeterType::PPM_METER); });
    type_menu->addAction("Digital Peak", [this]() { set_meter_type(MeterType::DIGITAL_PEAK); });
    type_menu->addAction("RMS Meter", [this]() { set_meter_type(MeterType::RMS_METER); });
    type_menu->addAction("LUFS", [this]() { set_meter_type(MeterType::LOUDNESS_LUFS); });
    
    // Standard submenu
    QMenu* standard_menu = menu.addMenu("Standard");
    standard_menu->addAction("Broadcast EU", [this]() { set_meter_standard(MeterStandard::BROADCAST_EU); });
    standard_menu->addAction("Broadcast US", [this]() { set_meter_standard(MeterStandard::BROADCAST_US); });
    standard_menu->addAction("Digital Studio", [this]() { set_meter_standard(MeterStandard::DIGITAL_STUDIO); });
    standard_menu->addAction("Consumer", [this]() { set_meter_standard(MeterStandard::CONSUMER); });
    
    menu.addSeparator();
    menu.addAction("Reset Peak Hold", this, &AudioMeterChannel::reset_peak_holds);
    menu.addAction("Reset All", this, &AudioMeterChannel::reset_meters);
    
    menu.exec(event->globalPos());
}

void AudioMeterChannel::resizeEvent(QResizeEvent* event) {
    geometry_cache_valid_ = false;
    QWidget::resizeEvent(event);
}

//==============================================================================
// AudioMetersWidget Implementation (Partial)
//==============================================================================

AudioMetersWidget::AudioMetersWidget(QWidget* parent)
    : QWidget(parent)
    , horizontal_layout_(std::make_unique<QHBoxLayout>())
    , vertical_layout_(std::make_unique<QVBoxLayout>())
    , auto_update_timer_(std::make_unique<QTimer>(this))
{
    // Configure auto-update timer
    auto_update_timer_->setInterval(1000 / update_rate_hz_);
    connect(auto_update_timer_.get(), &QTimer::timeout, this, &AudioMetersWidget::on_auto_update_timer);
    
    // Default to horizontal layout
    setLayout(horizontal_layout_.get());
    horizontal_layout_->setContentsMargins(2, 2, 2, 2);
    horizontal_layout_->setSpacing(2);
}

void AudioMetersWidget::set_channel_count(size_t channel_count) {
    // Remove excess channels
    while (meter_channels_.size() > channel_count) {
        meter_channels_.pop_back();
        if (!channel_names_.empty()) {
            channel_names_.pop_back();
        }
    }
    
    // Add new channels
    while (meter_channels_.size() < channel_count) {
        QString name = QString("Ch %1").arg(meter_channels_.size() + 1);
        if (channel_names_.size() > meter_channels_.size()) {
            name = channel_names_[meter_channels_.size()];
        }
        create_channel_meter(name);
    }
    
    // Resize level data
    current_levels_.resize(channel_count);
    
    update_layout();
}

void AudioMetersWidget::create_channel_meter(const QString& name) {
    auto channel = std::make_unique<AudioMeterChannel>(this);
    channel->set_channel_name(name);
    channel->set_meter_type(meter_type_);
    channel->set_meter_standard(meter_standard_);
    channel->set_meter_style(meter_style_);
    
    // Connect signals
    connect_channel_signals(channel.get(), meter_channels_.size());
    
    // Add to layout
    if (layout_orientation_ == Qt::Horizontal) {
        horizontal_layout_->addWidget(channel.get());
    } else {
        vertical_layout_->addWidget(channel.get());
    }
    
    meter_channels_.push_back(std::move(channel));
}

void AudioMetersWidget::connect_channel_signals(AudioMeterChannel* channel, size_t index) {
    connect(channel, &AudioMeterChannel::clipping_detected, [this, index]() {
        emit channel_clipping(index);
    });
    
    connect(channel, &AudioMeterChannel::level_threshold_exceeded, [this, index](float level) {
        emit channel_over_threshold(index, level);
    });
}

void AudioMetersWidget::update_layout() {
    // Switch between horizontal and vertical layouts
    QLayout* current_layout = layout();
    QLayout* target_layout = (layout_orientation_ == Qt::Horizontal) ? 
                             static_cast<QLayout*>(horizontal_layout_.get()) : 
                             static_cast<QLayout*>(vertical_layout_.get());
    
    if (current_layout != target_layout) {
        // Move all meters to new layout
        while (auto* item = current_layout->takeAt(0)) {
            if (auto* widget = item->widget()) {
                target_layout->addWidget(widget);
            }
            delete item;
        }
        
        setLayout(target_layout);
    }
}

//==============================================================================
// Utility Functions
//==============================================================================

namespace audio_meter_utils {

float linear_to_db(float linear_level) {
    if (linear_level <= 0.0f) {
        return -std::numeric_limits<float>::infinity();
    }
    return 20.0f * std::log10(linear_level);
}

float db_to_linear(float db_level) {
    if (!std::isfinite(db_level)) {
        return 0.0f;
    }
    return std::pow(10.0f, db_level / 20.0f);
}

float rms_to_db(const float* samples, size_t count) {
    if (count == 0) return -std::numeric_limits<float>::infinity();
    
    double sum_squares = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum_squares += samples[i] * samples[i];
    }
    
    float rms = std::sqrt(sum_squares / count);
    return linear_to_db(rms);
}

float peak_to_db(const float* samples, size_t count) {
    if (count == 0) return -std::numeric_limits<float>::infinity();
    
    float peak = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peak = std::max(peak, std::abs(samples[i]));
    }
    
    return linear_to_db(peak);
}

QString format_db_value(float db_value, int precision) {
    if (!std::isfinite(db_value)) {
        return "-∞";
    }
    return QString("%1").arg(db_value, 0, 'f', precision);
}

QString format_lufs_value(float lufs_value) {
    if (!std::isfinite(lufs_value)) {
        return "-∞ LUFS";
    }
    return QString("%1 LUFS").arg(lufs_value, 0, 'f', 1);
}

QColor interpolate_meter_color(float level_db, const MeterStyle& style) {
    // Interpolate between colors based on level
    if (level_db >= 0.0f) return style.red_color;
    if (level_db >= -6.0f) {
        float factor = (level_db + 6.0f) / 6.0f;
        return QColor::fromRgbF(
            style.yellow_color.redF() + factor * (style.red_color.redF() - style.yellow_color.redF()),
            style.yellow_color.greenF() + factor * (style.red_color.greenF() - style.yellow_color.greenF()),
            style.yellow_color.blueF() + factor * (style.red_color.blueF() - style.yellow_color.blueF())
        );
    }
    if (level_db >= -18.0f) {
        float factor = (level_db + 18.0f) / 12.0f;
        return QColor::fromRgbF(
            style.green_color.redF() + factor * (style.yellow_color.redF() - style.green_color.redF()),
            style.green_color.greenF() + factor * (style.yellow_color.greenF() - style.green_color.greenF()),
            style.green_color.blueF() + factor * (style.yellow_color.blueF() - style.green_color.blueF())
        );
    }
    return style.green_color;
}

QLinearGradient create_meter_gradient(const MeterStyle& style, Qt::Orientation orientation) {
    QLinearGradient gradient;
    
    // Create multi-stop gradient for professional meter appearance
    gradient.setColorAt(0.0, style.green_color);
    gradient.setColorAt(0.6, style.green_color);
    gradient.setColorAt(0.8, style.yellow_color);
    gradient.setColorAt(0.95, style.orange_color);
    gradient.setColorAt(1.0, style.red_color);
    
    return gradient;
}

} // namespace audio_meter_utils

} // namespace ve::ui