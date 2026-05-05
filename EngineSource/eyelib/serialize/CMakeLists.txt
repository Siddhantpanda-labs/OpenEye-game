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
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/thirdparty
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glm
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/nlohmann
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

