# OpenEye Animation Component
# Animation systems: Skeleton, AnimationClip, AnimationLoader, AnimationSystem, AnimationStateMachine

set(ANIMATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Skeleton.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationClip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationStateMachine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationStateMachine.h
)

add_library(OpenEyeAnimation STATIC ${ANIMATION_SOURCES})

target_include_directories(OpenEyeAnimation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        # API folder for apps to use
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include/animation/api>
    PRIVATE
        # Third-party includes - internal only
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/include
)

# Dependencies
target_link_libraries(OpenEyeAnimation
    PUBLIC
        OpenEyeCore
        OpenEyeRendering
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/assimp/lib/assimp-vc143-mt.lib
)

