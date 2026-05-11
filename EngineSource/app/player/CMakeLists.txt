# OpenEye Unified Runtime Player
#
# Migration from EyelibRenderAPI to EyelibRenderingAPI is incremental.
# OpenEyeGameRuntime currently links EyelibRenderAPI PUBLIC and exposes legacy types via
# WorldRender / GameRuntime headers. The legacy render firewall below is OPT-IN per migrated
# subset; do NOT enable wholesale until M4 closes (otherwise build breaks immediately on
# WorldRender.cpp et al.).
#
#   M4.1 + M4.7 closeout -> uncomment the GAME_RUNTIME_MIGRATED block (WorldRender + cache + overlay + frame sync migrated)
#   M4.8 closeout        -> uncomment the FULL_DIRECTORY firewall (everything migrated; PUBLIC link to EyelibRenderAPI dropped)

# include(${OPENEYE_ENGINE_SOURCE}/cmake/LegacyRenderFirewall.cmake)
# Pre-staged opt-in invocations (uncomment when the matching section closes):
# openeye_enforce_no_legacy_render_includes(
#     FILES
#         ${CMAKE_CURRENT_SOURCE_DIR}/WorldRender.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/WorldRender.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/RendererGPUResourceCache.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/RendererGPUResourceCache.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/BuiltinResourceManager.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/BuiltinResourceManager.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/RenderFrameSync.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/RenderFrameSync.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/GameRuntime.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/GameRuntime.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/PlayerConfig.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/PlayerConfig.h
#     LABEL "OpenEyeGameRuntime + OpenEyePlayer (M4: migrated)"
# )
# openeye_enforce_no_legacy_render_includes(
#     DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     LABEL "OpenEyeGameRuntime + OpenEyePlayer (M4 closeout: full directory)"
# )

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
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererGPUResourceCacheNew.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RendererGPUResourceCacheNew.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BuiltinResourceManagerNew.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BuiltinResourceManagerNew.h
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
        ${OPENEYE_ENGINE_SOURCE}
        ${OPENEYE_ENGINE_SOURCE}/api
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# M4.8 — `EyelibRenderAPI` demoted to PRIVATE. After the GameRuntime.h header diet
# (this milestone), no public OpenEyeGameRuntime header references legacy `eyelib::api::*`
# render types other than four narrow ones (RenderDevice / CameraView / RenderScene /
# RenderBackend) — and consumers of those types include the corresponding header directly.
# OpenEyeEditor still uses legacy types in TUs that have not migrated yet (game-view play
# path, editor-context legacy pointer fields, MaterialPreview); it now links
# `EyelibRenderAPI` itself. The PRIVATE link here means new editor TUs cannot accidentally
# pick up legacy headers via `#include "GameRuntime.h"` — they must opt in explicitly,
# which makes M3.9's "delete legacy from editor" sweep auditable.
target_link_libraries(OpenEyeGameRuntime
    PUBLIC
        EyelibCoreAPI
        EyelibGeometryAPI
        EyelibPlatformAPI
        EyelibAssetsAPI
        EyelibWorldAPI
        EyelibRenderingAPI
        EyelibPhysicsAPI
        EyelibAnimationAPI
        EyelibAudio
        EyelibScriptingAPI
        EyelibVR
        OpenEyeImGui
    PRIVATE
        EyelibRenderAPI
)

if(OPENEYE_BUILD_STREAMING)
    target_link_libraries(OpenEyeGameRuntime PUBLIC OpenEyeStreaming)
    target_compile_definitions(OpenEyeGameRuntime PUBLIC OPENEYE_BUILD_STREAMING)
endif()

# Player executable (uses game runtime); only when building the player app
if(OPENEYE_BUILD_PLAYER)
    set(PLAYER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PlayerApp.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Overlay.h
    )
    if(WIN32)
        list(APPEND PLAYER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/OpenEyePlayer.rc)
    endif()

    add_executable(OpenEyePlayer ${PLAYER_SOURCES})

    target_include_directories(OpenEyePlayer
        PRIVATE
            ${OPENEYE_ENGINE_SOURCE}
            ${OPENEYE_ENGINE_SOURCE}/api
            ${OPENEYE_ENGINE_SOURCE}/thirdparty/nlohmann
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(OpenEyePlayer
        PRIVATE
            OpenEyeGameRuntime
    )
    # EyelibRender.lib + EyelibRendering.lib duplicate stb_image / VMA — same dual-stack window as OpenEyeEditor (M4.1).
    if(MSVC)
        target_link_options(OpenEyePlayer PRIVATE /FORCE:MULTIPLE)
    endif()
    if(OPENEYE_BUILD_STREAMING)
        target_link_libraries(OpenEyePlayer PRIVATE qrcodegen)
        target_include_directories(OpenEyePlayer PRIVATE ${OPENEYE_ENGINE_SOURCE}/thirdparty/qrcodegen)
    endif()
endif()
