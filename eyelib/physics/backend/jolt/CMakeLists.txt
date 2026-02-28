# Jolt physics backend: facade + shape store + body store + queries.
# Self-contained; parent EyelibPhysics links this target.
# When thirdparty/joltPhysics is added, link Jolt library here and swap impl in these modules.

set(JOLT_BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltWorld.h
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltShapeStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltShapeStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltQueries.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JoltQueries.h
)

add_library(EyelibPhysicsJolt STATIC ${JOLT_BACKEND_SOURCES})

target_include_directories(EyelibPhysicsJolt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibPhysicsJolt
    PUBLIC
        EyelibCore
        EyelibMath
        Jolt
)

# Jolt headers are under JoltPhysics/Jolt/, so include path must be JoltPhysics (parent) for #include <Jolt/...>
target_include_directories(EyelibPhysicsJolt
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/JoltPhysics
)

if(MSVC)
    set_source_files_properties(
        ${JOLT_BACKEND_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "/wd4127 /wd4456"
    )
endif()
