/**
 * Professional Audio Monitoring UI - Phase 2 Implementation
 * 
 * Implementation of comprehensive UI widgets for professional audio monitoring.
 */

#include "ui/professional_audio_monitoring_ui.hpp"
#include "core/log.hpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtCore/QTimer>
#include <algorithm>
#include <cmath>

namespace ve::ui {

// ============================================================================
// LoudnessDisplayWidget Implementation
// ============================================================================

LoudnessDisplayWidget::LoudnessDisplayWidget(QWidget* parent)
    : QWidget(parent)
    , main_layout_(new QVBoxLayout(this))
    , controls_layout_(new QHBoxLayout())
    , platform_combo_(new QComboBox())
    , reset_button_(new QPushButton("Reset"))
{
    setMinimumSize(300, 200);
    setup_ui();
    apply_professional_styling();
}

void LoudnessDisplayWidget::setup_ui() {
    // Platform selection
    platform_combo_->addItem("EBU R128");
    platform_combo_->addItem("ATSC A/85");
    platform_combo_->addItem("Spotify");
    platform_combo_->addItem("YouTube");
    platform_combo_->setCurrentText("EBU R128");
    
    // Controls layout
    controls_layout_->addWidget(new QLabel("Standard:"));
    controls_layout_->addWidget(platform_combo_);
    controls_layout_->addStretch();
    controls_layout_->addWidget(reset_button_);
    
    // Main layout
    main_layout_->addLayout(controls_layout_);
    main_layout_->addStretch();
    
    // Set the layout
    setLayout(main_layout_);
    
    // Connections
    connect(platform_combo_, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            [this](const QString& text) { this->on_platform_changed(text); });
    connect(reset_button_, &QPushButton::clicked,
            [this]() { this->on_reset_clicked(); });
}

void LoudnessDisplayWidget::apply_professional_styling() {
    setStyleSheet(
        "LoudnessDisplayWidget {"
        "    background-color: rgb(30, 30, 30);"
        "    color: rgb(220, 220, 220);"
        "    border: 1px solid rgb(60, 60, 60);"
        "}"
        "QComboBox {"
        "    background-color: rgb(40, 40, 40);"
        "    color: rgb(220, 220, 220);"
        "    border: 1px solid rgb(80, 80, 80);"
        "    padding: 4px;"
        "}"
        "QPushButton {"
        "    background-color: rgb(50, 50, 50);"
        "    color: rgb(220, 220, 220);"
        "    border: 1px solid rgb(100, 100, 100);"
        "    padding: 4px 12px;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgb(70, 70, 70);"
        "}"
        "QLabel {"
        "    color: rgb(200, 200, 200);"
        "}"
    );
}

void LoudnessDisplayWidget::set_target_platform(const std::string& platform) {
    target_platform_ = platform;
    
    // Update combo box
    if (platform == "EBU") {
        platform_combo_->setCurrentText("EBU R128");
    } else if (platform == "ATSC") {
        platform_combo_->setCurrentText("ATSC A/85");
    } else if (platform == "Spotify") {
        platform_combo_->setCurrentText("Spotify");
    } else if (platform == "YouTube") {
        platform_combo_->setCurrentText("YouTube");
    }
    
    update();
}

void LoudnessDisplayWidget::update_loudness_data(const ve::audio::EnhancedEBUR128Monitor* monitor) {
    if (!monitor) return;
    
    display_data_.momentary_lufs = monitor->get_momentary_lufs();
    display_data_.short_term_lufs = monitor->get_short_term_lufs();
    display_data_.integrated_lufs = monitor->get_integrated_lufs();
    display_data_.loudness_range = monitor->get_loudness_range();
    display_data_.broadcast_compliant = monitor->is_broadcast_compliant();
    
    auto status = monitor->get_compliance_status();
    display_data_.compliance_text = status.compliance_text;
    display_data_.warnings = monitor->get_compliance_warnings();
    
    update();
}

void LoudnessDisplayWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), background_color_);
    
    if (!compact_mode_) {
        // Full display mode
        QRect content_rect = rect().adjusted(10, 40, -10, -10);
        
        // Split into sections
        int section_height = content_rect.height() / 3;
        QRect meters_rect = QRect(content_rect.x(), content_rect.y(), content_rect.width(), section_height);
        QRect numeric_rect = QRect(content_rect.x(), content_rect.y() + section_height, 
                                  content_rect.width(), section_height);
        QRect compliance_rect = QRect(content_rect.x(), content_rect.y() + 2 * section_height, 
                                     content_rect.width(), section_height);
        
        draw_loudness_meters(painter, meters_rect);
        draw_numeric_displays(painter, numeric_rect);
        draw_compliance_indicator(painter, compliance_rect);
    } else {
        // Compact mode - just essential info
        QRect content_rect = rect().adjusted(5, 25, -5, -5);
        draw_numeric_displays(painter, content_rect);
    }
}

void LoudnessDisplayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Handle any resize-specific logic here if needed
}

void LoudnessDisplayWidget::draw_loudness_meters(QPainter& painter, const QRect& rect) {
    // Draw horizontal bar meters for momentary, short-term, integrated
    painter.setPen(text_color_);
    painter.setFont(QFont("Arial", 9));
    
    int bar_height = 20;
    int spacing = 5;
    int label_width = 80;
    
    // Momentary LUFS bar
    QRect momentary_rect(rect.x() + label_width, rect.y(), 
                        rect.width() - label_width, bar_height);
    painter.drawText(rect.x(), rect.y() + 15, "Momentary:");
    this->draw_lufs_bar(painter, momentary_rect, display_data_.momentary_lufs);
    
    // Short-term LUFS bar
    QRect short_term_rect(rect.x() + label_width, rect.y() + bar_height + spacing, 
                         rect.width() - label_width, bar_height);
    painter.drawText(rect.x(), rect.y() + bar_height + spacing + 15, "Short-term:");
    this->draw_lufs_bar(painter, short_term_rect, display_data_.short_term_lufs);
    
    // Integrated LUFS bar
    QRect integrated_rect(rect.x() + label_width, rect.y() + 2 * (bar_height + spacing), 
                         rect.width() - label_width, bar_height);
    painter.drawText(rect.x(), rect.y() + 2 * (bar_height + spacing) + 15, "Integrated:");
    this->draw_lufs_bar(painter, integrated_rect, display_data_.integrated_lufs);
}

void LoudnessDisplayWidget::draw_lufs_bar(QPainter& painter, const QRect& rect, double lufs) {
    // Draw background
    painter.fillRect(rect, QColor(40, 40, 40));
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(rect);
    
    if (std::isfinite(lufs)) {
        // Calculate position on scale (-50 to 0 LUFS)
        double min_lufs = -50.0;
        double max_lufs = 0.0;
        double normalized = std::max(0.0, std::min(1.0, (lufs - min_lufs) / (max_lufs - min_lufs)));
        
        int bar_width = static_cast<int>(rect.width() * normalized);
        QRect level_rect(rect.x(), rect.y(), bar_width, rect.height());
        
        // Color based on target compliance
        QColor bar_color = this->get_loudness_color(lufs, -23.0); // Default EBU target
        painter.fillRect(level_rect, bar_color);
        
        // Draw numeric value
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 8, QFont::Bold));
        QString value_text = QString::number(lufs, 'f', 1) + " LUFS";
        painter.drawText(rect.adjusted(5, 0, -5, 0), Qt::AlignCenter, value_text);
    } else {
        // No signal
        painter.setPen(Qt::gray);
        painter.drawText(rect, Qt::AlignCenter, "---");
    }
}

