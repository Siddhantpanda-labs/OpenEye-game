add_library(AssetRuntime STATIC asset_manager.cpp)
target_include_directories(AssetRuntime PUBLIC ${ASSET_INCLUDE_DIR})
target_link_libraries(AssetRuntime PUBLIC AssetDatabase AssetReaders)
