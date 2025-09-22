#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_2_0>
#include <QMutex>
#include <QByteArray>
#include <cstdint>

namespace ve::ui {

// Lightweight OpenGL widget that owns a persistent RGBA8 texture and draws it.
// Future: accept external GPU texture handles directly (zero-copy path).
class GLVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions_2_0 {
    Q_OBJECT
public:
    explicit GLVideoWidget(QWidget* parent = nullptr);
    ~GLVideoWidget() override;

    // Provide a new RGBA frame (stride may differ). Copies into internal tightly-packed buffer.
    void set_frame(const uint8_t* rgba, int w, int h, int stride_bytes, int64_t pts);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void ensure_texture(int w, int h);
    void ensure_program();
    void init_pbos_if_needed(int needed_bytes);
    void destroy_pbos();
    bool upload_with_pbo(const QByteArray& rgba, int w, int h);

    GLuint texture_id_ = 0;
    int tex_w_ = 0;
    int tex_h_ = 0;
    GLuint program_ = 0;
    GLint attr_pos_ = -1;
    GLint attr_uv_ = -1;
    GLint uni_tex_ = -1;

    struct PendingFrame {
        QByteArray rgba;
        int w = 0;
        int h = 0;
        int64_t pts = 0;
    } pending_;
    bool new_frame_ = false;
    QMutex mutex_;

#ifndef VE_ENABLE_PBO_UPLOAD
// Default PBO uploads to off for stability on wider GPU/driver sets.
// Set VE_ENABLE_PBO_UPLOAD=1 at compile time or define env VE_DISABLE_PBO to override at runtime.
#define VE_ENABLE_PBO_UPLOAD 0
#endif
#if VE_ENABLE_PBO_UPLOAD
#ifndef VE_GL_PBO_TRIPLE
#define VE_GL_PBO_TRIPLE 0
#endif
#ifndef VE_GL_PBO_PERSISTENT_MAP
#define VE_GL_PBO_PERSISTENT_MAP 0
#endif
    static constexpr int kPboRingSize = VE_GL_PBO_TRIPLE ? 3 : 2;
    GLuint pbos_[3] = {0,0,0}; // allocate max; use first kPboRingSize
    size_t pbo_capacity_ = 0; // bytes per PBO
    int pbo_index_ = 0;
    bool pbo_persistent_ = false;
    void* pbo_mapped_ptrs_[3] = {nullptr,nullptr,nullptr};
    bool pbo_runtime_disabled_ = false; // env var toggle (VE_DISABLE_PBO)
#endif
};

} // namespace ve::ui
