#pragma once

#ifdef _WIN32
// Platform-specific D3D11 includes with proper C++ handling
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Forward declarations for D3D11 types
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct DXGI_FORMAT;

// D3D11 includes
#include <d3d11.h>
#include <dxgi.h>

#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif
