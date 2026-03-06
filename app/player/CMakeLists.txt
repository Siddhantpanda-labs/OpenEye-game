# OpenEye Unified Runtime Player

# Shared game runtime (used by both Player exe and Editor Game view)
set(GAME_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/BuiltinResourceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BuiltinResourceManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GameRuntime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GameRuntime.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetBinding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetBinding.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldRender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldRender.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererGPUResourceCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererGPUResourceCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldPhysics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldPhysics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldAnimation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldAnimation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldAudio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldAudio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldScripting.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/WorldLuaBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/WorldLuaBindings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/WorldLuaHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/WorldLuaHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/WorldLuaJson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/WorldLuaJson.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/WorldScriptingBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scripting/ScriptSchema.cpp
)

add_library(OpenEyeGameRuntime STATIC ${GAME_RUNTIME_SOURCES})

target_include_directories(OpenEyeGameRuntime
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/api
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(OpenEyeGameRuntime
    PUBLIC
        EyelibCoreAPI
        EyelibGeometryAPI
        EyelibPlatformAPI
        EyelibAssetsAPI
        EyelibWorldAPI
        EyelibRenderAPI
        EyelibPhysicsAPI
        EyelibAnimationAPI
        EyelibAudio
        EyelibScriptingAPI
        EyelibVR
        OpenEyeImGui
)

# Player executable (uses game runtime); only when building the player app
if(OPENEYE_BUILD_PLAYER)
    set(PLAYER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.h
    )

    add_executable(OpenEyePlayer ${PLAYER_SOURCES})

    target_include_directories(OpenEyePlayer
        PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/api
            ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(OpenEyePlayer
        PRIVATE
            OpenEyeGameRuntime
    )
endif()
