// TOKEN:VIEWER_PANEL_2025_08_27_A (sentinel for stale build detection)
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
#include <QMessageBox>
#include <QFileInfo>
#include <algorithm>
#include <QMetaObject>
#include <QDir>
#include <cstdlib>
#include <fstream>
#include <QMetaObject>
#include <QDateTime>

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

void ViewerPanel::set_playback_controller(ve::playback::PlaybackController* controller) {
    playback_controller_ = controller;
    
    if (controller) {
        // Thread-safe frame delivery: marshal to GUI thread (Qt requires widget updates on main thread)
        controller->add_video_callback([this](const ve::decode::VideoFrame& frame) {
            ve::decode::VideoFrame frame_copy = frame;
            QMetaObject::invokeMethod(this, [this, frame_copy]() { display_frame(frame_copy); }, Qt::QueuedConnection);
        });
        controller->add_state_callback([this](ve::playback::PlaybackState state) {
            QMetaObject::invokeMethod(this, [this, state]() {
                if (play_pause_button_) play_pause_button_->setText(state == ve::playback::PlaybackState::Playing ? "Pause" : "Play");
                update_playback_controls();
            }, Qt::QueuedConnection);
        });
    }
    
    update_playback_controls();
}

void ViewerPanel::display_frame(const ve::decode::VideoFrame& frame) {
    // Coalesce incoming frames if we are still rendering the previous one
    if (render_in_progress_) {
        pending_frame_ = frame;
        pending_frame_valid_ = true;
        return;
    }
    render_in_progress_ = true;
    current_frame_ = frame;
    has_frame_ = true;

    // Update small FPS overlay (throttle ~500ms)
    fps_frames_accum_++;
    qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    if (fps_last_ms_ == 0) fps_last_ms_ = now_ms;
    qint64 elapsed_ms = now_ms - fps_last_ms_;
    if (elapsed_ms >= 500 && fps_overlay_) {
        double inst_fps = (elapsed_ms > 0) ? (fps_frames_accum_ * 1000.0 / elapsed_ms) : 0.0;
        double avg_ms = 0.0;
        double avg_fps = 0.0;
        if (playback_controller_) {
            auto stats = playback_controller_->get_stats();
            avg_ms = stats.avg_frame_time_ms;
            if (avg_ms > 0.0) avg_fps = 1000.0 / avg_ms;
        }
        double target_fps = 0.0;
        if (playback_controller_) {
            auto us = playback_controller_->frame_duration_guess_us();
            if (us > 0) target_fps = 1'000'000.0 / static_cast<double>(us);
        }
        double show_fps = (avg_fps > 0.0 ? avg_fps : inst_fps);
        if (show_fps > 0.0) {
            // Decide overlay color: green ok, yellow warn, red bad
            double warn_threshold = (target_fps > 0.0) ? target_fps * 0.9 : 24.0;
            double bad_threshold  = (target_fps > 0.0) ? target_fps * 0.6 : 18.0;
            QString color = "#00ff66"; // green
            if (show_fps < warn_threshold) color = "#ffd24d"; // yellow
            if (show_fps < bad_threshold)  color = "#ff4d4d"; // red
            fps_overlay_->setStyleSheet(
                QString("QLabel { background-color: rgba(0,0,0,120); color: %1; padding: 2px 6px; border-radius: 3px; font: 10pt 'Segoe UI'; }")
                .arg(color)
            );
            if (avg_ms > 0.0) {
                fps_overlay_->setText(QString("%1 fps  %2 ms")
                                      .arg(show_fps, 0, 'f', 1)
                                      .arg(avg_ms, 0, 'f', 1));
            } else {
                fps_overlay_->setText(QString("%1 fps").arg(show_fps, 0, 'f', 1));
            }
            fps_overlay_->adjustSize();
        }
        fps_frames_accum_ = 0;
        fps_last_ms_ = now_ms;
    }

    // Optional frame dump (RGBA) for debugging conversion path
    static int dumped = 0;
    static const bool dump_enabled = qEnvironmentVariableIsSet("VE_DUMP_FRAMES");
    if (dump_enabled && dumped < 5) {
        auto rgba_opt = ve::decode::to_rgba(frame);
        if (rgba_opt) {
            const auto& rf = *rgba_opt;
            QDir().mkpath("frame_dumps");
            std::string fname = "frame_dumps/frame_" + std::to_string(rf.pts) + "_" + std::to_string(dumped) + ".ppm";
            std::ofstream ofs(fname, std::ios::binary);
            if (ofs) {
                ofs << "P6\n" << rf.width << " " << rf.height << "\n255\n";
                const uint8_t* data = rf.data.data();
                size_t pixels = static_cast<size_t>(rf.width) * rf.height;
                for (size_t i = 0; i < pixels; ++i) {
                    ofs.put(static_cast<char>(data[i*4 + 0])); // R
                    ofs.put(static_cast<char>(data[i*4 + 1])); // G
                    ofs.put(static_cast<char>(data[i*4 + 2])); // B
                }
                ve::log::info("Dumped frame to " + fname);
                ++dumped;
            }
        }
    }
    update_frame_display();

    render_in_progress_ = false;
    if (pending_frame_valid_) {
        // Display the most recent pending frame, dropping older ones
        auto next = pending_frame_;
        pending_frame_valid_ = false;
        QMetaObject::invokeMethod(this, [this, next]() { display_frame(next); }, Qt::QueuedConnection);
    }
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
    if (fps_overlay_) fps_overlay_->move(8, 8);
}

