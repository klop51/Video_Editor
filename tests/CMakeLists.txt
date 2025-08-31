# Tests root
find_package(Catch2 3 REQUIRED)

option(VE_ENABLE_QT_UI_TESTS "Enable Qt UI smoke tests" OFF)

set(UNIT_TEST_SOURCES
    test_time.cpp
    test_decoder.cpp
    test_frame_cache.cpp
    test_color_convert.cpp
    test_color_convert_guards.cpp
    test_playback_scheduler.cpp
    test_persistence.cpp
    test_timeline_basic.cpp
    test_timeline_invariants.cpp
    test_commands.cpp
    test_commands_macro_and_random.cpp
    test_timeline_additional_invariants.cpp
    test_command_coalescing.cpp
    test_profiling_stats.cpp
    test_project_roundtrip.cpp
    test_hdr_infrastructure.cpp  # Phase 2 Week 5: HDR Infrastructure tests
)

if(VE_ENABLE_QT_UI_TESTS)
    list(APPEND UNIT_TEST_SOURCES test_ui_smoke.cpp)
endif()

add_executable(unit_tests ${UNIT_TEST_SOURCES})

target_link_libraries(unit_tests PRIVATE VideoEditor::core VideoEditor::decode VideoEditor::timeline VideoEditor::persistence VideoEditor::commands VideoEditor::media_io Catch2::Catch2WithMain)

# Explicit include for commands until propagated include dirs confirmed across all generators
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/commands/include
)

if(VE_ENABLE_QT_UI_TESTS)
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
    target_link_libraries(unit_tests PRIVATE Qt6::Widgets VideoEditor::ui)
    target_compile_definitions(unit_tests PRIVATE VE_ENABLE_QT_UI_TESTS)
endif()

include(CTest)
add_test(NAME unit_tests COMMAND unit_tests)

# Minimal gfx header compile test (no Catch2) to ensure public headers stay clean
add_executable(test_gfx_headers test_gfx_headers.cpp)
target_link_libraries(test_gfx_headers PRIVATE ve_gfx)
add_test(NAME test_gfx_headers COMMAND test_gfx_headers)

# GPU System Comprehensive Test Suite
add_executable(gpu_test_suite 
    ${CMAKE_SOURCE_DIR}/src/gfx/gpu_test_suite.cpp
)
target_link_libraries(gpu_test_suite PRIVATE ve_gfx)
target_include_directories(gpu_test_suite PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gfx
    ${CMAKE_SOURCE_DIR}/src/gfx/include
)
add_test(NAME gpu_test_suite COMMAND gpu_test_suite)

# GPU System Production Validation
add_executable(gpu_system_validation 
    ${CMAKE_SOURCE_DIR}/src/gpu_system_validation.cpp
)
target_link_libraries(gpu_system_validation PRIVATE ve_gfx)
target_include_directories(gpu_system_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gfx
    ${CMAKE_SOURCE_DIR}/src/gfx/include
)
add_test(NAME gpu_system_validation COMMAND gpu_system_validation)

# Color Management Integration Validation Test
add_executable(color_management_validation_test 
    ${CMAKE_SOURCE_DIR}/color_management_validation_test.cpp
)
target_link_libraries(color_management_validation_test PRIVATE VideoEditor::media_io)
target_include_directories(color_management_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/media_io/include
    ${CMAKE_SOURCE_DIR}/src/core/include
)
add_test(NAME color_management_validation_test COMMAND color_management_validation_test)

# High Resolution 8K Support Validation Test
add_executable(high_resolution_validation_test 
    ${CMAKE_SOURCE_DIR}/high_resolution_validation_test.cpp
)
target_link_libraries(high_resolution_validation_test PRIVATE VideoEditor::media_io)
target_include_directories(high_resolution_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/media_io/include
    ${CMAKE_SOURCE_DIR}/src/core/include
)
add_test(NAME high_resolution_validation_test COMMAND high_resolution_validation_test)
