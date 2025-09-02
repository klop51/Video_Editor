#include <iostream>
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

using Microsoft::WRL::ComPtr;

class D3D11VideoLoadingDiagnostic {
public:
    bool initialize() {
        std::cout << "=== D3D11 Video Loading Diagnostic ===" << std::endl;
        
        // Step 1: Test basic D3D11 device creation
        if (!test_d3d11_device_creation()) {
            std::cout << "❌ D3D11 device creation failed" << std::endl;
            return false;
        }
        
        // Step 2: Test FFmpeg initialization
        if (!test_ffmpeg_initialization()) {
            std::cout << "❌ FFmpeg initialization failed" << std::endl;
            return false;
        }
        
        // Step 3: Test D3D11VA hardware context creation
        if (!test_d3d11va_context()) {
            std::cout << "❌ D3D11VA context creation failed" << std::endl;
            return false;
        }
        
        // Step 4: Test video file opening
        if (!test_video_file_opening()) {
            std::cout << "❌ Video file opening failed" << std::endl;
            return false;
        }
        
        // Step 5: Test hardware decoder creation
        if (!test_hardware_decoder()) {
            std::cout << "❌ Hardware decoder creation failed" << std::endl;
            return false;
        }
        
        std::cout << "✅ All D3D11 video loading tests passed!" << std::endl;
        return true;
    }

private:
    bool test_d3d11_device_creation() {
        std::cout << "\n1. Testing D3D11 device creation..." << std::endl;
        
        HRESULT hr = D3D11CreateDevice(
            nullptr,                    // Adapter
            D3D_DRIVER_TYPE_HARDWARE,   // Driver type
            nullptr,                    // Software
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT, // Flags for video support
            nullptr,                    // Feature levels
            0,                          // Number of feature levels
            D3D11_SDK_VERSION,          // SDK version
            &device_,                   // Device
            &feature_level_,            // Feature level
            &context_                   // Context
        );
        
        if (FAILED(hr)) {
            std::cout << "   ❌ D3D11CreateDevice failed with HRESULT: 0x" << std::hex << hr << std::endl;
            return false;
        }
        
        std::cout << "   ✅ D3D11 device created successfully" << std::endl;
        std::cout << "   📋 Feature level: " << get_feature_level_string(feature_level_) << std::endl;
        
        return true;
    }
    
    bool test_ffmpeg_initialization() {
        std::cout << "\n2. Testing FFmpeg initialization..." << std::endl;
        
        // Check FFmpeg version
        std::cout << "   📋 FFmpeg version: " << av_version_info() << std::endl;
        
        // Check available hardware device types
        AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
        std::cout << "   📋 Available hardware device types:" << std::endl;
        while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
            std::cout << "      - " << av_hwdevice_get_type_name(type) << std::endl;
        }
        
