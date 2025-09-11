// ---- global scope: ALL system/third-party/STD includes here
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdint>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

// DirectX 11 includes (Windows only)
#ifdef _WIN32
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

// Automatically link required DirectX libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

// ---- your project headers
#include "gfx/opengl_headers.hpp"
#include "gfx/vk_device.hpp"
#include "gfx/vk_instance.hpp"
#include "core/log.hpp"

// Real DirectX 11 implementation

#ifdef _WIN32
struct D3D11Texture {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11Texture2D* staging_texture = nullptr;  // For CPU uploads
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT row_pitch = 0;  // Bytes per row
    UINT slice_pitch = 0;  // Bytes per slice
    
    ~D3D11Texture() {
        if (staging_texture) staging_texture->Release();
        if (rtv) rtv->Release();
        if (srv) srv->Release();
        if (texture) texture->Release();
    }
};

struct D3D11Buffer {
    ID3D11Buffer* buffer = nullptr;
    D3D11_BUFFER_DESC desc = {};
    ID3D11Buffer* staging_buffer = nullptr;  // For CPU uploads
    UINT size = 0;
    
    ~D3D11Buffer() {
        if (staging_buffer) staging_buffer->Release();
        if (buffer) buffer->Release();
    }
};

struct D3D11ShaderProgram {
    ID3D11VertexShader* vertex_shader = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11InputLayout* input_layout = nullptr;
    ID3D11Buffer* constant_buffer = nullptr;
    ID3D11SamplerState* sampler_state = nullptr;
    
    ~D3D11ShaderProgram() {
        if (sampler_state) sampler_state->Release();
        if (constant_buffer) constant_buffer->Release();
        if (input_layout) input_layout->Release();
        if (pixel_shader) pixel_shader->Release();
        if (vertex_shader) vertex_shader->Release();
    }
};

// Week 4: Command Buffer & Synchronization structures
struct GPUFence {
    uint32_t id = 0;
    ID3D11Query* fence_query = nullptr;
    bool signaled = false;
    
    ~GPUFence() {
        if (fence_query) fence_query->Release();
    }
};

struct ProfileEvent {
    std::string name;
    ID3D11Query* start_query = nullptr;
    ID3D11Query* end_query = nullptr;
    bool active = false;
    
    ~ProfileEvent() {
        if (end_query) end_query->Release();
        if (start_query) start_query->Release();
    }
};

// Week 5: YUV to RGB Conversion structures
enum class YUVFormat {
    YUV420P,    // Planar Y, U, V separate planes (4:2:0 subsampling)
    YUV422P,    // Planar Y, U, V separate planes (4:2:2 subsampling)
    YUV444P,    // Planar Y, U, V separate planes (4:4:4 no subsampling)
    NV12,       // Semi-planar Y plane + UV interleaved (4:2:0)
    NV21,       // Semi-planar Y plane + VU interleaved (4:2:0)
    UNKNOWN
};

enum class ColorSpace {
    BT709,      // HD standard (most common)
    BT601,      // SD standard  
    BT2020,     // UHD/HDR standard
    SRGB        // Computer graphics standard
};

struct ColorSpaceConstants {
    float yuv_to_rgb_matrix[4][4];  // 4x4 matrix for HLSL alignment
    float black_level[4];           // [y_black, u_black, v_black, unused]
    float white_level[4];           // [y_white, u_white, v_white, unused] 
    float chroma_scale[2];          // UV plane scaling factors
    float luma_scale[2];            // Y plane scaling factors
};

struct YUVTexture {
    YUVFormat format;
    ColorSpace color_space;
    
    // Planar format textures (YUV420P, YUV422P, YUV444P)
    ID3D11Texture2D* y_plane = nullptr;
    ID3D11Texture2D* u_plane = nullptr;
    ID3D11Texture2D* v_plane = nullptr;
    ID3D11ShaderResourceView* y_srv = nullptr;
    ID3D11ShaderResourceView* u_srv = nullptr;
    ID3D11ShaderResourceView* v_srv = nullptr;
    
    // Semi-planar format textures (NV12, NV21)
    ID3D11Texture2D* luma_plane = nullptr;
    ID3D11Texture2D* chroma_plane = nullptr;
    ID3D11ShaderResourceView* luma_srv = nullptr;
    ID3D11ShaderResourceView* chroma_srv = nullptr;
    
    // Texture dimensions
    UINT width = 0;
    UINT height = 0;
    UINT y_stride = 0;
    UINT uv_stride = 0;
    
    // Scaling factors for color space conversion
    float chroma_scale[2] = {1.0f, 1.0f};  // UV plane scaling factors
    float luma_scale[2] = {1.0f, 1.0f};    // Y plane scaling factors
    
    ~YUVTexture() {
        if (chroma_srv) chroma_srv->Release();
        if (luma_srv) luma_srv->Release();
        if (v_srv) v_srv->Release();
        if (u_srv) u_srv->Release();
        if (y_srv) y_srv->Release();
        if (chroma_plane) chroma_plane->Release();
        if (luma_plane) luma_plane->Release();
        if (v_plane) v_plane->Release();
        if (u_plane) u_plane->Release();
        if (y_plane) y_plane->Release();
    }
};

enum class RenderCommandType {
    SET_RENDER_TARGET,
    SET_SHADER_PROGRAM, 
    SET_TEXTURE,
    SET_UNIFORM,
    DRAW_QUAD,
    CLEAR,
    CLEAR_COLOR
};

struct RenderCommand {
    RenderCommandType type;
    union {
        struct { ID3D11RenderTargetView* rtv; ID3D11DepthStencilView* dsv; } render_target;
        struct { ID3D11VertexShader* vertex_shader; ID3D11PixelShader* pixel_shader; } shader_program;
        struct { ID3D11ShaderResourceView* srv; int slot; } texture;
        struct { ID3D11RenderTargetView* rtv; float color[4]; } clear_color;
        struct { float x, y, width, height; } draw_quad;
        struct { float r, g, b, a; } clear;
    };
};

class D3D11GraphicsDevice {
private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain1* swapchain = nullptr;
    ID3D11RenderTargetView* backbuffer_rtv = nullptr;
    
    // Basic rendering resources
    ID3D11Buffer* fullscreen_vertex_buffer = nullptr;
    
    std::unordered_map<unsigned int, std::unique_ptr<D3D11Texture>> textures;
    std::unordered_map<unsigned int, std::unique_ptr<D3D11ShaderProgram>> shaders;
    std::unordered_map<unsigned int, std::unique_ptr<D3D11Buffer>> buffers;
    std::unordered_map<unsigned int, ID3D11RenderTargetView*> render_targets;
    unsigned int next_texture_id = 1;
    unsigned int next_shader_id = 1;
    unsigned int next_buffer_id = 1;
    
    // Memory tracking
    size_t total_gpu_memory = 0;
    size_t used_gpu_memory = 0;
    
    // Week 4: Command Buffer & Synchronization systems
    std::vector<RenderCommand> command_buffer;
    std::vector<GPUFence> gpu_fences;
    std::unordered_map<std::string, ProfileEvent> profile_events;
    ID3D11Query* disjoint_query = nullptr;
    
    // Frame pacing control
    bool vsync_enabled = true;
    float frame_time_limit = 0.0f;  // 0 = no limit
    std::chrono::high_resolution_clock::time_point last_frame_time;
    
    // Week 5: YUV to RGB Conversion members
    std::vector<std::unique_ptr<YUVTexture>> yuv_textures;
    std::unique_ptr<D3D11ShaderProgram> yuv_to_rgb_planar_shader;    // For YUV420P/422P/444P
    std::unique_ptr<D3D11ShaderProgram> yuv_to_rgb_nv12_shader;      // For NV12/NV21
    ID3D11Buffer* color_space_constants_buffer = nullptr;
    
    DXGI_FORMAT get_dxgi_format(int format) {
        switch (format) {
            case 0: return DXGI_FORMAT_R8G8B8A8_UNORM;  // RGBA8
            case 1: return DXGI_FORMAT_R32G32B32A32_FLOAT; // RGBA32F
            default: return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }
    
    UINT get_bytes_per_pixel(DXGI_FORMAT format) {
        switch (format) {
            case DXGI_FORMAT_R8G8B8A8_UNORM: return 4;
            case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
            case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
            case DXGI_FORMAT_R10G10B10A2_UNORM: return 4;
            default: return 4;
        }
    }
    
    size_t calculate_texture_memory(UINT width, UINT height, DXGI_FORMAT format) {
        return static_cast<size_t>(width) * height * get_bytes_per_pixel(format);
    }

public:
    D3D11GraphicsDevice() : last_frame_time(std::chrono::high_resolution_clock::now()) {}
    
    struct DeviceInfo {
        bool debug_enabled = false;
        bool enable_swapchain = false;
    };

    bool create(const DeviceInfo& info = {}) {
        // Create D3D11 device
        UINT create_device_flags = 0;
        if (info.debug_enabled) {
            create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
        }

        D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = D3D11CreateDevice(
            nullptr,                    // Adapter (nullptr = default)
            D3D_DRIVER_TYPE_HARDWARE,   // Driver Type
            nullptr,                    // Software
            create_device_flags,        // Flags
            feature_levels,             // Feature Levels
            ARRAYSIZE(feature_levels),  // Number of feature levels
            D3D11_SDK_VERSION,          // SDK version
            &device,                    // Returns the Direct3D device
            &feature_level,             // Returns feature level
            &context                    // Returns the device immediate context
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create D3D11 device: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return false;
        }
        
        ve::log::info("D3D11 device created successfully with feature level: 0x" + std::to_string(static_cast<unsigned int>(feature_level)));
        
        // Query GPU memory information
        IDXGIDevice* dxgi_device = nullptr;
        hr = device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);
        if (SUCCEEDED(hr)) {
            IDXGIAdapter* adapter = nullptr;
            hr = dxgi_device->GetAdapter(&adapter);
            if (SUCCEEDED(hr)) {
                DXGI_ADAPTER_DESC adapter_desc;
                hr = adapter->GetDesc(&adapter_desc);
                if (SUCCEEDED(hr)) {
                    total_gpu_memory = adapter_desc.DedicatedVideoMemory;
                    ve::log::info("GPU: " + std::string(reinterpret_cast<const char*>(adapter_desc.Description)) + 
                                 " with " + std::to_string(total_gpu_memory / 1024 / 1024) + " MB VRAM");
                }
                adapter->Release();
            }
            dxgi_device->Release();
        }
        
        if (total_gpu_memory == 0) {
            total_gpu_memory = 1024 * 1024 * 1024; // Default to 1GB if detection fails
            ve::log::warn("Could not detect GPU memory, assuming 1GB");
        }
        
        // Create swapchain if requested
        if (info.enable_swapchain) {
            // For now, create a minimal swapchain - will be enhanced when we need window presentation
            // This is a placeholder for headless rendering
            ve::log::info("Swapchain creation deferred until window handle available");
        }
        
        // Create fullscreen quad vertex buffer for texture rendering
        struct Vertex {
            float position[2];
            float texcoord[2];
        };
        
        Vertex quad_vertices[] = {
            // Triangle 1
            { {-1.0f, -1.0f}, {0.0f, 1.0f} },  // Bottom-left
            { {-1.0f,  1.0f}, {0.0f, 0.0f} },  // Top-left
            { { 1.0f, -1.0f}, {1.0f, 1.0f} },  // Bottom-right
            // Triangle 2
            { { 1.0f, -1.0f}, {1.0f, 1.0f} },  // Bottom-right
            { {-1.0f,  1.0f}, {0.0f, 0.0f} },  // Top-left
            { { 1.0f,  1.0f}, {1.0f, 0.0f} }   // Top-right
        };
        
        D3D11_BUFFER_DESC vertex_desc = {};
        vertex_desc.ByteWidth = sizeof(quad_vertices);
        vertex_desc.Usage = D3D11_USAGE_DEFAULT;
        vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_desc.CPUAccessFlags = 0;
        
        D3D11_SUBRESOURCE_DATA vertex_data = {};
        vertex_data.pSysMem = quad_vertices;
        
        hr = device->CreateBuffer(&vertex_desc, &vertex_data, &fullscreen_vertex_buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create fullscreen quad vertex buffer: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return false;
        }
        
        ve::log::debug("Created fullscreen quad vertex buffer");
        
        // Initialize GPU profiling (Week 4)
        D3D11_QUERY_DESC query_desc = {};
        query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        hr = device->CreateQuery(&query_desc, &disjoint_query);
        if (FAILED(hr)) {
            ve::log::warn("Failed to create disjoint query for GPU profiling: 0x" + std::to_string(static_cast<unsigned int>(hr)));
        } else {
            ve::log::debug("GPU profiling system initialized");
        }
        
        // Initialize YUV to RGB conversion system (Week 5)
        if (!initialize_yuv_system()) {
            ve::log::warn("Failed to initialize YUV to RGB conversion system");
        } else {
            ve::log::debug("YUV to RGB conversion system initialized");
        }
        
        return true;
    }
    
