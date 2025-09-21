/**
 * Professional Audio Monitoring UI - Phase 2 Implementation
 * 
 * Comprehensive UI widgets for professional audio monitoring including:
 * - EBU R128 Loudness Display with compliance indicators
 * - Professional Peak/RMS Meters with broadcast standards
 * - Audio Scopes (vectorscope, phase correlation, spectrum)
 * - Real-time monitoring integration with timeline playback
 * 
 * This system provides broadcast-quality visual monitoring for professional
 * video editing workflows with industry-standard UI design.
 */

#pragma once

#include "audio/professional_monitoring.hpp"
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QFrame>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#include <QtCore/QTimer>
#include <memory>

namespace ve::ui {

/**
 * EBU R128 Loudness Display Widget
 */
class LoudnessDisplayWidget : public QWidget {
    // Q_OBJECT

public:
    explicit LoudnessDisplayWidget(QWidget* parent = nullptr);
    ~LoudnessDisplayWidget() = default;

    // Configuration
    void set_target_platform(const std::string& platform);
    void set_update_rate(int fps = 10);

    // Data input
    void update_loudness_data(const ve::audio::EnhancedEBUR128Monitor* monitor);

    // Visual configuration
    void set_compact_mode(bool compact);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void on_reset_clicked();
    void on_platform_changed(const QString& platform);

private:
    // UI Elements
    QVBoxLayout* main_layout_;
    QHBoxLayout* controls_layout_;
    QComboBox* platform_combo_;
    QPushButton* reset_button_;
    
    // Display data
    struct DisplayData {
        double momentary_lufs{-std::numeric_limits<double>::infinity()};
        double short_term_lufs{-std::numeric_limits<double>::infinity()};
        double integrated_lufs{-std::numeric_limits<double>::infinity()};
        double loudness_range{0.0};
        bool broadcast_compliant{false};
        std::string compliance_text;
        std::vector<std::string> warnings;
    } display_data_;
    
    // Visual settings
    bool compact_mode_{false};
    std::string target_platform_{"EBU"};
    
    // Colors
    QColor compliant_color_{0, 200, 0};
    QColor warning_color_{255, 165, 0};
    QColor error_color_{255, 50, 50};
    QColor background_color_{30, 30, 30};
    QColor text_color_{220, 220, 220};
    
    // Drawing helpers
    void setup_ui();
    void apply_professional_styling();
    void draw_loudness_meters(QPainter& painter, const QRect& rect);
    void draw_compliance_indicator(QPainter& painter, const QRect& rect);
    void draw_numeric_displays(QPainter& painter, const QRect& rect);
    void draw_lufs_bar(QPainter& painter, const QRect& rect, double lufs);
    void draw_numeric_value(QPainter& painter, const QRect& rect, const QString& label, double value, const QString& unit);
    QColor get_loudness_color(double lufs, double target) const;

/*
signals:
    void reset_requested();
    void platform_changed(const std::string& platform);
*/
};

/**
 * Professional Peak/RMS Meters Widget
 */
class ProfessionalMetersWidget : public QWidget {
    // Q_OBJECT

public:
    explicit ProfessionalMetersWidget(uint16_t channels = 2, QWidget* parent = nullptr);
    ~ProfessionalMetersWidget() = default;

    // Configuration
    void set_meter_standard(ve::audio::ProfessionalMeterSystem::MeterStandard standard);
    void set_reference_level(double ref_db);
    void set_channel_count(uint16_t channels);

    // Data input
    void update_meter_data(const ve::audio::ProfessionalMeterSystem* meter_system);

    // Visual configuration
    void set_orientation(Qt::Orientation orientation);
    void set_show_peak_hold(bool show);
    void set_show_overload_indicators(bool show);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void on_reset_peak_holds();

private:
    // Configuration
    uint16_t channel_count_;
    Qt::Orientation orientation_{Qt::Vertical};
    bool show_peak_hold_{true};
    bool show_overload_indicators_{true};
    ve::audio::ProfessionalMeterSystem::MeterStandard meter_standard_{
        ve::audio::ProfessionalMeterSystem::MeterStandard::DIGITAL_PEAK};
    
    // Display data
    struct ChannelMeterData {
        double current_level_db{-std::numeric_limits<double>::infinity()};
        double peak_hold_db{-std::numeric_limits<double>::infinity()};
        double rms_level_db{-std::numeric_limits<double>::infinity()};
        bool overload{false};
        bool valid{false};
    };
    std::vector<ChannelMeterData> channel_data_;
    
    // Visual elements
    int meter_width_{20};
    int meter_spacing_{5};
    double reference_level_db_{-20.0};
    
    // Colors
    QColor green_zone_{0, 255, 0};
    QColor yellow_zone_{255, 255, 0};
    QColor red_zone_{255, 0, 0};
    QColor overload_color_{255, 100, 100};
    QColor peak_hold_color_{255, 255, 255};
    QColor background_color_{20, 20, 20};
    
    // Drawing helpers
    void draw_meter_channel(QPainter& painter, int channel, const QRect& meter_rect);
    void draw_meter_scale(QPainter& painter, const QRect& scale_rect);
    QColor get_meter_color(double level_db) const;
    int db_to_pixel(double db, const QRect& meter_rect) const;
    double pixel_to_db(int pixel, const QRect& meter_rect) const;

/*
signals:
    void peak_hold_reset();
*/
};

/**
 * Audio Scopes Widget (Vectorscope, Correlation, Spectrum)
 */
class AudioScopesWidget : public QWidget {
    // Q_OBJECT

public:
    enum class ScopeType {
        VECTORSCOPE,
        PHASE_CORRELATION,
        SPECTRUM_ANALYZER,
        ALL_SCOPES
    };

