cmake_minimum_required(VERSION 3.20)
project(OpenEye VERSION 0.1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:preprocessor)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Enable parallel compilation
    add_compile_options(/MP)
    # Disable warnings for third-party code
    add_compile_options(/wd4245 /wd4100 /wd4189 /wd4244 /wd4034)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Build configuration
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(OPENEYE_BUILD_EDITOR "Build development editor" ON)
option(OPENEYE_BUILD_GAME "Build final game" ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
    ${CMAKE_SOURCE_DIR}/thirdparty/glfw/include
    ${CMAKE_SOURCE_DIR}/thirdparty/glm
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo
    ${CMAKE_SOURCE_DIR}/thirdparty/assimp/include
    ${CMAKE_SOURCE_DIR}/thirdparty/stb
    ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
    ${CMAKE_SOURCE_DIR}/src
)

# --------------------------------------
# GLAD (match GameEngine approach - simple static library)
# --------------------------------------
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/thirdparty/glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/glad/include)

# --------------------------------------
# GLFW (use GLFW's own CMake build system - proper way)
# --------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/glfw)

set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)

set(IMGUIZMO_SOURCES
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImGuizmo.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImCurveEdit.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImGradient.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImSequencer.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/GraphEditor.cpp
)

# Core library
add_library(OpenEyeCore STATIC
    # Core systems
    src/core/window/Window.cpp
    src/core/window/Window.h
    src/core/input/Input.cpp
    src/core/input/Input.h
    src/core/time/Time.cpp
    src/core/time/Time.h
    src/core/log/Log.cpp
    src/core/log/Log.h
    src/core/filesystem/ShaderFileWatcher.cpp
    src/core/filesystem/ShaderFileWatcher.h
    
    # ECS
    src/ecs/Scene.cpp
    src/ecs/Scene.h
    src/ecs/Components.cpp
    src/ecs/Components.h
    src/ecs/Systems.cpp
    src/ecs/Systems.h
    
    # Rendering resources
    src/rendering/resources/Shader.cpp
    src/rendering/resources/Shader.h
    src/rendering/resources/Texture.cpp
    src/rendering/resources/Texture.h
    src/rendering/resources/Mesh.cpp
    src/rendering/resources/Mesh.h
    src/rendering/resources/Material.cpp
    src/rendering/resources/Material.h
    src/rendering/resources/MeshLoader.cpp
    src/rendering/resources/MeshLoader.h
    src/rendering/resources/UniformBuffer.cpp
    src/rendering/resources/UniformBuffer.h
    src/rendering/resources/Cubemap.cpp
    src/rendering/resources/Cubemap.h
    
    # Rendering systems
    src/rendering/Camera.cpp
    src/rendering/Camera.h
    src/rendering/Frustum.cpp
    src/rendering/Frustum.h
    src/rendering/LODManager.cpp
    src/rendering/LODManager.h
    src/rendering/InstancedRenderer.cpp
    src/rendering/InstancedRenderer.h
    src/rendering/GBuffer.cpp
    src/rendering/GBuffer.h
    src/rendering/FullscreenQuad.cpp
    src/rendering/FullscreenQuad.h
    src/rendering/SkyboxRenderer.cpp
    src/rendering/SkyboxRenderer.h
    src/rendering/ShadowMap.cpp
    src/rendering/ShadowMap.h
    src/rendering/RenderPipeline.cpp
    src/rendering/RenderPipeline.h
    src/rendering/passes/GBufferPass.cpp
    src/rendering/passes/GBufferPass.h
    src/rendering/passes/ShadowPass.cpp
    src/rendering/passes/ShadowPass.h
    src/rendering/passes/LightingPass.cpp
    src/rendering/passes/LightingPass.h
    src/rendering/framegraph/FrameGraph.cpp
    src/rendering/framegraph/FrameGraph.h
    
    ${IMGUI_SOURCES}
    ${IMGUIZMO_SOURCES}
)

# Link third-party libraries
if(WIN32)
    target_link_libraries(OpenEyeCore
        PUBLIC glad
        PUBLIC glfw
        PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/assimp/lib/assimp-vc143-mt.lib
        PRIVATE opengl32
        PRIVATE gdi32
        PRIVATE user32
        PRIVATE kernel32
    )
else()
    target_link_libraries(OpenEyeCore
        PUBLIC glad
        PUBLIC glfw
        PRIVATE opengl32
    )
endif()

# GLFW needs additional system libraries on some platforms
if(UNIX AND NOT APPLE)
    target_link_libraries(glfw PRIVATE m)
endif()

# Copy Assimp DLL to output directory on Windows
if(WIN32)
    add_custom_command(TARGET OpenEyeCore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/bin/assimp-vc143-mt.dll
        $<TARGET_FILE_DIR:OpenEyeCore>
    )
endif()

# Editor executable (development environment)
if(OPENEYE_BUILD_EDITOR)
    add_executable(Editor
        src/editor/main.cpp
    )
    
    target_link_libraries(Editor OpenEyeCore)
    
    # Debug configuration: enable editor features
    target_compile_definitions(Editor PRIVATE OPENEYE_EDITOR=1)
endif()

# Game executable (final runtime)
if(OPENEYE_BUILD_GAME)
    add_executable(Game
        src/game/main.cpp
    )
    
    target_link_libraries(Game OpenEyeCore)
    
    # Release configuration: disable editor features
    target_compile_definitions(Game PRIVATE OPENEYE_EDITOR=0)
endif()


