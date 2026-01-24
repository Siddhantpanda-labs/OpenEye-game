# Eyelib Assets Component

set(ASSETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetId.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioClip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationAsset.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamingManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compression/AssetCompression.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compression/AssetCompression.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AnimationLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AnimationLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AudioLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/AudioLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MeshLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MeshLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/ShaderLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/ShaderLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/TextureLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/TextureLoader.h
)

add_library(EyelibAssets STATIC ${ASSETS_SOURCES})

target_include_directories(EyelibAssets
    PUBLIC
        # Allow includes like "eyelib/assets/AssetRegistry.h"
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/include
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
        ${CMAKE_SOURCE_DIR}/thirdparty/stb
)

target_link_libraries(EyelibAssets
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibRender
        EyelibAnimation
)

if(WIN32)
    target_link_libraries(EyelibAssets PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/assimp/lib/assimp-vc143-mt.lib)
endif()

