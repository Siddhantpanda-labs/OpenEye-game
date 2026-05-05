# Eyelib Serialize API

set(API_SERIALIZE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/BinarySerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BinarySerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentSerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldSnapshot.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibSerializeAPI STATIC ${API_SERIALIZE_SOURCES})

target_include_directories(EyelibSerializeAPI
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/nlohmann
)

target_link_libraries(EyelibSerializeAPI
    PUBLIC
        EyelibSceneAPI
        EyelibWorld
        EyelibCore
)

