#pragma once

// VE_RUNTIME_DEBUG is defined by CMake. Default it OFF if someone includes this directly.
#ifndef VE_RUNTIME_DEBUG
  #define VE_RUNTIME_DEBUG 0
#endif

// Compile-out macro for heavy debug blocks (no runtime cost when OFF)
#if VE_RUNTIME_DEBUG
  #define VE_DEBUG_ONLY(stmt) do { stmt; } while (0)
#else
  #define VE_DEBUG_ONLY(stmt) do { } while (0)
#endif

// Lightweight assert you can keep in release if you want (or compile out)
#if VE_RUNTIME_DEBUG
  #include <cassert>
  #define VE_ASSERT(expr) assert(expr)
#else
  #define VE_ASSERT(expr) ((void)0)
#endif
