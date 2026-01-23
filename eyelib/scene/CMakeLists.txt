# Eyelib Scene Component (world ownership + orchestration)

set(SCENE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.h
)

add_library(EyelibScene STATIC ${SCENE_SOURCES})

target_include_directories(EyelibScene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibScene
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibECS
)

