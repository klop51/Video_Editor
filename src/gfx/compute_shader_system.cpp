// Week 10: Compute Shader System Implementation
// Advanced GPU compute capabilities for sophisticated video processing

#include "compute_shader_system.hpp"
#include "core/logging.hpp"
#include "core/profiling.hpp"
#include <d3dcompiler.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

namespace VideoEditor::GFX {

// =============================================================================
// Compute Buffer Implementation
// =============================================================================

Core::Result<void> ComputeBuffer::create(GraphicsDevice* device, const ComputeBufferDesc& desc) {
    try {
        device_ = device;
        desc_ = desc;
        
        auto d3d_device = device->get_d3d_device();
        if (!d3d_device) {
            return Core::Result<void>::error("Invalid D3D device");
        }

        d3d_device->GetImmediateContext(context_.GetAddressOf());

        // Calculate buffer size
        size_t buffer_size = desc.element_size * desc.element_count;
        if (buffer_size == 0) {
            return Core::Result<void>::error("Buffer size cannot be zero");
        }

        // Create main buffer
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = static_cast<UINT>(buffer_size);
        buffer_desc.StructureByteStride = static_cast<UINT>(desc.element_size);
        
        switch (desc.usage) {
            case ComputeBufferUsage::DEFAULT:
                buffer_desc.Usage = D3D11_USAGE_DEFAULT;
                buffer_desc.CPUAccessFlags = 0;
                break;
            case ComputeBufferUsage::IMMUTABLE:
                buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
                buffer_desc.CPUAccessFlags = 0;
                break;
            case ComputeBufferUsage::DYNAMIC:
                buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
                buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                break;
            case ComputeBufferUsage::STAGING:
                buffer_desc.Usage = D3D11_USAGE_STAGING;
                buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                break;
        }

        if (desc.allow_raw_views) {
            buffer_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        }
        
        if (desc.allow_unordered_access) {
            buffer_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }
        
        if (desc.usage != ComputeBufferUsage::STAGING) {
            buffer_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if (!desc.allow_raw_views) {
                buffer_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            }
        }

        HRESULT hr = d3d_device->CreateBuffer(&buffer_desc, nullptr, buffer_.GetAddressOf());
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create compute buffer: " + std::to_string(hr));
        }

        // Create staging buffer if needed for CPU access
        if (desc.cpu_accessible && desc.usage != ComputeBufferUsage::STAGING) {
            D3D11_BUFFER_DESC staging_desc = buffer_desc;
            staging_desc.Usage = D3D11_USAGE_STAGING;
            staging_desc.BindFlags = 0;
            staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            
            hr = d3d_device->CreateBuffer(&staging_desc, nullptr, staging_buffer_.GetAddressOf());
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to create staging buffer: " + std::to_string(hr));
            }
        }

        // Create shader resource view
        if (buffer_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            if (desc.allow_raw_views) {
                srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
                srv_desc.BufferEx.FirstElement = 0;
                srv_desc.BufferEx.NumElements = static_cast<UINT>(buffer_size / 4);
                srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
            } else {
                srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                srv_desc.Buffer.FirstElement = 0;
                srv_desc.Buffer.NumElements = static_cast<UINT>(desc.element_count);
            }

            hr = d3d_device->CreateShaderResourceView(buffer_.Get(), &srv_desc, srv_.GetAddressOf());
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to create SRV: " + std::to_string(hr));
            }
        }

        // Create unordered access view
        if (desc.allow_unordered_access) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            if (desc.allow_raw_views) {
                uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                uav_desc.Buffer.FirstElement = 0;
                uav_desc.Buffer.NumElements = static_cast<UINT>(buffer_size / 4);
                uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
            } else {
                uav_desc.Format = DXGI_FORMAT_UNKNOWN;
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                uav_desc.Buffer.FirstElement = 0;
                uav_desc.Buffer.NumElements = static_cast<UINT>(desc.element_count);
            }

            hr = d3d_device->CreateUnorderedAccessView(buffer_.Get(), &uav_desc, uav_.GetAddressOf());
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to create UAV: " + std::to_string(hr));
            }
        }

        Core::Logger::info("ComputeBuffer", "Created buffer: {} elements, {} bytes per element, {} total bytes",
                          desc.element_count, desc.element_size, buffer_size);

        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in ComputeBuffer::create: " + std::string(e.what()));
    }
}

