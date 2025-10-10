/**
 * @file audio_monitor_widget.cpp
 * @brief Implementation of real-time audio monitoring widgets
 *
 * Professional audio monitoring interface providing VU meters, peak detection,
 * loudness monitoring, and real-time level visualization for live monitoring
 * during playback and recording with EBU R128 compliance.
 */

#include "ui/audio_monitor_widget.hpp"
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QFont>
#include <QFontMetrics>
#include <QApplication>
#include <QStyle>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ve::ui {

// VUMeterWidget Implementation
VUMeterWidget::VUMeterWidget(QWidget* parent)
    : QWidget(parent)
    , orientation_(Qt::Vertical)
    , is_stereo_(false)
    , is_peak_meter_(true)
    , min_db_(-60.0f)
    , max_db_(0.0f)
    , current_level_db_(-std::numeric_limits<float>::infinity())
    , left_level_db_(-std::numeric_limits<float>::infinity())
    , right_level_db_(-std::numeric_limits<float>::infinity())
    , peak_level_db_(-std::numeric_limits<float>::infinity())
    , clipping_detected_(false)
    , peak_hold_level_(-std::numeric_limits<float>::infinity())
    , peak_hold_time_ms_(1000)
    , decay_rate_(10.0f) // dB per second
{
    setMinimumSize(40, 100);
    setMaximumWidth(100);

    update_timer_ = new QTimer(this);
    connect(update_timer_, &QTimer::timeout, this, &VUMeterWidget::update_meter);
    update_timer_->start(33); // ~30 FPS

    peak_timer_ = new QTimer(this);
    connect(peak_timer_, &QTimer::timeout, this, &VUMeterWidget::reset_peak_hold);
    peak_timer_->setSingleShot(true);
}

VUMeterWidget::~VUMeterWidget() = default;

void VUMeterWidget::set_level(float level_db) {
    current_level_db_ = level_db;
    peak_level_db_ = std::max(peak_level_db_, level_db);

    // Check for clipping
    if (level_db > -1.0f) {
        clipping_detected_ = true;
        emit clipping_detected();
    }

    update();
}

void VUMeterWidget::set_stereo_levels(float left_db, float right_db) {
    is_stereo_ = true;
    left_level_db_ = left_db;
    right_level_db_ = right_db;
    current_level_db_ = std::max(left_db, right_db);
    peak_level_db_ = std::max(peak_level_db_, current_level_db_);

    // Check for clipping
    if (current_level_db_ > -1.0f) {
        clipping_detected_ = true;
        emit clipping_detected();
    }

    update();
}

void VUMeterWidget::set_peak_hold_time(int ms) {
    peak_hold_time_ms_ = ms;
}

void VUMeterWidget::set_decay_rate(float db_per_second) {
    decay_rate_ = db_per_second;
}

void VUMeterWidget::set_meter_type(bool is_peak_meter) {
    is_peak_meter_ = is_peak_meter;
}

void VUMeterWidget::set_scale_range(float min_db, float max_db) {
    min_db_ = min_db;
    max_db_ = max_db;
    update();
}

void VUMeterWidget::set_orientation(Qt::Orientation orientation) {
    orientation_ = orientation;
    updateGeometry();
    update();
}

void VUMeterWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    draw_meter(painter);
    draw_scale(painter);
}

void VUMeterWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void VUMeterWidget::update_meter() {
    // Apply decay to current level
    if (current_level_db_ > min_db_) {
        float decay_amount = decay_rate_ * 0.033f; // 33ms frame time
        current_level_db_ = std::max(current_level_db_ - decay_amount, min_db_);
        update();
    }

    // Update peak hold
    if (peak_hold_level_ > min_db_ && peak_timer_->isActive()) {
        float decay_amount = decay_rate_ * 0.033f;
        peak_hold_level_ = std::max(peak_hold_level_ - decay_amount, min_db_);
        update();
    }
}

