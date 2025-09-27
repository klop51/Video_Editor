#pragma once
#include "playback_controller.hpp"
#include "../video/UiImageFrame.h"
#include <QObject>
#include <QThread>
#include <QMutex>

namespace ve::decode {

/**
 * @brief Qt-based playback controller that emits thread-safe signals
 * 
 * This class wraps the callback-based PlaybackController and converts
 * decoded VideoFrames into UI-owned UiImageFramePtr objects, emitting
 * them via queued signals to ensure thread safety.
 * 
 * Key safety features:
 * - Deep ARGB32 copies of frame data (no AVFrame aliasing)
 * - Queued signal emission for cross-thread communication
 * - Automatic metatype registration for Qt's signal/slot system
 */
class QtPlaybackController : public QObject {
    Q_OBJECT
    
public:
    explicit QtPlaybackController(std::unique_ptr<IDecoder> decoder, QObject* parent = nullptr);
    ~QtPlaybackController();
    
    // Playback control (thread-safe)
    bool start(int64_t start_pts_us);
    void stop();
    void set_media_path(const std::string& path);
    
    // State queries (thread-safe)
    int64_t current_pts() const;
    void set_playback_rate(double rate);
    PlaybackScheduler::TimingStats get_timing_stats() const;
    
signals:
    /**
     * @brief Emitted when a new UI-safe frame is ready for display
     * @param frame Deep-owned UiImageFramePtr safe for UI thread painting
     * 
     * This signal is emitted with Qt::QueuedConnection to ensure
     * cross-thread safety. The frame contains a deep ARGB32 copy
     * that the UI thread owns completely.
     */
    void uiFrameReady(UiImageFramePtr frame);
    
    /**
     * @brief Emitted when playback state changes
     * @param playing true if playback is active, false if stopped/paused
     */
    void playbackStateChanged(bool playing);

private slots:
    void processDecodedFrame(const VideoFrame& frame);
    
private:
    UiImageFramePtr convertToUiFrame(const VideoFrame& frame);
    
    std::unique_ptr<PlaybackController> controller_;
    mutable QMutex controller_mutex_; // Protect controller access (mutable for const methods)
    std::atomic<bool> running_{false};
};

} // namespace ve::decode