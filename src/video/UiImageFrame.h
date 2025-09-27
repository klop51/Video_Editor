#pragma once
#include <QImage>
#include <QSharedPointer>

/**
 * @brief UI-owned image frame for thread-safe video display
 * 
 * This class wraps a deep-owned QImage to ensure the UI thread
 * never accesses memory that the decoder thread might free.
 * 
 * Usage:
 *   - Decoder creates deep ARGB32 copy and wraps in UiImageFramePtr
 *   - Queued signal sends to UI thread
 *   - UI paints from guaranteed-owned QImage data
 */
class UiImageFrame {
public:
    /**
     * @brief Construct with owned QImage (default empty)
     * @param img QImage to take ownership of (moved)
     */
    explicit UiImageFrame(QImage img = {}) : img_(std::move(img)) {}
    
    /**
     * @brief Get const reference to owned image
     * @return const QImage& - safe for UI thread painting
     */
    const QImage& image() const { return img_; }
    
    /**
     * @brief Check if frame contains valid image data
     * @return true if image is not null and has valid format
     */
    bool isValid() const { return !img_.isNull() && img_.format() != QImage::Format_Invalid; }
    
    /**
     * @brief Get image dimensions for convenience
     * @return QSize of the contained image
     */
    QSize size() const { return img_.size(); }

private:
    QImage img_; ///< ARGB32 image deep-owned by UI thread
};

/**
 * @brief Shared pointer to UiImageFrame for cross-thread safety
 * 
 * Use this type for all signal/slot connections and storage.
 * Qt's meta-object system handles the reference counting safely.
 */
using UiImageFramePtr = QSharedPointer<UiImageFrame>;