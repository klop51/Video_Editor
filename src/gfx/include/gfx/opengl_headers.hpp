#pragma once

// Minimal graphics header stub - no actual graphics API headers included
// This avoids MSVC header conflicts while providing interface stubs

// Graphics API stubs - replaced with simple constants/types for now  
#ifndef VE_GRAPHICS_STUB_TYPES
#define VE_GRAPHICS_STUB_TYPES

// Simple type aliases to avoid including actual graphics headers
using GfxDevice = void*;
using GfxContext = void*;  
using GfxTexture = void*;
using GfxShader = void*;

// Graphics result codes
constexpr int GFX_OK = 0;
constexpr int GFX_ERROR = -1;

#endif // VE_GRAPHICS_STUB_TYPES
