# Eyelib Serialize Component (JSON first, binary later)

set(SERIALIZE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/BinarySerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BinarySerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentSerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SerializationTraits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SerializeTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SerializeTests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldSnapshot.h
)

add_library(EyelibSerialize STATIC ${SERIALIZE_SOURCES})

target_include_directories(EyelibSerialize
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_link_libraries(EyelibSerialize
    PUBLIC
        EyelibCore
        EyelibECS
        EyelibScene
)

