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
    add_compile_options(/wd4245 /wd4100 /wd4189 /wd4244 /wd4034 /wd4127)
    
    # Suppress Bullet3 warnings for physics files (these include Bullet3 headers)
    set_source_files_properties(
        src/physics/PhysicsWorld.cpp
        src/physics/shapes/CollisionShapeFactory.cpp
        src/physics/bodies/RigidBodyFactory.cpp
        src/physics/controllers/CharacterControllerFactory.cpp
        src/physics/queries/PhysicsQuery.cpp
        src/physics/utils/PhysicsUtils.cpp
        PROPERTIES
        COMPILE_FLAGS "/wd4127 /wd4456"
    )
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
    ${CMAKE_SOURCE_DIR}/thirdparty/bullet3/src
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

# --------------------------------------
# Bullet3 Physics Library
# --------------------------------------
# Configure Bullet3 build options
set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(USE_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(BULLET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BULLET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# Disable warnings for Bullet3 (third-party code)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/bullet3)
# Suppress warnings for Bullet3 targets (must be after add_subdirectory)
if(MSVC)
    if(TARGET BulletDynamics)
        target_compile_options(BulletDynamics PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletDynamics PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET BulletCollision)
        target_compile_options(BulletCollision PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletCollision PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET LinearMath)
        target_compile_options(LinearMath PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(LinearMath PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    # Suppress warnings for additional Bullet3 libraries
    if(TARGET BulletSoftBody)
        target_compile_options(BulletSoftBody PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456 /wd4458 /wd4459 /wd4463 /wd4756 /wd4018 /wd4389)
        set_target_properties(BulletSoftBody PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET BulletInverseDynamics)
        target_compile_options(BulletInverseDynamics PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletInverseDynamics PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET Bullet3Common)
        target_compile_options(Bullet3Common PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456 /wd4458)
        set_target_properties(Bullet3Common PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
endif()

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
    
    # ECS (Refactored - Modular Structure)
    src/ecs/Scene.cpp
    src/ecs/Scene.h
    src/ecs/Components.cpp
    src/ecs/Components.h
    src/ecs/Systems.h
    # ECS Systems (Modular)
    src/ecs/systems/TransformSystem.cpp
    src/ecs/systems/TransformSystem.h
    src/ecs/systems/LODSystem.cpp
    src/ecs/systems/LODSystem.h
    src/ecs/systems/CullingSystem.cpp
    src/ecs/systems/CullingSystem.h
    src/ecs/systems/CameraSystem.cpp
    src/ecs/systems/CameraSystem.h
    src/ecs/systems/RenderSystem.cpp
    src/ecs/systems/RenderSystem.h
    
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
    src/rendering/IBL.cpp
    src/rendering/IBL.h
    src/rendering/ShadowMap.cpp
    src/rendering/ShadowMap.h
    src/rendering/PostProcessor.cpp
    src/rendering/PostProcessor.h
    src/rendering/RenderPipeline.cpp
    src/rendering/RenderPipeline.h
    src/rendering/passes/GBufferPass.cpp
    src/rendering/passes/GBufferPass.h
    src/rendering/passes/ShadowPass.cpp
    src/rendering/passes/ShadowPass.h
    src/rendering/passes/LightingPass.cpp
    src/rendering/passes/LightingPass.h
    src/rendering/passes/TransparencyPass.cpp
    src/rendering/passes/TransparencyPass.h
    src/rendering/framegraph/FrameGraph.cpp
    src/rendering/framegraph/FrameGraph.h
    
    # Animation systems
    src/animation/Skeleton.cpp
    src/animation/Skeleton.h
    src/animation/AnimationClip.cpp
    src/animation/AnimationClip.h
    src/animation/AnimationLoader.cpp
    src/animation/AnimationLoader.h
    src/animation/AnimationSystem.cpp
    src/animation/AnimationSystem.h
    
    # Physics systems
    src/physics/PhysicsWorld.cpp
    src/physics/PhysicsWorld.h
    src/physics/shapes/CollisionShapeFactory.cpp
    src/physics/shapes/CollisionShapeFactory.h
    src/physics/bodies/RigidBodyFactory.cpp
    src/physics/bodies/RigidBodyFactory.h
    src/physics/controllers/CharacterControllerFactory.cpp
    src/physics/controllers/CharacterControllerFactory.h
    src/physics/queries/PhysicsQuery.cpp
    src/physics/queries/PhysicsQuery.h
    src/physics/queries/PhysicsQueryTypes.h
    src/physics/utils/PhysicsUtils.cpp
    src/physics/utils/PhysicsUtils.h
    
    ${IMGUI_SOURCES}
    ${IMGUIZMO_SOURCES}
)

# Link third-party libraries
if(WIN32)
    target_link_libraries(OpenEyeCore
        PUBLIC glad
        PUBLIC glfw
        PUBLIC BulletDynamics
        PUBLIC BulletCollision
        PUBLIC LinearMath
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
        PUBLIC BulletDynamics
        PUBLIC BulletCollision
        PUBLIC LinearMath
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


