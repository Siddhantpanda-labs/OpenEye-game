# Eyelib ECS Component (EnTT-backed, minimal bring-up)

set(ECS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/World.h
)

add_library(EyelibECS STATIC ${ECS_SOURCES})

target_include_directories(EyelibECS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibECS
    PUBLIC
        EyelibCore
        EyelibMath
)

