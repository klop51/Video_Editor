cmake_minimum_required(VERSION 3.18)

# Simple HDR Infrastructure Test
add_executable(test_hdr_infrastructure_simple 
    test_hdr_infrastructure_simple.cpp
)

target_link_libraries(test_hdr_infrastructure_simple PRIVATE ve_media_io ve_core)

target_include_directories(test_hdr_infrastructure_simple PRIVATE
    ${CMAKE_SOURCE_DIR}/src/media_io/include
    ${CMAKE_SOURCE_DIR}/src/decode/include
    ${CMAKE_SOURCE_DIR}/src/core/include
)

# Add to tests
add_test(NAME test_hdr_infrastructure_simple COMMAND test_hdr_infrastructure_simple)
