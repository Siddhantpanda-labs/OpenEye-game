# Eyelib Debug & Tools API

set(API_DEBUG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDraw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDraw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Profiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Validation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibDebugAPI STATIC ${API_DEBUG_SOURCES})

target_include_directories(EyelibDebugAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibDebugAPI
    PUBLIC
        EyelibDebug
)

