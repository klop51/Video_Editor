cmake_minimum_required(VERSION 3.25)

# Only build app if Qt is available and enabled
if(NOT ENABLE_QT_TOOLS)
    message(STATUS "Skipping app module: ENABLE_QT_TOOLS is OFF")
    return()
endif()

# Find Qt6
find_package(Qt6 QUIET COMPONENTS Core Widgets)
if(NOT Qt6_FOUND)
    message(WARNING "Qt6 not found, skipping app module")
    return()
endif()

# Enable Qt's MOC for this module
set(CMAKE_AUTOMOC ON)

# Application module
add_library(ve_app
    # Source files
    src/application.cpp
    
    # Headers with Q_OBJECT (for MOC processing)
    include/app/application.hpp
)

target_include_directories(ve_app
    PUBLIC include
    PRIVATE src
    PRIVATE ${CMAKE_SOURCE_DIR}/src/core/include
)

target_link_libraries(ve_app
    PUBLIC
        ve_core
        ve_timeline
        ve_playback
        ve_ui
        Qt6::Core
        Qt6::Widgets
)

target_compile_features(ve_app PUBLIC cxx_std_20)

# Qt specific settings
# Note: Removed QT_NO_KEYWORDS to allow Q_OBJECT and slots/signals macros
# Disable warnings as errors for Qt deprecated methods
if(MSVC)
    target_compile_options(ve_app PRIVATE /WX-)
endif()

# Create alias for consistency
add_library(app ALIAS ve_app)
