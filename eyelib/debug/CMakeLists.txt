# Eyelib Debug & Tools Component

set(DEBUG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Validation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Validation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDraw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugDraw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Profiler.h
)

add_library(EyelibDebug STATIC ${DEBUG_SOURCES})

target_include_directories(EyelibDebug
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
)

target_link_libraries(EyelibDebug
    PUBLIC
        EyelibCore
        EyelibMath
    PRIVATE
        glad
)

