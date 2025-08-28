#pragma once
#include <string>

// Central logging configuration / helper macros.
// Define VE_TIMELINE_DEBUG or VE_UI_DEBUG (e.g. via compiler flags) to enable verbose logs.
// Define VE_ENABLE_VERBOSE_LOG for extra verbose categories globally.

namespace ve { namespace log { void debug(const std::string&); } }

#if defined(VE_ENABLE_VERBOSE_LOG)
  #define VE_VERBOSE_LOG 1
#else
  #define VE_VERBOSE_LOG 0
#endif

#if defined(VE_TIMELINE_DEBUG)
  #define VE_TL_DEBUG(msg) ::ve::log::debug(msg)
#else
  #define VE_TL_DEBUG(msg) do {} while(0)
#endif

#if defined(VE_UI_DEBUG)
  #define VE_UI_DEBUG_LOG(msg) ::ve::log::debug(msg)
#else
  #define VE_UI_DEBUG_LOG(msg) do {} while(0)
#endif

// Utility macro to silence an unused variable only when verbose logging disabled
#define VE_USE(var) do { (void)(var); } while(0)