Core::Result<void> ComputeBuffer::upload_data(const void* data, size_t size) {
    if (!buffer_ || !context_) {
        return Core::Result<void>::error("Buffer not initialized");
    }

    if (size > get_total_size()) {
        return Core::Result<void>::error("Data size exceeds buffer capacity");
    }

    try {
        if (desc_.usage == ComputeBufferUsage::DYNAMIC) {
            // Map dynamic buffer directly
            D3D11_MAPPED_SUBRESOURCE mapped_resource;
            HRESULT hr = context_->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to map dynamic buffer");
            }

            std::memcpy(mapped_resource.pData, data, size);
            context_->Unmap(buffer_.Get(), 0);
        }
        else if (staging_buffer_) {
            // Use staging buffer for copy
            D3D11_MAPPED_SUBRESOURCE mapped_resource;
            HRESULT hr = context_->Map(staging_buffer_.Get(), 0, D3D11_MAP_WRITE, 0, &mapped_resource);
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to map staging buffer");
            }

            std::memcpy(mapped_resource.pData, data, size);
            context_->Unmap(staging_buffer_.Get(), 0);
            context_->CopyResource(buffer_.Get(), staging_buffer_.Get());
        }
        else {
            // Use UpdateSubresource for default buffers
            context_->UpdateSubresource(buffer_.Get(), 0, nullptr, data, 0, 0);
        }

        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in upload_data: " + std::string(e.what()));
    }
}

Core::Result<void> ComputeBuffer::download_data(void* data, size_t size) {
    if (!buffer_ || !context_) {
        return Core::Result<void>::error("Buffer not initialized");
    }

    if (size > get_total_size()) {
        return Core::Result<void>::error("Data size exceeds buffer capacity");
    }

    try {
        Microsoft::WRL::ComPtr<ID3D11Buffer> read_buffer;
        
        if (desc_.usage == ComputeBufferUsage::STAGING) {
            read_buffer = buffer_;
        }
        else if (staging_buffer_) {
            context_->CopyResource(staging_buffer_.Get(), buffer_.Get());
            read_buffer = staging_buffer_;
        }
        else {
            // Create temporary staging buffer
            D3D11_BUFFER_DESC staging_desc;
            buffer_->GetDesc(&staging_desc);
            staging_desc.Usage = D3D11_USAGE_STAGING;
            staging_desc.BindFlags = 0;
            staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging_desc.MiscFlags = 0;

            auto d3d_device = device_->get_d3d_device();
            HRESULT hr = d3d_device->CreateBuffer(&staging_desc, nullptr, read_buffer.GetAddressOf());
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to create temporary staging buffer");
            }

            context_->CopyResource(read_buffer.Get(), buffer_.Get());
        }

        // Map and read data
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        HRESULT hr = context_->Map(read_buffer.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource);
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to map buffer for reading");
        }

        std::memcpy(data, mapped_resource.pData, size);
        context_->Unmap(read_buffer.Get(), 0);

        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in download_data: " + std::string(e.what()));
    }
}

void ComputeBuffer::release() {
    uav_.Reset();
    srv_.Reset();
    staging_buffer_.Reset();
    buffer_.Reset();
    context_.Reset();
    device_ = nullptr;
}

// =============================================================================
// Compute Texture Implementation
// =============================================================================