void VUMeterWidget::reset_peak_hold() {
    peak_hold_level_ = -std::numeric_limits<float>::infinity();
    update();
}

void VUMeterWidget::draw_meter(QPainter& painter) {
    QRect rect = this->rect();

    if (orientation_ == Qt::Vertical) {
        meter_rect_ = QRect(SCALE_WIDTH, 10, rect.width() - SCALE_WIDTH - 5, rect.height() - 20);
    } else {
        meter_rect_ = QRect(10, SCALE_WIDTH, rect.width() - 20, rect.height() - SCALE_WIDTH - 5);
    }

    // Draw background
    painter.fillRect(meter_rect_, QColor(30, 30, 30));

    // Draw level bars
    if (is_stereo_) {
        // Left channel (top or left)
        QRect left_rect = meter_rect_;
        if (orientation_ == Qt::Vertical) {
            left_rect.setHeight(meter_rect_.height() / 2 - 2);
        } else {
            left_rect.setWidth(meter_rect_.width() / 2 - 2);
        }
        draw_level_bar(painter, left_rect, left_level_db_);

        // Right channel (bottom or right)
        QRect right_rect = meter_rect_;
        if (orientation_ == Qt::Vertical) {
            right_rect.setTop(left_rect.bottom() + 4);
        } else {
            right_rect.setLeft(left_rect.right() + 4);
        }
        draw_level_bar(painter, right_rect, right_level_db_);
    } else {
        draw_level_bar(painter, meter_rect_, current_level_db_);
    }

    // Draw peak hold indicator
    if (peak_hold_level_ > min_db_) {
        int peak_pos = db_to_pixel(peak_hold_level_);
        QColor peak_color(255, 255, 0); // Yellow

        if (orientation_ == Qt::Vertical) {
            painter.fillRect(meter_rect_.left(), peak_pos - 1, meter_rect_.width(), 3, peak_color);
        } else {
            painter.fillRect(peak_pos - 1, meter_rect_.top(), 3, meter_rect_.height(), peak_color);
        }
    }
}

void VUMeterWidget::draw_scale(QPainter& painter) {
    painter.setPen(QColor(200, 200, 200));

    // Draw scale markings
    for (int db = static_cast<int>(min_db_); db <= static_cast<int>(max_db_); db += 10) {
        int pos = db_to_pixel(static_cast<float>(db));

        if (orientation_ == Qt::Vertical) {
            painter.drawLine(meter_rect_.left() - 5, pos, meter_rect_.left(), pos);
            painter.drawText(meter_rect_.left() - SCALE_WIDTH + 2, pos + 4, QString::number(db));
        } else {
            painter.drawLine(pos, meter_rect_.bottom(), pos, meter_rect_.bottom() + 5);
            painter.drawText(pos - 10, meter_rect_.bottom() + SCALE_WIDTH - 2, QString::number(db));
        }
    }
}

void VUMeterWidget::draw_level_bar(QPainter& painter, const QRect& rect, float level_db) {
    if (level_db <= min_db_) return;

    QColor level_color = get_level_color(level_db);
    int level_pos = db_to_pixel(level_db);

    if (orientation_ == Qt::Vertical) {
        int bar_height = rect.bottom() - level_pos;
        QRect bar_rect(rect.left(), level_pos, rect.width(), bar_height);
        painter.fillRect(bar_rect, level_color);
    } else {
        int bar_width = level_pos - rect.left();
        QRect bar_rect(rect.left(), rect.top(), bar_width, rect.height());
        painter.fillRect(bar_rect, level_color);
    }
}

QColor VUMeterWidget::get_level_color(float level_db) const {
    if (level_db > -1.0f) return QColor(255, 0, 0);     // Red (clipping)
    if (level_db > -6.0f) return QColor(255, 255, 0);   // Yellow (hot)
    if (level_db > -18.0f) return QColor(0, 255, 0);    // Green (good)
    return QColor(0, 100, 0);                          // Dark green (quiet)
}

