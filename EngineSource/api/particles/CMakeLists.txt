# Eyelib Particles API

set(API_PARTICLES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleTemplates.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibParticlesAPI STATIC ${API_PARTICLES_SOURCES})

target_include_directories(EyelibParticlesAPI
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
)

target_link_libraries(EyelibParticlesAPI
    PUBLIC
        EyelibParticles
        EyelibRenderAPI
)

