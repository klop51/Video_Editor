// TOKEN:GL_VIDEO_WIDGET_2025_08_30_A - Sentinel for stale artifact detection
#include "ui/gl_video_widget.hpp"
#include "core/profiling.hpp" // profiling
#include "core/log.hpp" // for GL debug logging
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <cstring>
#include <QDebug>


namespace ve::ui {

GLVideoWidget::GLVideoWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    setAutoFillBackground(false);
#if VE_ENABLE_PBO_UPLOAD
    // Runtime disable via environment variable (checked once)
    if (std::getenv("VE_DISABLE_PBO") != nullptr) {
        pbo_runtime_disabled_ = true;
        qDebug() << "GLVideoWidget: PBO path disabled via VE_DISABLE_PBO env var";
    }
#endif
}

GLVideoWidget::~GLVideoWidget() {
    makeCurrent();
#if VE_ENABLE_PBO_UPLOAD
    destroy_pbos();
#endif
    if (texture_id_) glDeleteTextures(1, &texture_id_);
    if (program_) glDeleteProgram(program_);
    doneCurrent();
}

void GLVideoWidget::set_frame(const uint8_t* rgba, int w, int h, int stride_bytes, int64_t pts) {
    // Removed per-frame debug logging to prevent spam
    if (!rgba || w <= 0 || h <= 0) {
        ve::log::warn("GL_DEBUG: set_frame rejected - invalid parameters");
        return;
    }
    const int required_stride = w * 4; // RGBA8 bytes per row
    if (stride_bytes < required_stride) {
        qWarning() << "GLVideoWidget::set_frame stride too small" << stride_bytes << "<" << required_stride << "w=" << w << "h=" << h;
        ve::log::warn("GL_DEBUG: set_frame rejected - stride too small");
        return; // avoid potential over-read
    }
    if (stride_bytes > required_stride * 64) { // arbitrary sanity cap
        qWarning() << "GLVideoWidget::set_frame suspicious stride" << stride_bytes << "w=" << w << "h=" << h;
        return;
    }
    // Copy only the visible width (ignore right padding from stride)
    const qsizetype frame_bytes = static_cast<qsizetype>(w) * h * 4;
    QByteArray copy; copy.resize(frame_bytes);
    auto* dst = reinterpret_cast<uint8_t*>(copy.data());
    for (int y = 0; y < h; ++y) {
        const uint8_t* src_row = rgba + static_cast<size_t>(y) * stride_bytes;
        std::memcpy(dst + static_cast<size_t>(y) * required_stride, src_row, static_cast<size_t>(required_stride));
    }
    {
        QMutexLocker lock(&mutex_);
        pending_.rgba = std::move(copy);
        pending_.w = w;
        pending_.h = h;
        pending_.pts = pts;
        new_frame_ = true;
    }
    // Removed per-frame completion logging to prevent spam
    update();
}

void GLVideoWidget::initializeGL() {
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void GLVideoWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void GLVideoWidget::ensure_texture(int w, int h) {
    if (texture_id_ && (w != tex_w_ || h != tex_h_)) {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
    if (!texture_id_) {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        tex_w_ = w; tex_h_ = h;
    } else {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
    }
}

void GLVideoWidget::ensure_program() {
    if (program_) return;
    ve::log::info("GL_DEBUG: Creating shader program...");
    const char* vs_src = "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV; void main(){vUV=aUV; gl_Position=vec4(aPos,0.0,1.0);}";
    const char* fs_src = "varying vec2 vUV; uniform sampler2D uTex; void main(){ gl_FragColor = texture2D(uTex, vUV); }";
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&vs_src,nullptr); glCompileShader(vs);
    GLint ok = 0; glGetShaderiv(vs, GL_COMPILE_STATUS, &ok); if(!ok){ char log[512]; GLsizei len=0; glGetShaderInfoLog(vs,512,&len,log); ve::log::error("GL_ERROR: vertex shader compile failed: " + std::string(log)); }
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&fs_src,nullptr); glCompileShader(fs);
    ok = 0; glGetShaderiv(fs, GL_COMPILE_STATUS, &ok); if(!ok){ char log[512]; GLsizei len=0; glGetShaderInfoLog(fs,512,&len,log); ve::log::error("GL_ERROR: fragment shader compile failed: " + std::string(log)); }
    program_ = glCreateProgram(); glAttachShader(program_,vs); glAttachShader(program_,fs); glLinkProgram(program_);
    GLint linked = 0; glGetProgramiv(program_, GL_LINK_STATUS, &linked); if(!linked){ char log[512]; GLsizei len=0; glGetProgramInfoLog(program_,512,&len,log); ve::log::error("GL_ERROR: program link failed: " + std::string(log)); }
    glDeleteShader(vs); glDeleteShader(fs);
    attr_pos_ = glGetAttribLocation(program_, "aPos");
    attr_uv_  = glGetAttribLocation(program_, "aUV");
    uni_tex_  = glGetUniformLocation(program_, "uTex");
    ve::log::info("GL_DEBUG: Shader program created, attr_pos_=" + std::to_string(attr_pos_) + " attr_uv_=" + std::to_string(attr_uv_) + " uni_tex_=" + std::to_string(uni_tex_));
    if(attr_pos_ < 0 || attr_uv_ < 0) {
        ve::log::error("GL_ERROR: invalid attribute locations attr_pos_=" + std::to_string(attr_pos_) + " attr_uv_=" + std::to_string(attr_uv_));
    }
}

