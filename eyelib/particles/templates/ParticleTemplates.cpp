#include "eyelib/particles/templates/ParticleTemplates.h"

namespace eyelib {
namespace particles {

EmitterConfig ParticleTemplates::CreateFire() {
    EmitterConfig cfg{};
    cfg.direction = math::Vec3(0, 1, 0);
    cfg.spreadDegrees = 20.0f;
    cfg.minSpeed = 0.5f;
    cfg.maxSpeed = 2.0f;
    cfg.particlesPerSecond = 120.0f;
    cfg.maxParticles = 1000;
    cfg.minLifetime = 0.4f;
    cfg.maxLifetime = 1.2f;
    cfg.minSize = 0.05f;
    cfg.maxSize = 0.25f;
    cfg.startColor = math::Vec4(1.0f, 0.6f, 0.1f, 1.0f);
    cfg.endColor = math::Vec4(0.8f, 0.1f, 0.05f, 0.0f);
    cfg.useGravity = false;
    cfg.wind = math::Vec3(0.0f, 0.4f, 0.0f);
    cfg.useTurbulence = true;
    cfg.turbulenceStrength = 1.5f;
    cfg.turbulenceFrequency = 2.0f;
    return cfg;
}

EmitterConfig ParticleTemplates::CreateSmoke() {
    EmitterConfig cfg{};
    cfg.direction = math::Vec3(0, 1, 0);
    cfg.spreadDegrees = 25.0f;
    cfg.minSpeed = 0.2f;
    cfg.maxSpeed = 0.8f;
    cfg.particlesPerSecond = 60.0f;
    cfg.maxParticles = 800;
    cfg.minLifetime = 2.0f;
    cfg.maxLifetime = 4.5f;
    cfg.minSize = 0.15f;
    cfg.maxSize = 0.5f;
    cfg.startColor = math::Vec4(0.3f, 0.3f, 0.3f, 0.6f);
    cfg.endColor = math::Vec4(0.2f, 0.2f, 0.2f, 0.0f);
    cfg.useGravity = false;
    cfg.wind = math::Vec3(0.1f, 0.4f, 0.0f);
    cfg.useTurbulence = true;
    cfg.turbulenceStrength = 0.8f;
    cfg.turbulenceFrequency = 1.5f;
    return cfg;
}

EmitterConfig ParticleTemplates::CreateExplosion() {
    EmitterConfig cfg{};
    cfg.direction = math::Vec3(0, 1, 0);
    cfg.spreadDegrees = 180.0f;
    cfg.minSpeed = 4.0f;
    cfg.maxSpeed = 10.0f;
    cfg.particlesPerSecond = 0.0f; // use burst
    cfg.maxParticles = 600;
    cfg.minLifetime = 0.6f;
    cfg.maxLifetime = 1.2f;
    cfg.minSize = 0.08f;
    cfg.maxSize = 0.35f;
    cfg.startColor = math::Vec4(1.0f, 0.7f, 0.2f, 1.0f);
    cfg.endColor = math::Vec4(0.5f, 0.1f, 0.05f, 0.0f);
    cfg.useGravity = true;
    cfg.gravity = math::Vec3(0.0f, -6.0f, 0.0f);
    cfg.damping = 0.9f;
    return cfg;
}

EmitterConfig ParticleTemplates::CreateRain() {
    EmitterConfig cfg{};
    cfg.direction = math::Vec3(0, -1, 0);
    cfg.spreadDegrees = 5.0f;
    cfg.minSpeed = 10.0f;
    cfg.maxSpeed = 16.0f;
    cfg.particlesPerSecond = 300.0f;
    cfg.maxParticles = 2000;
    cfg.minLifetime = 0.4f;
    cfg.maxLifetime = 0.9f;
    cfg.minSize = 0.02f;
    cfg.maxSize = 0.04f;
    cfg.startColor = math::Vec4(0.6f, 0.7f, 0.9f, 0.7f);
    cfg.endColor = math::Vec4(0.6f, 0.7f, 0.9f, 0.0f);
    cfg.useGravity = true;
    cfg.gravity = math::Vec3(0.0f, -20.0f, 0.0f);
    cfg.damping = 0.99f;
    return cfg;
}

EmitterConfig ParticleTemplates::CreateSnow() {
    EmitterConfig cfg{};
    cfg.direction = math::Vec3(0, -1, 0);
    cfg.spreadDegrees = 15.0f;
    cfg.minSpeed = 0.4f;
    cfg.maxSpeed = 1.2f;
    cfg.particlesPerSecond = 120.0f;
    cfg.maxParticles = 1500;
    cfg.minLifetime = 4.0f;
    cfg.maxLifetime = 8.0f;
    cfg.minSize = 0.03f;
    cfg.maxSize = 0.08f;
    cfg.startColor = math::Vec4(1.0f, 1.0f, 1.0f, 0.8f);
    cfg.endColor = math::Vec4(1.0f, 1.0f, 1.0f, 0.0f);
    cfg.useGravity = true;
    cfg.gravity = math::Vec3(0.0f, -1.5f, 0.0f);
    cfg.wind = math::Vec3(0.4f, 0.0f, 0.0f);
    cfg.useTurbulence = true;
    cfg.turbulenceStrength = 0.4f;
    cfg.turbulenceFrequency = 0.8f;
    return cfg;
}

EmitterConfig ParticleTemplates::CreateSparkles() {
    EmitterConfig cfg{};
    cfg.direction = math::Vec3(0, 1, 0);
    cfg.spreadDegrees = 60.0f;
    cfg.minSpeed = 1.0f;
    cfg.maxSpeed = 3.0f;
    cfg.particlesPerSecond = 80.0f;
    cfg.maxParticles = 400;
    cfg.minLifetime = 0.4f;
    cfg.maxLifetime = 1.0f;
    cfg.minSize = 0.03f;
    cfg.maxSize = 0.09f;
    cfg.startColor = math::Vec4(1.0f, 0.9f, 0.4f, 1.0f);
    cfg.endColor = math::Vec4(1.0f, 0.9f, 0.4f, 0.0f);
    cfg.useGravity = false;
    cfg.damping = 0.95f;
    return cfg;
}

} // namespace particles
} // namespace eyelib
