# Eyelib Particles Component (CPU-only bring-up)

set(PARTICLES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/ParticleTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/ParticleTemplates.h
)

add_library(EyelibParticles STATIC ${PARTICLES_SOURCES})

target_include_directories(EyelibParticles
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibParticles
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibRender
)

