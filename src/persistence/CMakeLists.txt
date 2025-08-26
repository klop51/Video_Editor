add_library(ve_persistence STATIC
    src/project_serializer.cpp
)

target_include_directories(ve_persistence PUBLIC include)

target_link_libraries(ve_persistence PUBLIC ve_core ve_timeline)

add_library(VideoEditor::persistence ALIAS ve_persistence)
