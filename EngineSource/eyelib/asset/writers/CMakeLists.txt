# Canonical asset writers (MaterialWriter uses MaterialReader::EncodeToBuffer)
set(WRITERS_SOURCES
    mesh_writer.cpp
    material_writer.cpp
    texture_writer.cpp
    animation_writer.cpp
    prefab_writer.cpp
    oea_writer.cpp
)
add_library(AssetWriters STATIC ${WRITERS_SOURCES})
target_link_libraries(AssetWriters PUBLIC AssetCore AssetReaders)
target_include_directories(AssetWriters PUBLIC ${ASSET_INCLUDE_DIR})

if(TARGET zstd_static)
    target_link_libraries(AssetWriters PRIVATE zstd_static)
    target_include_directories(AssetWriters PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-src/lib")
    target_compile_definitions(AssetWriters PRIVATE ASSET_HAVE_ZSTD=1)
elseif(TARGET libzstd_static)
    target_link_libraries(AssetWriters PRIVATE libzstd_static)
    target_include_directories(AssetWriters PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-src/lib")
    target_compile_definitions(AssetWriters PRIVATE ASSET_HAVE_ZSTD=1)
elseif(TARGET zstd)
    target_link_libraries(AssetWriters PRIVATE zstd)
    target_include_directories(AssetWriters PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-src/lib")
    target_compile_definitions(AssetWriters PRIVATE ASSET_HAVE_ZSTD=1)
endif()
