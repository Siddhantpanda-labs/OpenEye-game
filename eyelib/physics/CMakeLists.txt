# Eyelib Physics Component (Bullet-backed, minimal bring-up)

set(PHYSICS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.h
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

