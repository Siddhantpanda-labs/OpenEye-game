# OpenMeta Game Application
# Multiplayer VR City Sandbox Game
# This is the final game executable (no editor tools)

# Game source files
set(OPENMETA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    # Core systems
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/GameApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/GameApplication.h
    # Add more source files as they are created
)

add_executable(OpenMeta ${OPENMETA_SOURCES})

target_include_directories(OpenMeta
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/app
        # Need Bullet includes for PhysicsSystem functions that take internal Physics::PhysicsWorld*
        ${CMAKE_SOURCE_DIR}/thirdparty/bullet3/src
)

# Link to OpenEye library components
target_link_libraries(OpenMeta
    PRIVATE
        OpenEyeECS
        OpenEyePhysics  # Needed for PhysicsSystem functions with internal types
        glad
        glfw
)

# Debug build: enable editor, Release build: disable editor
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    target_compile_definitions(OpenMeta PRIVATE OPENEYE_EDITOR=1)
    target_link_libraries(OpenMeta PRIVATE OpenMetaEditorLib OpenEyeImGui)
    target_include_directories(OpenMeta PRIVATE 
        ${CMAKE_SOURCE_DIR}/thirdparty/imgui
        ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends
    )
else()
    target_compile_definitions(OpenMeta PRIVATE OPENEYE_EDITOR=0)
endif()

# Copy assets to output directory
if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
    add_custom_command(TARGET OpenMeta POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:OpenMeta>/assets
    )
endif()

