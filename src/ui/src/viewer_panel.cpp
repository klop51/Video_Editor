#include "ui/viewer_panel.hpp"
#include "playback/controller.hpp"
#include "decode/decoder.hpp"
#include "decode/color_convert.hpp"
#include "core/log.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>

namespace ve::ui {

ViewerPanel::ViewerPanel(QWidget* parent)
    : QWidget(parent)
    , playback_controller_(nullptr)
    , has_frame_(false)
    , aspect_ratio_mode_(Qt::KeepAspectRatio)
{
    setup_ui();
    
    // Enable drag and drop
    setAcceptDrops(true);
}

void ViewerPanel::set_playback_controller(ve::playback::Controller* controller) {
    playback_controller_ = controller;
    
    if (controller) {
        // Connect playback controller to send frames to viewer during playback
        controller->set_video_callback([this](const ve::decode::VideoFrame& frame) { 
            display_frame(frame); 
        });
        
        // Connect state changes to update playback controls
        controller->set_state_callback([this](ve::playback::PlaybackState state) {
            update_playback_controls();
        });
    }
    
    update_playback_controls();
}

void ViewerPanel::display_frame(const ve::decode::VideoFrame& frame) {
    current_frame_ = frame;
    has_frame_ = true;
    update_frame_display();
}

void ViewerPanel::update_time_display(int64_t time_us) {
    // Convert microseconds to displayable time format
    int64_t total_ms = time_us / 1000;
    int hours = static_cast<int>(total_ms / 3600000);
    int minutes = static_cast<int>((total_ms % 3600000) / 60000);
    int seconds = static_cast<int>((total_ms % 60000) / 1000);
    int milliseconds = static_cast<int>(total_ms % 1000);
    
    QString time_str = QString("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds, 3, 10, QChar('0'));
    
    time_label_->setText(time_str);
}

void ViewerPanel::update_playback_controls() {
    bool has_controller = playback_controller_ != nullptr;
    
    play_pause_button_->setEnabled(has_controller);
    stop_button_->setEnabled(has_controller);
    step_backward_button_->setEnabled(has_controller);
    step_forward_button_->setEnabled(has_controller);
    position_slider_->setEnabled(has_controller);
    
    if (has_controller) {
        auto state = playback_controller_->state();
        play_pause_button_->setText(state == ve::playback::PlaybackState::Playing ? "Pause" : "Play");
    }
}

void ViewerPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update_frame_display();
}

void ViewerPanel::setup_ui() {
    setMinimumSize(400, 300);
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(5, 5, 5, 5);
    main_layout->setSpacing(5);
    
    // Video display area
    video_display_ = new QLabel();
    video_display_->setStyleSheet("QLabel { background-color: black; }");
    video_display_->setAlignment(Qt::AlignCenter);
    video_display_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    video_display_->setText("No Video");
    video_display_->setMinimumHeight(200);
    
    main_layout->addWidget(video_display_, 1); // Give it most of the space
    
    // Transport controls
    QHBoxLayout* transport_layout = new QHBoxLayout();
    
    step_backward_button_ = new QPushButton("⏮");
    step_backward_button_->setMaximumWidth(40);
    transport_layout->addWidget(step_backward_button_);
    
    play_pause_button_ = new QPushButton("Play");
    play_pause_button_->setMaximumWidth(60);
    transport_layout->addWidget(play_pause_button_);
    
    step_forward_button_ = new QPushButton("⏭");
    step_forward_button_->setMaximumWidth(40);
    transport_layout->addWidget(step_forward_button_);
    
    stop_button_ = new QPushButton("Stop");
    stop_button_->setMaximumWidth(50);
    transport_layout->addWidget(stop_button_);
    
    transport_layout->addSpacing(20);
    
    // Time display
    time_label_ = new QLabel("00:00:00.000");
    time_label_->setMinimumWidth(80);
    transport_layout->addWidget(time_label_);
    
    transport_layout->addStretch();
    
    main_layout->addLayout(transport_layout);
    
    // Position slider
    position_slider_ = new QSlider(Qt::Horizontal);
    position_slider_->setRange(0, 1000);
    position_slider_->setValue(0);
    main_layout->addWidget(position_slider_);
    
    // Connect signals
    connect(play_pause_button_, &QPushButton::clicked, this, &ViewerPanel::on_play_pause_clicked);
    connect(stop_button_, &QPushButton::clicked, this, &ViewerPanel::on_stop_clicked);
    connect(step_forward_button_, &QPushButton::clicked, this, &ViewerPanel::on_step_forward_clicked);
    connect(step_backward_button_, &QPushButton::clicked, this, &ViewerPanel::on_step_backward_clicked);
    connect(position_slider_, &QSlider::valueChanged, this, &ViewerPanel::on_position_slider_changed);
    
    // Initialize state
    update_playback_controls();
}

void ViewerPanel::update_frame_display() {
    if (!has_frame_) {
        video_display_->setText("No Video");
        return;
    }
    
    QPixmap pixmap = convert_frame_to_pixmap(current_frame_);
    if (pixmap.isNull()) {
        ve::log::warn("Failed to convert frame to pixmap");
        video_display_->setText("Invalid Frame");
        return;
    }
    
    // Scale pixmap to fit display while maintaining aspect ratio
    QSize display_size = video_display_->size();
    QPixmap scaled_pixmap = pixmap.scaled(display_size, aspect_ratio_mode_, Qt::SmoothTransformation);
    
    video_display_->setPixmap(scaled_pixmap);
}

QPixmap ViewerPanel::convert_frame_to_pixmap(const ve::decode::VideoFrame& frame) {
    if (frame.data.empty()) {
        ve::log::warn("Frame data is empty");
        return QPixmap();
    }

    // Convert frame data to QImage based on format
    QImage image;
    
    switch (frame.format) {
        case ve::decode::PixelFormat::RGB24: {
            int bytes_per_line = frame.width * 3;
            size_t expected_size = bytes_per_line * frame.height;
            
            if (frame.data.size() < expected_size) {
                ve::log::warn("Insufficient data for RGB24 image");
                return QPixmap();
            }
            
            image = QImage(frame.data.data(), frame.width, frame.height, bytes_per_line, QImage::Format_RGB888);
            break;
        }
            
        case ve::decode::PixelFormat::RGBA32: {
            int bytes_per_line = frame.width * 4;
            image = QImage(frame.data.data(), frame.width, frame.height, bytes_per_line, QImage::Format_RGBA8888);
            break;
        }
            
        case ve::decode::PixelFormat::YUV420P: {
            // Use optimized color conversion from decode module
            auto rgba_frame = ve::decode::to_rgba(frame);
            if (!rgba_frame) {
                ve::log::warn("Failed to convert YUV420P to RGBA");
                return QPixmap();
            }
            
            // Validate RGBA frame data
            size_t expected_rgba_size = static_cast<size_t>(rgba_frame->width) * rgba_frame->height * 4;
            if (rgba_frame->data.size() != expected_rgba_size) {
                ve::log::warn("Invalid RGBA frame data size: expected " + std::to_string(expected_rgba_size) + ", got " + std::to_string(rgba_frame->data.size()));
                return QPixmap();
            }
            
            int bytes_per_line = rgba_frame->width * 4;
            
            // Create QImage with data copy to avoid pointer issues
            QImage temp_image(rgba_frame->data.data(), rgba_frame->width, rgba_frame->height, bytes_per_line, QImage::Format_RGBA8888);
            image = temp_image.copy(); // Make a deep copy to avoid dangling pointer
            break;
        }
            
        default:
            ve::log::warn("Unsupported pixel format for display: " + std::to_string(static_cast<int>(frame.format)));
            return QPixmap();
    }
    
    if (image.isNull()) {
        ve::log::warn("Failed to create QImage from frame data");
        return QPixmap();
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    return pixmap;
}void ViewerPanel::on_play_pause_clicked() {
    emit play_pause_requested();
}

void ViewerPanel::on_stop_clicked() {
    emit stop_requested();
}

void ViewerPanel::on_position_slider_changed(int value) {
    if (!playback_controller_) return;
    
    // Convert slider value (0-1000) to timestamp
    int64_t duration_us = playback_controller_->duration_us();
    int64_t seek_time_us = (duration_us * value) / 1000;
    
    emit seek_requested(seek_time_us);
}

void ViewerPanel::on_step_forward_clicked() {
    if (!playback_controller_) return;
    
    // Step forward by one frame (assuming 30fps for now)
    int64_t current_time = playback_controller_->current_time_us();
    int64_t frame_duration = 1000000 / 30; // 30fps
    emit seek_requested(current_time + frame_duration);
}

void ViewerPanel::on_step_backward_clicked() {
    if (!playback_controller_) return;
    
    // Step backward by one frame (assuming 30fps for now)
    int64_t current_time = playback_controller_->current_time_us();
    int64_t frame_duration = 1000000 / 30; // 30fps
    int64_t new_time = std::max(0LL, current_time - frame_duration);
    emit seek_requested(new_time);
}

bool ViewerPanel::load_media(const QString& filePath) {
    ve::log::info("Loading media: " + filePath.toStdString());
    
    // Create a decoder to test the media
    auto decoder = ve::decode::create_decoder();
    if (!decoder) {
        ve::log::error("Failed to create decoder");
        return false;
    }
    
    ve::decode::OpenParams params;
    params.filepath = filePath.toStdString();
    params.video = true;
    params.audio = false; // For now, just test video
    
    ve::log::info("Attempting to open decoder with file: " + params.filepath);
    if (!decoder->open(params)) {
        QMessageBox::warning(this, "Media Load Failed", 
                           QString("Could not load media file:\n%1")
                           .arg(QFileInfo(filePath).fileName()));
        ve::log::warn("Failed to open decoder for: " + filePath.toStdString());
        return false;
    }
    
    ve::log::info("Decoder opened successfully");
    
    // Try to read the first frame
    ve::log::info("Attempting to read first video frame...");
    auto frame = decoder->read_video();
    if (frame) {
        ve::log::info("Got video frame: " + std::to_string(frame->width) + "x" + 
                     std::to_string(frame->height) + ", format: " + 
                     std::to_string(static_cast<int>(frame->format)) + 
                     ", data size: " + std::to_string(frame->data.size()));
        
        display_frame(*frame);
        ve::log::info("Successfully loaded media: " + filePath.toStdString());
        
        // Update video display to show it's loaded, but the actual frame should be displayed
        return true;
    } else {
        ve::log::warn("read_video() returned nullopt - no frame could be read");
        QMessageBox::warning(this, "Media Load Failed", 
                           QString("Could not decode video from:\n%1")
                           .arg(QFileInfo(filePath).fileName()));
        ve::log::warn("Failed to decode video from: " + filePath.toStdString());
        return false;
    }
}

void ViewerPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        // Check if any of the URLs are media files
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                QString suffix = QFileInfo(filePath).suffix().toLower();
                
                // Accept common video formats
                if (suffix == "mp4" || suffix == "avi" || suffix == "mov" || 
                    suffix == "mkv" || suffix == "wmv" || suffix == "flv" ||
                    suffix == "webm" || suffix == "m4v") {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
    event->ignore();
}

void ViewerPanel::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                if (load_media(filePath)) {
                    event->acceptProposedAction();
                    return; // Only load the first valid file
                }
            }
        }
    }
    event->ignore();
}

} // namespace ve::ui

#include "viewer_panel.moc"
