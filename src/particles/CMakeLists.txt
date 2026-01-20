# OpenEye Particles Component
# Particle systems: ParticleEmitter, ParticleRenderer

set(PARTICLES_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleEmitter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleEmitter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParticleRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/ParticleTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/ParticleTemplates.h
)

add_library(OpenEyeParticles STATIC ${PARTICLES_SOURCES})

target_include_directories(OpenEyeParticles
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
)

# Dependencies
target_link_libraries(OpenEyeParticles
    PUBLIC
        OpenEyeCore
        OpenEyeRendering
        glad
)

