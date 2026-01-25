# Eyelib Fluids Component (CPU-only bring-up)

set(FLUIDS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSimulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSimulation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidFoamRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidFoamRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSurfaceExtractor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSurfaceExtractor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidVolumeRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidVolumeRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/FluidTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/FluidTemplates.h
)

add_library(EyelibFluids STATIC ${FLUIDS_SOURCES})

target_include_directories(EyelibFluids
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/glad/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibFluids
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibRender
)

