#pragma once

#include <QWidget>
#include <QLabel>
#include <memory>
#include <string>
#include "decode/playback_controller.hpp"

class PlayerWindow : public QWidget {
    Q_OBJECT
public:
    PlayerWindow(std::string mediaPath, QWidget* parent = nullptr);

private:
    void startPlayback();
    
    std::string mediaPath_;
    QLabel* label_;
    std::unique_ptr<ve::decode::PlaybackController> controller_;
};
