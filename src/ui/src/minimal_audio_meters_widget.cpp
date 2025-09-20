/**
 * @file minimal_audio_meters_widget.cpp
 * @brief Professional Audio Level Meters Widget Implementation
 */

#include "ui/minimal_audio_meters_widget.hpp"
#include <QtWidgets/QApplication>
#include <QtCore/QDebug>
#include <algorithm>

namespace ve::ui {

MinimalAudioMetersWidget::MinimalAudioMetersWidget(QWidget* parent)
    : QWidget(parent)
    , meter_type_(MeterType::PPM)
    , scale_type_(ScaleType::Broadcast)
    , update_rate_fps_(30)
    , peak_hold_time_ms_(2000)
    , meter_width_(25)
    , meter_spacing_(8)
    , show_labels_(true)
    , show_peak_values_(true)
    , show_rms_values_(false)
    , stereo_correlation_enabled_(false)
    , stereo_correlation_(0.0)
    , total_width_(0)
    , total_height_(150)
    , update_timer_(nullptr)
    , peak_hold_timer_(nullptr) {
    
    setup_ui();
    setup_timers();
    set_channel_count(2); // Default stereo setup
}

MinimalAudioMetersWidget::~MinimalAudioMetersWidget() {
    if (update_timer_) update_timer_->stop();
    if (peak_hold_timer_) peak_hold_timer_->stop();
}

void MinimalAudioMetersWidget::setup_ui() {
    setMinimumSize(100, 100);
    setFixedHeight(total_height_);
    
    // Set dark theme styling
    setStyleSheet(
        "MinimalAudioMetersWidget { "
        "background-color: #2b2b2b; "
        "border: 1px solid #555555; "
        "border-radius: 3px; "
        "}"
    );
    
    // Enable mouse tracking
    setMouseTracking(true);
}

void MinimalAudioMetersWidget::setup_timers() {
    // Main update timer
    update_timer_ = new QTimer(this);
    connect(update_timer_, &QTimer::timeout, this, &MinimalAudioMetersWidget::update_meters);
    
    // Peak hold decay timer
    peak_hold_timer_ = new QTimer(this);
    connect(peak_hold_timer_, &QTimer::timeout, this, &MinimalAudioMetersWidget::update_peak_hold);
    peak_hold_timer_->start(50); // 20Hz for smooth peak decay
    
    set_update_rate(update_rate_fps_);
}

void MinimalAudioMetersWidget::set_meter_type(MeterType type) {
    meter_type_ = type;
    update();
}

void MinimalAudioMetersWidget::set_scale_type(ScaleType type) {
    scale_type_ = type;
    update();
}

void MinimalAudioMetersWidget::set_channel_count(int count) {
    channels_.clear();
    channels_.resize(count);
    
    // Initialize channels with default values
    for (int i = 0; i < count; ++i) {
        channels_[i].name = QString("Ch %1").arg(i + 1);
        channels_[i].current_level = MIN_DB;
        channels_[i].peak_level = MIN_DB;
        channels_[i].rms_level = MIN_DB;
        channels_[i].peak_hold_time = 0;
        channels_[i].is_clipping = false;
        channels_[i].is_muted = false;
        channels_[i].meter_color = get_level_color(MIN_DB);
    }
    
    calculate_layout();
    update();
}

void MinimalAudioMetersWidget::set_update_rate(int fps) {
    update_rate_fps_ = qBound(10, fps, 120);
    
    if (update_timer_) {
        update_timer_->stop();
        update_timer_->start(1000 / update_rate_fps_);
    }
}

void MinimalAudioMetersWidget::set_peak_hold_time(int milliseconds) {
    peak_hold_time_ms_ = qMax(0, milliseconds);
}

void MinimalAudioMetersWidget::set_channel_name(int channel, const QString& name) {
    if (channel >= 0 && channel < channels_.size()) {
        channels_[channel].name = name;
        update();
    }
}

void MinimalAudioMetersWidget::set_channel_mute(int channel, bool muted) {
    if (channel >= 0 && channel < channels_.size()) {
        channels_[channel].is_muted = muted;
        update();
    }
}

QString MinimalAudioMetersWidget::get_channel_name(int channel) const {
    if (channel >= 0 && channel < channels_.size()) {
        return channels_[channel].name;
    }
    return QString();
}

bool MinimalAudioMetersWidget::is_channel_muted(int channel) const {
    if (channel >= 0 && channel < channels_.size()) {
        return channels_[channel].is_muted;
    }
    return false;
}

void MinimalAudioMetersWidget::update_levels(const std::vector<double>& levels) {
    for (int i = 0; i < static_cast<int>(levels.size()) && i < static_cast<int>(channels_.size()); ++i) {
        update_level(i, levels[i]);
    }
}

void MinimalAudioMetersWidget::update_level(int channel, double level_db) {
    if (channel < 0 || channel >= channels_.size()) return;
    
    ChannelMeter& meter = channels_[channel];
    
    // Apply ballistics
    apply_ballistics(channel, level_db);
    
    // Update peak hold
    update_peak_level(channel, level_db);
    
    // Update RMS (simplified)
    update_rms_level(channel, level_db);
    
    // Check for clipping
    if (level_db > CLIPPING_THRESHOLD) {
        if (!meter.is_clipping) {
            meter.is_clipping = true;
            emit clipping_detected(channel);
        }
    }
    
    // Calculate stereo correlation if enabled
    if (stereo_correlation_enabled_ && channels_.size() >= 2) {
        calculate_stereo_correlation();
    }
    
    emit level_changed(channel, meter.current_level);
}

void MinimalAudioMetersWidget::reset_peak_holds() {
    for (auto& channel : channels_) {
        channel.peak_level = MIN_DB;
        channel.peak_hold_time = 0;
        channel.is_clipping = false;
    }
    update();
}

void MinimalAudioMetersWidget::reset_clipping_indicators() {
    for (auto& channel : channels_) {
        channel.is_clipping = false;
    }
    update();
}

void MinimalAudioMetersWidget::enable_stereo_correlation(bool enabled) {
    stereo_correlation_enabled_ = enabled;
    calculate_layout();
    update();
}

double MinimalAudioMetersWidget::get_stereo_correlation() const {
    return stereo_correlation_;
}

void MinimalAudioMetersWidget::set_meter_width(int width) {
    meter_width_ = qMax(15, width);
    calculate_layout();
    update();
}

void MinimalAudioMetersWidget::set_meter_spacing(int spacing) {
    meter_spacing_ = qMax(2, spacing);
    calculate_layout();
    update();
}

void MinimalAudioMetersWidget::set_show_labels(bool show) {
    show_labels_ = show;
    update();
}

void MinimalAudioMetersWidget::set_show_peak_values(bool show) {
    show_peak_values_ = show;
    update();
}

void MinimalAudioMetersWidget::set_show_rms_values(bool show) {
    show_rms_values_ = show;
    update();
}

void MinimalAudioMetersWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw each channel meter
    for (int i = 0; i < channels_.size(); ++i) {
        QRect meter_rect = get_meter_rect(i);
        
        draw_meter_background(painter, meter_rect, i);
        draw_meter_scale(painter, meter_rect);
        draw_meter_level(painter, meter_rect, i);
        draw_peak_indicator(painter, meter_rect, i);
        draw_clipping_indicator(painter, meter_rect, i);
        
        if (show_labels_) {
            draw_channel_label(painter, meter_rect, i);
        }
        
        if (show_peak_values_ || show_rms_values_) {
            draw_level_values(painter, meter_rect, i);
        }
    }
    
