add_library(AssetDatabase STATIC asset_database.cpp)
target_include_directories(AssetDatabase PUBLIC ${ASSET_INCLUDE_DIR})
target_link_libraries(AssetDatabase PUBLIC AssetCore AssetMeta)
