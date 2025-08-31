add_executable(test_d3d11_simple
    main.cpp
)

target_link_libraries(test_d3d11_simple 
    PRIVATE 
    ve_core 
    ve_gfx
)

set_target_properties(test_d3d11_simple PROPERTIES OUTPUT_NAME "ve_test_d3d11")
