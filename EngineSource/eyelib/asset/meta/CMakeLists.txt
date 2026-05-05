add_library(AssetMeta STATIC meta_file.cpp)
target_include_directories(AssetMeta PUBLIC ${ASSET_INCLUDE_DIR})
target_link_libraries(AssetMeta PUBLIC AssetCore)
