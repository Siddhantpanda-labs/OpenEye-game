# Eyelib ECS Component (EnTT-backed, minimal bring-up)

set(ECS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Entity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentStorage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentStorage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Query.h
    ${CMAKE_CURRENT_SOURCE_DIR}/QueryResult.h
    ${CMAKE_CURRENT_SOURCE_DIR}/System.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SystemManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SystemManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/World.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/AABB.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/components/AABB.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/Animator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/AudioListener.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/AudioSource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/Camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/Light.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/Name.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/Renderable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/RigidBody.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/Transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/components/Transform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/ScriptComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/ComponentHandles.h
    ${CMAKE_CURRENT_SOURCE_DIR}/components/All.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/TransformSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/TransformSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CameraSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CameraSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/LightSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/LightSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/AnimationSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/AnimationSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/AudioSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/AudioSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/PhysicsSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/PhysicsSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CullingSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/CullingSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/RenderSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/RenderSystem.h
)

add_library(EyelibECS STATIC ${ECS_SOURCES})

target_include_directories(EyelibECS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
)

target_link_libraries(EyelibECS
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibRender
        EyelibPhysics
        EyelibAnimation
        EyelibAudio
        EyelibScripting
)

