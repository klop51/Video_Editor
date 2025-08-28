add_library(ve_render STATIC
    src/render_graph.cpp
    src/nodes.cpp
    src/gpu_frame_resource.cpp
    src/shader_renderer.cpp
    src/gpu_render_graph.cpp
)

target_include_directories(ve_render
    PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/src/decode/include
        ${CMAKE_SOURCE_DIR}/src/gfx/include
)

# Link to core, decode, cache, and graphics modules
target_link_libraries(ve_render PUBLIC ve_core ve_decode ve_cache ve_gfx)