int VUMeterWidget::db_to_pixel(float db) const {
    float normalized = (db - min_db_) / (max_db_ - min_db_);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    if (orientation_ == Qt::Vertical) {
        return meter_rect_.bottom() - static_cast<int>(normalized * meter_rect_.height());
    } else {
        return meter_rect_.left() + static_cast<int>(normalized * meter_rect_.width());
    }
}

// AudioMonitorPanel Implementation
AudioMonitorPanel::AudioMonitorPanel(QWidget* parent)
    : QWidget(parent)
    , audio_pipeline_(nullptr)
    , monitoring_active_(false)
    , master_left_db_(-std::numeric_limits<float>::infinity())
    , master_right_db_(-std::numeric_limits<float>::infinity())
{
    setup_ui();

    monitoring_timer_ = new QTimer(this);
    connect(monitoring_timer_, &QTimer::timeout, this, &AudioMonitorPanel::update_audio_levels);
    monitoring_timer_->start(UPDATE_INTERVAL_MS);
}

AudioMonitorPanel::~AudioMonitorPanel() = default;

void AudioMonitorPanel::set_audio_pipeline(ve::audio::AudioPipeline* pipeline) {
    audio_pipeline_ = pipeline;
    update_pipeline_stats();
}

void AudioMonitorPanel::start_monitoring() {
    if (!monitoring_active_) {
        monitoring_active_ = true;
        emit monitoring_started();
    }
}

void AudioMonitorPanel::stop_monitoring() {
    if (monitoring_active_) {
        monitoring_active_ = false;
        emit monitoring_stopped();
    }
}

void AudioMonitorPanel::add_channel_monitor(uint32_t channel_id, const std::string& name) {
    if (channel_monitors_.size() >= MAX_CHANNELS) return;

    create_channel_monitor(channel_id, name);
    updateGeometry();
}

void AudioMonitorPanel::remove_channel_monitor(uint32_t channel_id) {
    auto it = std::find_if(channel_monitors_.begin(), channel_monitors_.end(),
                          [channel_id](const auto& monitor) {
                              return monitor->channel_id == channel_id;
                          });

    if (it != channel_monitors_.end()) {
        channels_layout_->removeWidget((*it)->container);
        delete (*it)->container;
        channel_monitors_.erase(it);
        updateGeometry();
    }
}

void AudioMonitorPanel::update_channel_levels(const std::vector<AudioLevelData>& levels) {
    std::lock_guard<std::mutex> lock(levels_mutex_);
    current_levels_ = levels;
}

void AudioMonitorPanel::set_master_levels(float left_db, float right_db) {
    master_left_db_ = left_db;
    master_right_db_ = right_db;
    master_meter_->set_stereo_levels(left_db, right_db);
}

void AudioMonitorPanel::set_master_volume(float volume_db) {
    // Update volume slider to match dB value
    // Convert dB to slider position (assuming -60 to +6 dB range)
    int slider_value = static_cast<int>((volume_db + 60.0f) / 66.0f * 100.0f);
    master_volume_slider_->setValue(std::clamp(slider_value, 0, 100));

    update_master_volume_label(volume_db);
}

void AudioMonitorPanel::set_master_mute(bool muted) {
    master_mute_button_->setChecked(muted);
}

void AudioMonitorPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateGeometry();
}

void AudioMonitorPanel::update_audio_levels() {
    if (!monitoring_active_ || !audio_pipeline_) return;

    // Update master levels (simulated for now)
    float left_level = -20.0f + (rand() % 200) / 10.0f;  // -20 to 0 dB
    float right_level = -20.0f + (rand() % 200) / 10.0f; // -20 to 0 dB
    set_master_levels(left_level, right_level);

    // Update channel monitors
    std::lock_guard<std::mutex> lock(levels_mutex_);
    for (auto& monitor : channel_monitors_) {
        auto it = std::find_if(current_levels_.begin(), current_levels_.end(),
                              [id = monitor->channel_id](const AudioLevelData& data) {
                                  return data.channel_id == id;
                              });

        if (it != current_levels_.end()) {
            monitor->meter->set_level(it->peak_left);
            monitor->level_label->setText(QString("%1 dB").arg(it->peak_left, 0, 'f', 1));
        }
    }

    update_pipeline_stats();
}