    // Draw stereo correlation if enabled
    if (stereo_correlation_enabled_ && channels_.size() >= 2) {
        QRect correlation_rect = get_correlation_rect();
        if (!correlation_rect.isEmpty()) {
            draw_stereo_correlation(painter, correlation_rect);
        }
    }
}

void MinimalAudioMetersWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    calculate_layout();
}

void MinimalAudioMetersWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int channel = get_meter_channel_at_position(event->pos());
        if (channel >= 0) {
            // Reset peak hold for clicked channel
            channels_[channel].peak_level = MIN_DB;
            channels_[channel].peak_hold_time = 0;
            channels_[channel].is_clipping = false;
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        // Show context menu (future enhancement)
    }
    
    QWidget::mousePressEvent(event);
}

void MinimalAudioMetersWidget::contextMenuEvent(QContextMenuEvent* event) {
    // Future enhancement: context menu for meter configuration
    QWidget::contextMenuEvent(event);
}

void MinimalAudioMetersWidget::update_meters() {
    // This slot is called by the update timer
    // In a real implementation, this would trigger repainting
    // if there are any level changes
    update();
}

void MinimalAudioMetersWidget::update_peak_hold() {
    bool needs_update = false;
    
    for (auto& channel : channels_) {
        if (channel.peak_hold_time > 0) {
            channel.peak_hold_time -= 50; // Decay timer (50ms intervals)
            if (channel.peak_hold_time <= 0) {
                // Start peak decay
                channel.peak_level -= 1.0; // Decay by 1dB
                if (channel.peak_level < channel.current_level) {
                    channel.peak_level = channel.current_level;
                }
                needs_update = true;
            }
        }
    }
    
    if (needs_update) {
        update();
    }
}