Core::Result<void> ComputeTexture::create_2d(GraphicsDevice* device, uint32_t width, uint32_t height, 
                                             DXGI_FORMAT format, bool allow_uav) {
    try {
        auto d3d_device = device->get_d3d_device();
        if (!d3d_device) {
            return Core::Result<void>::error("Invalid D3D device");
        }

        width_ = width;
        height_ = height;
        depth_ = 1;
        format_ = format;
        is_3d_ = false;

        // Create 2D texture
        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = width;
        tex_desc.Height = height;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = format;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage = D3D11_USAGE_DEFAULT;
        tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        if (allow_uav) {
            tex_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }

        HRESULT hr = d3d_device->CreateTexture2D(&tex_desc, nullptr, texture_2d_.GetAddressOf());
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create 2D texture: " + std::to_string(hr));
        }

        // Create SRV
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.MostDetailedMip = 0;

        hr = d3d_device->CreateShaderResourceView(texture_2d_.Get(), &srv_desc, srv_.GetAddressOf());
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create texture SRV: " + std::to_string(hr));
        }

        // Create UAV if requested
        if (allow_uav) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            uav_desc.Format = format;
            uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Texture2D.MipSlice = 0;

            hr = d3d_device->CreateUnorderedAccessView(texture_2d_.Get(), &uav_desc, uav_.GetAddressOf());
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to create texture UAV: " + std::to_string(hr));
            }
        }

        Core::Logger::info("ComputeTexture", "Created 2D texture: {}x{}, format: {}", width, height, format);
        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in create_2d: " + std::string(e.what()));
    }
}

Core::Result<void> ComputeTexture::create_3d(GraphicsDevice* device, uint32_t width, uint32_t height, 
                                             uint32_t depth, DXGI_FORMAT format, bool allow_uav) {
    try {
        auto d3d_device = device->get_d3d_device();
        if (!d3d_device) {
            return Core::Result<void>::error("Invalid D3D device");
        }

        width_ = width;
        height_ = height;
        depth_ = depth;
        format_ = format;
        is_3d_ = true;

        // Create 3D texture
        D3D11_TEXTURE3D_DESC tex_desc = {};
        tex_desc.Width = width;
        tex_desc.Height = height;
        tex_desc.Depth = depth;
        tex_desc.MipLevels = 1;
        tex_desc.Format = format;
        tex_desc.Usage = D3D11_USAGE_DEFAULT;
        tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        if (allow_uav) {
            tex_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }

        HRESULT hr = d3d_device->CreateTexture3D(&tex_desc, nullptr, texture_3d_.GetAddressOf());
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create 3D texture: " + std::to_string(hr));
        }

        // Create SRV
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        srv_desc.Texture3D.MipLevels = 1;
        srv_desc.Texture3D.MostDetailedMip = 0;

        hr = d3d_device->CreateShaderResourceView(texture_3d_.Get(), &srv_desc, srv_.GetAddressOf());
        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create 3D texture SRV: " + std::to_string(hr));
        }

        // Create UAV if requested
        if (allow_uav) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            uav_desc.Format = format;
            uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
            uav_desc.Texture3D.MipSlice = 0;
            uav_desc.Texture3D.FirstWSlice = 0;
            uav_desc.Texture3D.WSize = depth;

            hr = d3d_device->CreateUnorderedAccessView(texture_3d_.Get(), &uav_desc, uav_.GetAddressOf());
            if (FAILED(hr)) {
                return Core::Result<void>::error("Failed to create 3D texture UAV: " + std::to_string(hr));
            }
        }

        Core::Logger::info("ComputeTexture", "Created 3D texture: {}x{}x{}, format: {}", width, height, depth, format);
        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in create_3d: " + std::string(e.what()));
    }
}

void ComputeTexture::release() {
    uav_.Reset();
    srv_.Reset();
    texture_3d_.Reset();
    texture_2d_.Reset();
    width_ = height_ = depth_ = 0;
    format_ = DXGI_FORMAT_UNKNOWN;
    is_3d_ = false;
}

