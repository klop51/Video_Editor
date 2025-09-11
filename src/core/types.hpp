#pragma once

/**
 * Core Types for Video Editor
 * Fundamental data types and structures used throughout the video editor
 */

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace ve::core {

// Time representation types
using Timestamp = std::chrono::microseconds;
using Duration = std::chrono::microseconds;

// Basic identifier types
using ResourceId = uint64_t;
using FrameId = uint64_t;

// Color representation types
struct ColorRGB {
    float r, g, b;
    ColorRGB() : r(0.0f), g(0.0f), b(0.0f) {}
    ColorRGB(float red, float green, float blue) : r(red), g(green), b(blue) {}
};

struct ColorRGBA {
    float r, g, b, a;
    ColorRGBA() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
    ColorRGBA(float red, float green, float blue, float alpha = 1.0f) 
        : r(red), g(green), b(blue), a(alpha) {}
};

// Geometric types
struct Point2D {
    float x, y;
    Point2D() : x(0.0f), y(0.0f) {}
    Point2D(float x_val, float y_val) : x(x_val), y(y_val) {}
};

struct Size2D {
    uint32_t width, height;
    Size2D() : width(0), height(0) {}
    Size2D(uint32_t w, uint32_t h) : width(w), height(h) {}
};

struct Rectangle {
    Point2D position;
    Size2D size;
    Rectangle() = default;
    Rectangle(Point2D pos, Size2D sz) : position(pos), size(sz) {}
};

// Common result type for operations that may fail (specifically for media probing)
template<typename T>
struct ProbeResult {
    bool success = false;
    T value = {};
    std::string error_message;
    
    ProbeResult() = default;
    explicit ProbeResult(T val) : success(true), value(std::move(val)) {}
    explicit ProbeResult(const std::string& error) : success(false), error_message(error) {}
    
    bool is_success() const { return success; }
    bool is_error() const { return !success; }
    
    const T& get() const { return value; }
    T& get() { return value; }
};

// Version information
struct Version {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    std::string build_info;
    
    Version() = default;
    Version(uint32_t maj, uint32_t min, uint32_t p) 
        : major(maj), minor(min), patch(p) {}
    
    std::string to_string() const {
        return std::to_string(major) + "." + 
               std::to_string(minor) + "." + 
               std::to_string(patch);
    }
};

} // namespace ve::core
