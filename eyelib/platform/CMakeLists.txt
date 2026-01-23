# Eyelib Platform Component
# Windowing + input + minimal platform services

set(PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/input/Input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/input/Input.h
)

add_library(EyelibPlatform STATIC ${PLATFORM_SOURCES})

target_include_directories(EyelibPlatform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glfw/include
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
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

