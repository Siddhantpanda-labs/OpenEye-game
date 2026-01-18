# OpenEye Fluids Component
# Fluid systems: FluidSimulation, FluidRenderer

set(FLUIDS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidCommon.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSimulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidSimulation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FluidRenderer.h
)

add_library(OpenEyeFluids STATIC ${FLUIDS_SOURCES})

target_include_directories(OpenEyeFluids
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
)

# Dependencies
target_link_libraries(OpenEyeFluids
    PUBLIC
        OpenEyeCore
        OpenEyeRendering
        glad
)