void LoudnessDisplayWidget::draw_numeric_displays(QPainter& painter, const QRect& rect) {
    painter.setPen(text_color_);
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    
    // Create grid layout for numeric values
    int cols = 2;
    int rows = 2;
    int cell_width = rect.width() / cols;
    int cell_height = rect.height() / rows;
    
    // Integrated LUFS (main value)
    QRect integrated_rect(rect.x(), rect.y(), cell_width, cell_height);
    this->draw_numeric_value(painter, integrated_rect, "Integrated LUFS", 
                      display_data_.integrated_lufs, "LUFS");
    
    // Loudness Range
    QRect range_rect(rect.x() + cell_width, rect.y(), cell_width, cell_height);
    this->draw_numeric_value(painter, range_rect, "Loudness Range", 
                      display_data_.loudness_range, "LU");
    
    // Short-term LUFS
    QRect short_term_rect(rect.x(), rect.y() + cell_height, cell_width, cell_height);
    this->draw_numeric_value(painter, short_term_rect, "Short-term LUFS", 
                      display_data_.short_term_lufs, "LUFS");
    
    // Momentary LUFS
    QRect momentary_rect(rect.x() + cell_width, rect.y() + cell_height, cell_width, cell_height);
    this->draw_numeric_value(painter, momentary_rect, "Momentary LUFS", 
                      display_data_.momentary_lufs, "LUFS");
}

void LoudnessDisplayWidget::draw_numeric_value(QPainter& painter, const QRect& rect, 
                                              const QString& label, double value, const QString& unit) {
    // Draw border
    painter.setPen(QColor(80, 80, 80));
    painter.drawRect(rect);
    
    // Draw label
    painter.setPen(QColor(180, 180, 180));
    painter.setFont(QFont("Arial", 8));
    QRect label_rect = rect.adjusted(5, 5, -5, -rect.height()/2);
    painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignTop, label);
    
    // Draw value
    QColor value_color = text_color_;
    if (label.contains("Integrated") && std::isfinite(value)) {
        value_color = this->get_loudness_color(value, -23.0);
    }
    
    painter.setPen(value_color);
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    
    QString value_text;
    if (std::isfinite(value)) {
        value_text = QString::number(value, 'f', 1) + " " + unit;
    } else {
        value_text = "--- " + unit;
    }
    
    QRect value_rect = rect.adjusted(5, rect.height()/2, -5, -5);
    painter.drawText(value_rect, Qt::AlignCenter, value_text);
}

void LoudnessDisplayWidget::draw_compliance_indicator(QPainter& painter, const QRect& rect) {
    // Draw compliance status
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    
    QColor status_color = display_data_.broadcast_compliant ? compliant_color_ : error_color_;
    painter.setPen(status_color);
    
    QString status_text = QString::fromStdString(display_data_.compliance_text);
    painter.drawText(rect.adjusted(5, 5, -5, -rect.height()/2), Qt::AlignLeft, status_text);
    
    // Draw warnings
    if (!display_data_.warnings.empty()) {
        painter.setPen(warning_color_);
        painter.setFont(QFont("Arial", 8));
        
        int y_offset = rect.height() / 2;
        for (const auto& warning : display_data_.warnings) {
            if (y_offset < rect.height() - 10) {
                QString warning_text = QString::fromStdString(warning);
                painter.drawText(rect.x() + 5, rect.y() + y_offset, warning_text);
                y_offset += 12;
            }
        }
    }
}

QColor LoudnessDisplayWidget::get_loudness_color(double lufs, double target) const {
    if (!std::isfinite(lufs)) return text_color_;
    
    double difference = std::abs(lufs - target);
    
    if (difference <= 1.0) {
        return compliant_color_; // Green - compliant
    } else if (difference <= 2.0) {
        return warning_color_;   // Yellow - warning
    } else {
        return error_color_;     // Red - error
    }
}

void LoudnessDisplayWidget::on_reset_clicked() {
    // Reset the display data
    display_data_ = {};
    update();
    // emit reset_requested(); // Removed since no Q_OBJECT
}

