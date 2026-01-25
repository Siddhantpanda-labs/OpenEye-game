# Eyelib Fluids API

set(API_FLUIDS_SOURCES
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
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidTemplates.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibFluidsAPI STATIC ${API_FLUIDS_SOURCES})

target_include_directories(EyelibFluidsAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibFluidsAPI
    PUBLIC
        EyelibFluids
        EyelibRenderAPI
)

