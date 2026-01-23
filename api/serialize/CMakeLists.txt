# Eyelib Serialize API

set(API_SERIALIZE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneJson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneJson.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibSerializeAPI STATIC ${API_SERIALIZE_SOURCES})

target_include_directories(EyelibSerializeAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_link_libraries(EyelibSerializeAPI
    PUBLIC
        EyelibSceneAPI
        EyelibCore
)

