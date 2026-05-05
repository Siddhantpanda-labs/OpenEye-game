#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace particles {

using EmitterHandle = uint32_t;
static constexpr EmitterHandle NullEmitter = 0;

struct Particle {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Vec3 prevPosition{0.0f, 0.0f, 0.0f};
    math::Vec3 velocity{0.0f, 0.0f, 0.0f};
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float size = 0.1f;
    float life = 0.0f;     // remaining
    float maxLife = 0.0f;  // initial
    float age = 0.0f;
    bool hasPrev = false;
};

struct CollisionPlane {
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;      // plane equation: dot(n, x) + d = 0
    float restitution = 0.4f;   // bounce
    float friction = 0.1f;      // tangential damping
};

struct EmitterConfig {
    // Emission
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Vec3 direction{0.0f, 1.0f, 0.0f};
    float spreadDegrees = 15.0f;
    float minSpeed = 1.0f;
    float maxSpeed = 3.0f;
    float particlesPerSecond = 50.0f;
    uint32_t maxParticles = 512;

    // Lifetime
    float minLifetime = 1.0f;
    float maxLifetime = 3.0f;

    // Size
    float minSize = 0.05f;
    float maxSize = 0.25f;

    // Color
    math::Vec4 startColor{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 endColor{1.0f, 1.0f, 1.0f, 0.0f};

    // Physics
    bool useGravity = true;
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};
    float damping = 0.98f;
    math::Vec3 wind{0.0f, 0.0f, 0.0f};
    bool useTurbulence = false;
    float turbulenceStrength = 0.0f;
    float turbulenceFrequency = 1.0f;

    // Collisions
    bool enableCollisions = false;
    std::vector<CollisionPlane> collisionPlanes{};

    // Sub-emitters
    EmitterHandle subEmitter = NullEmitter;
    uint32_t subEmitCount = 0;

    // Trails
    bool enableTrails = false;
    float trailWidth = 0.02f;
};

/**
 * @brief Minimal particle system (CPU-only). Rendering will consume snapshots later.
 */
class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    EmitterHandle CreateEmitter(const EmitterConfig& config);
    void DestroyEmitter(EmitterHandle emitter);
    bool IsValid(EmitterHandle emitter) const;

    void SetEmitterConfig(EmitterHandle emitter, const EmitterConfig& config);
    EmitterConfig GetEmitterConfig(EmitterHandle emitter) const;

    void Start(EmitterHandle emitter);
    void Stop(EmitterHandle emitter);
    void Reset(EmitterHandle emitter);
    bool IsPlaying(EmitterHandle emitter) const;

    void EmitBurst(EmitterHandle emitter, uint32_t count);
    void Update(float dt);

    uint32_t GetActiveCount(EmitterHandle emitter) const;
    std::vector<Particle> Snapshot(EmitterHandle emitter) const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace particles
} // namespace eyelib