        std::cout << "   ✅ FFmpeg initialized successfully" << std::endl;
        return true;
    }
    
    bool test_d3d11va_context() {
        std::cout << "\n3. Testing D3D11VA hardware context..." << std::endl;
        
        // Create D3D11VA device context
        AVBufferRef* hw_device_ctx = nullptr;
        int ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
        
        if (ret < 0) {
            char error_buf[256];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cout << "   ❌ av_hwdevice_ctx_create failed: " << error_buf << std::endl;
            return false;
        }
        
        std::cout << "   ✅ D3D11VA hardware context created successfully" << std::endl;
        
        // Set our D3D11 device in the hardware context
        AVHWDeviceContext* hw_dev_ctx = (AVHWDeviceContext*)hw_device_ctx->data;
        AVD3D11VADeviceContext* d3d11_ctx = (AVD3D11VADeviceContext*)hw_dev_ctx->hwctx;
        
        // Use our existing D3D11 device
        d3d11_ctx->device = device_.Get();
        d3d11_ctx->device_context = context_.Get();
        d3d11_ctx->device->AddRef();
        d3d11_ctx->device_context->AddRef();
        
        ret = av_hwdevice_ctx_init(hw_device_ctx);
        if (ret < 0) {
            char error_buf[256];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cout << "   ❌ av_hwdevice_ctx_init failed: " << error_buf << std::endl;
            av_buffer_unref(&hw_device_ctx);
            return false;
        }
        
        std::cout << "   ✅ D3D11VA hardware context initialized successfully" << std::endl;
        
        hw_device_ctx_ = hw_device_ctx;
        return true;
    }
    
    bool test_video_file_opening() {
        std::cout << "\n4. Testing video file opening..." << std::endl;
        
        const char* filename = "LOL.mp4";
        
        AVFormatContext* fmt_ctx = nullptr;
        int ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr);
        
        if (ret < 0) {
            char error_buf[256];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cout << "   ❌ avformat_open_input failed: " << error_buf << std::endl;
            return false;
        }
        
        ret = avformat_find_stream_info(fmt_ctx, nullptr);
        if (ret < 0) {
            char error_buf[256];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cout << "   ❌ avformat_find_stream_info failed: " << error_buf << std::endl;
            avformat_close_input(&fmt_ctx);
            return false;
        }
        
        // Find video stream
        int video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (video_stream_index < 0) {
            std::cout << "   ❌ No video stream found" << std::endl;
            avformat_close_input(&fmt_ctx);
            return false;
        }
        
        AVStream* video_stream = fmt_ctx->streams[video_stream_index];
        std::cout << "   ✅ Video file opened successfully" << std::endl;
        std::cout << "   📋 Codec: " << avcodec_get_name(video_stream->codecpar->codec_id) << std::endl;
        std::cout << "   📋 Resolution: " << video_stream->codecpar->width << "x" << video_stream->codecpar->height << std::endl;
        std::cout << "   📋 Pixel format: " << av_get_pix_fmt_name((AVPixelFormat)video_stream->codecpar->format) << std::endl;
        
        fmt_ctx_ = fmt_ctx;
        video_stream_index_ = video_stream_index;
        return true;
    }
    
    bool test_hardware_decoder() {
        std::cout << "\n5. Testing hardware decoder creation..." << std::endl;
        
        if (!fmt_ctx_ || video_stream_index_ < 0) {
            std::cout << "   ❌ No valid video stream available" << std::endl;
            return false;
        }
        
        AVStream* video_stream = fmt_ctx_->streams[video_stream_index_];
        
        // Find decoder
        const AVCodec* decoder = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!decoder) {
            std::cout << "   ❌ No decoder found for codec" << std::endl;
            return false;
        }
        
        std::cout << "   📋 Using decoder: " << decoder->name << std::endl;
        
        // Check if decoder supports D3D11VA
        bool supports_d3d11va = false;
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
            if (!config) break;
            
            if (config->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
                supports_d3d11va = true;
                std::cout << "   ✅ Decoder supports D3D11VA hardware acceleration" << std::endl;
                break;
            }
        }
        
        if (!supports_d3d11va) {
            std::cout << "   ⚠️ Decoder does not support D3D11VA, falling back to software" << std::endl;
        }
        
        // Create codec context
        AVCodecContext* codec_ctx = avcodec_alloc_context3(decoder);
        if (!codec_ctx) {
            std::cout << "   ❌ Failed to allocate codec context" << std::endl;
            return false;
        }
        
        int ret = avcodec_parameters_to_context(codec_ctx, video_stream->codecpar);
        if (ret < 0) {
            char error_buf[256];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cout << "   ❌ avcodec_parameters_to_context failed: " << error_buf << std::endl;
            avcodec_free_context(&codec_ctx);
            return false;
        }
        
        // Set hardware device context if available
        if (supports_d3d11va && hw_device_ctx_) {
            codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
            std::cout << "   ✅ Hardware device context assigned to decoder" << std::endl;
        }
        
        // Open codec
        ret = avcodec_open2(codec_ctx, decoder, nullptr);
        if (ret < 0) {
            char error_buf[256];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cout << "   ❌ avcodec_open2 failed: " << error_buf << std::endl;
            avcodec_free_context(&codec_ctx);
            return false;
        }
        
        std::cout << "   ✅ Hardware decoder opened successfully" << std::endl;
        
        // Try to decode one frame
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        
        bool frame_decoded = false;
        for (int i = 0; i < 10; i++) { // Try up to 10 packets
            ret = av_read_frame(fmt_ctx_, packet);
            if (ret < 0) break;
            
            if (packet->stream_index != video_stream_index_) {
                av_packet_unref(packet);
                continue;
            }
            
            ret = avcodec_send_packet(codec_ctx, packet);
            av_packet_unref(packet);
            
            if (ret < 0) continue;
            
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == 0) {
                std::cout << "   ✅ Successfully decoded test frame" << std::endl;
                std::cout << "   📋 Frame format: " << av_get_pix_fmt_name((AVPixelFormat)frame->format) << std::endl;
                std::cout << "   📋 Frame size: " << frame->width << "x" << frame->height << std::endl;
                
                if (frame->format == AV_PIX_FMT_D3D11) {
                    std::cout << "   ✅ Frame is hardware-accelerated (D3D11)" << std::endl;
                } else {
                    std::cout << "   📋 Frame is software-decoded" << std::endl;
                }
                
                frame_decoded = true;
                break;
            }
        }
        
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_ctx);
        
        if (!frame_decoded) {
            std::cout << "   ❌ Failed to decode any frames" << std::endl;
            return false;
        }
        
        return true;
    }
    
    std::string get_feature_level_string(D3D_FEATURE_LEVEL level) {
        switch (level) {
            case D3D_FEATURE_LEVEL_11_1: return "11.1";
            case D3D_FEATURE_LEVEL_11_0: return "11.0";
            case D3D_FEATURE_LEVEL_10_1: return "10.1";
            case D3D_FEATURE_LEVEL_10_0: return "10.0";
            case D3D_FEATURE_LEVEL_9_3: return "9.3";
            case D3D_FEATURE_LEVEL_9_2: return "9.2";
            case D3D_FEATURE_LEVEL_9_1: return "9.1";
            default: return "Unknown";
        }
    }
    
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    D3D_FEATURE_LEVEL feature_level_;
    AVBufferRef* hw_device_ctx_ = nullptr;
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_index_ = -1;
    
public:
    ~D3D11VideoLoadingDiagnostic() {
        if (fmt_ctx_) {
            avformat_close_input(&fmt_ctx_);
        }
        if (hw_device_ctx_) {
            av_buffer_unref(&hw_device_ctx_);
        }
    }
};

int main() {
    D3D11VideoLoadingDiagnostic diagnostic;
    
    bool success = diagnostic.initialize();
    
    std::cout << "\n=== Diagnostic Summary ===" << std::endl;
    if (success) {
        std::cout << "✅ D3D11 video loading is working correctly" << std::endl;
        std::cout << "   The issue may be in the application's video loading logic" << std::endl;
    } else {
        std::cout << "❌ D3D11 video loading has issues" << std::endl;
        std::cout << "   Check the error messages above for details" << std::endl;
    }
    
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    
    return success ? 0 : 1;
}
