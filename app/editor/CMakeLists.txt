# OpenEye Editor (client of api/ only; no eyelib/ includes)

set(EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorApp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorSceneBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MenuBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/StatusPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/HierarchyPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/InspectorPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/panels/SceneViewPanel.cpp
)

add_executable(OpenEyeEditor ${EDITOR_SOURCES})

target_include_directories(OpenEyeEditor
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/api
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/panels
)

# Phase 0: bootstrap. Phase 1: world. Phase 4b: scene view render.
target_link_libraries(OpenEyeEditor
    PRIVATE
        EyelibCoreAPI
        EyelibPlatformAPI
        EyelibWorldAPI
        EyelibRenderAPI
        EyelibAssetsAPI
        EyelibAnimationAPI
        EyelibMathAPI
        OpenEyeImGui
)
