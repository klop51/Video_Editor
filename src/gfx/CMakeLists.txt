add_library(ve_gfx STATIC
    src/vk_instance.cpp
    src/vk_device.cpp
)

# Consumers include this for GPU rendering abstraction
target_include_directories(ve_gfx PUBLIC include)

# Use C++17 for this module to avoid MSVC OpenGL header conflicts with C++20
set_target_properties(ve_gfx PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# OpenGL dependency (Qt provides OpenGL)
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
    target_compile_definitions(ve_gfx PUBLIC VE_GFX_OPENGL)
    target_link_libraries(ve_gfx PUBLIC OpenGL::GL)
    message(STATUS "OpenGL found - building gfx module with OpenGL support")
else()
    message(STATUS "OpenGL not found - building gfx module in stub mode")
endif()

target_link_libraries(ve_gfx PUBLIC ve_core)