// =============================================================================
// Compute Shader Implementation
// =============================================================================

Core::Result<void> ComputeShader::create_from_source(GraphicsDevice* device, const ComputeShaderDesc& desc) {
    try {
        device_ = device;
        entry_point_ = desc.entry_point;
        defines_ = desc.defines;

        auto d3d_device = device->get_d3d_device();
        if (!d3d_device) {
            return Core::Result<void>::error("Invalid D3D device");
        }

        d3d_device->GetImmediateContext(context_.GetAddressOf());

        // Compile shader
        auto result = compile_shader(desc.shader_source, desc);
        if (!result.is_success()) {
            return result;
        }

        // Create timestamp queries for performance measurement
        D3D11_QUERY_DESC query_desc = {};
        query_desc.Query = D3D11_QUERY_TIMESTAMP;
        
        HRESULT hr = d3d_device->CreateQuery(&query_desc, timestamp_start_.GetAddressOf());
        if (FAILED(hr)) {
            Core::Logger::warning("ComputeShader", "Failed to create start timestamp query");
        }

        hr = d3d_device->CreateQuery(&query_desc, timestamp_end_.GetAddressOf());
        if (FAILED(hr)) {
            Core::Logger::warning("ComputeShader", "Failed to create end timestamp query");
        }

        query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        hr = d3d_device->CreateQuery(&query_desc, timestamp_disjoint_.GetAddressOf());
        if (FAILED(hr)) {
            Core::Logger::warning("ComputeShader", "Failed to create disjoint timestamp query");
        }

        // Initialize resource binding arrays
        bound_constant_buffers_.resize(16);
        bound_srvs_.resize(16);
        bound_uavs_.resize(8);

        Core::Logger::info("ComputeShader", "Created compute shader: {}", desc.entry_point);
        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in create_from_source: " + std::string(e.what()));
    }
}

Core::Result<void> ComputeShader::create_from_file(GraphicsDevice* device, const std::string& file_path,
                                                   const std::string& entry_point) {
    try {
        // Read shader file
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return Core::Result<void>::error("Failed to open shader file: " + file_path);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string shader_source = buffer.str();

        ComputeShaderDesc desc;
        desc.shader_source = shader_source;
        desc.entry_point = entry_point;

        return create_from_source(device, desc);

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in create_from_file: " + std::string(e.what()));
    }
}

Core::Result<void> ComputeShader::compile_shader(const std::string& source, const ComputeShaderDesc& desc) {
    try {
        // Prepare defines
        std::vector<D3D_SHADER_MACRO> macros;
        for (const auto& define : desc.defines) {
            D3D_SHADER_MACRO macro;
            macro.Name = define.c_str();
            macro.Definition = "1";
            macros.push_back(macro);
        }
        macros.push_back({nullptr, nullptr}); // Terminator

        // Compile flags
        UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
        if (desc.enable_debug) {
            compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        }

        Microsoft::WRL::ComPtr<ID3DBlob> shader_blob;
        Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

        HRESULT hr = D3DCompile(
            source.c_str(),
            source.length(),
            nullptr, // Source name
            macros.data(),
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            desc.entry_point.c_str(),
            desc.shader_model.c_str(),
            compile_flags,
            0,
            shader_blob.GetAddressOf(),
            error_blob.GetAddressOf()
        );

        if (FAILED(hr)) {
            std::string error_msg = "Shader compilation failed";
            if (error_blob) {
                error_msg += ": " + std::string(static_cast<char*>(error_blob->GetBufferPointer()), 
                                               error_blob->GetBufferSize());
            }
            return Core::Result<void>::error(error_msg);
        }

        // Create compute shader
        auto d3d_device = device_->get_d3d_device();
        hr = d3d_device->CreateComputeShader(
            shader_blob->GetBufferPointer(),
            shader_blob->GetBufferSize(),
            nullptr,
            shader_.GetAddressOf()
        );

        if (FAILED(hr)) {
            return Core::Result<void>::error("Failed to create compute shader object: " + std::to_string(hr));
        }

        return Core::Result<void>::success();

    } catch (const std::exception& e) {
        return Core::Result<void>::error("Exception in compile_shader: " + std::string(e.what()));
    }
}

