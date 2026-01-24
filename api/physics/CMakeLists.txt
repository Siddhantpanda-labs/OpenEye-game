# Eyelib Physics API

set(API_PHYSICS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CollisionShape.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CollisionShape.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RigidBody.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RigidBody.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Constraint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Constraint.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CharacterController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CharacterController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Query.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SoftBody.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SoftBody.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Vehicle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Vehicle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibPhysicsAPI STATIC ${API_PHYSICS_SOURCES})

target_include_directories(EyelibPhysicsAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibPhysicsAPI
    PUBLIC
        EyelibPhysics
)

