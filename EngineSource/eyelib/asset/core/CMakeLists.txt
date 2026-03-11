# Asset pipeline core: shared types, format headers, Guid (no Assimp dependency)
add_library(AssetCore STATIC guid.cpp)
target_include_directories(AssetCore PUBLIC ${ASSET_INCLUDE_DIR})