void ComputeShader::bind_constant_buffer(uint32_t slot, ComputeBuffer* buffer) {
    if (slot < bound_constant_buffers_.size() && buffer) {
        bound_constant_buffers_[slot] = buffer->get_buffer();
    }
}

void ComputeShader::bind_structured_buffer(uint32_t slot, ComputeBuffer* buffer) {
    if (slot < bound_srvs_.size() && buffer) {
        bound_srvs_[slot] = buffer->get_srv();
    }
}

void ComputeShader::bind_texture_srv(uint32_t slot, ComputeTexture* texture) {
    if (slot < bound_srvs_.size() && texture) {
        bound_srvs_[slot] = texture->get_srv();
    }
}

void ComputeShader::bind_texture_uav(uint32_t slot, ComputeTexture* texture) {
    if (slot < bound_uavs_.size() && texture) {
        bound_uavs_[slot] = texture->get_uav();
    }
}

void ComputeShader::bind_buffer_uav(uint32_t slot, ComputeBuffer* buffer) {
    if (slot < bound_uavs_.size() && buffer) {
        bound_uavs_[slot] = buffer->get_uav();
    }
}

Core::Result<ComputePerformanceMetrics> ComputeShader::dispatch(const ComputeDispatchParams& params) {
    if (!shader_ || !context_) {
        return Core::Result<ComputePerformanceMetrics>::error("Shader not initialized");
    }

    try {
        // Set compute shader
        context_->CSSetShader(shader_.Get(), nullptr, 0);

        // Bind constant buffers
        std::vector<ID3D11Buffer*> cb_ptrs;
        for (auto& cb : bound_constant_buffers_) {
            cb_ptrs.push_back(cb.Get());
        }
        if (!cb_ptrs.empty()) {
            context_->CSSetConstantBuffers(0, static_cast<UINT>(cb_ptrs.size()), cb_ptrs.data());
        }

        // Bind shader resource views
        std::vector<ID3D11ShaderResourceView*> srv_ptrs;
        for (auto& srv : bound_srvs_) {
            srv_ptrs.push_back(srv.Get());
        }
        if (!srv_ptrs.empty()) {
            context_->CSSetShaderResources(0, static_cast<UINT>(srv_ptrs.size()), srv_ptrs.data());
        }

        // Bind unordered access views
        std::vector<ID3D11UnorderedAccessView*> uav_ptrs;
        for (auto& uav : bound_uavs_) {
            uav_ptrs.push_back(uav.Get());
        }
        if (!uav_ptrs.empty()) {
            context_->CSSetUnorderedAccessViews(0, static_cast<UINT>(uav_ptrs.size()), 
                                               uav_ptrs.data(), nullptr);
        }

        // Start performance measurement
        auto start_time = std::chrono::high_resolution_clock::now();
        if (timestamp_disjoint_ && timestamp_start_) {
            context_->Begin(timestamp_disjoint_.Get());
            context_->End(timestamp_start_.Get());
        }

        // Dispatch compute shader
        context_->Dispatch(params.thread_groups_x, params.thread_groups_y, params.thread_groups_z);

        // End performance measurement
        if (timestamp_end_ && timestamp_disjoint_) {
            context_->End(timestamp_end_.Get());
            context_->End(timestamp_disjoint_.Get());
        }
        auto end_time = std::chrono::high_resolution_clock::now();

        // Clear bindings for safety
        clear_bindings();

        // Calculate performance metrics
        ComputePerformanceMetrics metrics = measure_performance(params);
        metrics.dispatch_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

        return Core::Result<ComputePerformanceMetrics>::success(metrics);

    } catch (const std::exception& e) {
        return Core::Result<ComputePerformanceMetrics>::error("Exception in dispatch: " + std::string(e.what()));
    }
}

