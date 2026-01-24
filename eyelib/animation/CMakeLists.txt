# Eyelib Animation Component (minimal bring-up)

set(ANIMATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/KeyframeEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/KeyframeEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Pose.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Pose.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendTree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendTree.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationStateMachine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationStateMachine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StateMachineConditions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StateMachineConditions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StateMachineTransitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/IK.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IK.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Procedural.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Procedural.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Animator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Animator.h
)

add_library(EyelibAnimation STATIC ${ANIMATION_SOURCES})

target_include_directories(EyelibAnimation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
)

target_link_libraries(EyelibAnimation
    PUBLIC
        EyelibCore
        EyelibMath
)

