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
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
        ${OPENEYE_ENGINE_SOURCE}/thirdparty
)

target_link_libraries(EyelibSceneAPI
    PUBLIC
        EyelibScene
)

