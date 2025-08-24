#pragma once
#include "decode/frame.hpp"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QPixmap>

QT_BEGIN_NAMESPACE
class QDragEnterEvent;
class QDropEvent;
QT_END_NAMESPACE

namespace ve::playback { class Controller; }

namespace ve::ui {

class ViewerPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit ViewerPanel(QWidget* parent = nullptr);
    
    void set_playback_controller(ve::playback::Controller* controller);
    
    // Media loading
    bool load_media(const QString& filePath);
    
public slots:
    void display_frame(const ve::decode::VideoFrame& frame);
    void update_time_display(int64_t time_us);
    void update_playback_controls();
    
signals:
    void play_pause_requested();
    void stop_requested();
    void seek_requested(int64_t time_us);
    
protected:
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    
private slots:
    void on_play_pause_clicked();
    void on_stop_clicked();
    void on_position_slider_changed(int value);
    void on_step_forward_clicked();
    void on_step_backward_clicked();
    
private:
    void setup_ui();
    void update_frame_display();
    QPixmap convert_frame_to_pixmap(const ve::decode::VideoFrame& frame);
    
    // UI components
    QLabel* video_display_;
    QLabel* time_label_;
    QPushButton* play_pause_button_;
    QPushButton* stop_button_;
    QPushButton* step_backward_button_;
    QPushButton* step_forward_button_;
    QSlider* position_slider_;
    
    // Data
    ve::playback::Controller* playback_controller_;
    ve::decode::VideoFrame current_frame_;
    bool has_frame_;
    
    // Display scaling
    QSize display_size_;
    Qt::AspectRatioMode aspect_ratio_mode_;
};

} // namespace ve::ui