void LoudnessDisplayWidget::on_platform_changed(const QString& platform) {
    std::string platform_std;
    
    if (platform == "EBU R128") {
        platform_std = "EBU";
    } else if (platform == "ATSC A/85") {
        platform_std = "ATSC";
    } else if (platform == "Spotify") {
        platform_std = "Spotify";
    } else if (platform == "YouTube") {
        platform_std = "YouTube";
    }
    
    target_platform_ = platform_std;
    // emit platform_changed(platform_std); // Removed since no Q_OBJECT
}

// ============================================================================
// ProfessionalMetersWidget Implementation
// ============================================================================

ProfessionalMetersWidget::ProfessionalMetersWidget(uint16_t channels, QWidget* parent)
    : QWidget(parent)
    , channel_count_(channels)
{
    channel_data_.resize(channel_count_);
    setMinimumSize(channels * (meter_width_ + meter_spacing_) + 50, 200);
    
    setStyleSheet(
        "ProfessionalMetersWidget {"
        "    background-color: rgb(20, 20, 20);"
        "    border: 1px solid rgb(60, 60, 60);"
        "}"
    );
}

void ProfessionalMetersWidget::set_meter_standard(ve::audio::ProfessionalMeterSystem::MeterStandard standard) {
    meter_standard_ = standard;
    update();
}

void ProfessionalMetersWidget::set_reference_level(double ref_db) {
    reference_level_db_ = ref_db;
    update();
}

void ProfessionalMetersWidget::update_meter_data(const ve::audio::ProfessionalMeterSystem* meter_system) {
    if (!meter_system) return;
    
    auto readings = meter_system->get_all_readings();
    
    for (size_t ch = 0; ch < std::min(channel_data_.size(), readings.size()); ++ch) {
        const auto& reading = readings[ch];
        channel_data_[ch].current_level_db = reading.current_level_db;
        channel_data_[ch].peak_hold_db = reading.peak_hold_db;
        channel_data_[ch].rms_level_db = reading.rms_level_db;
        channel_data_[ch].overload = reading.overload;
        channel_data_[ch].valid = reading.valid;
    }
    
    update();
}

void ProfessionalMetersWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), background_color_);
    
    // Calculate layout
    int scale_width = 40;
    int meters_width = rect().width() - scale_width;
    int meter_total_width = meter_width_ + meter_spacing_;
    
    // Draw scale
    QRect scale_rect(rect().x(), rect().y(), scale_width, rect().height());
    draw_meter_scale(painter, scale_rect);
    
    // Draw meters
    for (uint16_t ch = 0; ch < channel_count_; ++ch) {
        int meter_x = scale_width + ch * meter_total_width;
        if (meter_x + meter_width_ <= rect().width()) {
            QRect meter_rect(meter_x, rect().y() + 10, meter_width_, rect().height() - 20);
            draw_meter_channel(painter, ch, meter_rect);
        }
    }
}

void ProfessionalMetersWidget::draw_meter_channel(QPainter& painter, int channel, const QRect& meter_rect) {
    if (channel >= static_cast<int>(channel_data_.size())) return;
    
    const auto& data = channel_data_[channel];
    
    // Draw meter background
    painter.fillRect(meter_rect, QColor(30, 30, 30));
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(meter_rect);
    
    if (!data.valid) {
        // No signal
        painter.setPen(Qt::gray);
        painter.drawText(meter_rect, Qt::AlignCenter, "NO\nSIG");
        return;
    }
    
    // Draw level bar
    if (std::isfinite(data.current_level_db)) {
        int level_pixel = db_to_pixel(data.current_level_db, meter_rect);
        QRect level_rect(meter_rect.x(), level_pixel, meter_rect.width(), 
                        meter_rect.bottom() - level_pixel);
        
        QColor level_color = get_meter_color(data.current_level_db);
        painter.fillRect(level_rect, level_color);
    }
    
    // Draw peak hold
    if (show_peak_hold_ && std::isfinite(data.peak_hold_db)) {
        int peak_pixel = db_to_pixel(data.peak_hold_db, meter_rect);
        painter.setPen(QPen(peak_hold_color_, 2));
        painter.drawLine(meter_rect.x(), peak_pixel, meter_rect.right(), peak_pixel);
    }
    
    // Draw overload indicator
    if (show_overload_indicators_ && data.overload) {
        QRect overload_rect(meter_rect.x(), meter_rect.y(), meter_rect.width(), 10);
        painter.fillRect(overload_rect, overload_color_);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 7, QFont::Bold));
        painter.drawText(overload_rect, Qt::AlignCenter, "OL");
    }
    
    // Draw channel label
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8));
    QString label = QString("L");
    if (channel == 1) label = "R";
    else if (channel > 1) label = QString::number(channel + 1);
    
    QRect label_rect(meter_rect.x(), meter_rect.bottom() + 2, meter_rect.width(), 12);
    painter.drawText(label_rect, Qt::AlignCenter, label);
}

