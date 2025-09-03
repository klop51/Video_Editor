// TOKEN:VIEWER_PANEL_2025_08_29_CLEAN
#include "ui/viewer_panel.hpp"
#include "playback/controller.hpp"
#include "decode/decoder.hpp"
#include "decode/color_convert.hpp"
#include "decode/rgba_view.hpp"
#include "render/render_graph.hpp"
#include "gfx/vk_device.hpp"
#include "ui/gl_video_widget.hpp"
#include "core/profiling.hpp"
#include "core/log.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <algorithm>
#include <memory>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#ifndef VE_HEAP_DEBUG
#define VE_HEAP_DEBUG 0
#endif

namespace ve::ui {

ViewerPanel::ViewerPanel(QWidget* parent)
    : QWidget(parent)
    , playback_controller_(nullptr)
    , has_frame_(false)
    , aspect_ratio_mode_(Qt::KeepAspectRatio) {
    setup_ui();
    setAcceptDrops(true);
}

ViewerPanel::~ViewerPanel() = default;

void ViewerPanel::set_playback_controller(ve::playback::PlaybackController* controller) {
    playback_controller_ = controller;
    if (controller) {
        controller->add_video_callback([this](const ve::decode::VideoFrame& f) {
            ve::decode::VideoFrame copy = f;
            QMetaObject::invokeMethod(this,[this,copy](){ display_frame(copy); }, Qt::QueuedConnection);
        });
        controller->add_state_callback([this](ve::playback::PlaybackState st){
            QMetaObject::invokeMethod(this,[this,st](){ if (play_pause_button_) play_pause_button_->setText(st==ve::playback::PlaybackState::Playing?"Pause":"Play"); update_playback_controls(); }, Qt::QueuedConnection);
        });
    }
    update_playback_controls();
}

bool ViewerPanel::enable_gpu_pipeline() {
    if (gpu_enabled_) return true;
    if (!gpu_initialized_) {
        gfx_device_ = std::make_shared<ve::gfx::GraphicsDevice>();
        ve::gfx::GraphicsDeviceInfo info; if (!gfx_device_->create(info)) return false;
        render_graph_ = ve::render::create_gpu_render_graph(gfx_device_); if (!render_graph_) { gfx_device_.reset(); return false; }
        if (auto* gpu_rg = dynamic_cast<ve::render::GpuRenderGraph*>(render_graph_.get())) { QSize s = video_display_ ? video_display_->size() : size(); gpu_rg->set_viewport(s.width(), s.height()); }
        gpu_initialized_ = true;
    }
    if (!gl_widget_) {
        gl_widget_ = new GLVideoWidget(this);
        if (auto* lay = qobject_cast<QBoxLayout*>(layout())) {
            if (video_display_label_) { lay->removeWidget(video_display_label_); video_display_label_->hide(); }
            lay->insertWidget(0, gl_widget_, 1);
        }
        video_display_ = gl_widget_;
        if (fps_overlay_) { fps_overlay_->setParent(gl_widget_); fps_overlay_->move(8,8); fps_overlay_->raise(); }
    } else {
        gl_widget_->show(); if (video_display_label_) video_display_label_->hide(); video_display_ = gl_widget_;
    }
    gpu_enabled_ = true; ve::log::info("ViewerPanel: GPU pipeline enabled");
    return true;
}

void ViewerPanel::disable_gpu_pipeline() {
    if (!gpu_enabled_) return; gpu_enabled_ = false;
    if (gl_widget_) gl_widget_->hide();
    if (video_display_label_) {
        if (auto* lay = qobject_cast<QBoxLayout*>(layout())) {
            bool present=false; for (int i=0;i<lay->count();++i) if (lay->itemAt(i)->widget()==video_display_label_) { present=true; break; }
            if (!present) lay->insertWidget(0, video_display_label_,1);
        }
        video_display_label_->show(); video_display_ = video_display_label_;
        if (fps_overlay_) { fps_overlay_->setParent(video_display_label_); fps_overlay_->move(8,8); fps_overlay_->raise(); }
    }
    ve::log::info("ViewerPanel: GPU pipeline disabled");
}

void ViewerPanel::display_frame(const ve::decode::VideoFrame& frame) {
    VE_PROFILE_SCOPE_UNIQ("viewer.display_frame");
#if VE_HEAP_DEBUG && defined(_MSC_VER)
    _CrtCheckMemory();
#endif
    if (render_in_progress_) {
        pending_frame_ = frame;
        pending_frame_valid_ = true;
        return;
    }
    render_in_progress_ = true; current_frame_ = frame; has_frame_ = true;

    // FPS overlay update (every ~500ms)
    fps_frames_accum_++; qint64 now_ms = QDateTime::currentMSecsSinceEpoch(); if (fps_last_ms_==0) fps_last_ms_=now_ms;
    qint64 elapsed = now_ms - fps_last_ms_;
    if (elapsed >= 500 && fps_overlay_) {
        double inst_fps = elapsed>0 ? (fps_frames_accum_ * 1000.0 / elapsed) : 0.0;
        double avg_ms=0.0, avg_fps=0.0, target_fps=0.0;
        if (playback_controller_) {
            auto stats = playback_controller_->get_stats();
            avg_ms = stats.avg_frame_time_ms;
            if (avg_ms > 0) {
                avg_fps = 1000.0 / avg_ms;
            }
            auto us = playback_controller_->frame_duration_guess_us();
            if (us > 0) {
                target_fps = 1'000'000.0 / static_cast<double>(us);
            }
        }
        double show_fps = avg_fps > 0 ? avg_fps : inst_fps;
        if (show_fps > 0) {
            double warn_th = target_fps > 0 ? target_fps * 0.9 : 24.0;
            double bad_th = target_fps > 0 ? target_fps * 0.6 : 18.0;
            QString color = "#00ff66";
            if (show_fps < warn_th) {
                color = "#ffd24d";
            }
            if (show_fps < bad_th) {
                color = "#ff4d4d";
            }
            fps_overlay_->setStyleSheet(QString("QLabel { background-color: rgba(0,0,0,120); color:%1; padding:2px 6px; border-radius:3px; font:10pt 'Segoe UI'; }").arg(color));
            QString gpuTag = gpu_enabled_ ? " GPU" : "";
            if (avg_ms > 0) {
                fps_overlay_->setText(QString("%1 fps  %2 ms%3").arg(show_fps, 0, 'f', 1).arg(avg_ms, 0, 'f', 1).arg(gpuTag));
            } else {
                fps_overlay_->setText(QString("%1 fps%2").arg(show_fps, 0, 'f', 1).arg(gpuTag));
            }
            fps_overlay_->adjustSize();
        }
        fps_frames_accum_ = 0;
        fps_last_ms_ = now_ms;
    }

    if (gpu_enabled_ && gl_widget_) {
        // CRT check right before conversion
#if VE_HEAP_DEBUG && defined(_MSC_VER)
    _CrtCheckMemory();
#endif
        bool uploaded = false;
        if (auto view = ve::decode::to_rgba_scaled_view(current_frame_, current_frame_.width, current_frame_.height)) {
            gl_widget_->set_frame(view->data, view->width, view->height, view->stride, current_frame_.pts);
            uploaded = true;
        } else if (auto rgba = ve::decode::to_rgba(current_frame_)) {
            auto& rf = *rgba; size_t needed = static_cast<size_t>(rf.width) * static_cast<size_t>(rf.height) * 4;
            if (rf.data.size() >= needed) gl_widget_->set_frame(rf.data.data(), rf.width, rf.height, rf.width*4, rf.pts);
        }
        if (render_graph_) {
            if (auto* gpu_rg = dynamic_cast<ve::render::GpuRenderGraph*>(render_graph_.get())) {
                gpu_rg->set_current_frame(current_frame_);
                ve::render::FrameRequest req{current_frame_.pts};
                (void)gpu_rg->render(req);
            }
        }
    } else {
        update_frame_display();
    }

    render_in_progress_ = false;
    if (pending_frame_valid_) {
        auto next = pending_frame_;
        pending_frame_valid_ = false;
        QMetaObject::invokeMethod(this, [this, next]() {
            display_frame(next);
        }, Qt::QueuedConnection);
    }
}

void ViewerPanel::update_time_display(int64_t time_us) {
    int64_t total_ms = time_us / 1000; int h = int(total_ms / 3600000); int m = int((total_ms % 3600000)/60000); int s = int((total_ms % 60000)/1000); int ms = int(total_ms % 1000);
    time_label_->setText(QString("%1:%2:%3.%4").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0')).arg(ms,3,10,QChar('0')));
}

void ViewerPanel::update_playback_controls() {
    bool has = playback_controller_!=nullptr; play_pause_button_->setEnabled(has); stop_button_->setEnabled(has); step_backward_button_->setEnabled(has); step_forward_button_->setEnabled(has); position_slider_->setEnabled(has); if (has) play_pause_button_->setText(playback_controller_->state()==ve::playback::PlaybackState::Playing?"Pause":"Play");
}

void ViewerPanel::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e); if (!gpu_enabled_) update_frame_display(); else if (video_display_) video_display_->update();
    if (gpu_enabled_ && render_graph_) if (auto* gpu_rg = dynamic_cast<ve::render::GpuRenderGraph*>(render_graph_.get())) { QSize s = video_display_?video_display_->size():size(); gpu_rg->set_viewport(s.width(), s.height()); }
    if (fps_overlay_) fps_overlay_->move(8,8);
}

