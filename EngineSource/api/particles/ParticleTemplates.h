#pragma once

#include "api/particles/ParticleSystem.h"

namespace eyelib {
namespace api {

/**
 * @brief Preset particle emitter configurations.
 */
class ParticleTemplates {
public:
    static EmitterConfig CreateFire();
    static EmitterConfig CreateSmoke();
    static EmitterConfig CreateExplosion();
    static EmitterConfig CreateRain();
    static EmitterConfig CreateSnow();
    static EmitterConfig CreateSparkles();
};

} // namespace api
} // namespace eyelib
