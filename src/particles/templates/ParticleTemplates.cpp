#include "ParticleTemplates.h"

namespace OpenEye {
namespace Particles {
namespace Templates {

std::shared_ptr<ParticleEmitter> CreateFire(const FireConfig& config) {
    ParticleEmitterConfig emitterConfig;
    emitterConfig.position = config.position;
    emitterConfig.direction = config.direction;
    emitterConfig.spread = config.spread;
    emitterConfig.minSpeed = config.minSpeed;
    emitterConfig.maxSpeed = config.maxSpeed;
    emitterConfig.minLifetime = config.minLifetime;
    emitterConfig.maxLifetime = config.maxLifetime;
    emitterConfig.minSize = config.minSize;
    emitterConfig.maxSize = config.maxSize;
    emitterConfig.sizeOverLifetime = config.sizeOverLifetime;
    emitterConfig.startColor = config.startColor;
    emitterConfig.endColor = config.endColor;
    emitterConfig.particlesPerSecond = config.particlesPerSecond;
    emitterConfig.maxParticles = config.maxParticles;
    emitterConfig.useGravity = config.useGravity;
    emitterConfig.gravity = config.gravity;
    emitterConfig.damping = config.damping;
    
    auto emitter = std::make_shared<ParticleEmitter>(emitterConfig);
    emitter->Start();
    return emitter;
}

std::shared_ptr<ParticleEmitter> CreateWater(const WaterConfig& config) {
    ParticleEmitterConfig emitterConfig;
    emitterConfig.position = config.position;
    emitterConfig.direction = config.direction;
    emitterConfig.spread = config.spread;
    emitterConfig.minSpeed = config.minSpeed;
    emitterConfig.maxSpeed = config.maxSpeed;
    emitterConfig.minLifetime = config.minLifetime;
    emitterConfig.maxLifetime = config.maxLifetime;
    emitterConfig.minSize = config.minSize;
    emitterConfig.maxSize = config.maxSize;
    emitterConfig.sizeOverLifetime = config.sizeOverLifetime;
    emitterConfig.startColor = config.startColor;
    emitterConfig.endColor = config.endColor;
    emitterConfig.particlesPerSecond = config.particlesPerSecond;
    emitterConfig.maxParticles = config.maxParticles;
    emitterConfig.useGravity = config.useGravity;
    emitterConfig.gravity = config.gravity;
    emitterConfig.damping = config.damping;
    emitterConfig.spawnRadius = config.spawnRadius;
    
    auto emitter = std::make_shared<ParticleEmitter>(emitterConfig);
    emitter->Start();
    return emitter;
}

std::shared_ptr<ParticleEmitter> CreateExplosion(const ExplosionConfig& config) {
    ParticleEmitterConfig emitterConfig;
    emitterConfig.position = config.position;
    emitterConfig.direction = config.direction;
    emitterConfig.spread = config.spread;
    emitterConfig.minSpeed = config.minSpeed;
    emitterConfig.maxSpeed = config.maxSpeed;
    emitterConfig.minLifetime = config.minLifetime;
    emitterConfig.maxLifetime = config.maxLifetime;
    emitterConfig.minSize = config.minSize;
    emitterConfig.maxSize = config.maxSize;
    emitterConfig.sizeOverLifetime = config.sizeOverLifetime;
    emitterConfig.startColor = config.startColor;
    emitterConfig.endColor = config.endColor;
    emitterConfig.particlesPerSecond = config.particlesPerSecond;
    emitterConfig.maxParticles = config.maxParticles;
    emitterConfig.useGravity = config.useGravity;
    emitterConfig.gravity = config.gravity;
    emitterConfig.damping = config.damping;
    emitterConfig.spawnRadius = config.spawnRadius;
    
    auto emitter = std::make_shared<ParticleEmitter>(emitterConfig);
    emitter->Emit(config.burstCount);
    emitter->Start();
    return emitter;
}

std::shared_ptr<ParticleEmitter> CreateSmoke(const SmokeConfig& config) {
    ParticleEmitterConfig emitterConfig;
    emitterConfig.position = config.position;
    emitterConfig.direction = config.direction;
    emitterConfig.spread = config.spread;
    emitterConfig.minSpeed = config.minSpeed;
    emitterConfig.maxSpeed = config.maxSpeed;
    emitterConfig.minLifetime = config.minLifetime;
    emitterConfig.maxLifetime = config.maxLifetime;
    emitterConfig.minSize = config.minSize;
    emitterConfig.maxSize = config.maxSize;
    emitterConfig.sizeOverLifetime = config.sizeOverLifetime;
    emitterConfig.startColor = config.startColor;
    emitterConfig.endColor = config.endColor;
    emitterConfig.particlesPerSecond = config.particlesPerSecond;
    emitterConfig.maxParticles = config.maxParticles;
    emitterConfig.useGravity = config.useGravity;
    emitterConfig.gravity = config.gravity;
    emitterConfig.damping = config.damping;
    emitterConfig.spawnRadius = config.spawnRadius;
    emitterConfig.enableTurbulence = config.enableTurbulence;
    emitterConfig.turbulenceStrength = config.turbulenceStrength;
    emitterConfig.turbulenceScale = config.turbulenceScale;
    emitterConfig.turbulenceSpeed = config.turbulenceSpeed;
    emitterConfig.renderMode = config.renderMode; // Use cross-billboard for volumetric smoke
    
    auto emitter = std::make_shared<ParticleEmitter>(emitterConfig);
    emitter->Start();
    return emitter;
}

} // namespace Templates
} // namespace Particles
} // namespace OpenEye
