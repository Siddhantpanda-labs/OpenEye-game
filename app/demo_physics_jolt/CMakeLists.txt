# Jolt physics demo: floor + sphere, step, raycast, AABB query.
# Run from build/bin/Debug or Release.
set(DEMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(DemoPhysicsJolt ${DEMO_SOURCES})

target_include_directories(DemoPhysicsJolt
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/api
)

target_link_libraries(DemoPhysicsJolt
    PRIVATE
        EyelibPhysicsAPI
        EyelibMath
        EyelibCore
)
