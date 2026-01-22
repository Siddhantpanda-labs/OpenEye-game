# Editor System
# Development editor with ImGui panels
# Only builds in Debug configuration

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    # Editor panels
    set(EDITOR_PANELS_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/HierarchyPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/HierarchyPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/InspectorPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/InspectorPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/LightingPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/LightingPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/AnimatorPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/AnimatorPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/SoftBodyPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/SoftBodyPanel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/panels/IEditorPanel.h
    )
    
    # Editor utilities
    set(EDITOR_UTILS_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/SelectionManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/SelectionManager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/SceneManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/SceneManager.h
    )
    
    # Editor application
    set(EDITOR_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/EditorApplication.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/EditorApplication.h
    )
    
    # Editor gizmos
    set(EDITOR_GIZMOS_SOURCES
        # ${CMAKE_CURRENT_SOURCE_DIR}/gizmos/TransformGizmo.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/gizmos/GizmoRenderer.cpp
        # Add gizmo sources as they are created
    )
    
    # Create editor library
    add_library(OpenMetaEditorLib STATIC
        ${EDITOR_PANELS_SOURCES}
        ${EDITOR_UTILS_SOURCES}
        ${EDITOR_GIZMOS_SOURCES}
        ${EDITOR_APP_SOURCES}
    )
    
target_include_directories(OpenMetaEditorLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/thirdparty/imgui
        ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends
        ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo
)
    
    target_link_libraries(OpenMetaEditorLib
        PUBLIC
            OpenEyeECS
            OpenEyeImGui
            OpenEyeRendering
            OpenEyeAnimation
            OpenEyeParticles
            OpenEyeFluids
    )
    
    target_compile_definitions(OpenMetaEditorLib PRIVATE OPENEYE_EDITOR=1)
    
    message(STATUS "Editor system enabled (Debug build)")
else()
    message(STATUS "Editor system disabled (Release build)")
endif()

