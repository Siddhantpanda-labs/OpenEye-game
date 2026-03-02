# Eyelib Serialize Component (JSON first, binary later)

set(SERIALIZE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/BinarySerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BinarySerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SerializationTraits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SerializeTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SerializeTests.h
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
        EyelibWorld
        EyelibScene
)

if(OPENEYE_USE_LZ4 AND OPENEYE_LZ4_AVAILABLE)
    target_link_libraries(EyelibSerialize PRIVATE lz4)
    target_compile_definitions(EyelibSerialize PRIVATE EYELIB_WITH_LZ4=1)
    target_include_directories(EyelibSerialize PRIVATE ${lz4_SOURCE_DIR}/lib)
endif()
if(OPENEYE_USE_ZSTD AND OPENEYE_ZSTD_AVAILABLE)
    target_link_libraries(EyelibSerialize PRIVATE libzstd_static)
    target_compile_definitions(EyelibSerialize PRIVATE EYELIB_WITH_ZSTD=1)
    target_include_directories(EyelibSerialize PRIVATE ${zstd_SOURCE_DIR}/lib)
endif()

