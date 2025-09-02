#pragma once

// FFmpeg D3D11VA headers wrapper
// Ensures proper extern "C" linkage for FFmpeg C APIs

#ifdef _WIN32
#ifdef HAVE_D3D11VA

// Ensure extern "C" linkage for FFmpeg headers
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavcodec/d3d11va.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/hwcontext_d3d11va.h>
    #include <libavutil/pixfmt.h>
}

namespace thirdparty {
namespace ffmpeg {

// Safe wrapper functions for FFmpeg D3D11VA operations
inline int create_d3d11va_device_ctx(AVBufferRef** out_device_ctx, void* d3d11_device) {
    return av_hwdevice_ctx_create(out_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, 
                                  nullptr, nullptr, 0);
}

inline int init_d3d11va_decoder(AVCodecContext* avctx, void* d3d11va_ctx) {
    avctx->hw_device_ctx = static_cast<AVBufferRef*>(d3d11va_ctx);
    avctx->get_format = [](AVCodecContext*, const enum AVPixelFormat* pix_fmts) -> enum AVPixelFormat {
        const enum AVPixelFormat* p;
        for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            if (*p == AV_PIX_FMT_D3D11) {
                return *p;
            }
        }
        return AV_PIX_FMT_NONE;
    };
    return 0;
}

} // namespace ffmpeg
} // namespace thirdparty

#endif // HAVE_D3D11VA
#endif // _WIN32
