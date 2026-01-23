# Eyelib Platform API

set(API_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibPlatformAPI STATIC ${API_PLATFORM_SOURCES})

target_include_directories(EyelibPlatformAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibPlatformAPI
    PUBLIC
        EyelibPlatform
)

