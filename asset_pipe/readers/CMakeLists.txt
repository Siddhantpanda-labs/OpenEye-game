add_library(AssetPipeReaders STATIC mesh_reader.cpp material_reader.cpp prefab_reader.cpp texture_reader.cpp oea_reader.cpp)
target_link_libraries(AssetPipeReaders PUBLIC AssetPipeCore)
target_include_directories(AssetPipeReaders PUBLIC ${CMAKE_SOURCE_DIR})
