# Eyelib Physics Component (Bullet-backed, minimal bring-up)

set(PHYSICS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shapes/CollisionShapeFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shapes/CollisionShapeFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bodies/RigidBodyFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bodies/RigidBodyFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/constraints/ConstraintFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/constraints/ConstraintFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers/CharacterController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers/CharacterController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/queries/PhysicsQuery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/queries/PhysicsQuery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/queries/PhysicsQueryTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/SoftBodyFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/SoftBodyFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/Cloth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/Cloth.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vehicle/Vehicle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vehicle/Vehicle.h
)

add_library(EyelibPhysics STATIC ${PHYSICS_SOURCES})

target_include_directories(EyelibPhysics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/bullet3/src
)

target_link_libraries(EyelibPhysics
    PUBLIC
        EyelibCore
        EyelibMath
        BulletSoftBody
        BulletDynamics
        BulletCollision
        LinearMath
)

if(MSVC)
    set_source_files_properties(
        ${PHYSICS_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "/wd4127 /wd4456"
    )
endif()

