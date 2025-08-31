add_library(ve_gfx STATIC
    src/vk_instance.cpp
    src/vk_device.cpp
    graphics_device_bridge.cpp
)

# Consumers include this for GPU rendering abstraction
target_include_directories(ve_gfx PUBLIC include)

# Use C++17 for this module to avoid MSVC OpenGL header conflicts with C++20
set_target_properties(ve_gfx PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# DirectX 11 support (Windows only)
if(WIN32)
    target_compile_definitions(ve_gfx PUBLIC VE_GFX_D3D11)
    # DirectX libraries are linked via #pragma comment in the source file
    message(STATUS "Building gfx module with DirectX 11 support")
else()
    message(STATUS "DirectX 11 not available on this platform")
endif()

# OpenGL dependency (Qt provides OpenGL) - keep as fallback
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
    target_compile_definitions(ve_gfx PUBLIC VE_GFX_OPENGL)
    target_link_libraries(ve_gfx PUBLIC OpenGL::GL)
    message(STATUS "OpenGL found - gfx module has OpenGL fallback support")
else()
    message(STATUS "OpenGL not found")
endif()

target_link_libraries(ve_gfx PUBLIC ve_core)
