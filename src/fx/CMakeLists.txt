add_library(ve_fx STATIC
    src/fx_placeholder.cpp
)

target_include_directories(ve_fx PUBLIC include)

target_link_libraries(ve_fx PUBLIC ve_render)