void ProfessionalMetersWidget::draw_meter_scale(QPainter& painter, const QRect& scale_rect) {
    painter.setPen(QColor(180, 180, 180));
    painter.setFont(QFont("Arial", 7));
    
    // Draw scale marks and labels
    std::vector<double> scale_marks = {0, -3, -6, -9, -12, -18, -24, -30, -40, -50, -60};
    
    for (double db : scale_marks) {
        int y = db_to_pixel(db, QRect(0, scale_rect.y() + 10, 1, scale_rect.height() - 20));
        
        // Draw tick mark
        painter.drawLine(scale_rect.right() - 10, y, scale_rect.right(), y);
        
        // Draw label
        QString label = QString::number(static_cast<int>(db));
        QRect label_rect(scale_rect.x(), y - 6, scale_rect.width() - 12, 12);
        painter.drawText(label_rect, Qt::AlignRight | Qt::AlignVCenter, label);
    }
    
    // Draw reference level indicator
    int ref_y = db_to_pixel(reference_level_db_, QRect(0, scale_rect.y() + 10, 1, scale_rect.height() - 20));
    painter.setPen(QPen(Qt::yellow, 2));
    painter.drawLine(scale_rect.right() - 15, ref_y, scale_rect.right(), ref_y);
}

QColor ProfessionalMetersWidget::get_meter_color(double level_db) const {
    if (level_db >= -6.0) {
        return red_zone_;     // Red zone (> -6 dB)
    } else if (level_db >= -18.0) {
        return yellow_zone_;  // Yellow zone (-18 to -6 dB)
    } else {
        return green_zone_;   // Green zone (< -18 dB)
    }
}

int ProfessionalMetersWidget::db_to_pixel(double db, const QRect& meter_rect) const {
    // Map dB range to pixel range (inverted Y axis)
    double min_db = -60.0;
    double max_db = 0.0;
    
    double normalized = std::max(0.0, std::min(1.0, (db - min_db) / (max_db - min_db)));
    return meter_rect.bottom() - static_cast<int>(normalized * meter_rect.height());
}

void ProfessionalMetersWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        on_reset_peak_holds();
    }
    QWidget::mousePressEvent(event);
}

void ProfessionalMetersWidget::on_reset_peak_holds() {
    // emit peak_hold_reset(); // Removed since no Q_OBJECT
}

// ============================================================================
// AudioScopesWidget Implementation  
// ============================================================================

AudioScopesWidget::AudioScopesWidget(ScopeType type, QWidget* parent)
    : QWidget(parent)
    , scope_type_(type)
    , main_layout_(new QVBoxLayout(this))
{
    setMinimumSize(300, 200);
    setup_ui();
}

void AudioScopesWidget::setup_ui() {
    // Simple setup for now
    setLayout(main_layout_);
}

void AudioScopesWidget::update_scope_data(const ve::audio::ProfessionalAudioScopes* scopes) {
    // Update scope data and trigger repaint
    update();
}

void AudioScopesWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, "Audio Scopes");
}

void AudioScopesWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Update layout based on new size
    update();
}

