# Eyelib Scene API

set(API_SCENE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefab.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibSceneAPI STATIC ${API_SCENE_SOURCES})

target_include_directories(EyelibSceneAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/thirdparty
)

target_link_libraries(EyelibSceneAPI
    PUBLIC
        EyelibScene
        EyelibECSAPI
)