    explicit AudioScopesWidget(ScopeType type = ScopeType::ALL_SCOPES, QWidget* parent = nullptr);
    ~AudioScopesWidget() = default;

    // Configuration
    void set_scope_type(ScopeType type);
    void set_update_rate(int fps = 30);

    // Data input
    void update_scope_data(const ve::audio::ProfessionalAudioScopes* scopes);

    // Visual configuration
    void set_persistence(bool enable);
    void set_grid_enabled(bool enable);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void on_scope_reset();

private:
    void setup_ui();
    
    ScopeType scope_type_;
    bool persistence_enabled_{true};
    bool grid_enabled_{true};
    
    // UI Layout
    QVBoxLayout* main_layout_;
    
    // Scope data
    ve::audio::ProfessionalAudioScopes::VectorscopeData vectorscope_data_;
    ve::audio::ProfessionalAudioScopes::PhaseCorrelationData correlation_data_;
    ve::audio::ProfessionalAudioScopes::SpectrumData spectrum_data_;
    
    // Visual settings
    QColor scope_color_{0, 255, 0};
    QColor grid_color_{100, 100, 100};
    QColor background_color_{10, 10, 10};
    QColor warning_color_{255, 165, 0};
    
    // Drawing helpers
    void draw_vectorscope(QPainter& painter, const QRect& rect);
    void draw_phase_correlation(QPainter& painter, const QRect& rect);
    void draw_spectrum_analyzer(QPainter& painter, const QRect& rect);
    void draw_grid(QPainter& painter, const QRect& rect, ScopeType type);
    void draw_scope_labels(QPainter& painter, const QRect& rect, ScopeType type);

/*
signals:
    void scope_reset();
*/
};

/**
 * Unified Professional Audio Monitoring Panel
 */
class ProfessionalAudioMonitoringPanel : public QWidget {
    // Q_OBJECT

public:
    explicit ProfessionalAudioMonitoringPanel(QWidget* parent = nullptr);
    ~ProfessionalAudioMonitoringPanel() = default;

    // Integration with monitoring system
    void set_monitoring_system(std::shared_ptr<ve::audio::ProfessionalAudioMonitoringSystem> system);
    
    // Configuration
    void set_compact_layout(bool compact);
    void set_target_platform(const std::string& platform);
    void set_show_advanced_scopes(bool show);

    // Control
    void start_monitoring();
    void stop_monitoring();
    void reset_all_meters();

    // Update (called from audio processing thread)
    void update_monitoring_data();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void on_monitoring_timer();
    void on_platform_changed(const std::string& platform);
    void on_reset_all();
    void on_scope_type_changed();

private:
    // Monitoring system
    std::shared_ptr<ve::audio::ProfessionalAudioMonitoringSystem> monitoring_system_;
    
    // UI Layout
    QVBoxLayout* main_layout_;
    QHBoxLayout* top_layout_;
    QHBoxLayout* bottom_layout_;
    
    // UI Components
    std::unique_ptr<LoudnessDisplayWidget> loudness_widget_;
    std::unique_ptr<ProfessionalMetersWidget> meters_widget_;
    std::unique_ptr<AudioScopesWidget> scopes_widget_;
    
    // Controls
    QGroupBox* controls_group_;
    QComboBox* platform_combo_;
    QPushButton* reset_button_;
    QPushButton* start_stop_button_;
    QComboBox* scope_type_combo_;
    
    // Status
    QLabel* status_label_;
    QLabel* performance_label_;
    
    // Configuration
    bool compact_layout_{false};
    bool show_advanced_scopes_{true};
    bool monitoring_active_{false};
    
    // Update timer
    QTimer* update_timer_;
    int update_rate_ms_{33}; // ~30fps
    
    // Visual themes
    void apply_professional_styling();
    void setup_layout();
    void setup_controls();
    void update_status_display();

/*
signals:
    void monitoring_started();
    void monitoring_stopped();
    void platform_changed(const std::string& platform);
*/
};

/**
 * Timeline-Integrated Audio Monitoring Widget
 */
class TimelineAudioMonitorWidget : public QWidget {
    // Q_OBJECT

public:
    explicit TimelineAudioMonitorWidget(QWidget* parent = nullptr);
    ~TimelineAudioMonitorWidget() = default;

    // Integration
    void set_monitoring_system(std::shared_ptr<ve::audio::ProfessionalAudioMonitoringSystem> system);
    void set_timeline_position(ve::TimePoint position);

    // Configuration
    void set_compact_mode(bool compact);
    void set_show_only_compliance(bool compliance_only);

    // Real-time updates
    void update_from_playback(const ve::audio::AudioFrame& frame);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    // Monitoring integration
    std::shared_ptr<ve::audio::ProfessionalAudioMonitoringSystem> monitoring_system_;
    ve::TimePoint current_position_{0, 1};
    
    // Compact display data
    struct CompactDisplayData {
        double integrated_lufs{-std::numeric_limits<double>::infinity()};
        double peak_level_db{-std::numeric_limits<double>::infinity()};
        bool broadcast_compliant{false};
        bool overload_detected{false};
    } display_data_;
    
    // Configuration
    bool compact_mode_{true};
    bool show_only_compliance_{false};
    
    // Visual elements
    void draw_compact_loudness_bar(QPainter& painter, const QRect& rect);
    void draw_peak_indicators(QPainter& painter, const QRect& rect);
    void draw_compliance_status(QPainter& painter, const QRect& rect);

/*
signals:
    void monitoring_warning(const QString& message);
    void compliance_status_changed(bool compliant);
*/
};

} // namespace ve::ui