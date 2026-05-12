# OpenEye Editor (client of api/ only; no eyelib/ includes)
#
# Migration from EyelibRenderAPI to EyelibRenderingAPI is incremental. The legacy render
# firewall below is OPT-IN per migrated subset:
# enable each invocation only when the corresponding migration step closes.
#
#   M2 closeout  -> uncomment the SCENE_VIEW_MIGRATED block (scene view + EditorSceneBuilder)
#   M3 closeout  -> uncomment the EDITOR_REMAINDER_MIGRATED block (everything except game-runtime-driven game-view path)
#   M4 closeout  -> uncomment the FULL_DIRECTORY firewall (game-runtime no longer leaks legacy types)
#
# Do NOT enable all three from day one; existing editor sources still reference api/render
# and eyelib/render directly. The firewall is intentionally surgical so the editor keeps
# building during the migration.

# include(${OPENEYE_ENGINE_SOURCE}/cmake/LegacyRenderFirewall.cmake)
# Pre-staged opt-in invocations (uncomment when the matching section closes):
# openeye_enforce_no_legacy_render_includes(
#     FILES
#         ${CMAKE_CURRENT_SOURCE_DIR}/panels/SceneViewPanel.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/EditorSceneBuilder.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/EditorSceneBuilder.h
#     LABEL "OpenEyeEditor (M2: scene view migrated)"
# )
# openeye_enforce_no_legacy_render_includes(
#     FILES
#         ${CMAKE_CURRENT_SOURCE_DIR}/EditorApp.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/EditorContext.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/EditorContext.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/EditorVulkanRenderSlice.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/EditorVulkanRenderSlice.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/MenuBar.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/ScenePipelineMode.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/panels/MaterialPreview.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/panels/MaterialPreview.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/panels/InspectorPanel.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/panels/InspectorMaterialEditor.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/panels/ProfilerPanel.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/panels/ShadowDebugPanel.cpp
#     LABEL "OpenEyeEditor (M3: editor remainder migrated, excluding play-mode game view)"
# )
# openeye_enforce_no_legacy_render_includes(
#     DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     LABEL "OpenEyeEditor (M4: full editor migrated; OpenEyeGameRuntime no longer leaks legacy)"
# )

set(EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorFolderPicker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorProject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorSceneBuilder.cpp
    # M2.3 — parallel scene builder targeting the new public API (`api/rendering`).
    # Cold path during the M2/M3 dual-stack window per the M2.1 device-strategy decision;
    # exercised once `EditorContext::sceneViewPipelineNew` is set at the M3 closeout flip.
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorSceneBuilderNew.cpp
    # Per-scene skybox cache moved to `app/player/SkyboxCacheNew.{h,cpp}` (2026-05-12) so
    # editor preview and editor-embedded Play share a single cache instance — the editor
    # passes its `EditorContext::skyboxCacheNew` straight into `GameRuntime` on Play start,
    # avoiding a second cubemap upload pass against the same shared RHI device.
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorVulkanRenderSlice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/InstantiateOmesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MenuBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RunImporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/LightGizmoIcons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/PhysicsColliderIcons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/AssetPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/StatusPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/HierarchyPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/InspectorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/InspectorMaterialEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/MaterialPreview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/SceneViewPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/GameViewPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/ProfilerPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/ShadowDebugPanel.cpp
)
if(WIN32)
    list(APPEND EDITOR_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/OpenEyeEditor.rc)
endif()

add_executable(OpenEyeEditor ${EDITOR_SOURCES})

target_include_directories(OpenEyeEditor
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}
        ${OPENEYE_ENGINE_SOURCE}/api
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/panels
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

# Phase 0: bootstrap. Phase 1: world. Phase 4b: scene view render. Game view: OpenEyeGameRuntime.
#
# M3.8 (2026-05-10) — direct `EyelibRenderAPI` link dropped; relied on `OpenEyeGameRuntime`'s
# PUBLIC link for transitive reach.
# M4.8 (2026-05-10) — `OpenEyeGameRuntime` demoted its `EyelibRenderAPI` link to PRIVATE
# (header diet on `GameRuntime.h` made the transitive expose unnecessary). The editor still
# uses legacy `eyelib::api::*` types in unmigrated TUs (game-view play branch in
# `panels/GameViewPanel.cpp`, legacy pipeline pointers in `EditorContext`, `MaterialPreview`,
# `EditorVulkanRenderSlice`, `MenuBar`, the legacy `EditorSceneBuilder` parallel to `*New`),
# so it now links `EyelibRenderAPI` itself. M3.9 will retire this link entirely once the
# remaining editor TUs flip to `eyelib::api::rendering::*` types and the legacy fields can
# be dropped.
target_link_libraries(OpenEyeEditor
    PRIVATE
        EyelibCoreAPI
        EyelibDebugAPI
        EyelibGeometryAPI
        EyelibPlatformAPI
        EyelibWorldAPI
        EyelibRenderAPI       # M4.8 — explicit; removed in M3.9 closeout
        EyelibRenderingAPI
        EyelibAssetsAPI
        EyelibAnimationAPI
        EyelibMathAPI
        OpenEyeImGui
        OpenEyeGameRuntime
        AssetReaders
)
if(WIN32)
    target_link_libraries(OpenEyeEditor PRIVATE ole32)
endif()

# EyelibRender.lib + EyelibRendering.lib both compile third-party TU duplicates (stb_image,
# Vulkan Memory Allocator in AllocatorVK.cpp). Linking both into one executable triggers
# LNK2005 unless one definition wins. This is inherent to the M2–M4 dual-stack window;
# remove when OpenEyeEditor stops linking EyelibRenderAPI (M5 / legacy decommission).
if(MSVC)
    target_link_options(OpenEyeEditor PRIVATE /FORCE:MULTIPLE /IGNORE:4006 /IGNORE:4075 /IGNORE:4088 /IGNORE:4099)
endif()