void ViewerPanel::set_fps_overlay_visible(bool on) { if (fps_overlay_) fps_overlay_->setVisible(on); }

void ViewerPanel::setup_ui() {
    setMinimumSize(400,300); auto* main_layout = new QVBoxLayout(this); main_layout->setContentsMargins(5,5,5,5); main_layout->setSpacing(5);
    video_display_label_ = new QLabel(); video_display_label_->setStyleSheet("QLabel { background-color: black; }"); video_display_label_->setAlignment(Qt::AlignCenter); video_display_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); video_display_label_->setText("No Video"); video_display_label_->setMinimumHeight(200); video_display_ = video_display_label_; main_layout->addWidget(video_display_label_,1);
    fps_overlay_ = new QLabel(video_display_label_); fps_overlay_->setText("fps"); fps_overlay_->setStyleSheet("QLabel { background-color: rgba(0,0,0,120); color: white; padding:2px 6px; border-radius:3px; font:10pt 'Segoe UI'; }"); fps_overlay_->setAttribute(Qt::WA_TransparentForMouseEvents); fps_overlay_->move(8,8); fps_overlay_->raise();
    auto* transport = new QHBoxLayout(); step_backward_button_ = new QPushButton("⏮"); step_backward_button_->setMaximumWidth(40); play_pause_button_ = new QPushButton("Play"); play_pause_button_->setMaximumWidth(60); step_forward_button_ = new QPushButton("⏭"); step_forward_button_->setMaximumWidth(40); stop_button_ = new QPushButton("Stop"); stop_button_->setMaximumWidth(50);
    transport->addWidget(step_backward_button_); transport->addWidget(play_pause_button_); transport->addWidget(step_forward_button_); transport->addWidget(stop_button_); transport->addSpacing(20); time_label_ = new QLabel("00:00:00.000"); time_label_->setMinimumWidth(80); transport->addWidget(time_label_); transport->addStretch(); main_layout->addLayout(transport);
    position_slider_ = new QSlider(Qt::Horizontal); position_slider_->setRange(0,1000); position_slider_->setValue(0); main_layout->addWidget(position_slider_);
    connect(play_pause_button_, &QPushButton::clicked, this, &ViewerPanel::on_play_pause_clicked);
    connect(stop_button_, &QPushButton::clicked, this, &ViewerPanel::on_stop_clicked);
    connect(step_forward_button_, &QPushButton::clicked, this, &ViewerPanel::on_step_forward_clicked);
    connect(step_backward_button_, &QPushButton::clicked, this, &ViewerPanel::on_step_backward_clicked);
    connect(position_slider_, &QSlider::valueChanged, this, &ViewerPanel::on_position_slider_changed);
    update_playback_controls();
}

