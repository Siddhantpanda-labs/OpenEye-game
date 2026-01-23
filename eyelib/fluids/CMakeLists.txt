# Eyelib Fluids Component (CPU-only bring-up)

set(FLUIDS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSimulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSimulation.h
)

add_library(EyelibFluids STATIC ${FLUIDS_SOURCES})

target_include_directories(EyelibFluids
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibFluids
    PUBLIC
        EyelibCore
        EyelibMath
)