void GLVideoWidget::paintGL() {
    VE_PROFILE_SCOPE_UNIQ("gl_widget.paintGL");
    // Removed per-frame paintGL debug logging to prevent spam
    PendingFrame local; bool has_new = false;
    {
        QMutexLocker lock(&mutex_);
        if (new_frame_) { 
            local = pending_; 
            new_frame_ = false; 
            has_new = true; 
            // Removed per-frame new frame debug logging to prevent spam
        }
    }
    if (has_new && local.w > 0 && local.h > 0 && !local.rgba.isEmpty()) {
    // Removed per-frame texture upload debug logging to prevent spam
    ensure_texture(local.w, local.h);
    // Removed per-frame ensure_texture debug logging to prevent spam
#if VE_ENABLE_PBO_UPLOAD
    bool pbo_ok = false;
    if (!pbo_runtime_disabled_) {
        pbo_ok = upload_with_pbo(local.rgba, local.w, local.h);
    }
    if (!pbo_ok) {
        VE_PROFILE_SCOPE_DETAILED("gl_widget.upload.fallback");
        // Removed per-frame direct upload debug logging to prevent spam
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,local.w,local.h,GL_RGBA,GL_UNSIGNED_BYTE,local.rgba.constData());
    }
#else
    VE_PROFILE_SCOPE_DETAILED("gl_widget.upload");
    // Removed per-frame direct upload debug logging to prevent spam
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,local.w,local.h,GL_RGBA,GL_UNSIGNED_BYTE,local.rgba.constData());
#endif
#if VE_HEAP_DEBUG && defined(_MSC_VER)
    _CrtCheckMemory();
#endif
    }
    // Removed per-frame viewport setup debug logging to prevent spam
    glViewport(0,0,width(),height());
    glClearColor(0.f,0.f,0.f,1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!texture_id_) {
        // Keep warning for no texture (not per-frame, only when missing)
        return;
    }
    // Removed per-frame rendering debug logging to prevent spam
    ensure_program();

    float widget_w = static_cast<float>(width());
    float widget_h = static_cast<float>(height());
    float tex_aspect = tex_h_>0 ? static_cast<float>(tex_w_)/static_cast<float>(tex_h_) : 1.f;
    float widget_aspect = widget_h>0 ? widget_w/widget_h : tex_aspect;
    float quad_w=1.f, quad_h=1.f;
    if (tex_aspect > widget_aspect) quad_h = widget_aspect/tex_aspect; else quad_w = tex_aspect/widget_aspect;
    float x0=-quad_w, x1=quad_w, y0=-quad_h, y1=quad_h;
    float verts[] = { x0,y0,0.f,1.f,  x1,y0,1.f,1.f,  x0,y1,0.f,0.f,  x1,y1,1.f,0.f };
    if(attr_pos_ >=0 && attr_uv_ >=0) {
    VE_PROFILE_SCOPE_DETAILED("gl_widget.draw");
        // Removed per-frame quad rendering debug logging to prevent spam
        glUseProgram(program_);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture_id_); if(uni_tex_>=0) glUniform1i(uni_tex_,0);
        glEnableVertexAttribArray(static_cast<GLuint>(attr_pos_));
        glEnableVertexAttribArray(static_cast<GLuint>(attr_uv_));
        glVertexAttribPointer(static_cast<GLuint>(attr_pos_),2,GL_FLOAT,GL_FALSE,4*sizeof(float),verts);
        glVertexAttribPointer(static_cast<GLuint>(attr_uv_),2,GL_FLOAT,GL_FALSE,4*sizeof(float),verts+2);
        // Removed per-frame glDrawArrays debug logging to prevent spam
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        // Check for OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            ve::log::error("GL_ERROR: OpenGL error after glDrawArrays: " + std::to_string(error));
        }
        glDisableVertexAttribArray(static_cast<GLuint>(attr_pos_));
        glDisableVertexAttribArray(static_cast<GLuint>(attr_uv_));
        glUseProgram(0);
        // Removed per-frame quad completion debug logging to prevent spam
    } else {
        ve::log::error("GL_ERROR: skipping draw due to invalid attributes attr_pos_=" + std::to_string(attr_pos_) + " attr_uv_=" + std::to_string(attr_uv_));
    }
}

