#pragma once

// Platform wrapper for D3D11 headers
// Isolates C++ D3D11 APIs from C FFmpeg headers to prevent conflicts

#ifdef _WIN32

// Include D3D11 headers in proper order
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <dxgi1_2.h>

// D3D11 video acceleration headers - conditionally include if available
#include <d3d11_4.h>
#ifdef HAVE_D3D11VA
// Try to include D3D11 video headers if available in SDK
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602) // Windows 8+
    #if __has_include(<d3d11_video.h>)
        #include <d3d11_video.h>
        #define D3D11_VIDEO_HEADERS_AVAILABLE 1
    #else
        #define D3D11_VIDEO_HEADERS_AVAILABLE 0
    #endif
#else
    #define D3D11_VIDEO_HEADERS_AVAILABLE 0
#endif
#endif

// Helper macros for safe interface release
#define SAFE_RELEASE(x) \
    do { \
        if (x) { \
            (x)->Release(); \
            (x) = nullptr; \
        } \
    } while(0)

namespace platform {
namespace d3d11 {

// Forward declarations for common D3D11 types
using Device = ID3D11Device;
using DeviceContext = ID3D11DeviceContext;

#if defined(HAVE_D3D11VA) && (D3D11_VIDEO_HEADERS_AVAILABLE == 1)
using VideoDevice = ID3D11VideoDevice;
using VideoContext = ID3D11VideoContext;
using VideoDecoder = ID3D11VideoDecoder;
using VideoDecoderOutputView = ID3D11VideoDecoderOutputView;
#endif

} // namespace d3d11
} // namespace platform

#endif // _WIN32
