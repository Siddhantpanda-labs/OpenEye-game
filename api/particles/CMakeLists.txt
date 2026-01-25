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
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(EyelibParticlesAPI
    PUBLIC
        EyelibParticles
        EyelibRenderAPI
)

