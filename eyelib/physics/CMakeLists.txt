# Eyelib Physics: interface + factory; Jolt backend has its own CMake under backend/jolt.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/backend/jolt)

set(PHYSICS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/IPhysicsWorld.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsBackendFactory.cpp
)

add_library(EyelibPhysics STATIC ${PHYSICS_SOURCES})

target_include_directories(EyelibPhysics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibPhysics
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibPhysicsJolt
)

if(MSVC)
    set_source_files_properties(
        ${PHYSICS_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "/wd4127 /wd4456"
    )
endif()

