#include "decode/qt_playback_controller.hpp"
#include "decode/color_convert.hpp"
#include "core/log.hpp"
#include <QImage>
#include <QMetaType>
#include <QPointer>
#include <QTimer>

namespace ve::decode {

QtPlaybackController::QtPlaybackController(std::unique_ptr<IDecoder> decoder, QObject* parent)
    : QObject(parent)
    , controller_(std::make_unique<PlaybackController>(std::move(decoder))) {
    
    // Ensure UiImageFramePtr is registered for signal/slot system
    qRegisterMetaType<UiImageFramePtr>("UiImageFramePtr");
}

QtPlaybackController::~QtPlaybackController() {
    stop();
}

bool QtPlaybackController::start(int64_t start_pts_us) {
    QMutexLocker lock(&controller_mutex_);
    
    if (!controller_) {
        ve::log::error("QtPlaybackController: No decoder available");
        return false;
    }
    
    // Set up frame callback to process decoded frames with QPointer protection
    QPointer<QtPlaybackController> safe_self(this);
    auto frame_callback = [safe_self](const VideoFrame& frame) {
        // This runs on the decode thread - use QTimer for better Qt compatibility
        if (!safe_self) {
            ve::log::debug("QtPlaybackController lambda called on destroyed controller - aborting");
            return;
        }
        
        // Use QTimer::singleShot for thread-safe cross-thread calls
        QTimer::singleShot(0, safe_self.data(), [safe_self, frame]() {
            if (safe_self) {
                safe_self->processDecodedFrame(frame);
            }
        });
    };
    
    bool success = controller_->start(start_pts_us, frame_callback);
    if (success) {
        running_ = true;
        emit playbackStateChanged(true);
        ve::log::info("QtPlaybackController: Playback started successfully");
    } else {
        ve::log::error("QtPlaybackController: Failed to start playback");
    }
    
    return success;
}

void QtPlaybackController::stop() {
    {
        QMutexLocker lock(&controller_mutex_);
        if (controller_) {
            controller_->stop();
        }
    }
    
    if (running_.exchange(false)) {
        emit playbackStateChanged(false);
        ve::log::info("QtPlaybackController: Playback stopped");
    }
}

void QtPlaybackController::set_media_path(const std::string& path) {
    QMutexLocker lock(&controller_mutex_);
    if (controller_) {
        controller_->set_media_path(path);
    }
}

int64_t QtPlaybackController::current_pts() const {
    QMutexLocker lock(&controller_mutex_);
    return controller_ ? controller_->current_pts() : 0;
}

void QtPlaybackController::set_playback_rate(double rate) {
    QMutexLocker lock(&controller_mutex_);
    if (controller_) {
        controller_->set_playback_rate(rate);
    }
}

PlaybackScheduler::TimingStats QtPlaybackController::get_timing_stats() const {
    QMutexLocker lock(&controller_mutex_);
    return controller_ ? controller_->get_timing_stats() : PlaybackScheduler::TimingStats{};
}

void QtPlaybackController::processDecodedFrame(const VideoFrame& frame) {
    // This runs on the UI thread (via queued connection)
    if (!running_) {
        return; // Ignore frames if we're not running
    }
    
    // Convert to UI-safe frame with deep copy
    auto ui_frame = convertToUiFrame(frame);
    if (ui_frame && ui_frame->isValid()) {
        // Debug: Verify signal connection before emission
        int receivers = QObject::receivers(SIGNAL(uiFrameReady(UiImageFramePtr)));
        ve::log::debug("QtPlaybackController: Emitting uiFrameReady signal, receivers: " + std::to_string(receivers));
        
        // Emit with queued connection to ensure thread safety
        emit uiFrameReady(ui_frame);
    } else {
        ve::log::warn("QtPlaybackController: Failed to convert frame to UI format");
    }
}

UiImageFramePtr QtPlaybackController::convertToUiFrame(const VideoFrame& frame) {
    try {
        // Convert to ARGB32 format for Qt
        auto rgba = ve::decode::to_rgba(frame);
        if (!rgba || rgba->data.empty()) {
            ve::log::warn("QtPlaybackController: Failed to convert frame to RGBA");
            return nullptr;
        }
        
        // Create deep-owned QImage (ARGB32 is Qt's preferred format)
        QImage image(rgba->width, rgba->height, QImage::Format_ARGB32);
        
        // Copy RGBA data to ARGB32 (swapping R and B channels)
        const uint8_t* src = rgba->data.data();
        uint8_t* dst = image.bits();
        const size_t pixel_count = rgba->width * rgba->height;
        
        for (size_t i = 0; i < pixel_count; ++i) {
            const size_t src_offset = i * 4;
            const size_t dst_offset = i * 4;
            
            // RGBA -> ARGB (Qt byte order: BGRA in memory on little-endian)
            dst[dst_offset + 0] = src[src_offset + 2]; // B
            dst[dst_offset + 1] = src[src_offset + 1]; // G
            dst[dst_offset + 2] = src[src_offset + 0]; // R
            dst[dst_offset + 3] = src[src_offset + 3]; // A
        }
        
        // Create UI frame with deep-owned image
        auto ui_frame = UiImageFramePtr::create(std::move(image));
        
        ve::log::debug("QtPlaybackController: Converted frame " + std::to_string(frame.pts) + 
                       " to UI format (" + std::to_string(ui_frame->size().width()) + "x" + 
                       std::to_string(ui_frame->size().height()) + ")");
        
        return ui_frame;
        
    } catch (const std::exception& e) {
        ve::log::error("QtPlaybackController: Exception converting frame: " + std::string(e.what()));
        return nullptr;
    } catch (...) {
        ve::log::error("QtPlaybackController: Unknown exception converting frame");
        return nullptr;
    }
}

} // namespace ve::decode