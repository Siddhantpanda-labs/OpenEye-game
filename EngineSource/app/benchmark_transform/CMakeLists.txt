# Benchmark: TransformSystem with 100k-entity hierarchy.
# Build when OPENEYE_BUILD_TESTS is ON (or always). Links EyelibWorld only.

set(BENCH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(BenchmarkTransform ${BENCH_SOURCES})

target_include_directories(BenchmarkTransform
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}
        ${OPENEYE_ENGINE_SOURCE}/eyelib
)

target_link_libraries(BenchmarkTransform
    PRIVATE
        EyelibWorld
        EyelibMath
        EyelibCore
)
