# Asset pipeline core: shared types, format headers, Guid (no Assimp dependency)
add_library(AssetPipeCore STATIC guid.cpp)
target_include_directories(AssetPipeCore PUBLIC ${CMAKE_SOURCE_DIR})
