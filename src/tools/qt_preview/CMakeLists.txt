find_package(Qt6 COMPONENTS Widgets QUIET)

if(NOT Qt6_FOUND)
    message(WARNING "Qt6 not found, skipping qt_preview")
    return()
endif()

qt_standard_project_setup()

qt_add_executable(qt_preview
    src/main.cpp
    src/player_window.cpp
)

target_link_libraries(qt_preview PRIVATE Qt6::Widgets ve_decode ve_media_io ve_core)

set_target_properties(qt_preview PROPERTIES OUTPUT_NAME "ve_qt_preview")
