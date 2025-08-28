add_executable(gpu_render_demo
    src/main.cpp
)

target_link_libraries(gpu_render_demo PRIVATE ve_core ve_decode ve_render ve_gfx)

set_target_properties(gpu_render_demo PROPERTIES OUTPUT_NAME "ve_gpu_render_demo")