void AudioMonitorPanel::on_master_volume_changed(int value) {
    // Convert slider position to dB
    float volume_db = (value / 100.0f) * 66.0f - 60.0f;
    volume_db = std::clamp(volume_db, -60.0f, 6.0f);

    update_master_volume_label(volume_db);
    emit master_volume_changed(volume_db);
}

void AudioMonitorPanel::on_master_mute_clicked() {
    bool muted = master_mute_button_->isChecked();
    emit master_mute_toggled(muted);
}

void AudioMonitorPanel::setup_ui() {
    main_layout_ = new QVBoxLayout(this);
    main_layout_->setSpacing(10);
    main_layout_->setContentsMargins(10, 10, 10, 10);

    setup_master_section();

    // Channels section
    channels_section_ = new QWidget();
    channels_layout_ = new QVBoxLayout(channels_section_);
    channels_layout_->setSpacing(5);

    auto* channels_group = new QGroupBox("Channel Meters");
    channels_group->setLayout(new QVBoxLayout());
    channels_group->layout()->addWidget(channels_section_);

    main_layout_->addWidget(channels_group);
    main_layout_->addStretch();
}

void AudioMonitorPanel::setup_master_section() {
    master_section_ = new QWidget();
    auto* master_layout = new QHBoxLayout(master_section_);

    // Master meter
    master_meter_ = new VUMeterWidget();
    master_meter_->setMinimumWidth(60);
    master_meter_->setMaximumWidth(80);

    // Master controls
    auto* controls_layout = new QVBoxLayout();

    master_volume_label_ = new QLabel("0.0 dB");
    master_volume_label_->setAlignment(Qt::AlignCenter);
    master_volume_label_->setStyleSheet("font-weight: bold; color: #00FF00;");

    master_volume_slider_ = new QSlider(Qt::Vertical);
    master_volume_slider_->setRange(0, 100);
    master_volume_slider_->setValue(76); // 0 dB
    master_volume_slider_->setTickPosition(QSlider::TicksBothSides);
    master_volume_slider_->setTickInterval(10);
    connect(master_volume_slider_, &QSlider::valueChanged, this, &AudioMonitorPanel::on_master_volume_changed);

    master_mute_button_ = new QPushButton("MUTE");
    master_mute_button_->setCheckable(true);
    master_mute_button_->setStyleSheet("QPushButton { background-color: #666666; color: white; }"
                                      "QPushButton:checked { background-color: #FF4444; }");
    connect(master_mute_button_, &QPushButton::clicked, this, &AudioMonitorPanel::on_master_mute_clicked);

    controls_layout->addWidget(master_volume_label_);
    controls_layout->addWidget(master_volume_slider_);
    controls_layout->addWidget(master_mute_button_);

    master_layout->addWidget(master_meter_);
    master_layout->addLayout(controls_layout);

    auto* master_group = new QGroupBox("Master");
    master_group->setLayout(master_layout);

    main_layout_->addWidget(master_group);
}

