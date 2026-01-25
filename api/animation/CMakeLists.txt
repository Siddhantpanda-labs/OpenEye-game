# Eyelib Animation API

set(API_ANIMATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Animator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Animator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendTree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendTree.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StateMachine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StateMachine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Pose.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Pose.h
    ${CMAKE_CURRENT_SOURCE_DIR}/IK.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IK.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/KeyframeEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/KeyframeEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Procedural.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Procedural.h
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

