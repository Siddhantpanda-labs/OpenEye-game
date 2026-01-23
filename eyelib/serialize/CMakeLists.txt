# Eyelib Serialize Component (JSON first, binary later)

set(SERIALIZE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneJson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneJson.h
)

add_library(EyelibSerialize STATIC ${SERIALIZE_SOURCES})

target_include_directories(EyelibSerialize
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_link_libraries(EyelibSerialize
    PUBLIC
        EyelibCore
        EyelibScene
)