void ViewerPanel::update_frame_display() {
    if (!has_frame_) { if (video_display_label_) video_display_label_->setText("No Video"); return; }
    QPixmap pix = convert_frame_to_pixmap(current_frame_); if (pix.isNull()) { if (video_display_label_) video_display_label_->setText("Invalid Frame"); return; }
    QSize ds = video_display_label_ ? video_display_label_->size() : size(); QPixmap scaled = pix.scaled(ds, aspect_ratio_mode_, Qt::FastTransformation); if (video_display_label_) video_display_label_->setPixmap(scaled);
}

QPixmap ViewerPanel::convert_frame_to_pixmap(const ve::decode::VideoFrame& frame) {
    if (frame.data.empty()) return QPixmap();
    QImage image;
    auto convert_yuv_like = [this,&frame]() -> QPixmap {
        int tw = frame.width, th = frame.height;
        if (preview_scale_to_widget_ && video_display_label_) {
            QSize disp = video_display_label_->size();
            if (disp.width()>0 && disp.height()>0 && frame.width>0 && frame.height>0) {
                double sx = double(disp.width())/frame.width; double sy = double(disp.height())/frame.height; double s = std::min(sx,sy); s = std::max(0.01,std::min(4.0,s));
                tw = std::max(1,int(frame.width*s+0.5)); th = std::max(1,int(frame.height*s+0.5));
            }
        }
        for (auto it = pix_cache_.begin(); it != pix_cache_.end(); ++it) {
            if (it->pts == frame.pts && it->w == tw && it->h == th) {
                return it->pix;
            }
        }
        if (auto view = ve::decode::to_rgba_scaled_view(frame, tw, th)) {
            QImage tmp(view->data, view->width, view->height, view->stride, QImage::Format_RGBA8888);
            QPixmap p = QPixmap::fromImage(tmp); // copies pixel data internally
            if (preview_scale_to_widget_ && pix_cache_capacity_ > 0) {
                if (int(pix_cache_.size()) >= pix_cache_capacity_) {
                    pix_cache_.pop_front();
                }
                pix_cache_.push_back(PixCacheEntry{frame.pts, view->width, view->height, p});
            }
            return p;
        }
        auto rgba = ve::decode::to_rgba_scaled(frame, tw, th);
        if (!rgba) {
            return QPixmap();
        }
        size_t expect = size_t(rgba->width) * rgba->height * 4;
        if (rgba->data.size() != expect) {
            return QPixmap(); // fallback owning path
        }
        QImage tmp(rgba->data.data(), rgba->width, rgba->height, rgba->width * 4, QImage::Format_RGBA8888);
        QPixmap p = QPixmap::fromImage(tmp);
        if (preview_scale_to_widget_ && pix_cache_capacity_ > 0) {
            if (int(pix_cache_.size()) >= pix_cache_capacity_) {
                pix_cache_.pop_front();
            }
            pix_cache_.push_back(PixCacheEntry{frame.pts, rgba->width, rgba->height, p});
        }
        return p;
    };
    switch (frame.format) {
        case ve::decode::PixelFormat::RGB24: { int bpl = frame.width*3; size_t expect=bpl*frame.height; if (frame.data.size()<expect) return QPixmap(); image = QImage(frame.data.data(), frame.width, frame.height, bpl, QImage::Format_RGB888); break; }
        case ve::decode::PixelFormat::RGBA32: { int bpl = frame.width*4; image = QImage(frame.data.data(), frame.width, frame.height, bpl, QImage::Format_RGBA8888); break; }
        case ve::decode::PixelFormat::YUV420P: case ve::decode::PixelFormat::YUV422P: case ve::decode::PixelFormat::YUV444P: case ve::decode::PixelFormat::YUV420P10LE: case ve::decode::PixelFormat::YUV422P10LE: case ve::decode::PixelFormat::YUV444P10LE: case ve::decode::PixelFormat::NV12: case ve::decode::PixelFormat::NV21: case ve::decode::PixelFormat::P010LE: case ve::decode::PixelFormat::YUYV422: case ve::decode::PixelFormat::UYVY422: case ve::decode::PixelFormat::GRAY8: return convert_yuv_like();
        default: return QPixmap();
    }
    if (image.isNull()) return QPixmap();
    return QPixmap::fromImage(image);
}