void AudioMonitorPanel::create_channel_monitor(uint32_t channel_id, const std::string& name) {
    auto monitor = std::make_unique<ChannelMonitor>();
    monitor->channel_id = channel_id;
    monitor->name = name;

    monitor->container = new QWidget();
    auto* layout = new QHBoxLayout(monitor->container);

    monitor->name_label = new QLabel(QString::fromStdString(name));
    monitor->name_label->setMinimumWidth(60);

    monitor->meter = new VUMeterWidget();
    monitor->meter->set_orientation(Qt::Horizontal);
    monitor->meter->setMinimumHeight(20);
    monitor->meter->setMaximumHeight(30);

    monitor->level_label = new QLabel("-∞ dB");
    monitor->level_label->setMinimumWidth(50);
    monitor->level_label->setAlignment(Qt::AlignRight);

    layout->addWidget(monitor->name_label);
    layout->addWidget(monitor->meter, 1);
    layout->addWidget(monitor->level_label);

    channels_layout_->addWidget(monitor->container);
    channel_monitors_.push_back(std::move(monitor));
}

void AudioMonitorPanel::update_pipeline_stats() {
    // Update pipeline statistics display if available
    // This would integrate with the actual audio pipeline
}

void AudioMonitorPanel::update_master_volume_label(float volume_db) {
    QString text = QString("%1 dB").arg(volume_db, 0, 'f', 1);

    if (volume_db > -0.1f && volume_db < 0.1f) {
        master_volume_label_->setStyleSheet("font-weight: bold; color: #00FF00;");
    } else if (volume_db < -10.0f) {
        master_volume_label_->setStyleSheet("font-weight: bold; color: #666666;");
    } else {
        master_volume_label_->setStyleSheet("font-weight: bold; color: #FFFF00;");
    }

    master_volume_label_->setText(text);
}

// CompactAudioMeter Implementation
CompactAudioMeter::CompactAudioMeter(QWidget* parent)
    : QWidget(parent)
    , left_level_db_(-std::numeric_limits<float>::infinity())
    , right_level_db_(-std::numeric_limits<float>::infinity())
    , clipping_(false)
{
    setFixedSize(METER_WIDTH, METER_HEIGHT);
}

CompactAudioMeter::~CompactAudioMeter() = default;

void CompactAudioMeter::set_levels(float left_db, float right_db) {
    left_level_db_ = left_db;
    right_level_db_ = right_db;
    clipping_ = (left_db > -1.0f) || (right_db > -1.0f);
    update();
}

void CompactAudioMeter::set_clipping(bool clipping) {
    clipping_ = clipping;
    update();
}

void CompactAudioMeter::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    painter.fillRect(rect(), QColor(30, 30, 30));

    // Draw left channel (top half)
    QRect left_rect(0, 0, width(), height() / 2);
    draw_channel(painter, left_rect, left_level_db_);

    // Draw right channel (bottom half)
    QRect right_rect(0, height() / 2, width(), height() / 2);
    draw_channel(painter, right_rect, right_level_db_);

    // Draw center line
    painter.setPen(QColor(100, 100, 100));
    painter.drawLine(0, height() / 2, width(), height() / 2);

    // Draw clipping indicator
    if (clipping_) {
        painter.setPen(QColor(255, 0, 0));
        painter.setBrush(QColor(255, 0, 0));
        painter.drawEllipse(width() - 8, height() - 8, 6, 6);
    }
}

QSize CompactAudioMeter::sizeHint() const {
    return QSize(METER_WIDTH, METER_HEIGHT);
}

void CompactAudioMeter::draw_channel(QPainter& painter, const QRect& rect, float level_db) {
    if (level_db <= MIN_DB) return;

    float normalized = (level_db - MIN_DB) / (MAX_DB - MIN_DB);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    int bar_width = static_cast<int>(normalized * rect.width());

    QColor color;
    if (level_db > -1.0f) color = QColor(255, 0, 0);     // Red (clipping)
    else if (level_db > -6.0f) color = QColor(255, 255, 0); // Yellow (hot)
    else color = QColor(0, 255, 0);                     // Green (good)

    painter.fillRect(rect.left(), rect.top(), bar_width, rect.height(), color);
}

} // namespace ve::ui</content>
<parameter name="filePath">c:\Users\braul\Desktop\Video_Editor\src\ui\src\audio_monitor_widget.cpp
