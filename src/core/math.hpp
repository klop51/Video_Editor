#pragma once

/**
 * Core Mathematics for Video Editor
 * Mathematical types and operations used throughout the video editor
 */

#include <cmath>
#include <array>
#include <algorithm>

namespace ve::core {

// 2D Vector
struct Vec2 {
    float x, y;
    
    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float x_val, float y_val) : x(x_val), y(y_val) {}
    
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
    
    float dot(const Vec2& other) const { return x * other.x + y * other.y; }
    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const { float len = length(); return len > 0.0f ? *this / len : Vec2(); }
};

// 3D Vector
struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x_val, float y_val, float z_val) : x(x_val), y(y_val), z(z_val) {}
    explicit Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}
    
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
    Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
    
    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    
    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3 cross(const Vec3& other) const {
        return Vec3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
    }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const { float len = length(); return len > 0.0f ? *this / len : Vec3(); }
};

// 4D Vector
struct Vec4 {
    float x, y, z, w;
    
    Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    Vec4(float x_val, float y_val, float z_val, float w_val) : x(x_val), y(y_val), z(z_val), w(w_val) {}
    Vec4(const Vec3& xyz, float w_val) : x(xyz.x), y(xyz.y), z(xyz.z), w(w_val) {}
    
    Vec4 operator+(const Vec4& other) const { return Vec4(x + other.x, y + other.y, z + other.z, w + other.w); }
    Vec4 operator-(const Vec4& other) const { return Vec4(x - other.x, y - other.y, z - other.z, w - other.w); }
    Vec4 operator*(float scalar) const { return Vec4(x * scalar, y * scalar, z * scalar, w * scalar); }
    Vec4 operator/(float scalar) const { return Vec4(x / scalar, y / scalar, z / scalar, w / scalar); }
    
    Vec3 xyz() const { return Vec3(x, y, z); }
};

// 4x4 Matrix for transformations
struct Mat4 {
    std::array<std::array<float, 4>, 4> m;
    
    Mat4() {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m[i][j] = 0.0f;
            }
        }
    }
    
    static Mat4 identity() {
        Mat4 result;
        for (int i = 0; i < 4; ++i) {
            result.m[i][i] = 1.0f;
        }
        return result;
    }
    
    static Mat4 translation(const Vec3& t) {
        Mat4 result = identity();
        result.m[0][3] = t.x;
        result.m[1][3] = t.y;
        result.m[2][3] = t.z;
        return result;
    }
    
    static Mat4 rotation_x(float angle) {
        Mat4 result = identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[1][1] = c;
        result.m[1][2] = -s;
        result.m[2][1] = s;
        result.m[2][2] = c;
        return result;
    }
    
    static Mat4 rotation_y(float angle) {
        Mat4 result = identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][2] = s;
        result.m[2][0] = -s;
        result.m[2][2] = c;
        return result;
    }
    
    static Mat4 rotation_z(float angle) {
        Mat4 result = identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][1] = -s;
        result.m[1][0] = s;
        result.m[1][1] = c;
        return result;
    }
    
    static Mat4 scale(const Vec3& s) {
        Mat4 result = identity();
        result.m[0][0] = s.x;
        result.m[1][1] = s.y;
        result.m[2][2] = s.z;
        return result;
    }
    
    Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < 4; ++k) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }
    
    Vec4 operator*(const Vec4& v) const {
        Vec4 result;
        result.x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w;
        result.y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w;
        result.z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w;
        result.w = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w;
        return result;
    }
    
    Vec3 transform_point(const Vec3& point) const {
        Vec4 result = *this * Vec4(point, 1.0f);
        return result.w != 0.0f ? result.xyz() / result.w : result.xyz();
    }
    
    Vec3 transform_direction(const Vec3& direction) const {
        Vec4 result = *this * Vec4(direction, 0.0f);
        return result.xyz();
    }
};

// Utility functions
inline float clamp(float value, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, value));
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return Vec3(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t));
}

inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float degrees_to_radians(float degrees) {
    return degrees * (3.14159265f / 180.0f);
}

inline float radians_to_degrees(float radians) {
    return radians * (180.0f / 3.14159265f);
}

} // namespace ve::core

// Make types available in gfx namespace for convenience
namespace gfx {
using Vec2 = ve::core::Vec2;
using Vec3 = ve::core::Vec3;
using Vec4 = ve::core::Vec4;
using Mat4 = ve::core::Mat4;
}
