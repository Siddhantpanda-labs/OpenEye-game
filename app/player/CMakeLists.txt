# OpenEye Unified Runtime Player

set(PLAYER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlayerConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetBinding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetBinding.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldRender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldRender.h
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
        EyelibCoreAPI
        EyelibPlatformAPI
        EyelibAssetsAPI
        EyelibWorldAPI
        EyelibRenderAPI
        EyelibPhysicsAPI
        EyelibAnimationAPI
        EyelibAudio
        EyelibScriptingAPI
        OpenEyeImGui
)
