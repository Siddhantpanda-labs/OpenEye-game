# OpenEye Core Component
# Base systems: Window, Input, Time, Log, Filesystem

set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window/Window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/input/Input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/input/Input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/time/Time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/time/Time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/log/Log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/log/Log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/filesystem/ShaderFileWatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filesystem/ShaderFileWatcher.h
)

add_library(OpenEyeCore STATIC ${CORE_SOURCES})

target_include_directories(OpenEyeCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        # API folder for apps to use
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include/core/api>
    PRIVATE
        # Third-party includes - internal only (but needed for Window/Input headers)
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
        ${CMAKE_SOURCE_DIR}/thirdparty/glfw/include
)

# Link GLFW for window management
target_link_libraries(OpenEyeCore
    PUBLIC
        glfw
        glad
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(OpenEyeCore
        PRIVATE
            opengl32
            gdi32
            user32
            kernel32
    )
endif()