void AudioScopesWidget::on_scope_reset() {
    // Reset scope data to clear displays
    vectorscope_data_ = {};
    correlation_data_ = {};
    spectrum_data_ = {};
    update();
    // emit scope_reset(); // Removed since no Q_OBJECT
}

// ============================================================================
// Unified Professional Audio Monitoring Panel Implementation
// ============================================================================

ProfessionalAudioMonitoringPanel::ProfessionalAudioMonitoringPanel(QWidget* parent)
    : QWidget(parent)
    , main_layout_(new QVBoxLayout(this))
    , top_layout_(new QHBoxLayout())
    , bottom_layout_(new QHBoxLayout())
    , update_timer_(new QTimer(this))
{
    setup_layout();
    setup_controls();
    apply_professional_styling();
    
    // Setup update timer
    connect(update_timer_, &QTimer::timeout, [this]() { this->on_monitoring_timer(); });
}

void ProfessionalAudioMonitoringPanel::setup_layout() {
    // Create monitoring widgets
    loudness_widget_ = std::make_unique<LoudnessDisplayWidget>();
    meters_widget_ = std::make_unique<ProfessionalMetersWidget>(2);
    scopes_widget_ = std::make_unique<AudioScopesWidget>(AudioScopesWidget::ScopeType::ALL_SCOPES);
    
    // Top layout - loudness and meters
    top_layout_->addWidget(loudness_widget_.get(), 2);
    top_layout_->addWidget(meters_widget_.get(), 1);
    
    // Bottom layout - scopes
    bottom_layout_->addWidget(scopes_widget_.get());
    
    // Main layout
    main_layout_->addLayout(top_layout_, 1);
    main_layout_->addLayout(bottom_layout_, 1);
}

void ProfessionalAudioMonitoringPanel::setup_controls() {
    // Status labels
    status_label_ = new QLabel("Monitoring: Stopped");
    performance_label_ = new QLabel("Performance: ---");
    
    // Control buttons
    start_stop_button_ = new QPushButton("Start Monitoring");
    reset_button_ = new QPushButton("Reset All");
    
    // Platform combo
    platform_combo_ = new QComboBox();
    platform_combo_->addItems({"EBU R128", "ATSC A/85", "Spotify", "YouTube"});
    
    // Create controls group
    controls_group_ = new QGroupBox("Monitoring Controls");
    auto controls_layout = new QHBoxLayout(controls_group_);
    controls_layout->addWidget(new QLabel("Platform:"));
    controls_layout->addWidget(platform_combo_);
    controls_layout->addStretch();
    controls_layout->addWidget(status_label_);
    controls_layout->addWidget(performance_label_);
    controls_layout->addStretch();
    controls_layout->addWidget(start_stop_button_);
    controls_layout->addWidget(reset_button_);
    
    main_layout_->insertWidget(0, controls_group_);
    
    // Connections
    connect(start_stop_button_, &QPushButton::clicked, [this]() {
        if (monitoring_active_) {
            stop_monitoring();
        } else {
            start_monitoring();
        }
    });
    
    connect(reset_button_, &QPushButton::clicked, [this]() { this->on_reset_all(); });
    
    connect(platform_combo_, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            [this](const QString& text) {
                std::string platform = "EBU";
                if (text == "ATSC A/85") platform = "ATSC";
                else if (text == "Spotify") platform = "Spotify";
                else if (text == "YouTube") platform = "YouTube";
                
                on_platform_changed(platform);
            });
}

void ProfessionalAudioMonitoringPanel::apply_professional_styling() {
    setStyleSheet(
        "ProfessionalAudioMonitoringPanel {"
        "    background-color: rgb(25, 25, 25);"
        "    color: rgb(220, 220, 220);"
        "}"
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid rgb(80, 80, 80);"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    padding-top: 5px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
        "QPushButton {"
        "    background-color: rgb(60, 60, 60);"
        "    color: rgb(220, 220, 220);"
        "    border: 1px solid rgb(100, 100, 100);"
        "    padding: 6px 12px;"
        "    border-radius: 3px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgb(80, 80, 80);"
        "}"
        "QComboBox {"
        "    background-color: rgb(40, 40, 40);"
        "    color: rgb(220, 220, 220);"
        "    border: 1px solid rgb(80, 80, 80);"
        "    padding: 4px;"
        "}"
        "QLabel {"
        "    color: rgb(200, 200, 200);"
        "}"
    );
}

