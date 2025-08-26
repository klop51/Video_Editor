# Core module
add_library(ve_core STATIC
    src/log.cpp
    src/time.cpp
    src/profiling.cpp
    src/job_system.cpp
)

add_library(VideoEditor::core ALIAS ve_core)
add_library(core ALIAS ve_core)

target_include_directories(ve_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ve_core
    PRIVATE
        $<$<CONFIG:Debug>:VE_DEBUG>
)

include(FetchContent)

# Try to locate via package manager (vcpkg / system). Use CONFIG to prefer CMake package configs.
find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND)
    message(STATUS "fmt not found; fetching via FetchContent")
    FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.0.2
    )
    FetchContent_MakeAvailable(fmt)
endif()

# spdlog can use external fmt; ensure fmt is available first
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt" FORCE)
find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found; fetching via FetchContent")
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

target_link_libraries(ve_core PUBLIC fmt::fmt spdlog::spdlog)

install(TARGETS ve_core EXPORT VideoEditorTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
