add_library(ve_cache STATIC
    src/cache_placeholder.cpp
)

target_include_directories(ve_cache
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/decode/include>
)

target_link_libraries(ve_cache PUBLIC ve_core ve_decode)

# Provide consistent alias target
add_library(VideoEditor::cache ALIAS ve_cache)
