# Eyelib Platform Component
# Windowing + input + minimal platform services

set(PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/input/Input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/input/Input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/filesystem/FileSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/time/HighResTimer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/time/FramePacer.cpp
)

add_library(EyelibPlatform STATIC ${PLATFORM_SOURCES})

target_include_directories(EyelibPlatform
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glad/include
)

target_link_libraries(EyelibPlatform
    PUBLIC
        EyelibCore
        EyelibMath
    PRIVATE
        glfw
        glad
)

if(WIN32)
    target_link_libraries(EyelibPlatform PRIVATE opengl32)
endif()

