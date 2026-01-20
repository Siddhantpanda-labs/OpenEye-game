#pragma once

#include "../ParticleEmitter.h"
#include <memory>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Particles {
namespace Templates {

// Configuration structs for each preset type
struct FireConfig {
    glm::vec3 position = glm::vec3(0.0f, 0.5f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
    float spread = 20.0f;
    float minSpeed = 0.5f;
    float maxSpeed = 2.0f;
    float minLifetime = 0.5f;
    float maxLifetime = 1.5f;
    float minSize = 0.1f;
    float maxSize = 0.4f;
    float sizeOverLifetime = -0.3f;
    glm::vec4 startColor = glm::vec4(1.0f, 0.3f, 0.0f, 1.0f);
    glm::vec4 endColor = glm::vec4(0.8f, 0.1f, 0.0f, 0.0f);
    float particlesPerSecond = 150.0f;
    int maxParticles = 1000;
    bool useGravity = false;
    glm::vec3 gravity = glm::vec3(0.0f, 2.0f, 0.0f);
    float damping = 0.98f;
};

struct WaterConfig {
    glm::vec3 position = glm::vec3(0.0f, 2.0f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    float spread = 5.0f;
    float minSpeed = 2.0f;
    float maxSpeed = 5.0f;
    float minLifetime = 1.0f;
    float maxLifetime = 2.0f;
    float minSize = 0.05f;
    float maxSize = 0.15f;
    float sizeOverLifetime = 0.0f;
    glm::vec4 startColor = glm::vec4(0.2f, 0.5f, 1.0f, 0.8f);
    glm::vec4 endColor = glm::vec4(0.3f, 0.6f, 1.0f, 0.6f);
    float particlesPerSecond = 200.0f;
    int maxParticles = 2000;
    bool useGravity = true;
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float damping = 0.99f;
    float spawnRadius = 0.1f;
};

struct ExplosionConfig {
    glm::vec3 position = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
    float spread = 180.0f;
    float minSpeed = 5.0f;
    float maxSpeed = 15.0f;
    float minLifetime = 0.3f;
    float maxLifetime = 1.0f;
    float minSize = 0.2f;
    float maxSize = 0.8f;
    float sizeOverLifetime = -0.5f;
    glm::vec4 startColor = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f);
    glm::vec4 endColor = glm::vec4(0.5f, 0.1f, 0.0f, 0.0f);
    float particlesPerSecond = 0.0f; // Burst only
    int maxParticles = 500;
    int burstCount = 500;
    bool useGravity = true;
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float damping = 0.95f;
    float spawnRadius = 0.0f;
};

struct SmokeConfig {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
    float spread = 15.0f;
    float minSpeed = 0.3f;
    float maxSpeed = 1.5f;
    float minLifetime = 2.0f;
    float maxLifetime = 4.0f;
    float minSize = 0.2f;
    float maxSize = 1.0f;
    float sizeOverLifetime = 0.5f; // Smoke grows over time
    glm::vec4 startColor = glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);
    glm::vec4 endColor = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
    float particlesPerSecond = 50.0f;
    int maxParticles = 800;
    bool useGravity = false;
    glm::vec3 gravity = glm::vec3(0.0f, 0.5f, 0.0f); // Slight upward force
    float damping = 0.99f;
    float spawnRadius = 0.05f;
    bool enableTurbulence = true;
    float turbulenceStrength = 0.8f;
    float turbulenceScale = 2.0f;
    float turbulenceSpeed = 1.0f;
    ParticleRenderMode renderMode = ParticleRenderMode::CrossBillboard; // Use cross-billboard for volumetric smoke
};

// Factory functions - create configured particle emitters
std::shared_ptr<ParticleEmitter> CreateFire(const FireConfig& config = FireConfig{});
std::shared_ptr<ParticleEmitter> CreateWater(const WaterConfig& config = WaterConfig{});
std::shared_ptr<ParticleEmitter> CreateExplosion(const ExplosionConfig& config = ExplosionConfig{});
std::shared_ptr<ParticleEmitter> CreateSmoke(const SmokeConfig& config = SmokeConfig{});

} // namespace Templates
} // namespace Particles
} // namespace OpenEye
