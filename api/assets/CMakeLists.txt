# Eyelib Assets API

set(API_ASSETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AnimationLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AnimationLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AudioLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AudioLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MaterialLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MaterialLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MeshLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MeshLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/ShaderLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/ShaderLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/TextureLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/TextureLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibAssetsAPI STATIC ${API_ASSETS_SOURCES})

target_include_directories(EyelibAssetsAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibAssetsAPI
    PUBLIC
        EyelibAssets
)

