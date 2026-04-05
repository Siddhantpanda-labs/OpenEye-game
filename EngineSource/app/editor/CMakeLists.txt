# OpenEye Editor (client of api/ only; no eyelib/ includes)

set(EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorFolderPicker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorProject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorSceneBuilder.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/SceneViewPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/GameViewPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/ProfilerPanel.cpp
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
target_link_libraries(OpenEyeEditor
    PRIVATE
        EyelibCoreAPI
        EyelibDebugAPI
        EyelibGeometryAPI
        EyelibPlatformAPI
        EyelibWorldAPI
        EyelibRenderAPI
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

# Copy AssetImporter next to editor so "Convert to canonical" can run it
add_dependencies(OpenEyeEditor AssetImporter)
add_custom_command(TARGET OpenEyeEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:AssetImporter>
        $<TARGET_FILE_DIR:OpenEyeEditor>/$<TARGET_FILE_NAME:AssetImporter>
    COMMENT "Copy AssetImporter next to OpenEyeEditor")
