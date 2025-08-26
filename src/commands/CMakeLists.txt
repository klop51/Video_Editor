cmake_minimum_required(VERSION 3.20)

# Commands module
project(ve_commands)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include compiler warnings
include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)

# Create commands library
add_library(ve_commands STATIC
    # Headers
    include/commands/command.hpp
    include/commands/timeline_commands.hpp
    
    # Sources
    src/command.cpp
    src/timeline_commands.cpp
)

add_library(VideoEditor::commands ALIAS ve_commands)

# Include directories
target_include_directories(ve_commands
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core/include
        ${CMAKE_SOURCE_DIR}/src/timeline/include
)

# Link dependencies
target_link_libraries(ve_commands
    PUBLIC
        ve_core
        ve_timeline
)

# Set target properties
set_target_properties(ve_commands PROPERTIES
    FOLDER "Core"
    OUTPUT_NAME "ve_commands"
)

# Apply compiler warnings if the function exists
if(COMMAND set_project_warnings)
    set_project_warnings(ve_commands)
endif()

install(TARGETS ve_commands EXPORT VideoEditorTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
