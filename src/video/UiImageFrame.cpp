#include "UiImageFrame.h"
#include <QMetaType>

/**
 * @file UiImageFrame.cpp
 * @brief Implementation of UI-owned image frame for thread-safe video display
 * 
 * This implementation ensures proper registration of the UiImageFramePtr
 * metatype for Qt's signal/slot system across threads.
 */

namespace {
    /**
     * @brief One-time registration of UiImageFramePtr metatype
     * 
     * This static initialization ensures the type is registered
     * before any cross-thread signals are connected.
     */
    class MetaTypeRegistrar {
    public:
        MetaTypeRegistrar() {
            qRegisterMetaType<UiImageFramePtr>("UiImageFramePtr");
        }
    };
    
    // Automatic registration on library load
    [[maybe_unused]] static MetaTypeRegistrar registrar;
}

// Note: All functionality is in the header for inline performance.
// This file exists primarily for metatype registration and future extensibility.