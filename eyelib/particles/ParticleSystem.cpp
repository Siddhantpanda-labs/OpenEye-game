#include "ParticleSystem.h"

#include "eyelib/core/Log.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>

namespace eyelib {
namespace particles {

namespace {
static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

static math::Vec4 Lerp(const math::Vec4& a, const math::Vec4& b, float t) {
    return math::Vec4(
        Lerp(a.x, b.x, t),
        Lerp(a.y, b.y, t),
        Lerp(a.z, b.z, t),
        Lerp(a.w, b.w, t)
    );
}

struct EmitterState {
    EmitterConfig cfg{};
    std::vector<Particle> particles{};
    float emitAccumulator = 0.0f;
    bool playing = true;
    uint32_t activeCount = 0;
};

static math::Vec3 RandomDirectionInCone(std::mt19937& rng, const math::Vec3& dir, float spreadDeg) {
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    const float u = dist01(rng);
    const float v = dist01(rng);

    const float spreadRad = glm::radians(spreadDeg);
    const float cosTheta = Lerp(std::cos(spreadRad), 1.0f, u);
    const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    const float phi = 2.0f * 3.1415926535f * v;

    // Local cone direction around +Z
    const math::Vec3 local(
        std::cos(phi) * sinTheta,
        std::sin(phi) * sinTheta,
        cosTheta
    );

    // Build an orthonormal basis around dir
    math::Vec3 w = glm::normalize(dir);
    math::Vec3 up = (std::abs(w.y) < 0.99f) ? math::Vec3(0, 1, 0) : math::Vec3(1, 0, 0);
    math::Vec3 uAxis = glm::normalize(glm::cross(up, w));
    math::Vec3 vAxis = glm::cross(w, uAxis);

    return glm::normalize(uAxis * local.x + vAxis * local.y + w * local.z);
}
} // namespace

class ParticleSystem::Impl {
public:
    std::unordered_map<EmitterHandle, EmitterState> emitters{};
    EmitterHandle next = 1;
    std::mt19937 rng{ std::random_device{}() };
};

ParticleSystem::ParticleSystem() : m_impl(new Impl()) {}

ParticleSystem::~ParticleSystem() {
    delete m_impl;
    m_impl = nullptr;
}

EmitterHandle ParticleSystem::CreateEmitter(const EmitterConfig& config) {
    const auto h = m_impl->next++;
    EmitterState st;
    st.cfg = config;
    st.particles.resize(config.maxParticles);
    st.emitAccumulator = 0.0f;
    st.playing = true;
    st.activeCount = 0;
    m_impl->emitters.emplace(h, std::move(st));
    return h;
}

void ParticleSystem::DestroyEmitter(EmitterHandle emitter) {
    m_impl->emitters.erase(emitter);
}

bool ParticleSystem::IsValid(EmitterHandle emitter) const {
    return emitter != NullEmitter && m_impl->emitters.find(emitter) != m_impl->emitters.end();
}

void ParticleSystem::SetEmitterConfig(EmitterHandle emitter, const EmitterConfig& config) {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return;
    it->second.cfg = config;
    it->second.particles.resize(config.maxParticles);
    it->second.activeCount = std::min<uint32_t>(it->second.activeCount, config.maxParticles);
}

EmitterConfig ParticleSystem::GetEmitterConfig(EmitterHandle emitter) const {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return {};
    return it->second.cfg;
}

void ParticleSystem::Start(EmitterHandle emitter) {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return;
    it->second.playing = true;
}

void ParticleSystem::Stop(EmitterHandle emitter) {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return;
    it->second.playing = false;
}

void ParticleSystem::Reset(EmitterHandle emitter) {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return;
    for (auto& p : it->second.particles) {
        p.life = 0.0f;
        p.maxLife = 0.0f;
    }
    it->second.activeCount = 0;
    it->second.emitAccumulator = 0.0f;
}

bool ParticleSystem::IsPlaying(EmitterHandle emitter) const {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return false;
    return it->second.playing;
}

void ParticleSystem::EmitBurst(EmitterHandle emitter, uint32_t count) {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return;
    auto& st = it->second;

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distLife(st.cfg.minLifetime, st.cfg.maxLifetime);
    std::uniform_real_distribution<float> distSize(st.cfg.minSize, st.cfg.maxSize);
    std::uniform_real_distribution<float> distSpeed(st.cfg.minSpeed, st.cfg.maxSpeed);

    for (uint32_t i = 0; i < count; ++i) {
        // find a dead slot
        auto deadIt = std::find_if(st.particles.begin(), st.particles.end(), [](const Particle& p) { return p.life <= 0.0f; });
        if (deadIt == st.particles.end()) break;

        Particle& p = *deadIt;
        p.position = st.cfg.position;
        p.size = distSize(m_impl->rng);
        p.maxLife = distLife(m_impl->rng);
        p.life = p.maxLife;
        p.color = st.cfg.startColor;

        const auto dir = RandomDirectionInCone(m_impl->rng, st.cfg.direction, st.cfg.spreadDegrees);
        const float spd = distSpeed(m_impl->rng);
        p.velocity = dir * spd;
    }
}

void ParticleSystem::Update(float dt) {
    if (dt <= 0.0f) return;

    for (auto& kv : m_impl->emitters) {
        auto& st = kv.second;

        // Spawn
        if (st.playing && st.cfg.particlesPerSecond > 0.0f) {
            st.emitAccumulator += dt * st.cfg.particlesPerSecond;
            const uint32_t toEmit = static_cast<uint32_t>(st.emitAccumulator);
            if (toEmit > 0) {
                st.emitAccumulator -= static_cast<float>(toEmit);
                EmitBurst(kv.first, toEmit);
            }
        }

        // Update particles
        uint32_t alive = 0;
        for (auto& p : st.particles) {
            if (p.life <= 0.0f) continue;
            p.life -= dt;
            if (p.life <= 0.0f) {
                p.life = 0.0f;
                continue;
            }

            if (st.cfg.useGravity) {
                p.velocity += st.cfg.gravity * dt;
            }
            p.velocity *= std::pow(st.cfg.damping, dt * 60.0f);
            p.position += p.velocity * dt;

            const float t = 1.0f - (p.life / std::max(0.0001f, p.maxLife));
            p.color = Lerp(st.cfg.startColor, st.cfg.endColor, t);
            ++alive;
        }
        st.activeCount = alive;
    }
}

uint32_t ParticleSystem::GetActiveCount(EmitterHandle emitter) const {
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return 0;
    return it->second.activeCount;
}

std::vector<Particle> ParticleSystem::Snapshot(EmitterHandle emitter) const {
    std::vector<Particle> out;
    auto it = m_impl->emitters.find(emitter);
    if (it == m_impl->emitters.end()) return out;
    const auto& st = it->second;
    out.reserve(st.activeCount);
    for (const auto& p : st.particles) {
        if (p.life > 0.0f) out.push_back(p);
    }
    return out;
}

} // namespace particles
} // namespace eyelib