#if VE_ENABLE_PBO_UPLOAD
void GLVideoWidget::init_pbos_if_needed(int needed_bytes) {
    if (needed_bytes <= 0) return;
    if (pbo_capacity_ >= static_cast<size_t>(needed_bytes) && pbos_[0] != 0) return;
    // (Re)allocate PBOs
    if (pbos_[0] != 0) destroy_pbos();
    glGenBuffers(kPboRingSize, pbos_);
    pbo_capacity_ = static_cast<size_t>(needed_bytes);
    pbo_persistent_ = false;
    // Allocate each PBO; optionally use persistent mapping if supported.
    for (int i=0;i<kPboRingSize;++i) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos_[i]);
        bool allocated = false;
#if VE_GL_PBO_PERSISTENT_MAP
        if (!pbo_persistent_) { // only attempt once until success
            if (QOpenGLContext* ctx = QOpenGLContext::currentContext()) {
                const QSurfaceFormat& fmt = ctx->format();
                bool ver_ok = (fmt.majorVersion() > 4) || (fmt.majorVersion()==4 && fmt.minorVersion()>=4);
                bool ext_ok = ctx->extensions().contains("GL_ARB_buffer_storage");
                if (ver_ok && ext_ok) {
                    if (auto* extra = ctx->extraFunctions()) {
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT 0x0080
#endif
#ifndef GL_DYNAMIC_STORAGE_BIT
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#endif
                        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT;
                        extra->glBufferStorage(GL_PIXEL_UNPACK_BUFFER, pbo_capacity_, nullptr, flags);
                        GLbitfield map_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
                        void* ptr = extra->glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, pbo_capacity_, map_flags);
                        if (ptr) {
                            pbo_persistent_ = true;
                            pbo_mapped_ptrs_[i] = ptr;
                            allocated = true;
                        }
                    }
                }
            }
        }
#endif // VE_GL_PBO_PERSISTENT_MAP
        if (!allocated) {
            glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_capacity_, nullptr, GL_STREAM_DRAW);
        }
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    pbo_index_ = 0;
}

void GLVideoWidget::destroy_pbos() {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    for (int i=0;i<kPboRingSize;++i) {
        if (pbos_[i]) {
#if VE_GL_PBO_PERSISTENT_MAP
            if (pbo_persistent_ && pbo_mapped_ptrs_[i]) {
                if (auto* ctx = QOpenGLContext::currentContext()) {
                    if (auto* extra = ctx->extraFunctions()) {
                        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos_[i]);
                        extra->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                    }
                }
            }
#endif
            glDeleteBuffers(1, &pbos_[i]); pbos_[i]=0; }
        pbo_mapped_ptrs_[i] = nullptr;
    }
    pbo_capacity_ = 0;
    pbo_index_ = 0;
    pbo_persistent_ = false;
}

bool GLVideoWidget::upload_with_pbo(const QByteArray& rgba, int w, int h) {
    VE_PROFILE_SCOPE_DETAILED("gl_widget.upload.pbo");
    const int64_t needed = int64_t(w) * int64_t(h) * 4;
    if (needed <= 0) return false;
    if (needed > std::numeric_limits<int>::max()) return false;
    init_pbos_if_needed(static_cast<int>(needed));
    if (pbos_[0] == 0) return false;

    GLuint pbo = pbos_[pbo_index_];
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    // Orphan to avoid stall (unless we attempt a persistent mapping strategy)
    if (!pbo_persistent_) {
        glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_capacity_, nullptr, GL_STREAM_DRAW);
        {
            VE_PROFILE_SCOPE_DETAILED("gl_widget.upload.pbo.map");
            void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (!ptr) { glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); return false; }
            size_t copy_bytes = std::min<size_t>(static_cast<size_t>(needed), static_cast<size_t>(rgba.size()));
            std::memcpy(ptr, rgba.constData(), copy_bytes);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }
    } else {
        // Persistent mapping path: direct memcpy into mapped pointer, then bind and issue tex update
        void* ptr = pbo_mapped_ptrs_[pbo_index_];
        if (!ptr) { glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); return false; }
        VE_PROFILE_SCOPE_DETAILED("gl_widget.upload.pbo.memcpy");
        size_t copy_bytes = std::min<size_t>(static_cast<size_t>(needed), static_cast<size_t>(rgba.size()));
        std::memcpy(ptr, rgba.constData(), copy_bytes);
    }
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    {
        VE_PROFILE_SCOPE_DETAILED("gl_widget.upload.pbo.tex");
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,nullptr); // nullptr = bound PBO
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    pbo_index_ = (pbo_index_ + 1) % kPboRingSize;
    return true;
}
#endif // VE_ENABLE_PBO_UPLOAD

} // namespace ve::ui
