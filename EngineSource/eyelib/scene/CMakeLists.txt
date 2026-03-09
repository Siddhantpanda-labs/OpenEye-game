# Eyelib Scene Component (world ownership + orchestration)

set(SCENE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Prefab.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PrefabInstantiator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PrefabInstantiator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PrefabVariant.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PrefabVariant.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene.h
)

add_library(EyelibScene STATIC ${SCENE_SOURCES})

target_include_directories(EyelibScene
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/thirdparty
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/nlohmann
)

target_link_libraries(EyelibScene
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibWorld
        EyelibSerialize
)

