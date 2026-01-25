#include "ParticleSystem.h"

#include "eyelib/particles/ParticleSystem.h"

namespace eyelib {
namespace api {

namespace {
static eyelib::particles::CollisionPlane ToInternalPlane(const CollisionPlane& p) {
    eyelib::particles::CollisionPlane out;
    out.normal = p.normal;
    out.distance = p.distance;
    out.restitution = p.restitution;
    out.friction = p.friction;
    return out;
}

static CollisionPlane FromInternalPlane(const eyelib::particles::CollisionPlane& p) {
    CollisionPlane out;
    out.normal = p.normal;
    out.distance = p.distance;
    out.restitution = p.restitution;
    out.friction = p.friction;
    return out;
}

static std::vector<eyelib::particles::CollisionPlane> ToInternalPlanes(const std::vector<CollisionPlane>& planes) {
    std::vector<eyelib::particles::CollisionPlane> out;
    out.reserve(planes.size());
    for (const auto& p : planes) out.push_back(ToInternalPlane(p));
    return out;
}

static std::vector<CollisionPlane> FromInternalPlanes(const std::vector<eyelib::particles::CollisionPlane>& planes) {
    std::vector<CollisionPlane> out;
    out.reserve(planes.size());
    for (const auto& p : planes) out.push_back(FromInternalPlane(p));
    return out;
}

static eyelib::particles::EmitterConfig ToInternal(const EmitterConfig& c) {
    eyelib::particles::EmitterConfig out;
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
    out.collisionPlanes = ToInternalPlanes(c.collisionPlanes);
    out.subEmitter = c.subEmitter;
    out.subEmitCount = c.subEmitCount;
    out.enableTrails = c.enableTrails;
    out.trailWidth = c.trailWidth;
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

static Particle FromInternal(const eyelib::particles::Particle& p) {
    Particle out;
    out.position = p.position;
    out.prevPosition = p.prevPosition;
    out.velocity = p.velocity;
    out.color = p.color;
    out.size = p.size;
    out.life = p.life;
    out.maxLife = p.maxLife;
    out.age = p.age;
    out.hasPrev = p.hasPrev;
    return out;
}
} // namespace

class ParticleSystem::Impl {
public:
    eyelib::particles::ParticleSystem sys{};
};

ParticleSystem::ParticleSystem() : m_impl(new Impl()) {}

ParticleSystem::~ParticleSystem() {
    delete m_impl;
    m_impl = nullptr;
}

EmitterHandle ParticleSystem::CreateEmitter(const EmitterConfig& config) {
    return m_impl->sys.CreateEmitter(ToInternal(config));
}

void ParticleSystem::DestroyEmitter(EmitterHandle emitter) {
    m_impl->sys.DestroyEmitter(emitter);
}

bool ParticleSystem::IsValid(EmitterHandle emitter) const {
    return m_impl->sys.IsValid(emitter);
}

void ParticleSystem::SetEmitterConfig(EmitterHandle emitter, const EmitterConfig& config) {
    m_impl->sys.SetEmitterConfig(emitter, ToInternal(config));
}

EmitterConfig ParticleSystem::GetEmitterConfig(EmitterHandle emitter) const {
    return FromInternal(m_impl->sys.GetEmitterConfig(emitter));
}

void ParticleSystem::Start(EmitterHandle emitter) {
    m_impl->sys.Start(emitter);
}

void ParticleSystem::Stop(EmitterHandle emitter) {
    m_impl->sys.Stop(emitter);
}

void ParticleSystem::Reset(EmitterHandle emitter) {
    m_impl->sys.Reset(emitter);
}

bool ParticleSystem::IsPlaying(EmitterHandle emitter) const {
    return m_impl->sys.IsPlaying(emitter);
}

void ParticleSystem::EmitBurst(EmitterHandle emitter, uint32_t count) {
    m_impl->sys.EmitBurst(emitter, count);
}

void ParticleSystem::Update(float dt) {
    m_impl->sys.Update(dt);
}

uint32_t ParticleSystem::GetActiveCount(EmitterHandle emitter) const {
    return m_impl->sys.GetActiveCount(emitter);
}

std::vector<Particle> ParticleSystem::Snapshot(EmitterHandle emitter) const {
    auto snap = m_impl->sys.Snapshot(emitter);
    std::vector<Particle> out;
    out.reserve(snap.size());
    for (const auto& p : snap) out.push_back(FromInternal(p));
    return out;
}

} // namespace api
} // namespace eyelib