void MinimalAudioMetersWidget::calculate_layout() {
    int channel_count = static_cast<int>(channels_.size());
    int channels_width = channel_count * meter_width_ + (channel_count - 1) * meter_spacing_;
    int correlation_width = stereo_correlation_enabled_ ? 30 : 0;
    int margin = 10;
    
    total_width_ = channels_width + correlation_width + margin * 2;
    setFixedWidth(total_width_);
    
    meters_area_ = QRect(margin, margin, channels_width, height() - margin * 2);
}

void MinimalAudioMetersWidget::draw_meter_background(QPainter& painter, const QRect& meter_rect, int /*channel*/) {
    // Draw meter background
    painter.fillRect(meter_rect, QColor(40, 40, 40));
    
    // Draw meter border
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawRect(meter_rect);
    
    // Draw scale marks
    painter.setPen(QPen(QColor(120, 120, 120), 1));
    for (double db = 0.0; db >= MIN_DB; db -= 10.0) {
        int y = db_to_pixel(db, meter_rect);
        painter.drawLine(meter_rect.left(), y, meter_rect.left() + 3, y);
        painter.drawLine(meter_rect.right() - 3, y, meter_rect.right(), y);
    }
}

void MinimalAudioMetersWidget::draw_meter_scale(QPainter& painter, const QRect& meter_rect) {
    // Draw scale labels on the right side
    painter.setPen(QColor(180, 180, 180));
    QFont font = painter.font();
    font.setPointSize(7);
    painter.setFont(font);
    
    for (double db = 0.0; db >= MIN_DB; db -= 10.0) {
        int y = db_to_pixel(db, meter_rect);
        QString label = (db == 0.0) ? "0" : QString("%1").arg(static_cast<int>(db));
        
        QRect text_rect(meter_rect.right() + 5, y - 6, 30, 12);
        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }
}

void MinimalAudioMetersWidget::draw_meter_level(QPainter& painter, const QRect& meter_rect, int channel) {
    const ChannelMeter& meter = channels_[channel];
    
    if (meter.is_muted) {
        // Draw muted indicator
        painter.fillRect(meter_rect.adjusted(2, 2, -2, -2), QColor(60, 60, 60));
        painter.setPen(QColor(120, 120, 120));
        painter.drawText(meter_rect, Qt::AlignCenter, "MUTE");
        return;
    }
    
    // Calculate level bar height
    int level_height = db_to_pixel(meter.current_level, meter_rect) - meter_rect.bottom();
    if (level_height > 0) {
        QRect level_rect(meter_rect.left() + 2, meter_rect.bottom() - level_height, 
                        meter_rect.width() - 4, level_height);
        
        // Create gradient based on level
        QLinearGradient gradient(level_rect.topLeft(), level_rect.bottomLeft());
        
        if (meter.current_level > CLIPPING_THRESHOLD) {
            // Clipping - red
            gradient.setColorAt(0.0, QColor(255, 100, 100));
            gradient.setColorAt(1.0, QColor(200, 0, 0));
        } else if (meter.current_level > WARNING_THRESHOLD) {
            // Warning zone - yellow to red
            gradient.setColorAt(0.0, QColor(255, 255, 100));
            gradient.setColorAt(1.0, QColor(255, 200, 0));
        } else if (meter.current_level > NORMAL_THRESHOLD) {
            // Normal zone - green to yellow
            gradient.setColorAt(0.0, QColor(100, 255, 100));
            gradient.setColorAt(1.0, QColor(0, 200, 0));
        } else {
            // Low zone - green
            gradient.setColorAt(0.0, QColor(50, 200, 50));
            gradient.setColorAt(1.0, QColor(0, 150, 0));
        }
        
        painter.fillRect(level_rect, gradient);
    }
}

void MinimalAudioMetersWidget::draw_peak_indicator(QPainter& painter, const QRect& meter_rect, int channel) {
    const ChannelMeter& meter = channels_[channel];
    
    if (meter.peak_level > MIN_DB) {
        int peak_y = db_to_pixel(meter.peak_level, meter_rect);
        QColor peak_color = get_peak_color(meter.peak_level);
        
        painter.setPen(QPen(peak_color, 2));
        painter.drawLine(meter_rect.left() + 1, peak_y, meter_rect.right() - 1, peak_y);
    }
}