void ProfessionalAudioMonitoringPanel::set_monitoring_system(
    std::shared_ptr<ve::audio::ProfessionalAudioMonitoringSystem> system) {
    monitoring_system_ = system;
}

void ProfessionalAudioMonitoringPanel::start_monitoring() {
    if (!monitoring_system_) {
        ve::log::error("No monitoring system configured");
        return;
    }
    
    monitoring_active_ = true;
    start_stop_button_->setText("Stop Monitoring");
    status_label_->setText("Monitoring: Active");
    
    update_timer_->start(update_rate_ms_);
    
    // emit monitoring_started(); // Removed since no Q_OBJECT
    ve::log::info("Professional audio monitoring started");
}

void ProfessionalAudioMonitoringPanel::stop_monitoring() {
    monitoring_active_ = false;
    start_stop_button_->setText("Start Monitoring");
    status_label_->setText("Monitoring: Stopped");
    
    update_timer_->stop();
    
    // emit monitoring_stopped(); // Removed since no Q_OBJECT
    ve::log::info("Professional audio monitoring stopped");
}

void ProfessionalAudioMonitoringPanel::reset_all_meters() {
    if (monitoring_system_) {
        monitoring_system_->reset_all();
    }
    ve::log::info("All monitoring meters reset");
}

void ProfessionalAudioMonitoringPanel::on_monitoring_timer() {
    update_monitoring_data();
}

void ProfessionalAudioMonitoringPanel::update_monitoring_data() {
    if (!monitoring_system_) return;
    
    // Update loudness display
    if (loudness_widget_) {
        loudness_widget_->update_loudness_data(monitoring_system_->get_loudness_monitor());
    }
    
    // Update meters
    if (meters_widget_) {
        meters_widget_->update_meter_data(monitoring_system_->get_meter_system());
    }
    
    // Update scopes
    if (scopes_widget_) {
        scopes_widget_->update_scope_data(monitoring_system_->get_scopes());
    }
    
    // Update performance display
    update_status_display();
}

void ProfessionalAudioMonitoringPanel::update_status_display() {
    if (!monitoring_system_) return;
    
    auto perf_stats = monitoring_system_->get_performance_stats();
    
    QString perf_text = QString("Performance: %1ms avg, %2% CPU")
                       .arg(perf_stats.avg_processing_time_ms, 0, 'f', 2)
                       .arg(perf_stats.cpu_usage_percent, 0, 'f', 1);
    
    performance_label_->setText(perf_text);
    
    // Update status color based on system status
    auto system_status = monitoring_system_->get_system_status();
    if (system_status.broadcast_compliant) {
        status_label_->setStyleSheet("color: rgb(0, 255, 0);"); // Green
    } else if (!system_status.warnings.empty()) {
        status_label_->setStyleSheet("color: rgb(255, 165, 0);"); // Orange
    } else {
        status_label_->setStyleSheet("color: rgb(220, 220, 220);"); // Default
    }
}

void ProfessionalAudioMonitoringPanel::on_platform_changed(const std::string& platform) {
    if (monitoring_system_) {
        monitoring_system_->set_target_platform(platform);
    }
    
    if (loudness_widget_) {
        loudness_widget_->set_target_platform(platform);
    }
    
    // emit platform_changed(platform); // Removed since no Q_OBJECT
}

void ProfessionalAudioMonitoringPanel::on_reset_all() {
    reset_all_meters();
}

void ProfessionalAudioMonitoringPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Handle any resize-specific logic here if needed
}

} // namespace ve::ui