#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace api {

using EmitterHandle = uint32_t;
static constexpr EmitterHandle NullEmitter = 0;

struct Particle {
    eyelib::math::Vec3 position{0.0f, 0.0f, 0.0f};
    eyelib::math::Vec3 prevPosition{0.0f, 0.0f, 0.0f};
    eyelib::math::Vec3 velocity{0.0f, 0.0f, 0.0f};
    eyelib::math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float size = 0.1f;
    float life = 0.0f;
    float maxLife = 0.0f;
    float age = 0.0f;
    bool hasPrev = false;
};

struct CollisionPlane {
    eyelib::math::Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    float restitution = 0.4f;
    float friction = 0.1f;
};

struct EmitterConfig {
    eyelib::math::Vec3 position{0.0f, 0.0f, 0.0f};
    eyelib::math::Vec3 direction{0.0f, 1.0f, 0.0f};
    float spreadDegrees = 15.0f;
    float minSpeed = 1.0f;
    float maxSpeed = 3.0f;
    float particlesPerSecond = 50.0f;
    uint32_t maxParticles = 512;

    float minLifetime = 1.0f;
    float maxLifetime = 3.0f;

    float minSize = 0.05f;
    float maxSize = 0.25f;

    eyelib::math::Vec4 startColor{1.0f, 1.0f, 1.0f, 1.0f};
    eyelib::math::Vec4 endColor{1.0f, 1.0f, 1.0f, 0.0f};

    bool useGravity = true;
    eyelib::math::Vec3 gravity{0.0f, -9.81f, 0.0f};
    float damping = 0.98f;
    eyelib::math::Vec3 wind{0.0f, 0.0f, 0.0f};
    bool useTurbulence = false;
    float turbulenceStrength = 0.0f;
    float turbulenceFrequency = 1.0f;

    bool enableCollisions = false;
    std::vector<CollisionPlane> collisionPlanes{};

    EmitterHandle subEmitter = NullEmitter;
    uint32_t subEmitCount = 0;

    bool enableTrails = false;
    float trailWidth = 0.02f;
};

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

} // namespace api
} // namespace eyelib