void ViewerPanel::set_fps_overlay_visible(bool on) {
    if (fps_overlay_) fps_overlay_->setVisible(on);
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

    // Lightweight FPS overlay on top-left of the video display
    fps_overlay_ = new QLabel(video_display_);
    fps_overlay_->setText("fps");
    fps_overlay_->setStyleSheet(
        "QLabel { background-color: rgba(0,0,0,120); color: white;"
        " padding: 2px 6px; border-radius: 3px; font: 10pt 'Segoe UI'; }"
    );
    fps_overlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    fps_overlay_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    fps_overlay_->setMinimumWidth(70);
    fps_overlay_->setMinimumHeight(22);
    fps_overlay_->move(8, 8);
    fps_overlay_->raise();
    
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
    // Use fast transformation for real-time playback performance
    QSize display_size = video_display_->size();
    QPixmap scaled_pixmap = pixmap.scaled(display_size, aspect_ratio_mode_, Qt::FastTransformation);
    
    video_display_->setPixmap(scaled_pixmap);
}

QPixmap ViewerPanel::convert_frame_to_pixmap(const ve::decode::VideoFrame& frame) {
    if (frame.data.empty()) {
        static int empty_warns = 0;
        if (empty_warns < 5) {
            ve::log::warn("Frame data is empty");
            if (++empty_warns == 5) ve::log::warn("Further empty frame warnings suppressed");
        }
        return QPixmap();
    }

    // Convert frame data to QImage based on format
    QImage image;

    auto convert_yuv_like = [this,&frame]() -> QPixmap {
        // Compute target size if scaling preview is enabled
        int target_w = frame.width;
        int target_h = frame.height;
        if (preview_scale_to_widget_ && video_display_) {
            QSize disp = video_display_->size();
            if (disp.width() > 0 && disp.height() > 0 && frame.width > 0 && frame.height > 0) {
                double sx = static_cast<double>(disp.width()) / static_cast<double>(frame.width);
                double sy = static_cast<double>(disp.height()) / static_cast<double>(frame.height);
                double s = std::min(sx, sy);
                s = std::max(0.01, std::min(4.0, s)); // clamp sanity
                target_w = std::max(1, static_cast<int>(frame.width * s + 0.5));
                target_h = std::max(1, static_cast<int>(frame.height * s + 0.5));
            }
        }

        // Cache lookup for converted pixmap
        for (auto it = pix_cache_.begin(); it != pix_cache_.end(); ++it) {
            if (it->pts == frame.pts && it->w == target_w && it->h == target_h) {
                return it->pix;
            }
        }

        auto rgba_frame = ve::decode::to_rgba_scaled(frame, target_w, target_h);
        if (!rgba_frame) {
            return QPixmap();
        }
        size_t expected_rgba_size = static_cast<size_t>(rgba_frame->width) * rgba_frame->height * 4;
        if (rgba_frame->data.size() != expected_rgba_size) {
            ve::log::warn("Invalid RGBA frame size (" + std::to_string(rgba_frame->data.size()) + ") expected " + std::to_string(expected_rgba_size));
            return QPixmap();
        }
        int bytes_per_line = rgba_frame->width * 4;
        QImage temp_image(rgba_frame->data.data(), rgba_frame->width, rgba_frame->height, bytes_per_line, QImage::Format_RGBA8888);
        QPixmap pix = QPixmap::fromImage(temp_image);
        // Insert into small cache (avoid caching massive full-res pixmaps)
        if (preview_scale_to_widget_ && pix_cache_capacity_ > 0) {
            if (static_cast<int>(pix_cache_.size()) >= pix_cache_capacity_) pix_cache_.pop_front();
            pix_cache_.push_back(PixCacheEntry{frame.pts, rgba_frame->width, rgba_frame->height, pix});
        }
        return pix;
    };

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
        case ve::decode::PixelFormat::YUV420P:
        case ve::decode::PixelFormat::YUV422P:
        case ve::decode::PixelFormat::YUV444P:
        case ve::decode::PixelFormat::YUV420P10LE:
        case ve::decode::PixelFormat::YUV422P10LE:
        case ve::decode::PixelFormat::YUV444P10LE:
        case ve::decode::PixelFormat::NV12:
        case ve::decode::PixelFormat::NV21:
        case ve::decode::PixelFormat::P010LE:
        case ve::decode::PixelFormat::YUYV422:
        case ve::decode::PixelFormat::UYVY422:
        case ve::decode::PixelFormat::GRAY8: {
            return convert_yuv_like(); // Use common conversion path (will log if unsupported)
        }
        default: {
            static int unsupported_count = 0;
            if (unsupported_count < 10) {
                ve::log::warn("Unsupported pixel format for display: " + std::to_string(static_cast<int>(frame.format)));
                if (++unsupported_count == 10) {
                    ve::log::warn("Further unsupported pixel format warnings suppressed");
                }
            }
            // Attempt a best-effort treat-as YUV conversion if it looks like planar 420 (size heuristic)
            if (frame.format == ve::decode::PixelFormat::Unknown) {
                size_t y_size = static_cast<size_t>(frame.width) * frame.height;
                if (frame.data.size() == y_size + 2 * (y_size / 4)) { // Looks like 420 planar
                    ve::log::info("Heuristic: treating Unknown frame as YUV420P for display");
                    ve::decode::VideoFrame f2 = frame;
                    f2.format = ve::decode::PixelFormat::YUV420P; // temporary reinterpret
                    return convert_yuv_like();
                }
            }
            return QPixmap();
        }
    }

    if (image.isNull()) {
        static int create_fail = 0;
        if (create_fail < 5) {
            ve::log::warn("Failed to create QImage from frame data");
            if (++create_fail == 5) ve::log::warn("Further QImage creation warnings suppressed");
        }
        return QPixmap();
    }
    return QPixmap::fromImage(image);
}
void ViewerPanel::on_play_pause_clicked() {
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
    // Do not seek for forward step: many decoders jump to previous keyframe.
    // Instead request the controller to advance by one frame internally.
    playback_controller_->step_once();
}

void ViewerPanel::on_step_backward_clicked() {
    if (!playback_controller_) return;
    int64_t frame_duration = playback_controller_->frame_duration_guess_us();
    if (frame_duration <= 0) frame_duration = 1000000 / 30;
    int64_t cur = playback_controller_->current_time_us();
    int64_t tgt = cur - frame_duration; if (tgt < 0) tgt = 0;
    playback_controller_->seek(tgt);
    playback_controller_->step_once();
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

// Removed explicit moc include; handled by AUTOMOC