Core::Result<ComputePerformanceMetrics> ComputeShader::dispatch_1d(uint32_t num_elements, uint32_t threads_per_group) {
    ComputeDispatchParams params;
    params.thread_groups_x = (num_elements + threads_per_group - 1) / threads_per_group;
    params.thread_groups_y = 1;
    params.thread_groups_z = 1;
    params.threads_per_group_x = threads_per_group;
    params.threads_per_group_y = 1;
    params.threads_per_group_z = 1;
    
    return dispatch(params);
}

Core::Result<ComputePerformanceMetrics> ComputeShader::dispatch_2d(uint32_t width, uint32_t height, 
                                                                   uint32_t threads_x, uint32_t threads_y) {
    ComputeDispatchParams params;
    params.thread_groups_x = (width + threads_x - 1) / threads_x;
    params.thread_groups_y = (height + threads_y - 1) / threads_y;
    params.thread_groups_z = 1;
    params.threads_per_group_x = threads_x;
    params.threads_per_group_y = threads_y;
    params.threads_per_group_z = 1;
    
    return dispatch(params);
}

Core::Result<ComputePerformanceMetrics> ComputeShader::dispatch_3d(uint32_t width, uint32_t height, uint32_t depth,
                                                                   uint32_t threads_x, uint32_t threads_y, uint32_t threads_z) {
    ComputeDispatchParams params;
    params.thread_groups_x = (width + threads_x - 1) / threads_x;
    params.thread_groups_y = (height + threads_y - 1) / threads_y;
    params.thread_groups_z = (depth + threads_z - 1) / threads_z;
    params.threads_per_group_x = threads_x;
    params.threads_per_group_y = threads_y;
    params.threads_per_group_z = threads_z;
    
    return dispatch(params);
}

void ComputeShader::clear_bindings() {
    if (!context_) return;

    // Clear constant buffers
    std::vector<ID3D11Buffer*> null_cbs(bound_constant_buffers_.size(), nullptr);
    context_->CSSetConstantBuffers(0, static_cast<UINT>(null_cbs.size()), null_cbs.data());

    // Clear SRVs
    std::vector<ID3D11ShaderResourceView*> null_srvs(bound_srvs_.size(), nullptr);
    context_->CSSetShaderResources(0, static_cast<UINT>(null_srvs.size()), null_srvs.data());

    // Clear UAVs
    std::vector<ID3D11UnorderedAccessView*> null_uavs(bound_uavs_.size(), nullptr);
    context_->CSSetUnorderedAccessViews(0, static_cast<UINT>(null_uavs.size()), null_uavs.data(), nullptr);

    // Clear shader
    context_->CSSetShader(nullptr, nullptr, 0);
}

ComputePerformanceMetrics ComputeShader::measure_performance(const ComputeDispatchParams& params) {
    ComputePerformanceMetrics metrics = {};
    
    if (!timestamp_disjoint_ || !timestamp_start_ || !timestamp_end_) {
        return metrics;
    }

    try {
        // Wait for GPU to complete
        while (context_->GetData(timestamp_disjoint_.Get(), nullptr, 0, 0) == S_FALSE) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        // Get timing data
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
        UINT64 start_time, end_time;

        if (SUCCEEDED(context_->GetData(timestamp_disjoint_.Get(), &disjoint_data, sizeof(disjoint_data), 0)) &&
            !disjoint_data.Disjoint &&
            SUCCEEDED(context_->GetData(timestamp_start_.Get(), &start_time, sizeof(start_time), 0)) &&
            SUCCEEDED(context_->GetData(timestamp_end_.Get(), &end_time, sizeof(end_time), 0))) {
            
            metrics.gpu_execution_time_ms = static_cast<float>(end_time - start_time) / 
                                          (disjoint_data.Frequency / 1000.0f);
        }

        // Calculate derived metrics
        metrics.active_thread_groups = params.thread_groups_x * params.thread_groups_y * params.thread_groups_z;
        uint64_t total_threads = static_cast<uint64_t>(metrics.active_thread_groups) *
                                params.threads_per_group_x * params.threads_per_group_y * params.threads_per_group_z;
        
        if (metrics.gpu_execution_time_ms > 0.0f) {
            metrics.operations_per_second = static_cast<uint64_t>(total_threads / (metrics.gpu_execution_time_ms / 1000.0f));
        }

    } catch (...) {
        // Ignore timing errors
    }

    return metrics;
}

