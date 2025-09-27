#include "player_window.h"
#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QImage>
#include <QWidget>
#include "decode/decoder.hpp"
#include "decode/color_convert.hpp"
#include "decode/gpu_upload.hpp"

PlayerWindow::PlayerWindow(std::string mediaPath, QWidget* parent)
    : QWidget(parent), mediaPath_(std::move(mediaPath)) {
    auto layout = new QVBoxLayout(this);
    label_ = new QLabel("Loading...", this);
    label_->setMinimumSize(320,180);
    layout->addWidget(label_);
    setLayout(layout);
    startPlayback();
}

void PlayerWindow::startPlayback() {
    auto dec = ve::decode::create_decoder();
    controller_ = std::make_unique<ve::decode::PlaybackController>(std::move(dec));
    controller_->set_media_path(mediaPath_);
    controller_->start(0, [this](const ve::decode::VideoFrame& vf){
        // Convert RGBA frame to QImage
        if(vf.format != ve::decode::PixelFormat::RGBA32) return; // expectation from controller conversion
        QImage img(vf.data.data(), vf.width, vf.height, QImage::Format_RGBA8888);
        QTimer::singleShot(0, this, [this, img]() {
            label_->setPixmap(QPixmap::fromImage(img.scaled(label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        });
    });
}

#include "moc_player_window.cpp"