void MinimalAudioMetersWidget::draw_clipping_indicator(QPainter& painter, const QRect& meter_rect, int channel) {
    const ChannelMeter& meter = channels_[channel];
    
    if (meter.is_clipping) {
        // Draw clipping indicator at top of meter
        QRect clip_rect(meter_rect.left(), meter_rect.top(), meter_rect.width(), 10);
        painter.fillRect(clip_rect, QColor(255, 0, 0));
        
        painter.setPen(QColor(255, 255, 255));
        QFont font = painter.font();
        font.setPointSize(6);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(clip_rect, Qt::AlignCenter, "CLIP");
    }
}

void MinimalAudioMetersWidget::draw_channel_label(QPainter& painter, const QRect& meter_rect, int channel) {
    const ChannelMeter& meter = channels_[channel];
    
    painter.setPen(QColor(200, 200, 200));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    
    QRect label_rect(meter_rect.left(), meter_rect.bottom() + 2, meter_rect.width(), 15);
    painter.drawText(label_rect, Qt::AlignCenter, meter.name);
}

void MinimalAudioMetersWidget::draw_level_values(QPainter& painter, const QRect& meter_rect, int channel) {
    const ChannelMeter& meter = channels_[channel];
    
    painter.setPen(QColor(180, 180, 180));
    QFont font = painter.font();
    font.setPointSize(6);
    painter.setFont(font);
    
    int y_offset = meter_rect.bottom() + 20;
    
    if (show_peak_values_) {
        QString peak_text = format_db_value(meter.peak_level);
        QRect peak_rect(meter_rect.left(), y_offset, meter_rect.width(), 10);
        painter.drawText(peak_rect, Qt::AlignCenter, peak_text);
        y_offset += 12;
    }
    
    if (show_rms_values_) {
        QString rms_text = format_db_value(meter.rms_level);
        QRect rms_rect(meter_rect.left(), y_offset, meter_rect.width(), 10);
        painter.drawText(rms_rect, Qt::AlignCenter, rms_text);
    }
}

void MinimalAudioMetersWidget::draw_stereo_correlation(QPainter& painter, const QRect& correlation_rect) {
    // Draw correlation meter background
    painter.fillRect(correlation_rect, QColor(40, 40, 40));
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawRect(correlation_rect);
    
    // Draw correlation scale (-1 to +1)
    painter.setPen(QColor(120, 120, 120));
    int center_y = correlation_rect.center().y();
    painter.drawLine(correlation_rect.left(), center_y, correlation_rect.right(), center_y);
    
    // Draw correlation value
    double correlation_norm = (stereo_correlation_ + 1.0) / 2.0; // Normalize to 0-1
    int correlation_y = correlation_rect.bottom() - static_cast<int>(correlation_norm * correlation_rect.height());
    
    QColor correlation_color = (stereo_correlation_ > 0.5) ? QColor(0, 255, 0) : 
                              (stereo_correlation_ < -0.5) ? QColor(255, 0, 0) : QColor(255, 255, 0);
    
    painter.setPen(QPen(correlation_color, 3));
    painter.drawLine(correlation_rect.left() + 2, correlation_y, correlation_rect.right() - 2, correlation_y);
    
    // Draw correlation label
    painter.setPen(QColor(200, 200, 200));
    QFont font = painter.font();
    font.setPointSize(6);
    painter.setFont(font);
    painter.drawText(correlation_rect.adjusted(0, correlation_rect.height() + 2, 0, 15), 
                    Qt::AlignCenter, "CORR");
}

double MinimalAudioMetersWidget::db_to_linear(double db) const {
    return std::pow(10.0, db / 20.0);
}

double MinimalAudioMetersWidget::linear_to_db(double linear) const {
    return 20.0 * std::log10(qMax(1e-10, linear));
}

int MinimalAudioMetersWidget::db_to_pixel(double db, const QRect& meter_rect) const {
    double normalized = (db - MIN_DB) / (MAX_DB - MIN_DB);
    normalized = qBound(0.0, normalized, 1.0);
    return meter_rect.bottom() - static_cast<int>(normalized * meter_rect.height());
}

double MinimalAudioMetersWidget::pixel_to_db(int pixel, const QRect& meter_rect) const {
    double normalized = static_cast<double>(meter_rect.bottom() - pixel) / meter_rect.height();
    normalized = qBound(0.0, normalized, 1.0);
    return MIN_DB + normalized * (MAX_DB - MIN_DB);
}

