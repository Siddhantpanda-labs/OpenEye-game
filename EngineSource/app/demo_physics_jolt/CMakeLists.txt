# Jolt physics demo: floor + sphere, step, raycast, AABB query.
# Run from build/bin/Debug or Release.
set(DEMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(DemoPhysicsJolt ${DEMO_SOURCES})

target_include_directories(DemoPhysicsJolt
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}
        ${OPENEYE_ENGINE_SOURCE}/eyelib
        ${OPENEYE_ENGINE_SOURCE}/api
)

target_link_libraries(DemoPhysicsJolt
    PRIVATE
        EyelibPhysicsAPI
        EyelibMath
        EyelibCore
)
