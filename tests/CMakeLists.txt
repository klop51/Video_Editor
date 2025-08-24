# Tests root
find_package(Catch2 3 REQUIRED)

add_executable(unit_tests
    test_time.cpp
    test_decoder.cpp
    test_frame_cache.cpp
    test_color_convert.cpp
    test_playback_scheduler.cpp
)

target_link_libraries(unit_tests PRIVATE VideoEditor::core VideoEditor::decode Catch2::Catch2WithMain)

include(CTest)
add_test(NAME unit_tests COMMAND unit_tests)
