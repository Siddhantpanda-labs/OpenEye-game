# Eyelib Animation API

set(API_ANIMATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibAnimationAPI STATIC ${API_ANIMATION_SOURCES})

target_include_directories(EyelibAnimationAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibAnimationAPI
    PUBLIC
        EyelibAnimation
)

