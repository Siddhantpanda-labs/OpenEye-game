# OpenEye ECS Component
# Entity Component System: Scene, Components, Systems, Prefab, SceneSerializer

set(ECS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Components.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Components.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Systems.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefab.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneSerializer.h
)

set(ECS_SYSTEMS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/TransformSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/TransformSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/LODSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/LODSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CullingSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CullingSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CameraSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CameraSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/RenderSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/RenderSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/PhysicsSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/PhysicsSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/ParticleSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/ParticleSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/FluidSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/FluidSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/SoftBodySystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/SoftBodySystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/AnimatorSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/AnimatorSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/XPBDClothSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/XPBDClothSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/JellyBlobSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/JellyBlobSystem.h
)

add_library(OpenEyeECS STATIC
    ${ECS_SOURCES}
    ${ECS_SYSTEMS_SOURCES}
)

target_include_directories(OpenEyeECS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        # API folder for apps to use
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include/ecs/api>
        # Third-party includes needed by public headers (apps get these through library, not directly)
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/glm>
        $<INSTALL_INTERFACE:include/thirdparty/glm>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/nlohmann>
        $<INSTALL_INTERFACE:include/thirdparty/nlohmann>
    PRIVATE
        # Third-party includes - internal only
        ${CMAKE_SOURCE_DIR}/thirdparty/entt
        ${CMAKE_SOURCE_DIR}/thirdparty/bullet3/src
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/include
)

# Dependencies - ECS depends on all other components
target_link_libraries(OpenEyeECS
    PUBLIC
        OpenEyeCore
        OpenEyeAssets
        OpenEyeRendering
        OpenEyeAnimation
        OpenEyeParticles
        OpenEyePhysics
        OpenEyeFluids
)