void ViewerPanel::on_play_pause_clicked() { emit play_pause_requested(); }
void ViewerPanel::on_stop_clicked() { emit stop_requested(); }
void ViewerPanel::on_position_slider_changed(int value) { if (!playback_controller_) return; int64_t dur = playback_controller_->duration_us(); emit seek_requested((dur * value)/1000); }
void ViewerPanel::on_step_forward_clicked() { if (playback_controller_) playback_controller_->step_once(); }
void ViewerPanel::on_step_backward_clicked() { if (!playback_controller_) return; int64_t fd = playback_controller_->frame_duration_guess_us(); if (fd<=0) fd = 1000000/30; int64_t cur = playback_controller_->current_time_us(); int64_t tgt = cur - fd; if (tgt<0) tgt=0; playback_controller_->seek(tgt); playback_controller_->step_once(); }

bool ViewerPanel::load_media(const QString& filePath) {
    ve::log::info("Loading media: " + filePath.toStdString());

    auto decoder = ve::decode::create_decoder();
    if (!decoder) { ve::log::error("Failed to create decoder"); return false; }

    ve::decode::OpenParams params; params.filepath = filePath.toStdString(); params.video = true; params.audio = false;
    ve::log::info("Attempting to open decoder with file: " + params.filepath);
    if (!decoder->open(params)) {
        QMessageBox::warning(this, "Media Load Failed", QString("Could not load media file:\n%1").arg(QFileInfo(filePath).fileName()));
        ve::log::warn("Failed to open decoder for: " + filePath.toStdString());
        return false;
    }
    ve::log::info("Decoder opened successfully");
    ve::log::info("Attempting to read first video frame...");
    if (auto frame = decoder->read_video()) {
        ve::log::info("Got video frame: " + std::to_string(frame->width) + "x" + std::to_string(frame->height) + ", format: " + std::to_string(static_cast<int>(frame->format)) + ", data size: " + std::to_string(frame->data.size()));
        display_frame(*frame);
        ve::log::info("Successfully loaded media: " + filePath.toStdString());
        return true;
    }
    ve::log::warn("read_video() returned nullopt - no frame could be read");
    QMessageBox::warning(this, "Media Load Failed", QString("Could not decode video from:\n%1").arg(QFileInfo(filePath).fileName()));
    ve::log::warn("Failed to decode video from: " + filePath.toStdString());
    return false;
}

void ViewerPanel::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) {
        for (const QUrl& url : e->mimeData()->urls()) if (url.isLocalFile()) {
            QString suf = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (suf=="mp4"||suf=="avi"||suf=="mov"||suf=="mkv"||suf=="wmv"||suf=="flv"||suf=="webm"||suf=="m4v") { e->acceptProposedAction(); return; }
        }
    }
    e->ignore();
}

void ViewerPanel::dropEvent(QDropEvent* e) {
    if (e->mimeData()->hasUrls()) {
        for (const QUrl& url : e->mimeData()->urls()) if (url.isLocalFile()) {
            if (load_media(url.toLocalFile())) { e->acceptProposedAction(); return; }
        }
    }
    e->ignore();
}

} // namespace ve::ui

// Removed explicit moc include; handled by AUTOMOC
