# Eyelib Assets Component

set(ASSETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderWatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderWatcher.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MaterialLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/MaterialLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/ModelLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/loaders/ModelLoader.h
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
        EyelibPlatform
)

if(OPENEYE_USE_LZ4 AND OPENEYE_LZ4_AVAILABLE)
    target_link_libraries(EyelibAssets PRIVATE lz4)
    target_compile_definitions(EyelibAssets PRIVATE EYELIB_WITH_LZ4=1)
    target_include_directories(EyelibAssets PRIVATE ${lz4_SOURCE_DIR}/lib)
endif()
if(OPENEYE_USE_ZSTD AND OPENEYE_ZSTD_AVAILABLE)
    target_link_libraries(EyelibAssets PRIVATE libzstd_static)
    target_compile_definitions(EyelibAssets PRIVATE EYELIB_WITH_ZSTD=1)
    target_include_directories(EyelibAssets PRIVATE ${zstd_SOURCE_DIR}/lib)
endif()

if(WIN32)
    target_link_libraries(EyelibAssets PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/assimp/lib/assimp-vc143-mt.lib)
    target_link_libraries(EyelibAssets PRIVATE mfplat mfreadwrite mfuuid)
endif()