void ComputeShader::release() {
    clear_bindings();
    timestamp_disjoint_.Reset();
    timestamp_end_.Reset();
    timestamp_start_.Reset();
    bound_uavs_.clear();
    bound_srvs_.clear();
    bound_constant_buffers_.clear();
    shader_.Reset();
    context_.Reset();
    device_ = nullptr;
}

// =============================================================================
// Compute Utils Implementation
// =============================================================================

namespace ComputeUtils {

ComputeDispatchParams calculate_dispatch_params(uint32_t width, uint32_t height, uint32_t depth,
                                               uint32_t preferred_group_size_x,
                                               uint32_t preferred_group_size_y,
                                               uint32_t preferred_group_size_z) {
    ComputeDispatchParams params;
    
    params.threads_per_group_x = preferred_group_size_x;
    params.threads_per_group_y = preferred_group_size_y;
    params.threads_per_group_z = preferred_group_size_z;
    
    params.thread_groups_x = (width + preferred_group_size_x - 1) / preferred_group_size_x;
    params.thread_groups_y = (height + preferred_group_size_y - 1) / preferred_group_size_y;
    params.thread_groups_z = (depth + preferred_group_size_z - 1) / preferred_group_size_z;
    
    return params;
}

size_t calculate_shared_memory_size(ComputeDataType data_type, uint32_t element_count) {
    size_t element_size = 0;
    
    switch (data_type) {
        case ComputeDataType::FLOAT:
        case ComputeDataType::INT:
        case ComputeDataType::UINT:
            element_size = 4;
            break;
        case ComputeDataType::FLOAT2:
        case ComputeDataType::INT2:
        case ComputeDataType::UINT2:
            element_size = 8;
            break;
        case ComputeDataType::FLOAT3:
        case ComputeDataType::INT3:
        case ComputeDataType::UINT3:
            element_size = 12;
            break;
        case ComputeDataType::FLOAT4:
        case ComputeDataType::INT4:
        case ComputeDataType::UINT4:
            element_size = 16;
            break;
    }
    
    return element_size * element_count;
}

bool validate_dispatch_params(const ComputeDispatchParams& params, 
                             const ComputeShaderSystem::ComputeCapabilities& caps) {
    return params.thread_groups_x <= caps.max_thread_groups_x &&
           params.thread_groups_y <= caps.max_thread_groups_y &&
           params.thread_groups_z <= caps.max_thread_groups_z &&
           (params.threads_per_group_x * params.threads_per_group_y * params.threads_per_group_z) <= caps.max_threads_per_group;
}

float estimate_execution_time_ms(uint32_t operations, float gpu_gflops) {
    if (gpu_gflops <= 0.0f) return 0.0f;
    return (operations / (gpu_gflops * 1e9f)) * 1000.0f;
}

float estimate_memory_bandwidth_gb_s(size_t data_size_bytes, float execution_time_ms) {
    if (execution_time_ms <= 0.0f) return 0.0f;
    return (data_size_bytes / (1024.0f * 1024.0f * 1024.0f)) / (execution_time_ms / 1000.0f);
}

} // namespace ComputeUtils

} // namespace VideoEditor::GFX
