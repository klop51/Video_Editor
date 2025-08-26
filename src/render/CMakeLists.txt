add_library(ve_render STATIC
    src/render_graph.cpp
    src/nodes.cpp
)

target_include_directories(ve_render
    PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/src/decode/include
)

# Placeholder: link to core for time/logging; future links: decode, fx, gpu backend
target_link_libraries(ve_render PUBLIC ve_core ve_decode ve_cache)