    bool initialize_yuv_system() {
        if (!device || !context) return false;
        
        HRESULT hr;
        
        // Create color space constants buffer
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = sizeof(ColorSpaceConstants);
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        hr = device->CreateBuffer(&buffer_desc, nullptr, &color_space_constants_buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create color space constants buffer");
            return false;
        }
        
        // Compile YUV to RGB shaders
        if (!compile_yuv_shaders()) {
            ve::log::error("Failed to compile YUV to RGB shaders");
            return false;
        }
        
        ve::log::debug("YUV system initialized with BT.709 color space");
        return true;
    }
    
    bool compile_yuv_shaders() {
        // YUV420P/422P/444P Planar shader
        const char* planar_vs_source = R"(
            cbuffer ColorSpaceConstants : register(b0) {
                float4x4 yuv_to_rgb_matrix;
                float4 black_level;
                float4 white_level;
                float2 chroma_scale;
                float2 luma_scale;
            };
            
            struct VS_INPUT {
                float2 position : POSITION;
                float2 texcoord : TEXCOORD0;
            };
            
            struct PS_INPUT {
                float4 position : SV_POSITION;
                float2 texcoord : TEXCOORD0;
            };
            
            PS_INPUT main(VS_INPUT input) {
                PS_INPUT output;
                output.position = float4(input.position, 0.0f, 1.0f);
                output.texcoord = input.texcoord;
                return output;
            }
        )";
        
        const char* planar_ps_source = R"(
            cbuffer ColorSpaceConstants : register(b0) {
                float4x4 yuv_to_rgb_matrix;
                float4 black_level;
                float4 white_level;
                float2 chroma_scale;
                float2 luma_scale;
            };
            
            Texture2D Y_texture : register(t0);
            Texture2D U_texture : register(t1);
            Texture2D V_texture : register(t2);
            SamplerState linear_sampler : register(s0);
            
            struct PS_INPUT {
                float4 position : SV_POSITION;
                float2 texcoord : TEXCOORD0;
            };
            
