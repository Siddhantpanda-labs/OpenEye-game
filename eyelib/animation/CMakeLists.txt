# Eyelib Animation Component (minimal bring-up)

set(ANIMATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.h
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

