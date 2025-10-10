/**
 * @file audio_monitor_widget.hpp
 * @brief Professional audio monitoring widgets for real-time level visualization
 *
 * Provides Qt-based widgets for professional audio monitoring including VU meters,
 * peak meters, loudness monitoring, and comprehensive audio analysis displays
 * with EBU R128 compliance and broadcast-standard ballistics.
 */

#pragma once

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QFont>
#include <memory>
#include <vector>
#include <array>

namespace ve {
namespace ui {

/**
 * @brief Professional VU meter widget with accurate ballistics
 *
 * Implements broadcast-standard VU meter with proper attack/decay times,
 * peak hold functionality, and stereo/mono display modes.
 */
class VUMeterWidget : public QWidget {
    Q_OBJECT

public:
    enum class Orientation { Horizontal, Vertical };
    enum class MeterType { VU, Peak, DigitalPeak };

    explicit VUMeterWidget(QWidget* parent = nullptr);
    ~VUMeterWidget() override = default;

    // Configuration
    void setOrientation(Orientation orientation);
    void setMeterType(MeterType type);
    void setStereo(bool stereo);
    void setRange(float minDb, float maxDb);
    void setPeakHoldTime(int milliseconds);
    void setDecayRate(float rate);

    // Level updates
    void updateLevel(float levelDb);
    void updateStereoLevels(float leftDb, float rightDb);
    void resetPeakHold();

signals:
    void clippingDetected(bool detected);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateMeter();
    void resetPeakHoldTimeout();

private:
    void drawMeter(QPainter& painter);
    void drawScale(QPainter& painter);
    void drawLevelBar(QPainter& painter, const QRectF& rect, float levelDb,
                     const QColor& color, bool isPeak = false);

    // Configuration
    Orientation orientation_ = Orientation::Vertical;
    MeterType meterType_ = MeterType::VU;
    bool isStereo_ = false;
    float minDb_ = -60.0f;
    float maxDb_ = 6.0f;
    int peakHoldTimeMs_ = 3000;
    float decayRate_ = 0.1f;

    // State
    float currentLevelDb_ = -60.0f;
    float leftLevelDb_ = -60.0f;
    float rightLevelDb_ = -60.0f;
    float peakLevelDb_ = -60.0f;
    float peakHoldLevel_ = -60.0f;
    bool clippingDetected_ = false;

    // UI
    QRectF meterRect_;
    static constexpr int SCALE_WIDTH = 30;

    // Timers
    QTimer* updateTimer_;
    QTimer* peakTimer_;
};

/**
 * @brief Comprehensive audio monitor panel with multiple meters
 *
 * Provides a complete audio monitoring interface with VU meters, correlation meter,
 * loudness display, and master controls for professional audio monitoring.
 */
class AudioMonitorPanel : public QWidget {
    Q_OBJECT

public:
    explicit AudioMonitorPanel(QWidget* parent = nullptr);
    ~AudioMonitorPanel() override = default;

    // Channel configuration
    void setChannelCount(size_t channels);
    void setChannelLabels(const std::vector<std::string>& labels);

    // Level updates
    void updateAudioLevels(const std::vector<float>& levelsDb);
    void updateLoudness(float integratedLufs, float shortTermLufs, float momentaryLufs);

    // Controls
    void setMasterGain(float gainDb);
    float getMasterGain() const;

signals:
    void masterGainChanged(float gainDb);
    void soloChannel(int channel);
    void muteChannel(int channel);

private slots:
    void onMasterGainChanged(int value);
    void onSoloClicked();
    void onMuteClicked();

private:
    void setupUI();
    void updateMeters();

    // UI components
    QVBoxLayout* mainLayout_;
    QHBoxLayout* metersLayout_;
    QHBoxLayout* controlsLayout_;

    std::vector<std::unique_ptr<VUMeterWidget>> channelMeters_;
    std::vector<QPushButton*> soloButtons_;
    std::vector<QPushButton*> muteButtons_;

    QLabel* integratedLabel_;
    QLabel* shortTermLabel_;
    QLabel* momentaryLabel_;

    QSlider* masterGainSlider_;
    QLabel* masterGainLabel_;

    // State
    size_t channelCount_ = 2;
    float masterGainDb_ = 0.0f;
    std::vector<float> currentLevels_;
    std::vector<std::string> channelLabels_;
};

/**
 * @brief Compact audio meter for toolbar integration
 *
 * Minimal audio level display suitable for embedding in toolbars,
 * status bars, or other compact UI areas.
 */
class CompactAudioMeter : public QWidget {
    Q_OBJECT

public:
    explicit CompactAudioMeter(QWidget* parent = nullptr);
    ~CompactAudioMeter() override = default;

    void setLevel(float levelDb);
    void setStereoLevels(float leftDb, float rightDb);
    void setShowPeak(bool show);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void drawCompactMeter(QPainter& painter);

    float levelDb_ = -60.0f;
    float leftDb_ = -60.0f;
    float rightDb_ = -60.0f;
    float peakDb_ = -60.0f;
    bool showPeak_ = true;
    bool isStereo_ = false;
};

} // namespace ui
} // namespace ve