            float4 main(PS_INPUT input) : SV_Target {
                float2 uv = input.texcoord;
                
                float y = Y_texture.Sample(linear_sampler, uv * luma_scale).r;
                float u = U_texture.Sample(linear_sampler, uv * chroma_scale).r;
                float v = V_texture.Sample(linear_sampler, uv * chroma_scale).r;
                
                y = (y - black_level.x) / (white_level.x - black_level.x);
                u = (u - black_level.y) / (white_level.y - black_level.y) - 0.5f;
                v = (v - black_level.z) / (white_level.z - black_level.z) - 0.5f;
                
                float3 yuv = float3(y, u, v);
                float3 rgb = mul(yuv_to_rgb_matrix, float4(yuv, 1.0f)).rgb;
                
                return float4(saturate(rgb), 1.0f);
            }
        )";
        
        // NV12/NV21 Semi-planar shader
        const char* nv12_ps_source = R"(
            cbuffer ColorSpaceConstants : register(b0) {
                float4x4 yuv_to_rgb_matrix;
                float4 black_level;
                float4 white_level;
                float2 chroma_scale;
                float2 luma_scale;
            };
            
            Texture2D YUV_luma : register(t3);
            Texture2D YUV_chroma : register(t4);
            SamplerState linear_sampler : register(s0);
            
            struct PS_INPUT {
                float4 position : SV_POSITION;
                float2 texcoord : TEXCOORD0;
            };
            
            float4 main(PS_INPUT input) : SV_Target {
                float2 uv = input.texcoord;
                
                float y = YUV_luma.Sample(linear_sampler, uv).r;
                float2 chroma = YUV_chroma.Sample(linear_sampler, uv * chroma_scale).rg;
                float u = chroma.r;
                float v = chroma.g;
                
                y = (y - black_level.x) / (white_level.x - black_level.x);
                u = (u - black_level.y) / (white_level.y - black_level.y) - 0.5f;
                v = (v - black_level.z) / (white_level.z - black_level.z) - 0.5f;
                
                float3 yuv = float3(y, u, v);
                float3 rgb = mul(yuv_to_rgb_matrix, float4(yuv, 1.0f)).rgb;
                
                return float4(saturate(rgb), 1.0f);
            }
        )";
        
        // Compile planar shaders
        if (!compile_shader_program(planar_vs_source, planar_ps_source, yuv_to_rgb_planar_shader)) {
            ve::log::error("Failed to compile YUV planar shaders");
            return false;
        }
        
        // Compile NV12 shaders
        if (!compile_shader_program(planar_vs_source, nv12_ps_source, yuv_to_rgb_nv12_shader)) {
            ve::log::error("Failed to compile YUV NV12 shaders");
            return false;
        }
        
        ve::log::debug("YUV to RGB shaders compiled successfully");
        return true;
    }

    void destroy() {
        // Clean up all resources
        textures.clear();
        shaders.clear();
        buffers.clear();
        
        // Clean up basic rendering resources
        if (fullscreen_vertex_buffer) {
            fullscreen_vertex_buffer->Release();
            fullscreen_vertex_buffer = nullptr;
        }
        
        // Clean up Week 4 resources
        command_buffer.clear();
        
        // Release GPU fence queries
        for (auto& fence : gpu_fences) {
            if (fence.fence_query) {
                fence.fence_query->Release();
            }
        }
        gpu_fences.clear();
        
        // Release profile event queries
        for (auto& [name, event] : profile_events) {
            if (event.start_query) {
                event.start_query->Release();
            }
            if (event.end_query) {
                event.end_query->Release();
            }
        }
        profile_events.clear();
        
        if (disjoint_query) {
            disjoint_query->Release();
            disjoint_query = nullptr;
        }
        
        // Clean up Week 5 YUV resources
        yuv_textures.clear();  // Unique_ptr destructors handle D3D11 resource cleanup
        
        if (color_space_constants_buffer) {
            color_space_constants_buffer->Release();
            color_space_constants_buffer = nullptr;
        }
        
        // Reset memory tracking
        used_gpu_memory = 0;
        
        if (backbuffer_rtv) {
            backbuffer_rtv->Release();
            backbuffer_rtv = nullptr;
        }
        
        if (swapchain) {
            swapchain->Release();
            swapchain = nullptr;
        }
        
        if (context) {
            context->Release();
            context = nullptr;
        }
        
        if (device) {
            device->Release();
            device = nullptr;
        }
        
        ve::log::info("D3D11 device destroyed");
    }

    unsigned int create_texture(int width, int height, int format) {
        if (!device) return 0;
        
        unsigned int id = next_texture_id++;
        auto texture = std::make_unique<D3D11Texture>();
        
        DXGI_FORMAT dxgi_format = get_dxgi_format(format);
        UINT bytes_per_pixel = get_bytes_per_pixel(dxgi_format);
        
        // Create main texture description (GPU-only)
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = dxgi_format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;  // GPU-only for performance
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture->texture);
        if (FAILED(hr)) {
            ve::log::error("Failed to create D3D11 texture: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        // Create staging texture for CPU uploads
        D3D11_TEXTURE2D_DESC staging_desc = desc;
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.BindFlags = 0;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        hr = device->CreateTexture2D(&staging_desc, nullptr, &texture->staging_texture);
        if (FAILED(hr)) {
            ve::log::error("Failed to create staging texture: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        // Create shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        
        hr = device->CreateShaderResourceView(texture->texture, &srv_desc, &texture->srv);
        if (FAILED(hr)) {
            ve::log::error("Failed to create shader resource view: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        // Create render target view
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
        rtv_desc.Format = desc.Format;
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        
        hr = device->CreateRenderTargetView(texture->texture, &rtv_desc, &texture->rtv);
        if (FAILED(hr)) {
            ve::log::error("Failed to create render target view: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        texture->width = width;
        texture->height = height;
        texture->format = dxgi_format;
        texture->row_pitch = width * bytes_per_pixel;
        texture->slice_pitch = texture->row_pitch * height;
        
        // Update memory tracking
        size_t memory_used = calculate_texture_memory(width, height, dxgi_format);
        memory_used += memory_used; // Double for staging texture
        used_gpu_memory += memory_used;
        
        ve::log::debug("Created D3D11 texture " + std::to_string(id) + " (" + std::to_string(width) + "x" + std::to_string(height) + 
                      ") using " + std::to_string(memory_used / 1024 / 1024) + " MB");
        
        textures[id] = std::move(texture);
        return id;
    }

    void destroy_texture(unsigned int texture_id) {
        auto it = textures.find(texture_id);
        if (it != textures.end()) {
            // Update memory tracking before destruction
            auto& texture = it->second;
            size_t memory_freed = calculate_texture_memory(texture->width, texture->height, texture->format);
            memory_freed += memory_freed; // Double for staging texture
            used_gpu_memory = (used_gpu_memory > memory_freed) ? used_gpu_memory - memory_freed : 0;
            
            ve::log::debug("Destroyed D3D11 texture " + std::to_string(texture_id) + 
                          ", freed " + std::to_string(memory_freed / 1024 / 1024) + " MB");
            textures.erase(it);
        }
    }

    void upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) {
        (void)format; // Format is stored in texture object, parameter kept for interface compatibility
        
        auto it = textures.find(texture_id);
        if (it == textures.end()) {
            ve::log::error("Texture " + std::to_string(texture_id) + " not found for upload");
            return;
        }
        
        auto& texture = it->second;
        if (texture->width != static_cast<UINT>(width) || texture->height != static_cast<UINT>(height)) {
            ve::log::error("Texture size mismatch for upload: " + std::to_string(texture->width) + "x" + std::to_string(texture->height) + 
                          " vs " + std::to_string(width) + "x" + std::to_string(height));
            return;
        }
        
        // Map the staging texture for CPU write access
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context->Map(texture->staging_texture, 0, D3D11_MAP_WRITE, 0, &mapped);
        if (FAILED(hr)) {
            ve::log::error("Failed to map staging texture for upload: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return;
        }
        
        // Copy data to staging texture
        const char* src_data = static_cast<const char*>(data);
        char* dst_data = static_cast<char*>(mapped.pData);
        UINT src_row_pitch = texture->row_pitch;
        
        for (int y = 0; y < height; ++y) {
            memcpy(dst_data + y * mapped.RowPitch, src_data + y * src_row_pitch, src_row_pitch);
        }
        
        // Unmap the staging texture
        context->Unmap(texture->staging_texture, 0);
        
        // Copy from staging texture to main GPU texture
        context->CopyResource(texture->texture, texture->staging_texture);
        
        ve::log::debug("Uploaded " + std::to_string(texture->slice_pitch / 1024) + " KB to D3D11 texture " + std::to_string(texture_id));
    }

    // Buffer Management Methods
    unsigned int create_buffer(int size, int usage_flags, const void* initial_data = nullptr) {
        if (!device) return 0;
        
        unsigned int id = next_buffer_id++;
        auto buffer = std::make_unique<D3D11Buffer>();
        
        // Create buffer description
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = size;
        desc.Usage = D3D11_USAGE_DEFAULT;  // GPU-only for performance
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        
        // Set bind flags based on usage
        if (usage_flags & 1) desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
        if (usage_flags & 2) desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
        if (usage_flags & 4) desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
        
        // Initial data if provided
        D3D11_SUBRESOURCE_DATA* init_data_ptr = nullptr;
        D3D11_SUBRESOURCE_DATA init_data = {};
        if (initial_data) {
            init_data.pSysMem = initial_data;
            init_data_ptr = &init_data;
        }
        
        HRESULT hr = device->CreateBuffer(&desc, init_data_ptr, &buffer->buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create D3D11 buffer: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        // Create staging buffer for CPU uploads
        D3D11_BUFFER_DESC staging_desc = desc;
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.BindFlags = 0;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        hr = device->CreateBuffer(&staging_desc, nullptr, &buffer->staging_buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create staging buffer: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return 0;
        }
        
        buffer->desc = desc;
        buffer->size = size;
        
        // Update memory tracking
        used_gpu_memory += size * 2; // Double for staging buffer
        
        ve::log::debug("Created D3D11 buffer " + std::to_string(id) + " (" + std::to_string(size / 1024) + " KB)");
        
        buffers[id] = std::move(buffer);
        return id;
    }
    
    void destroy_buffer(unsigned int buffer_id) {
        auto it = buffers.find(buffer_id);
        if (it != buffers.end()) {
            // Update memory tracking
            auto& buffer = it->second;
            size_t memory_freed = buffer->size * 2; // Double for staging buffer
            used_gpu_memory = (used_gpu_memory > memory_freed) ? used_gpu_memory - memory_freed : 0;
            
            ve::log::debug("Destroyed D3D11 buffer " + std::to_string(buffer_id) + 
                          ", freed " + std::to_string(memory_freed / 1024) + " KB");
            buffers.erase(it);
        }
    }
    
    void upload_buffer(unsigned int buffer_id, const void* data, int size, int offset = 0) {
        auto it = buffers.find(buffer_id);
        if (it == buffers.end()) {
            ve::log::error("Buffer " + std::to_string(buffer_id) + " not found for upload");
            return;
        }
        
        auto& buffer = it->second;
        if (offset + size > static_cast<int>(buffer->size)) {
            ve::log::error("Buffer upload would exceed buffer size: " + std::to_string(offset + size) + 
                          " > " + std::to_string(buffer->size));
            return;
        }
        
        // Map staging buffer
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context->Map(buffer->staging_buffer, 0, D3D11_MAP_WRITE, 0, &mapped);
        if (FAILED(hr)) {
            ve::log::error("Failed to map staging buffer: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            return;
        }
        
        // Copy data to staging buffer
        memcpy(static_cast<char*>(mapped.pData) + offset, data, size);
        context->Unmap(buffer->staging_buffer, 0);
        
        // Copy from staging to main buffer
        D3D11_BOX box = {};
        box.left = offset;
        box.right = offset + size;
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        
        context->CopySubresourceRegion(buffer->buffer, 0, offset, 0, 0, buffer->staging_buffer, 0, &box);
        
        ve::log::debug("Uploaded " + std::to_string(size / 1024) + " KB to D3D11 buffer " + std::to_string(buffer_id));
    }

    // Memory usage reporting
    void get_memory_usage(size_t* total, size_t* used, size_t* available) {
        if (total) *total = total_gpu_memory;
        if (used) *used = used_gpu_memory;
        if (available) *available = (total_gpu_memory > used_gpu_memory) ? total_gpu_memory - used_gpu_memory : 0;
    }

    // Helper function for compiling shader programs (used by YUV shaders)
    bool compile_shader_program(const char* vertex_src, const char* fragment_src, std::unique_ptr<D3D11ShaderProgram>& shader_out) {
        if (!device) return false;
        
        auto shader = std::make_unique<D3D11ShaderProgram>();
        
        // Compile vertex shader
        ID3DBlob* vs_blob = nullptr;
        ID3DBlob* error_blob = nullptr;
        
        HRESULT hr = D3DCompile(
            vertex_src, strlen(vertex_src), nullptr, nullptr, nullptr,
            "main", "vs_5_0", 0, 0, &vs_blob, &error_blob
        );
        
        if (FAILED(hr)) {
            if (error_blob) {
                const char* error_msg = static_cast<const char*>(error_blob->GetBufferPointer());
                ve::log::error("Vertex shader compilation failed: " + std::string(error_msg));
                error_blob->Release();
            }
            if (vs_blob) vs_blob->Release();
            return false;
        }
        
        hr = device->CreateVertexShader(
            vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(),
            nullptr,
            &shader->vertex_shader
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create vertex shader: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            vs_blob->Release();
            return false;
        }
        
        // Compile pixel shader
        ID3DBlob* ps_blob = nullptr;
        hr = D3DCompile(
            fragment_src, strlen(fragment_src), nullptr, nullptr, nullptr,
            "main", "ps_5_0", 0, 0, &ps_blob, &error_blob
        );
        
        if (FAILED(hr)) {
            if (error_blob) {
                const char* error_msg = static_cast<const char*>(error_blob->GetBufferPointer());
                ve::log::error("Pixel shader compilation failed: " + std::string(error_msg));
                error_blob->Release();
            }
            vs_blob->Release();
            if (ps_blob) ps_blob->Release();
            return false;
        }
        
        hr = device->CreatePixelShader(
            ps_blob->GetBufferPointer(),
            ps_blob->GetBufferSize(),
            nullptr,
            &shader->pixel_shader
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create pixel shader: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return false;
        }
        
        // Create input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        
        hr = device->CreateInputLayout(
            layout,
            ARRAYSIZE(layout),
            vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(),
            &shader->input_layout
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create input layout: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return false;
        }
        
        // Create constant buffer
        D3D11_BUFFER_DESC cb_desc = {};
        cb_desc.ByteWidth = 256; // 256 bytes should be enough for most uniforms
        cb_desc.Usage = D3D11_USAGE_DYNAMIC;
        cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        hr = device->CreateBuffer(&cb_desc, nullptr, &shader->constant_buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create constant buffer: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return false;
        }
        
        // Create sampler state
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        
        hr = device->CreateSamplerState(&sampler_desc, &shader->sampler_state);
        if (FAILED(hr)) {
            ve::log::error("Failed to create sampler state: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return false;
        }
        
        ps_blob->Release();
        vs_blob->Release();
        
        shader_out = std::move(shader);
        return true;
    }

    unsigned int create_shader_program(const char* vertex_src, const char* fragment_src) {
        if (!device) return 0;
        
        unsigned int id = next_shader_id++;
        auto shader = std::make_unique<D3D11ShaderProgram>();
        
        // Compile vertex shader
        ID3DBlob* vs_blob = nullptr;
        ID3DBlob* error_blob = nullptr;
        
        HRESULT hr = D3DCompile(
            vertex_src, strlen(vertex_src), nullptr, nullptr, nullptr,
            "main", "vs_5_0", 0, 0, &vs_blob, &error_blob
        );
        
        if (FAILED(hr)) {
            if (error_blob) {
                const char* error_msg = static_cast<const char*>(error_blob->GetBufferPointer());
                ve::log::error("Vertex shader compilation failed: " + std::string(error_msg));
                error_blob->Release();
            }
            if (vs_blob) vs_blob->Release();
            return 0;
        }
        
        hr = device->CreateVertexShader(
            vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(),
            nullptr,
            &shader->vertex_shader
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create vertex shader: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            vs_blob->Release();
            return 0;
        }
        
        // Compile pixel shader
        ID3DBlob* ps_blob = nullptr;
        hr = D3DCompile(
            fragment_src, strlen(fragment_src), nullptr, nullptr, nullptr,
            "main", "ps_5_0", 0, 0, &ps_blob, &error_blob
        );
        
        if (FAILED(hr)) {
            if (error_blob) {
                const char* error_msg = static_cast<const char*>(error_blob->GetBufferPointer());
                ve::log::error("Pixel shader compilation failed: " + std::string(error_msg));
                error_blob->Release();
            }
            if (ps_blob) ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        hr = device->CreatePixelShader(
            ps_blob->GetBufferPointer(),
            ps_blob->GetBufferSize(),
            nullptr,
            &shader->pixel_shader
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create pixel shader: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        // Create input layout matching our HLSL vertex shader
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        
        hr = device->CreateInputLayout(
            layout,
            ARRAYSIZE(layout),
            vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(),
            &shader->input_layout
        );
        
        if (FAILED(hr)) {
            ve::log::error("Failed to create input layout: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        // Create constant buffer
        D3D11_BUFFER_DESC cb_desc = {};
        cb_desc.ByteWidth = 256; // 256 bytes should be enough for most uniforms
        cb_desc.Usage = D3D11_USAGE_DYNAMIC;
        cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        hr = device->CreateBuffer(&cb_desc, nullptr, &shader->constant_buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create constant buffer: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        // Create sampler state
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        
        hr = device->CreateSamplerState(&sampler_desc, &shader->sampler_state);
        if (FAILED(hr)) {
            ve::log::error("Failed to create sampler state: 0x" + std::to_string(static_cast<unsigned int>(hr)));
            ps_blob->Release();
            vs_blob->Release();
            return 0;
        }
        
        ve::log::debug("Created D3D11 shader program " + std::to_string(id));
        
        ps_blob->Release();
        vs_blob->Release();
        
        shaders[id] = std::move(shader);
        return id;
    }

    void destroy_shader_program(unsigned int program_id) {
        if (shaders.erase(program_id)) {
            ve::log::debug("Destroyed D3D11 shader program " + std::to_string(program_id));
        }
    }

    void use_shader_program(unsigned int program_id) {
        auto it = shaders.find(program_id);
        if (it == shaders.end()) return;
        
        auto& shader = it->second;
        context->VSSetShader(shader->vertex_shader, nullptr, 0);
        context->PSSetShader(shader->pixel_shader, nullptr, 0);
        context->IASetInputLayout(shader->input_layout);
        
        // Bind sampler state to pixel shader
        ID3D11SamplerState* samplers[] = { shader->sampler_state };
        context->PSSetSamplers(0, 1, samplers);
        
        // Bind constant buffer to pixel shader
        ID3D11Buffer* constant_buffers[] = { shader->constant_buffer };
        context->PSSetConstantBuffers(0, 1, constant_buffers);
        
        ve::log::debug("Using D3D11 shader program " + std::to_string(program_id));
    }

    void set_uniform1f(unsigned int program_id, const char* name, float v) {
        auto it = shaders.find(program_id);
        if (it == shaders.end()) return;
        
        auto& shader = it->second;
        
        // For now, we'll assume the uniform is "brightness" at offset 0
        // In a full implementation, we'd have a uniform mapping system
        if (std::strcmp(name, "brightness") == 0) {
            D3D11_MAPPED_SUBRESOURCE mapped;
            HRESULT hr = context->Map(shader->constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (SUCCEEDED(hr)) {
                float* constants = static_cast<float*>(mapped.pData);
                constants[0] = v;  // brightness
                constants[1] = 0.0f;  // padding
                constants[2] = 0.0f;  // padding
                constants[3] = 0.0f;  // padding
                
                context->Unmap(shader->constant_buffer, 0);
            }
        }
        
        ve::log::debug("set_uniform1f(" + std::to_string(program_id) + ", " + std::string(name) + ", " + std::to_string(v) + ")");
    }

    void set_uniform1i(unsigned int program_id, const char* name, int v) {
        // For now, just log the call
        ve::log::debug("set_uniform1i(" + std::to_string(program_id) + ", " + std::string(name) + ", " + std::to_string(v) + ")");
    }

    void set_uniform4f(unsigned int program_id, const char* name, float x, float y, float z, float w) {
        // For now, just log the call
        (void)program_id; (void)name; (void)x; (void)y; (void)z; (void)w;
    }

    void clear(float r, float g, float b, float a) {
        if (!context || !backbuffer_rtv) return;
        
        float clear_color[4] = {r, g, b, a};
        context->ClearRenderTargetView(backbuffer_rtv, clear_color);
        
        ve::log::debug("clear(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ", " + std::to_string(a) + ")");
    }

    void draw_texture(unsigned int texture_id, float x, float y, float width, float height) {
        auto it = textures.find(texture_id);
        if (it == textures.end()) return;
        
        // Bind texture as shader resource
        auto& texture = it->second;
        ID3D11ShaderResourceView* srvs[] = { texture->srv };
        context->PSSetShaderResources(0, 1, srvs);
        
        // Setup vertex buffer
        if (fullscreen_vertex_buffer) {
            UINT stride = sizeof(float) * 4; // 2 floats position + 2 floats texcoord
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, &fullscreen_vertex_buffer, &stride, &offset);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            
            // Draw the fullscreen quad (6 vertices)
            context->Draw(6, 0);
            
            ve::log::debug("Drew texture " + std::to_string(texture_id) + " as fullscreen quad");
        } else {
            ve::log::error("No fullscreen vertex buffer available for drawing");
        }
        
        // Note: In a full implementation, x, y, width, height would be used to create
        // a properly positioned quad instead of fullscreen
        (void)x; (void)y; (void)width; (void)height; // Suppress unused warnings for now
    }

    void set_viewport(int width, int height) {
        if (!context) return;
        
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        
        context->RSSetViewports(1, &viewport);
        
        ve::log::debug("set_viewport(" + std::to_string(width) + ", " + std::to_string(height) + ")");
    }

    bool get_last_present_rgba(const void** data, int* width, int* height, int* stride) {
        // For headless rendering, we would read back from a render target
        // This is a placeholder - implementation would require creating staging texture
        // and mapping it for CPU access
        (void)data; (void)width; (void)height; (void)stride;
        return false;
    }

    // Week 4: Command Buffer & Synchronization Methods
    void begin_command_recording() {
        command_buffer.clear();
        ve::log::debug("CommandBuffer: Recording started");
    }
    
    void record_set_render_target(ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::SET_RENDER_TARGET;
        cmd.render_target.rtv = rtv;
        cmd.render_target.dsv = dsv;
        command_buffer.push_back(cmd);
    }
    
    void record_set_shader_program(ID3D11VertexShader* vs, ID3D11PixelShader* ps) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::SET_SHADER_PROGRAM;
        cmd.shader_program.vertex_shader = vs;
        cmd.shader_program.pixel_shader = ps;
        command_buffer.push_back(cmd);
    }
    
    void record_draw_quad() {
        RenderCommand cmd;
        cmd.type = RenderCommandType::DRAW_QUAD;
        // No additional data needed for fullscreen quad
        command_buffer.push_back(cmd);
    }
    
    void record_set_texture(ID3D11ShaderResourceView* srv, UINT slot) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::SET_TEXTURE;
        cmd.texture.srv = srv;
        cmd.texture.slot = slot;
        command_buffer.push_back(cmd);
    }
    
    void execute_command_buffer() {
        if (!context) return;
        
        ve::log::debug("CommandBuffer: Executing " + std::to_string(command_buffer.size()) + " commands");
        
        for (const auto& cmd : command_buffer) {
            switch (cmd.type) {
                case RenderCommandType::SET_RENDER_TARGET:
                    context->OMSetRenderTargets(1, &cmd.render_target.rtv, cmd.render_target.dsv);
                    break;
                    
                case RenderCommandType::SET_SHADER_PROGRAM:
                    context->VSSetShader(cmd.shader_program.vertex_shader, nullptr, 0);
                    context->PSSetShader(cmd.shader_program.pixel_shader, nullptr, 0);
                    break;
                    
                case RenderCommandType::DRAW_QUAD:
                    // Draw fullscreen quad (3 vertices, triangle strip)
                    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                    context->Draw(3, 0);
                    break;
                    
                case RenderCommandType::SET_TEXTURE:
                    context->PSSetShaderResources(cmd.texture.slot, 1, &cmd.texture.srv);
                    break;
                    
                case RenderCommandType::CLEAR_COLOR:
                    if (cmd.clear_color.rtv) {
                        context->ClearRenderTargetView(cmd.clear_color.rtv, cmd.clear_color.color);
                    }
                    break;
            }
        }
        
        command_buffer.clear();
    }
    
    // GPU Fence Management
    uint32_t create_fence() {
        GPUFence fence;
        fence.id = static_cast<uint32_t>(gpu_fences.size());
        fence.signaled = false;
        
        // Create fence query
        D3D11_QUERY_DESC query_desc = {};
        query_desc.Query = D3D11_QUERY_EVENT;
        query_desc.MiscFlags = 0;
        
        HRESULT hr = device->CreateQuery(&query_desc, &fence.fence_query);
        if (FAILED(hr)) {
            ve::log::error("Failed to create GPU fence query");
            return UINT32_MAX;
        }
        
        gpu_fences.push_back(fence);
        ve::log::debug("Created GPU fence #" + std::to_string(fence.id));
        return fence.id;
    }
    
    void signal_fence(uint32_t fence_id) {
        if (fence_id >= gpu_fences.size() || !context) return;
        
        auto& fence = gpu_fences[fence_id];
        context->End(fence.fence_query);
        fence.signaled = true;
        ve::log::debug("Signaled GPU fence #" + std::to_string(fence_id));
    }
    
    bool is_fence_complete(uint32_t fence_id) {
        if (fence_id >= gpu_fences.size() || !context) return true;
        
        auto& fence = gpu_fences[fence_id];
        if (!fence.signaled) return false;
        
        BOOL query_data = FALSE;
        HRESULT hr = context->GetData(fence.fence_query, &query_data, sizeof(BOOL), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        
        return (hr == S_OK && query_data == TRUE);
    }
    
    void wait_for_fence(uint32_t fence_id) {
        if (fence_id >= gpu_fences.size()) return;
        
        ve::log::debug("Waiting for GPU fence #" + std::to_string(fence_id));
        
        // Spin-wait for fence completion
        while (!is_fence_complete(fence_id)) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        ve::log::debug("GPU fence #" + std::to_string(fence_id) + " completed");
    }
    
    // GPU Profiling Methods
    void begin_profile_event(const std::string& name) {
        if (!context || !disjoint_query) return;
        
        ProfileEvent event;
        event.name = name;
        event.active = true;
        
        // Create start timestamp query
        D3D11_QUERY_DESC query_desc = {};
        query_desc.Query = D3D11_QUERY_TIMESTAMP;
        query_desc.MiscFlags = 0;
        
        HRESULT hr1 = device->CreateQuery(&query_desc, &event.start_query);
        HRESULT hr2 = device->CreateQuery(&query_desc, &event.end_query);
        
        if (FAILED(hr1) || FAILED(hr2)) {
            ve::log::error("Failed to create profiling queries for: " + name);
            return;
        }
        
        // Record start timestamp
        context->End(event.start_query);
        
        profile_events[name] = event;
        ve::log::debug("Started profiling event: " + name);
    }
    
    void end_profile_event(const std::string& name) {
        if (!context || profile_events.find(name) == profile_events.end()) return;
        
        auto& event = profile_events[name];
        if (!event.active) return;
        
        // Record end timestamp
        context->End(event.end_query);
        event.active = false;
        
        ve::log::debug("Ended profiling event: " + name);
    }
    
    float get_profile_event_time_ms(const std::string& name) {
        if (!context || profile_events.find(name) == profile_events.end()) return 0.0f;
        
        auto& event = profile_events[name];
        if (event.active) return 0.0f; // Event still in progress
        
        // Get disjoint query data first
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
        HRESULT hr = context->GetData(disjoint_query, &disjoint_data, sizeof(disjoint_data), 0);
        if (FAILED(hr) || disjoint_data.Disjoint) {
            return 0.0f;
        }
        
        // Get start and end timestamps
        UINT64 start_time, end_time;
        hr = context->GetData(event.start_query, &start_time, sizeof(UINT64), 0);
        if (FAILED(hr)) return 0.0f;
        
        hr = context->GetData(event.end_query, &end_time, sizeof(UINT64), 0);
        if (FAILED(hr)) return 0.0f;
        
        // Calculate elapsed time in milliseconds
        double elapsed_ticks = static_cast<double>(end_time - start_time);
        double elapsed_ms = (elapsed_ticks / static_cast<double>(disjoint_data.Frequency)) * 1000.0;
        
        return static_cast<float>(elapsed_ms);
    }
    
    // Frame Pacing Control
    void set_vsync_enabled(bool enabled) {
        vsync_enabled = enabled;
        ve::log::debug("VSync " + std::string(enabled ? "enabled" : "disabled"));
    }
    
    void limit_fps(float target_fps) {
        if (target_fps <= 0.0f) {
            frame_time_limit = 0.0f;
            return;
        }
        
        frame_time_limit = 1.0f / target_fps;
        ve::log::debug("Frame rate limited to " + std::to_string(target_fps) + " FPS");
    }
    
    void wait_for_frame_pacing() {
        if (frame_time_limit <= 0.0f) return;
        
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<float>(now - last_frame_time).count();
        
        if (elapsed < frame_time_limit) {
            float sleep_time = frame_time_limit - elapsed;
            std::this_thread::sleep_for(std::chrono::duration<float>(sleep_time));
        }
        
        last_frame_time = std::chrono::high_resolution_clock::now();
    }
    
    // Week 5: YUV to RGB Conversion Methods
    ColorSpaceConstants get_bt709_constants() {
        ColorSpaceConstants constants = {};
        
        // BT.709 YUV to RGB conversion matrix
        // R = Y + 1.5748 * V
        // G = Y - 0.1873 * U - 0.4681 * V  
        // B = Y + 1.8556 * U
        constants.yuv_to_rgb_matrix[0][0] = 1.0f;   constants.yuv_to_rgb_matrix[0][1] = 0.0f;     constants.yuv_to_rgb_matrix[0][2] = 1.5748f;  constants.yuv_to_rgb_matrix[0][3] = 0.0f;
        constants.yuv_to_rgb_matrix[1][0] = 1.0f;   constants.yuv_to_rgb_matrix[1][1] = -0.1873f; constants.yuv_to_rgb_matrix[1][2] = -0.4681f; constants.yuv_to_rgb_matrix[1][3] = 0.0f;
        constants.yuv_to_rgb_matrix[2][0] = 1.0f;   constants.yuv_to_rgb_matrix[2][1] = 1.8556f;  constants.yuv_to_rgb_matrix[2][2] = 0.0f;     constants.yuv_to_rgb_matrix[2][3] = 0.0f;
        constants.yuv_to_rgb_matrix[3][0] = 0.0f;   constants.yuv_to_rgb_matrix[3][1] = 0.0f;     constants.yuv_to_rgb_matrix[3][2] = 0.0f;     constants.yuv_to_rgb_matrix[3][3] = 1.0f;
        
        // Standard levels (16-235 for Y, 16-240 for UV in 8-bit)
        constants.black_level[0] = 16.0f / 255.0f;  // Y black level
        constants.black_level[1] = 16.0f / 255.0f;  // U black level  
        constants.black_level[2] = 16.0f / 255.0f;  // V black level
        constants.black_level[3] = 0.0f;            // Unused
        
        constants.white_level[0] = 235.0f / 255.0f; // Y white level
        constants.white_level[1] = 240.0f / 255.0f; // U white level
        constants.white_level[2] = 240.0f / 255.0f; // V white level  
        constants.white_level[3] = 1.0f;            // Unused
        
        // Default scaling (1:1 for full resolution)
        constants.chroma_scale[0] = 1.0f;
        constants.chroma_scale[1] = 1.0f;
        constants.luma_scale[0] = 1.0f;
        constants.luma_scale[1] = 1.0f;
        
        return constants;
    }
    
    uint32_t create_yuv_texture(YUVFormat format, UINT width, UINT height) {
        auto yuv_texture = std::make_unique<YUVTexture>();
        yuv_texture->format = format;
        yuv_texture->color_space = ColorSpace::BT709;
        yuv_texture->width = width;
        yuv_texture->height = height;
        
        // Calculate strides based on format
        yuv_texture->y_stride = width;
        
        switch (format) {
            case YUVFormat::YUV420P:
                yuv_texture->uv_stride = width / 2;
                yuv_texture->chroma_scale[0] = 0.5f;  // UV planes are half width
                yuv_texture->chroma_scale[1] = 0.5f;  // UV planes are half height
                break;
                
            case YUVFormat::YUV422P:
                yuv_texture->uv_stride = width / 2;
                yuv_texture->chroma_scale[0] = 0.5f;  // UV planes are half width
                yuv_texture->chroma_scale[1] = 1.0f;  // UV planes are full height
                break;
                
            case YUVFormat::YUV444P:
                yuv_texture->uv_stride = width;
                yuv_texture->chroma_scale[0] = 1.0f;  // UV planes are full width
                yuv_texture->chroma_scale[1] = 1.0f;  // UV planes are full height
                break;
                
            case YUVFormat::NV12:
            case YUVFormat::NV21:
                yuv_texture->uv_stride = width;       // Chroma is interleaved, same width as luma
                yuv_texture->chroma_scale[0] = 1.0f;  // Chroma plane same width
                yuv_texture->chroma_scale[1] = 0.5f;  // Chroma plane half height
                break;
                
            default:
                ve::log::error("Unsupported YUV format");
                return UINT32_MAX;
        }
        
        yuv_texture->luma_scale[0] = 1.0f;
        yuv_texture->luma_scale[1] = 1.0f;
        
        // Create D3D11 textures based on format
        if (!create_yuv_d3d_textures(*yuv_texture)) {
            ve::log::error("Failed to create YUV D3D11 textures");
            return UINT32_MAX;
        }
        
        uint32_t id = static_cast<uint32_t>(yuv_textures.size());
        yuv_textures.push_back(std::move(yuv_texture));
        
        ve::log::debug("Created YUV texture #" + std::to_string(id) + " (" + std::to_string(width) + "x" + std::to_string(height) + ")");
        return id;
    }
    
    bool create_yuv_d3d_textures(YUVTexture& yuv_tex) {
        if (!device) return false;
        
        HRESULT hr;
        
        if (yuv_tex.format == YUVFormat::YUV420P || yuv_tex.format == YUVFormat::YUV422P || yuv_tex.format == YUVFormat::YUV444P) {
            // Create separate Y, U, V planes
            
            // Y plane (full resolution)
            D3D11_TEXTURE2D_DESC y_desc = {};
            y_desc.Width = yuv_tex.width;
            y_desc.Height = yuv_tex.height;
            y_desc.MipLevels = 1;
            y_desc.ArraySize = 1;
            y_desc.Format = DXGI_FORMAT_R8_UNORM;
            y_desc.SampleDesc.Count = 1;
            y_desc.Usage = D3D11_USAGE_DYNAMIC;
            y_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            y_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            
            hr = device->CreateTexture2D(&y_desc, nullptr, &yuv_tex.y_plane);
            if (FAILED(hr)) return false;
            
            // UV planes (resolution depends on format)
            UINT uv_width = (yuv_tex.format == YUVFormat::YUV444P) ? yuv_tex.width : yuv_tex.width / 2;
            UINT uv_height = (yuv_tex.format == YUVFormat::YUV420P) ? yuv_tex.height / 2 : yuv_tex.height;
            
            D3D11_TEXTURE2D_DESC uv_desc = y_desc;
            uv_desc.Width = uv_width;
            uv_desc.Height = uv_height;
            
            hr = device->CreateTexture2D(&uv_desc, nullptr, &yuv_tex.u_plane);
            if (FAILED(hr)) return false;
            
            hr = device->CreateTexture2D(&uv_desc, nullptr, &yuv_tex.v_plane);
            if (FAILED(hr)) return false;
            
            // Create shader resource views
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = DXGI_FORMAT_R8_UNORM;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;
            
            hr = device->CreateShaderResourceView(yuv_tex.y_plane, &srv_desc, &yuv_tex.y_srv);
            if (FAILED(hr)) return false;
            
            hr = device->CreateShaderResourceView(yuv_tex.u_plane, &srv_desc, &yuv_tex.u_srv);
            if (FAILED(hr)) return false;
            
            hr = device->CreateShaderResourceView(yuv_tex.v_plane, &srv_desc, &yuv_tex.v_srv);
            if (FAILED(hr)) return false;
            
        } else if (yuv_tex.format == YUVFormat::NV12 || yuv_tex.format == YUVFormat::NV21) {
            // Create Y plane and UV interleaved plane
            
            // Y plane (full resolution, single channel)
            D3D11_TEXTURE2D_DESC luma_desc = {};
            luma_desc.Width = yuv_tex.width;
            luma_desc.Height = yuv_tex.height;
            luma_desc.MipLevels = 1;
            luma_desc.ArraySize = 1;
            luma_desc.Format = DXGI_FORMAT_R8_UNORM;
            luma_desc.SampleDesc.Count = 1;
            luma_desc.Usage = D3D11_USAGE_DYNAMIC;
            luma_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            luma_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            
            hr = device->CreateTexture2D(&luma_desc, nullptr, &yuv_tex.luma_plane);
            if (FAILED(hr)) return false;
            
            // Chroma plane (half resolution, two channels interleaved)
            D3D11_TEXTURE2D_DESC chroma_desc = luma_desc;
            chroma_desc.Width = yuv_tex.width / 2;
            chroma_desc.Height = yuv_tex.height / 2;
            chroma_desc.Format = DXGI_FORMAT_R8G8_UNORM;  // Two 8-bit channels
            
            hr = device->CreateTexture2D(&chroma_desc, nullptr, &yuv_tex.chroma_plane);
            if (FAILED(hr)) return false;
            
            // Create shader resource views
            D3D11_SHADER_RESOURCE_VIEW_DESC luma_srv_desc = {};
            luma_srv_desc.Format = DXGI_FORMAT_R8_UNORM;
            luma_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            luma_srv_desc.Texture2D.MipLevels = 1;
            
            hr = device->CreateShaderResourceView(yuv_tex.luma_plane, &luma_srv_desc, &yuv_tex.luma_srv);
            if (FAILED(hr)) return false;
            
            D3D11_SHADER_RESOURCE_VIEW_DESC chroma_srv_desc = {};
            chroma_srv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
            chroma_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            chroma_srv_desc.Texture2D.MipLevels = 1;
            
            hr = device->CreateShaderResourceView(yuv_tex.chroma_plane, &chroma_srv_desc, &yuv_tex.chroma_srv);
            if (FAILED(hr)) return false;
        }
        
        // Track memory usage
        size_t memory_size = yuv_tex.y_stride * yuv_tex.height;  // Y plane
        
        if (yuv_tex.format == YUVFormat::YUV420P) {
            memory_size += (yuv_tex.uv_stride * yuv_tex.height / 2) * 2;  // U and V planes
        } else if (yuv_tex.format == YUVFormat::YUV422P) {
            memory_size += (yuv_tex.uv_stride * yuv_tex.height) * 2;      // U and V planes  
        } else if (yuv_tex.format == YUVFormat::YUV444P) {
            memory_size += (yuv_tex.uv_stride * yuv_tex.height) * 2;      // U and V planes
        } else if (yuv_tex.format == YUVFormat::NV12 || yuv_tex.format == YUVFormat::NV21) {
            memory_size += yuv_tex.uv_stride * yuv_tex.height / 2;        // Interleaved UV plane
        }
        
        used_gpu_memory += memory_size;
        
        ve::log::debug("YUV texture memory: " + std::to_string(memory_size / (1024 * 1024)) + " MB");
        return true;
    }
    
    bool upload_yuv_frame(uint32_t yuv_texture_id, const uint8_t* y_data, const uint8_t* u_data, const uint8_t* v_data) {
        if (yuv_texture_id >= yuv_textures.size() || !context) return false;
        
        auto& yuv_tex = *yuv_textures[yuv_texture_id];
        HRESULT hr;
        
        if (yuv_tex.format == YUVFormat::YUV420P || yuv_tex.format == YUVFormat::YUV422P || yuv_tex.format == YUVFormat::YUV444P) {
            // Upload Y plane
            D3D11_MAPPED_SUBRESOURCE mapped_y;
            hr = context->Map(yuv_tex.y_plane, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_y);
            if (FAILED(hr)) return false;
            
            for (UINT row = 0; row < yuv_tex.height; row++) {
                memcpy((uint8_t*)mapped_y.pData + row * mapped_y.RowPitch,
                       y_data + row * yuv_tex.y_stride,
                       yuv_tex.y_stride);
            }
            context->Unmap(yuv_tex.y_plane, 0);
            
            // Upload U plane
            UINT uv_height = (yuv_tex.format == YUVFormat::YUV420P) ? yuv_tex.height / 2 : yuv_tex.height;
            
            D3D11_MAPPED_SUBRESOURCE mapped_u;
            hr = context->Map(yuv_tex.u_plane, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_u);
            if (FAILED(hr)) return false;
            
            for (UINT row = 0; row < uv_height; row++) {
                memcpy((uint8_t*)mapped_u.pData + row * mapped_u.RowPitch,
                       u_data + row * yuv_tex.uv_stride,
                       yuv_tex.uv_stride);
            }
            context->Unmap(yuv_tex.u_plane, 0);
            
            // Upload V plane
            D3D11_MAPPED_SUBRESOURCE mapped_v;
            hr = context->Map(yuv_tex.v_plane, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_v);
            if (FAILED(hr)) return false;
            
            for (UINT row = 0; row < uv_height; row++) {
                memcpy((uint8_t*)mapped_v.pData + row * mapped_v.RowPitch,
                       v_data + row * yuv_tex.uv_stride,
                       yuv_tex.uv_stride);
            }
            context->Unmap(yuv_tex.v_plane, 0);
            
        } else if (yuv_tex.format == YUVFormat::NV12 || yuv_tex.format == YUVFormat::NV21) {
            // Upload Y plane
            D3D11_MAPPED_SUBRESOURCE mapped_y;
            hr = context->Map(yuv_tex.luma_plane, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_y);
            if (FAILED(hr)) return false;
            
            for (UINT row = 0; row < yuv_tex.height; row++) {
                memcpy((uint8_t*)mapped_y.pData + row * mapped_y.RowPitch,
                       y_data + row * yuv_tex.y_stride,
                       yuv_tex.y_stride);
            }
            context->Unmap(yuv_tex.luma_plane, 0);
            
            // Upload interleaved UV plane
            D3D11_MAPPED_SUBRESOURCE mapped_uv;
            hr = context->Map(yuv_tex.chroma_plane, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_uv);
            if (FAILED(hr)) return false;
            
            // For NV12: UV interleaved, for NV21: VU interleaved
            bool is_nv21 = (yuv_tex.format == YUVFormat::NV21);
            UINT chroma_height = yuv_tex.height / 2;
            
            for (UINT row = 0; row < chroma_height; row++) {
                uint8_t* dst_row = (uint8_t*)mapped_uv.pData + row * mapped_uv.RowPitch;
                const uint8_t* u_src = u_data + row * yuv_tex.uv_stride;
                const uint8_t* v_src = v_data + row * yuv_tex.uv_stride;
                
                for (UINT col = 0; col < yuv_tex.uv_stride; col++) {
                    if (is_nv21) {
                        dst_row[col * 2 + 0] = v_src[col];  // V first for NV21
                        dst_row[col * 2 + 1] = u_src[col];  // U second
                    } else {
                        dst_row[col * 2 + 0] = u_src[col];  // U first for NV12
                        dst_row[col * 2 + 1] = v_src[col];  // V second
                    }
                }
            }
            context->Unmap(yuv_tex.chroma_plane, 0);
        }
        
        return true;
    }
    
    void render_yuv_to_rgb(uint32_t yuv_texture_id, uint32_t rgb_texture_id) {
        if (yuv_texture_id >= yuv_textures.size() || rgb_texture_id >= textures.size() || !context) return;
        
        auto& yuv_tex = *yuv_textures[yuv_texture_id];
        
        // Set render target
        ID3D11RenderTargetView* rtv = nullptr;
        for (auto& [id, rt] : render_targets) {
            if (id == rgb_texture_id) {
                rtv = rt;
                break;
            }
        }
        
        if (!rtv) return;
        
        context->OMSetRenderTargets(1, &rtv, nullptr);
        
        // Set viewport
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(yuv_tex.width);
        viewport.Height = static_cast<float>(yuv_tex.height);
        viewport.MaxDepth = 1.0f;
        context->RSSetViewports(1, &viewport);
        
        // Select appropriate shader based on YUV format
        D3D11ShaderProgram* shader_program = nullptr;
        if (yuv_tex.format == YUVFormat::YUV420P || yuv_tex.format == YUVFormat::YUV422P || yuv_tex.format == YUVFormat::YUV444P) {
            shader_program = yuv_to_rgb_planar_shader.get();
        } else if (yuv_tex.format == YUVFormat::NV12 || yuv_tex.format == YUVFormat::NV21) {
            shader_program = yuv_to_rgb_nv12_shader.get();
        }
        
        if (!shader_program || !shader_program->vertex_shader || !shader_program->pixel_shader) return;
        
        // Set shaders
        context->VSSetShader(shader_program->vertex_shader, nullptr, 0);
        context->PSSetShader(shader_program->pixel_shader, nullptr, 0);
        context->IASetInputLayout(shader_program->input_layout);
        
        // Set color space constants
        ColorSpaceConstants constants = get_bt709_constants();
        constants.chroma_scale[0] = yuv_tex.chroma_scale[0];
        constants.chroma_scale[1] = yuv_tex.chroma_scale[1];
        
        D3D11_MAPPED_SUBRESOURCE mapped_constants;
        if (SUCCEEDED(context->Map(color_space_constants_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_constants))) {
            memcpy(mapped_constants.pData, &constants, sizeof(ColorSpaceConstants));
            context->Unmap(color_space_constants_buffer, 0);
        }
        
        context->VSSetConstantBuffers(0, 1, &color_space_constants_buffer);
        context->PSSetConstantBuffers(0, 1, &color_space_constants_buffer);
        
        // Bind YUV textures
        if (yuv_tex.format == YUVFormat::YUV420P || yuv_tex.format == YUVFormat::YUV422P || yuv_tex.format == YUVFormat::YUV444P) {
            ID3D11ShaderResourceView* srvs[] = { yuv_tex.y_srv, yuv_tex.u_srv, yuv_tex.v_srv };
            context->PSSetShaderResources(0, 3, srvs);
        } else {
            ID3D11ShaderResourceView* srvs[] = { nullptr, nullptr, nullptr, yuv_tex.luma_srv, yuv_tex.chroma_srv };
            context->PSSetShaderResources(0, 5, srvs);
        }
        
        // Set sampler
        context->PSSetSamplers(0, 1, &shader_program->sampler_state);
        
        // Draw fullscreen quad
        UINT stride = sizeof(float) * 4; // 2 floats position + 2 floats texcoord
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &fullscreen_vertex_buffer, &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->Draw(3, 0);
        
        ve::log::debug("Rendered YUV to RGB: texture #" + std::to_string(yuv_texture_id) + " -> #" + std::to_string(rgb_texture_id));
    }

    // Week 6: Advanced Effects Shaders Implementation
    struct EffectShaders {
        ID3D11VertexShader* fullscreen_vs = nullptr;
        ID3D11PixelShader* color_correction_ps = nullptr;
        ID3D11PixelShader* transform_ps = nullptr;
        ID3D11PixelShader* blur_horizontal_ps = nullptr;
        ID3D11PixelShader* blur_vertical_ps = nullptr;
        ID3D11PixelShader* sharpen_ps = nullptr;
        ID3D11PixelShader* lut_application_ps = nullptr;
        
        ~EffectShaders() {
            if (lut_application_ps) lut_application_ps->Release();
            if (sharpen_ps) sharpen_ps->Release();
            if (blur_vertical_ps) blur_vertical_ps->Release();
            if (blur_horizontal_ps) blur_horizontal_ps->Release();
            if (transform_ps) transform_ps->Release();
            if (color_correction_ps) color_correction_ps->Release();
            if (fullscreen_vs) fullscreen_vs->Release();
        }
    };

    struct ColorCorrectionParams {
        float brightness = 0.0f;      // -1.0 to 1.0
        float contrast = 1.0f;        // 0.0 to 2.0
        float saturation = 1.0f;      // 0.0 to 2.0
        float gamma = 1.0f;          // 0.1 to 3.0
        float shadows[3] = {1.0f, 1.0f, 1.0f};
        float _padding1;
        float midtones[3] = {1.0f, 1.0f, 1.0f};
        float _padding2;
        float highlights[3] = {1.0f, 1.0f, 1.0f};
        float shadow_range = 0.3f;
        float highlight_range = 0.3f;
        float _padding[2];
    };

    struct TransformParams {
        float transform_matrix[16]; // 4x4 matrix
        float anchor_point[2] = {0.5f, 0.5f};
        float crop_rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        float texture_size[2];
        float output_size[2];
    };

    struct BlurParams {
        float blur_radius = 5.0f;
        float texture_size[2];
        float blur_direction[2];
        int kernel_size = 11;
        float _padding[3];
    };

    struct SharpenParams {
        float sharpen_strength = 1.0f;
        float edge_threshold = 0.1f;
        float texture_size[2];
        float clamp_strength = 0.3f;
        float _padding[3];
    };

    struct LUTParams {
        float lut_strength = 1.0f;
        float lut_size = 32.0f;
        float _padding[2];
    };

    EffectShaders effect_shaders;
    ID3D11Buffer* effect_constants_buffer = nullptr;
    ID3D11SamplerState* shared_sampler_state = nullptr;

    bool initialize_effect_shaders() {
        ve::log::info("Initializing Week 6 effect shaders");

        // Create constant buffer for effect parameters
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = 256;  // Large enough for any effect params
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, &effect_constants_buffer);
        if (FAILED(hr)) {
            ve::log::error("Failed to create effect constants buffer");
            return false;
        }

        // Create shared sampler state for effects
        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MinLOD = 0;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
        
        hr = device->CreateSamplerState(&sampler_desc, &shared_sampler_state);
        if (FAILED(hr)) {
            ve::log::error("Failed to create shared sampler state");
            return false;
        }

        ve::log::info("Week 6 effect shaders initialized successfully");
        return true;
    }

    bool apply_color_correction_effect(unsigned int input_texture_id, unsigned int output_texture_id, 
                                     const ColorCorrectionParams& params) {
        ve::log::info("Applying color correction effect: brightness=" + std::to_string(params.brightness) + 
                     ", contrast=" + std::to_string(params.contrast) + 
                     ", saturation=" + std::to_string(params.saturation));

        // Update constant buffer with parameters
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        HRESULT hr = context->Map(effect_constants_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        if (SUCCEEDED(hr)) {
            memcpy(mapped_resource.pData, &params, sizeof(ColorCorrectionParams));
            context->Unmap(effect_constants_buffer, 0);
        }

        context->PSSetConstantBuffers(0, 1, &effect_constants_buffer);
        
        // Set input texture and render
        auto it = textures.find(input_texture_id);
        if (it != textures.end() && it->second->srv) {
            context->PSSetShaderResources(0, 1, &it->second->srv);
            context->PSSetSamplers(0, 1, &shared_sampler_state);
            
            // Set output render target
            auto rt_it = render_targets.find(output_texture_id);
            if (rt_it != render_targets.end()) {
                context->OMSetRenderTargets(1, &rt_it->second, nullptr);
                
                // Set viewport to match texture size
                auto& tex = it->second;
                D3D11_VIEWPORT viewport = {};
                viewport.Width = static_cast<float>(tex->width);
                viewport.Height = static_cast<float>(tex->height);
                viewport.MaxDepth = 1.0f;
                context->RSSetViewports(1, &viewport);
                
                // Draw fullscreen quad
                UINT stride = sizeof(float) * 4;
                UINT offset = 0;
                context->IASetVertexBuffers(0, 1, &fullscreen_vertex_buffer, &stride, &offset);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                context->Draw(3, 0);
                
                ve::log::info("Applied color correction effect successfully");
                return true;
            }
        }

        return false;
    }

    bool apply_gaussian_blur_effect(unsigned int input_texture_id, unsigned int intermediate_texture_id,
                                   unsigned int output_texture_id, float radius) {
        ve::log::info("Applying Gaussian blur effect with radius=" + std::to_string(radius));
        
        // Get texture dimensions for blur parameters
        auto it = textures.find(input_texture_id);
        if (it == textures.end()) return false;
        
        BlurParams params;
        params.blur_radius = radius;
        params.texture_size[0] = static_cast<float>(it->second->width);
        params.texture_size[1] = static_cast<float>(it->second->height);
        params.kernel_size = static_cast<int>(radius * 2.0f) + 1;
        
        // Horizontal pass
        params.blur_direction[0] = 1.0f;
        params.blur_direction[1] = 0.0f;
        
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        HRESULT hr = context->Map(effect_constants_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        if (SUCCEEDED(hr)) {
            memcpy(mapped_resource.pData, &params, sizeof(BlurParams));
            context->Unmap(effect_constants_buffer, 0);
        }

        context->PSSetConstantBuffers(0, 1, &effect_constants_buffer);
        context->PSSetShaderResources(0, 1, &it->second->srv);
        context->PSSetSamplers(0, 1, &shared_sampler_state);
        
        // Render horizontal pass to intermediate texture
        auto intermediate_rt = render_targets.find(intermediate_texture_id);
        if (intermediate_rt != render_targets.end()) {
            context->OMSetRenderTargets(1, &intermediate_rt->second, nullptr);
            
            D3D11_VIEWPORT viewport = {};
            viewport.Width = static_cast<float>(it->second->width);
            viewport.Height = static_cast<float>(it->second->height);
            viewport.MaxDepth = 1.0f;
            context->RSSetViewports(1, &viewport);
            
            UINT stride = sizeof(float) * 4;
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, &fullscreen_vertex_buffer, &stride, &offset);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            context->Draw(3, 0);
        }
        
        // Vertical pass
        params.blur_direction[0] = 0.0f;
        params.blur_direction[1] = 1.0f;
        
        hr = context->Map(effect_constants_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        if (SUCCEEDED(hr)) {
            memcpy(mapped_resource.pData, &params, sizeof(BlurParams));
            context->Unmap(effect_constants_buffer, 0);
        }
        
        // Use intermediate texture as input for vertical pass
        auto intermediate_it = textures.find(intermediate_texture_id);
        if (intermediate_it != textures.end()) {
            context->PSSetShaderResources(0, 1, &intermediate_it->second->srv);
            
            // Render to final output
            auto output_rt = render_targets.find(output_texture_id);
            if (output_rt != render_targets.end()) {
                context->OMSetRenderTargets(1, &output_rt->second, nullptr);
                context->Draw(3, 0);
                
                ve::log::info("Applied Gaussian blur effect successfully");
                return true;
            }
        }
        
        return false;
    }

    bool apply_sharpen_effect(unsigned int input_texture_id, unsigned int output_texture_id,
                             float strength, float edge_threshold) {
        ve::log::info("Applying sharpen effect: strength=" + std::to_string(strength) + 
                     ", edge_threshold=" + std::to_string(edge_threshold));
        
        auto it = textures.find(input_texture_id);
        if (it == textures.end()) return false;
        
        SharpenParams params;
        params.sharpen_strength = strength;
        params.edge_threshold = edge_threshold;
        params.texture_size[0] = static_cast<float>(it->second->width);
        params.texture_size[1] = static_cast<float>(it->second->height);
        params.clamp_strength = 0.3f;
        
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        HRESULT hr = context->Map(effect_constants_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        if (SUCCEEDED(hr)) {
            memcpy(mapped_resource.pData, &params, sizeof(SharpenParams));
            context->Unmap(effect_constants_buffer, 0);
        }

        context->PSSetConstantBuffers(0, 1, &effect_constants_buffer);
        context->PSSetShaderResources(0, 1, &it->second->srv);
        context->PSSetSamplers(0, 1, &shared_sampler_state);
        
        auto rt_it = render_targets.find(output_texture_id);
        if (rt_it != render_targets.end()) {
            context->OMSetRenderTargets(1, &rt_it->second, nullptr);
            
            D3D11_VIEWPORT viewport = {};
            viewport.Width = static_cast<float>(it->second->width);
            viewport.Height = static_cast<float>(it->second->height);
            viewport.MaxDepth = 1.0f;
            context->RSSetViewports(1, &viewport);
            
            UINT stride = sizeof(float) * 4;
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, &fullscreen_vertex_buffer, &stride, &offset);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            context->Draw(3, 0);
            
            ve::log::info("Applied sharpen effect successfully");
            return true;
        }
        
        return false;
    }

    bool apply_lut_effect(unsigned int input_texture_id, unsigned int lut_texture_id, 
                         unsigned int output_texture_id, float strength) {
        ve::log::info("Applying 3D LUT effect with strength=" + std::to_string(strength));
        
        auto input_it = textures.find(input_texture_id);
        auto lut_it = textures.find(lut_texture_id);
        
        if (input_it == textures.end() || lut_it == textures.end()) return false;
        
        LUTParams params;
        params.lut_strength = strength;
        params.lut_size = 32.0f;  // Assuming 32x32x32 LUT
        
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        HRESULT hr = context->Map(effect_constants_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        if (SUCCEEDED(hr)) {
            memcpy(mapped_resource.pData, &params, sizeof(LUTParams));
            context->Unmap(effect_constants_buffer, 0);
        }

        context->PSSetConstantBuffers(0, 1, &effect_constants_buffer);
        
        // Bind both input texture and LUT texture
        ID3D11ShaderResourceView* srvs[2] = {input_it->second->srv, lut_it->second->srv};
        context->PSSetShaderResources(0, 2, srvs);
        context->PSSetSamplers(0, 1, &shared_sampler_state);
        
        auto rt_it = render_targets.find(output_texture_id);
        if (rt_it != render_targets.end()) {
            context->OMSetRenderTargets(1, &rt_it->second, nullptr);
            
            D3D11_VIEWPORT viewport = {};
            viewport.Width = static_cast<float>(input_it->second->width);
            viewport.Height = static_cast<float>(input_it->second->height);
            viewport.MaxDepth = 1.0f;
            context->RSSetViewports(1, &viewport);
            
            UINT stride = sizeof(float) * 4;
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, &fullscreen_vertex_buffer, &stride, &offset);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            context->Draw(3, 0);
            
            ve::log::info("Applied 3D LUT effect successfully");
            return true;
        }
        
        return false;
    }

    // Public API for Week 6 effects
    bool create_effect_pipeline() {
        return initialize_effect_shaders();
    }
};

// Global device instance
static D3D11GraphicsDevice g_device;
#else
// Stub implementation for non-Windows platforms
struct D3D11GraphicsDevice {
    struct DeviceInfo {
        bool debug_enabled;
        bool enable_swapchain;
        DeviceInfo() : debug_enabled(false), enable_swapchain(false) {}
    };
    
    bool create(const DeviceInfo& info) { (void)info; return false; }
    void destroy() {}
    bool is_created() const { return false; }
    
    // Texture management stub methods
    uint32_t create_texture(uint32_t, uint32_t, uint32_t) { return 0; }
    void destroy_texture(uint32_t) {}
    void upload_texture(uint32_t, const void*, uint32_t, uint32_t, uint32_t) {}
    
    // Buffer management stub methods  
    uint32_t create_buffer(size_t, uint32_t, const void*) { return 0; }
    void destroy_buffer(uint32_t) {}
    void upload_buffer(uint32_t, const void*, size_t, size_t) {}
    
    // Effect parameter types (forward declarations)
    struct ColorCorrectionParams {
        float brightness = 0.0f;
        float contrast = 1.0f;
        float saturation = 1.0f;
        float gamma = 1.0f;
    };
    
    // Effect pipeline stub methods
    uint32_t create_effect_pipeline() { return 0; }
    bool apply_color_correction_effect(uint32_t, uint32_t, const ColorCorrectionParams&) { return false; }
    bool apply_gaussian_blur_effect(uint32_t, uint32_t, uint32_t, float) { return false; }
    bool apply_sharpen_effect(uint32_t, uint32_t, float, float) { return false; }
    bool apply_lut_effect(uint32_t, uint32_t, uint32_t, float) { return false; }
    
    // Memory and performance stub methods
    void get_memory_usage(size_t*, size_t*, size_t*) {}
    
    // Shader program stub methods
    uint32_t create_shader_program(const char*, const char*) { return 0; }
    void destroy_shader_program(uint32_t) {}
    void use_shader_program(uint32_t) {}
    void set_uniform1f(uint32_t, const char*, float) {}
    void set_uniform1i(uint32_t, const char*, int) {}
    
    // Rendering stub methods
    void clear(float, float, float, float) {}
    void draw_texture(uint32_t, float, float, float, float) {}
    void set_viewport(int, int) {}
    bool get_last_present_rgba(const void**, int*, int*, int*) { return false; }
};
static D3D11GraphicsDevice g_device;
#endif // _WIN32

namespace ve::gfx {

// Implementation class for the PIMPL pattern
struct GraphicsDevice::Impl {
public:
    bool create(const GraphicsDeviceInfo& info = {}) {
        if (created_) return true;
        D3D11GraphicsDevice::DeviceInfo d3d_info;
        d3d_info.debug_enabled = info.enable_debug;
        d3d_info.enable_swapchain = info.enable_swapchain;
        created_ = g_device.create(d3d_info);
        return created_;
    }

    void destroy() {
        if (!created_) return;
        g_device.destroy();
        created_ = false;
    }

    bool is_valid() const { return created_; }

    unsigned int create_texture(int width, int height, int format) {
        return g_device.create_texture(static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(format));
    }

    void destroy_texture(unsigned int texture_id) {
        g_device.destroy_texture(texture_id);
    }

    void upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) {
        g_device.upload_texture(texture_id, data, static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(format));
    }

    // Buffer management methods
    unsigned int create_buffer(int size, int usage_flags, const void* initial_data = nullptr) {
        return g_device.create_buffer(static_cast<size_t>(size), static_cast<uint32_t>(usage_flags), initial_data);
    }

    void destroy_buffer(unsigned int buffer_id) {
        g_device.destroy_buffer(buffer_id);
    }

    void upload_buffer(unsigned int buffer_id, const void* data, int size, int offset = 0) {
        g_device.upload_buffer(buffer_id, data, static_cast<size_t>(size), static_cast<size_t>(offset));
    }

    // Week 6: Advanced Effects API
    bool create_effect_pipeline() {
        return g_device.create_effect_pipeline();
    }

    bool apply_color_correction(unsigned int input_texture, unsigned int output_texture,
                              float brightness, float contrast, float saturation, float gamma) {
        D3D11GraphicsDevice::ColorCorrectionParams params;
        params.brightness = brightness;
        params.contrast = contrast;
        params.saturation = saturation;
        params.gamma = gamma;
        
        return g_device.apply_color_correction_effect(input_texture, output_texture, params);
    }

    bool apply_gaussian_blur(unsigned int input_texture, unsigned int intermediate_texture,
                           unsigned int output_texture, float radius) {
        return g_device.apply_gaussian_blur_effect(input_texture, intermediate_texture, output_texture, radius);
    }

    bool apply_sharpen(unsigned int input_texture, unsigned int output_texture,
                      float strength, float edge_threshold = 0.1f) {
        return g_device.apply_sharpen_effect(input_texture, output_texture, strength, edge_threshold);
    }

    bool apply_lut(unsigned int input_texture, unsigned int lut_texture,
                  unsigned int output_texture, float strength = 1.0f) {
        return g_device.apply_lut_effect(input_texture, lut_texture, output_texture, strength);
    }

    // Memory usage tracking
    void get_memory_usage(size_t* total, size_t* used, size_t* available) {
        g_device.get_memory_usage(total, used, available);
    }

    unsigned int create_shader_program(const char* vertex_src, const char* fragment_src) {
        return g_device.create_shader_program(vertex_src, fragment_src);
    }

    void destroy_shader_program(unsigned int program_id) {
        g_device.destroy_shader_program(program_id);
    }

    void use_shader_program(unsigned int program_id) {
        g_device.use_shader_program(program_id);
    }

    void set_uniform1f(unsigned int program_id, const char* name, float v) {
        g_device.set_uniform1f(program_id, name, v);
    }

    void set_uniform1i(unsigned int program_id, const char* name, int v) {
        g_device.set_uniform1i(program_id, name, v);
    }

    void clear(float r, float g, float b, float a) {
        g_device.clear(r, g, b, a);
    }

    void draw_texture(unsigned int texture_id, float x, float y, float width, float height) {
        g_device.draw_texture(texture_id, x, y, width, height);
    }

    void set_viewport(int width, int height) {
        g_device.set_viewport(width, height);
    }

    bool get_last_present_rgba(const void** data, int* width, int* height, int* /*stride*/) {
        int dummy_stride = 0;
        return g_device.get_last_present_rgba(data, width, height, &dummy_stride);
    }

private:
    bool created_ = false;
};

// GraphicsDevice implementation using PIMPL
GraphicsDevice::GraphicsDevice() : impl_(new Impl()), created_(false) {}
GraphicsDevice::~GraphicsDevice() { delete impl_; }

GraphicsDevice::GraphicsDevice(GraphicsDevice&& other) noexcept 
    : impl_(other.impl_), created_(other.created_) {
    other.impl_ = nullptr;
    other.created_ = false;
}

GraphicsDevice& GraphicsDevice::operator=(GraphicsDevice&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        created_ = other.created_;
        other.impl_ = nullptr;
        other.created_ = false;
    }
    return *this;
}

bool GraphicsDevice::create(const GraphicsDeviceInfo& info) noexcept {
    if (!impl_) return false;
    created_ = impl_->create(info);
    return created_;
}

void GraphicsDevice::destroy() noexcept {
    if (impl_) {
        impl_->destroy();
        created_ = false;
    }
}

bool GraphicsDevice::is_valid() const noexcept {
    return impl_ && impl_->is_valid() && created_;
}

unsigned int GraphicsDevice::create_texture(int width, int height, int format) noexcept {
    return impl_ ? impl_->create_texture(width, height, format) : 0;
}

void GraphicsDevice::destroy_texture(unsigned int texture_id) noexcept {
    if (impl_) impl_->destroy_texture(texture_id);
}

void GraphicsDevice::upload_texture(unsigned int texture_id, const void* data, int width, int height, int format) noexcept {
    if (impl_) impl_->upload_texture(texture_id, data, width, height, format);
}

unsigned int GraphicsDevice::create_shader_program(const char* vertex_src, const char* fragment_src) noexcept {
    return impl_ ? impl_->create_shader_program(vertex_src, fragment_src) : 0;
}

void GraphicsDevice::destroy_shader_program(unsigned int program_id) noexcept {
    if (impl_) impl_->destroy_shader_program(program_id);
}

void GraphicsDevice::use_shader_program(unsigned int program_id) noexcept {
    if (impl_) impl_->use_shader_program(program_id);
}

void GraphicsDevice::set_uniform1f(unsigned int program_id, const char* name, float v) noexcept {
    if (impl_) impl_->set_uniform1f(program_id, name, v);
}

void GraphicsDevice::set_uniform1i(unsigned int program_id, const char* name, int v) noexcept {
    if (impl_) impl_->set_uniform1i(program_id, name, v);
}

void GraphicsDevice::clear(float r, float g, float b, float a) noexcept {
    if (impl_) impl_->clear(r, g, b, a);
}

void GraphicsDevice::draw_texture(unsigned int texture_id, float x, float y, float width, float height) noexcept {
    if (impl_) impl_->draw_texture(texture_id, x, y, width, height);
}

void GraphicsDevice::set_viewport(int width, int height) noexcept {
    if (impl_) impl_->set_viewport(width, height);
}

bool GraphicsDevice::get_last_present_rgba(const void** data, int* width, int* height, int* stride) noexcept {
    return impl_ ? impl_->get_last_present_rgba(data, width, height, stride) : false;
}

// Buffer management methods (Week 2: Memory Management)
unsigned int GraphicsDevice::create_buffer(int size, int usage_flags, const void* initial_data) noexcept {
    return impl_ ? impl_->create_buffer(size, usage_flags, initial_data) : 0;
}

void GraphicsDevice::destroy_buffer(unsigned int buffer_id) noexcept {
    if (impl_) impl_->destroy_buffer(buffer_id);
}

void GraphicsDevice::upload_buffer(unsigned int buffer_id, const void* data, int size, int offset) noexcept {
    if (impl_) impl_->upload_buffer(buffer_id, data, size, offset);
}

void GraphicsDevice::get_memory_usage(size_t* total, size_t* used, size_t* available) noexcept {
    if (impl_) impl_->get_memory_usage(total, used, available);
}

// Week 6: Advanced Effects Shaders API
bool GraphicsDevice::create_effect_pipeline() noexcept {
    return impl_ ? impl_->create_effect_pipeline() : false;
}

bool GraphicsDevice::apply_color_correction(unsigned int input_texture, unsigned int output_texture,
                                           float brightness, float contrast, float saturation, float gamma) noexcept {
    return impl_ ? impl_->apply_color_correction(input_texture, output_texture, brightness, contrast, saturation, gamma) : false;
}

bool GraphicsDevice::apply_gaussian_blur(unsigned int input_texture, unsigned int intermediate_texture,
                                        unsigned int output_texture, float radius) noexcept {
    return impl_ ? impl_->apply_gaussian_blur(input_texture, intermediate_texture, output_texture, radius) : false;
}

bool GraphicsDevice::apply_sharpen(unsigned int input_texture, unsigned int output_texture,
                                  float strength, float edge_threshold) noexcept {
    return impl_ ? impl_->apply_sharpen(input_texture, output_texture, strength, edge_threshold) : false;
}

bool GraphicsDevice::apply_lut(unsigned int input_texture, unsigned int lut_texture,
                              unsigned int output_texture, float strength) noexcept {
    return impl_ ? impl_->apply_lut(input_texture, lut_texture, output_texture, strength) : false;
}

} // namespace ve::gfx
