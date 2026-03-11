# Canonical asset writers (depend only on core)
set(WRITERS_SOURCES
    mesh_writer.cpp
    material_writer.cpp
    texture_writer.cpp
    animation_writer.cpp
    prefab_writer.cpp
    oea_writer.cpp
)
add_library(AssetPipeWriters STATIC ${WRITERS_SOURCES})
target_link_libraries(AssetPipeWriters PUBLIC AssetPipeCore)
target_include_directories(AssetPipeWriters PUBLIC ${ASSET_PIPE_INCLUDE_DIR})

if(TARGET zstd_static)
    target_link_libraries(AssetPipeWriters PRIVATE zstd_static)
    target_include_directories(AssetPipeWriters PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-src/lib")
    target_compile_definitions(AssetPipeWriters PRIVATE ASSET_PIPE_HAVE_ZSTD=1)
elseif(TARGET libzstd_static)
    target_link_libraries(AssetPipeWriters PRIVATE libzstd_static)
    target_include_directories(AssetPipeWriters PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-src/lib")
    target_compile_definitions(AssetPipeWriters PRIVATE ASSET_PIPE_HAVE_ZSTD=1)
elseif(TARGET zstd)
    target_link_libraries(AssetPipeWriters PRIVATE zstd)
    target_include_directories(AssetPipeWriters PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-src/lib")
    target_compile_definitions(AssetPipeWriters PRIVATE ASSET_PIPE_HAVE_ZSTD=1)
endif()