QColor MinimalAudioMetersWidget::get_level_color(double db) const {
    if (db > CLIPPING_THRESHOLD) {
        return QColor(255, 0, 0); // Red - clipping
    } else if (db > WARNING_THRESHOLD) {
        return QColor(255, 255, 0); // Yellow - warning
    } else if (db > NORMAL_THRESHOLD) {
        return QColor(0, 255, 0); // Green - normal
    } else {
        return QColor(0, 150, 0); // Dark green - low
    }
}

QColor MinimalAudioMetersWidget::get_peak_color(double db) const {
    if (db > CLIPPING_THRESHOLD) {
        return QColor(255, 100, 100); // Light red
    } else if (db > WARNING_THRESHOLD) {
        return QColor(255, 255, 100); // Light yellow
    } else {
        return QColor(100, 255, 100); // Light green
    }
}

QString MinimalAudioMetersWidget::format_db_value(double db) const {
    if (db <= MIN_DB) {
        return "-∞";
    } else if (db >= 0.0) {
        return QString("+%1").arg(db, 0, 'f', 1);
    } else {
        return QString("%1").arg(db, 0, 'f', 1);
    }
}

void MinimalAudioMetersWidget::apply_ballistics(int channel, double input_level) {
    ChannelMeter& meter = channels_[channel];
    
    double attack_time, release_time;
    
    switch (meter_type_) {
        case MeterType::VU:
            attack_time = VU_ATTACK_TIME;
            release_time = VU_RELEASE_TIME;
            break;
        case MeterType::PPM:
            attack_time = PPM_ATTACK_TIME;
            release_time = PPM_RELEASE_TIME;
            break;
        case MeterType::Digital:
        default:
            attack_time = DIGITAL_ATTACK_TIME;
            release_time = DIGITAL_RELEASE_TIME;
            break;
    }
    
    // Simple ballistics implementation
    double time_constant = (input_level > meter.current_level) ? attack_time : release_time;
    double alpha = 1.0 - std::exp(-1.0 / (time_constant * update_rate_fps_));
    
    meter.current_level = alpha * input_level + (1.0 - alpha) * meter.current_level;
}

void MinimalAudioMetersWidget::update_rms_level(int channel, double level) {
    ChannelMeter& meter = channels_[channel];
    
    // Simple RMS calculation (moving average)
    double alpha = 0.1; // Smoothing factor
    meter.rms_level = alpha * level + (1.0 - alpha) * meter.rms_level;
}

void MinimalAudioMetersWidget::update_peak_level(int channel, double level) {
    ChannelMeter& meter = channels_[channel];
    
    if (level > meter.peak_level) {
        meter.peak_level = level;
        meter.peak_hold_time = peak_hold_time_ms_;
        emit peak_detected(channel, level);
    }
}

void MinimalAudioMetersWidget::calculate_stereo_correlation() {
    if (channels_.size() < 2) {
        stereo_correlation_ = 0.0;
        return;
    }
    
    // Simple correlation calculation based on level difference
    double left_level = db_to_linear(channels_[0].current_level);
    double right_level = db_to_linear(channels_[1].current_level);
    
    double sum = left_level + right_level;
    double diff = left_level - right_level;
    
    if (sum > 1e-10) {
        stereo_correlation_ = 1.0 - (std::abs(diff) / sum);
    } else {
        stereo_correlation_ = 1.0;
    }
    
    stereo_correlation_ = qBound(-1.0, stereo_correlation_, 1.0);
    emit stereo_correlation_changed(stereo_correlation_);
}

QRect MinimalAudioMetersWidget::get_meter_rect(int channel) const {
    if (channel < 0 || channel >= channels_.size()) {
        return QRect();
    }
    
    int x = meters_area_.left() + channel * (meter_width_ + meter_spacing_);
    int y = meters_area_.top() + (show_labels_ ? 20 : 0);
    int height = meters_area_.height() - (show_labels_ ? 40 : 20) - 
                (show_peak_values_ || show_rms_values_ ? 25 : 0);
    
    return QRect(x, y, meter_width_, height);
}

QRect MinimalAudioMetersWidget::get_correlation_rect() const {
    if (!stereo_correlation_enabled_ || channels_.size() < 2) {
        return QRect();
    }
    
    int x = meters_area_.right() + meter_spacing_;
    int y = meters_area_.top() + 20;
    int height = meters_area_.height() - 60;
    
    return QRect(x, y, 25, height);
}

int MinimalAudioMetersWidget::get_meter_channel_at_position(const QPoint& pos) const {
    for (int i = 0; i < channels_.size(); ++i) {
        QRect meter_rect = get_meter_rect(i);
        if (meter_rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

} // namespace ve::ui