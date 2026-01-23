# Eyelib Physics API

set(API_PHYSICS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.h
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

