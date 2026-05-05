#include "api/particles/ParticleTemplates.h"

#include "eyelib/particles/templates/ParticleTemplates.h"

namespace eyelib {
namespace api {

namespace {
static CollisionPlane FromInternalPlane(const eyelib::particles::CollisionPlane& p) {
    CollisionPlane out;
    out.normal = p.normal;
    out.distance = p.distance;
    out.restitution = p.restitution;
    out.friction = p.friction;
    return out;
}

static std::vector<CollisionPlane> FromInternalPlanes(const std::vector<eyelib::particles::CollisionPlane>& planes) {
    std::vector<CollisionPlane> out;
    out.reserve(planes.size());
    for (const auto& p : planes) out.push_back(FromInternalPlane(p));
    return out;
}

static EmitterConfig FromInternal(const eyelib::particles::EmitterConfig& c) {
    EmitterConfig out;
    out.position = c.position;
    out.direction = c.direction;
    out.spreadDegrees = c.spreadDegrees;
    out.minSpeed = c.minSpeed;
    out.maxSpeed = c.maxSpeed;
    out.particlesPerSecond = c.particlesPerSecond;
    out.maxParticles = c.maxParticles;
    out.minLifetime = c.minLifetime;
    out.maxLifetime = c.maxLifetime;
    out.minSize = c.minSize;
    out.maxSize = c.maxSize;
    out.startColor = c.startColor;
    out.endColor = c.endColor;
    out.useGravity = c.useGravity;
    out.gravity = c.gravity;
    out.damping = c.damping;
    out.wind = c.wind;
    out.useTurbulence = c.useTurbulence;
    out.turbulenceStrength = c.turbulenceStrength;
    out.turbulenceFrequency = c.turbulenceFrequency;
    out.enableCollisions = c.enableCollisions;
    out.collisionPlanes = FromInternalPlanes(c.collisionPlanes);
    out.subEmitter = c.subEmitter;
    out.subEmitCount = c.subEmitCount;
    out.enableTrails = c.enableTrails;
    out.trailWidth = c.trailWidth;
    return out;
}
} // namespace

EmitterConfig ParticleTemplates::CreateFire() {
    return FromInternal(eyelib::particles::ParticleTemplates::CreateFire());
}

EmitterConfig ParticleTemplates::CreateSmoke() {
    return FromInternal(eyelib::particles::ParticleTemplates::CreateSmoke());
}

EmitterConfig ParticleTemplates::CreateExplosion() {
    return FromInternal(eyelib::particles::ParticleTemplates::CreateExplosion());
}

EmitterConfig ParticleTemplates::CreateRain() {
    return FromInternal(eyelib::particles::ParticleTemplates::CreateRain());
}

EmitterConfig ParticleTemplates::CreateSnow() {
    return FromInternal(eyelib::particles::ParticleTemplates::CreateSnow());
}

EmitterConfig ParticleTemplates::CreateSparkles() {
    return FromInternal(eyelib::particles::ParticleTemplates::CreateSparkles());
}

} // namespace api
} // namespace eyelib
