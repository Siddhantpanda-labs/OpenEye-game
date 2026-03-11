add_library(AssetPipeMeta STATIC meta_file.cpp)
target_include_directories(AssetPipeMeta PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(AssetPipeMeta PUBLIC AssetPipeCore)
