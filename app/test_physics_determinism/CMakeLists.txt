# Test: Physics determinism (60 FPS vs 144 FPS → identical transform results).
# Links EyelibWorld. Run from build/bin/Debug or Release.

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(TestPhysicsDeterminism ${TEST_SOURCES})

target_include_directories(TestPhysicsDeterminism
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_link_libraries(TestPhysicsDeterminism
    PRIVATE
        EyelibWorld
        EyelibMath
        EyelibCore
)
