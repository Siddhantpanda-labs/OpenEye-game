# Asset pipeline core: shared types and format headers (no Assimp dependency)
add_library(AssetPipeCore INTERFACE)
# Root so that #include "asset_pipe/core/..." works
target_include_directories(AssetPipeCore INTERFACE ${CMAKE_SOURCE_DIR})
