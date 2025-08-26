add_library(ve_gfx STATIC
    src/vk_instance.cpp
    src/vk_device.cpp
)

# Consumers include this for GPU rendering abstraction
 target_include_directories(ve_gfx PUBLIC include)

# Vulkan dependency (optional for now). Try find; if not found, build with stub fallbacks.
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_compile_definitions(ve_gfx PUBLIC VE_GFX_VULKAN)
    target_link_libraries(ve_gfx PUBLIC Vulkan::Vulkan)
else()
    message(STATUS "Vulkan SDK not found - building gfx module in stub mode")
endif()

target_link_libraries(ve_gfx PUBLIC ve_core)
