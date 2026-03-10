# Canonical asset writers (depend only on core)
set(WRITERS_SOURCES
    mesh_writer.cpp
    material_writer.cpp
    texture_writer.cpp
    animation_writer.cpp
    prefab_writer.cpp
)
add_library(AssetPipeWriters STATIC ${WRITERS_SOURCES})
target_link_libraries(AssetPipeWriters PUBLIC AssetPipeCore)
target_include_directories(AssetPipeWriters PUBLIC ${CMAKE_SOURCE_DIR})
