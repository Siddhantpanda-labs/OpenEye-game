#include "ParticleSystem.h"

#include "eyelib/particles/ParticleSystem.h"

namespace eyelib {
namespace api {

namespace {
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
    return out;
}

static Particle FromInternal(const eyelib::particles::Particle& p) {
    Particle out;
    out.position = p.position;
    out.velocity = p.velocity;
    out.color = p.color;
    out.size = p.size;
    out.life = p.life;
    out.maxLife = p.maxLife;
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

