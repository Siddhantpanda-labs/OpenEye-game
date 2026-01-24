# Eyelib ECS API

set(API_ECS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Entity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Components.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Component.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Query.h
    ${CMAKE_CURRENT_SOURCE_DIR}/QueryResult.h
    ${CMAKE_CURRENT_SOURCE_DIR}/System.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/System.h
    ${CMAKE_CURRENT_SOURCE_DIR}/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/World.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibECSAPI STATIC ${API_ECS_SOURCES})

target_include_directories(EyelibECSAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibECSAPI
    PUBLIC
        EyelibECS
        EyelibRenderAPI
        EyelibAnimationAPI
        EyelibPhysicsAPI
)

