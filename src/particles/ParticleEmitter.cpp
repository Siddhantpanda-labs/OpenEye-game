#include "ParticleEmitter.h"
#include "../rendering/resources/Texture.h"
#include "../core/time/Time.h"
#include <random>
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace OpenEye {
namespace Particles {

ParticleEmitter::ParticleEmitter()
    : m_activeParticleCount(0)
    , m_emissionTimer(0.0f)
    , m_isPlaying(true)
{
    m_particles.reserve(1000);
}

ParticleEmitter::ParticleEmitter(const ParticleEmitterConfig& config)
    : m_config(config)
    , m_activeParticleCount(0)
    , m_emissionTimer(0.0f)
    , m_isPlaying(true)
{
    // Lazy allocation: reserve smaller initial pool, grow as needed
    // This prevents 10-15s freeze during initialization
    size_t initialReserve = std::min(config.maxParticles, 1000);
    m_particles.reserve(initialReserve);
}

ParticleEmitter::~ParticleEmitter() = default;

void ParticleEmitter::Update(float deltaTime) {
    if (!m_isPlaying) {
        return;
    }
    
    // Clamp deltaTime to prevent huge jumps that kill particles instantly
    deltaTime = std::min(deltaTime, 0.1f);
    
    // Update existing particles
    for (size_t i = 0; i < m_activeParticleCount; ++i) {
        UpdateParticle(m_particles[i], deltaTime);
    }
    
    // Remove dead particles (swap with last active)
    for (size_t i = 0; i < m_activeParticleCount; ) {
        if (m_particles[i].life <= 0.0f) {
            // Swap with last active particle
            if (i < m_activeParticleCount - 1) {
                std::swap(m_particles[i], m_particles[m_activeParticleCount - 1]);
            }
            m_activeParticleCount--;
        } else {
            i++;
        }
    }
    
    // Emit new particles
    if (m_config.particlesPerSecond > 0.0f) {
        float emissionInterval = 1.0f / m_config.particlesPerSecond;
        m_emissionTimer += deltaTime;
        
        while (m_emissionTimer >= emissionInterval && m_activeParticleCount < m_config.maxParticles) {
            SpawnParticle();
            m_emissionTimer -= emissionInterval;
        }
    }
}

void ParticleEmitter::Emit(int count) {
    for (int i = 0; i < count && m_activeParticleCount < m_config.maxParticles; ++i) {
        SpawnParticle();
    }
}

void ParticleEmitter::SpawnParticle() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // Find or create particle slot
    int index = FindDeadParticle();
    if (index < 0) {
        // Create new particle at the end (lazy grow pool)
        if (m_particles.size() < m_config.maxParticles) {
            // Grow vector in chunks to avoid constant reallocation
            if (m_particles.size() == m_particles.capacity()) {
                size_t newCapacity = std::min(m_particles.capacity() + 1000, static_cast<size_t>(m_config.maxParticles));
                m_particles.reserve(newCapacity);
            }
            m_particles.resize(m_particles.size() + 1);
            index = static_cast<int>(m_particles.size() - 1);
            m_activeParticleCount = index + 1;
        } else {
            return; // Max particles reached
        }
    }
    // else: reusing dead particle at 'index' (already in active range)
    
    Particle& p = m_particles[index];
    
    // Initialize position (spawn within sphere radius if > 0)
    if (m_config.spawnRadius > 0.0f) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        glm::vec3 offset(0.0f);
        // Rejection sampling to get point inside unit sphere
        for (int tries = 0; tries < 8; ++tries) {
            offset = glm::vec3(dist(gen), dist(gen), dist(gen));
            if (glm::dot(offset, offset) <= 1.0f) break;
        }
        offset = glm::normalize(offset) * (m_config.spawnRadius * std::abs(dist(gen)));
        p.position = m_config.position + offset;
    } else {
        p.position = m_config.position;
    }
    
    // Initialize velocity (random direction within spread)
    std::uniform_real_distribution<float> spreadDist(-m_config.spread * 0.0174533f, m_config.spread * 0.0174533f); // Convert to radians
    std::uniform_real_distribution<float> speedDist(m_config.minSpeed, m_config.maxSpeed);
    
    float spreadX = spreadDist(gen);
    float spreadY = spreadDist(gen);
    float speed = speedDist(gen);
    
    // Calculate direction with spread
    glm::vec3 right = glm::cross(m_config.direction, glm::vec3(0.0f, 1.0f, 0.0f));
    if (glm::length(right) < 0.01f) {
        right = glm::cross(m_config.direction, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    right = glm::normalize(right);
    glm::vec3 up = glm::normalize(glm::cross(right, m_config.direction));
    
    // Calculate direction with spread using quaternion rotation
    glm::quat rotX = glm::angleAxis(spreadX, right);
    glm::quat rotY = glm::angleAxis(spreadY, up);
    glm::quat combinedRot = rotY * rotX;
    glm::vec3 dir = combinedRot * glm::normalize(m_config.direction);
    
    p.velocity = dir * speed;
    
    // Initialize lifetime
    std::uniform_real_distribution<float> lifeDist(m_config.minLifetime, m_config.maxLifetime);
    p.maxLife = lifeDist(gen);
    p.life = p.maxLife;
    
    // Initialize size
    std::uniform_real_distribution<float> sizeDist(m_config.minSize, m_config.maxSize);
    p.size = sizeDist(gen);
    
    // Initialize color
    p.color = m_config.startColor;
    
    // Initialize rotation
    p.rotation = 0.0f;
    std::uniform_real_distribution<float> rotSpeedDist(m_config.minRotationSpeed, m_config.maxRotationSpeed);
    p.rotationSpeed = rotSpeedDist(gen);
}

void ParticleEmitter::UpdateParticle(Particle& particle, float deltaTime) {
    // Update physics
    if (m_config.useGravity) {
        particle.velocity += m_config.gravity * deltaTime;
    }
    
    // Apply wind force
    if (m_config.windStrength > 0.0f) {
        glm::vec3 windForce = glm::normalize(m_config.windDirection) * m_config.windStrength * deltaTime;
        particle.velocity += windForce;
    }
    
    // Apply turbulence (optimized: cache time, simplified calculation)
    if (m_config.enableTurbulence && m_config.turbulenceStrength > 0.0f) {
        static float cachedTime = 0.0f;
        static float lastUpdateTime = 0.0f;
        float currentTime = OpenEye::Core::Time::TotalTime();
        
        // Update cache every 0.1s to reduce expensive Time::TotalTime() calls
        if (currentTime - lastUpdateTime > 0.1f) {
            cachedTime = currentTime;
            lastUpdateTime = currentTime;
        }
        
        glm::vec3 pos = particle.position * m_config.turbulenceScale;
        float t = cachedTime * m_config.turbulenceSpeed;
        
        // Simplified turbulence (faster than full curl calculation)
        glm::vec3 curl = glm::vec3(
            sin(pos.y * 0.7f + t) - cos(pos.z * 0.5f + t * 0.8f),
            sin(pos.z * 0.7f + t) - cos(pos.x * 0.5f + t * 0.8f),
            sin(pos.x * 0.7f + t) - cos(pos.y * 0.5f + t * 0.8f)
        );
        curl = glm::normalize(curl) * m_config.turbulenceStrength * deltaTime;
        particle.velocity += curl;
    }
    
    // Apply damping
    particle.velocity *= m_config.damping;
    
    // Update position
    particle.position += particle.velocity * deltaTime;
    
    // Update lifetime
    particle.life -= deltaTime;
    
    // Update size with exponential growth and power curve
    float lifeRatio = particle.life / particle.maxLife;
    float ageRatio = 1.0f - lifeRatio; // 0 = just spawned, 1 = end of life
    
    if (m_config.sizeGrowthRate > 0.0f) {
        // Exponential growth over time
        particle.size *= (1.0f + m_config.sizeGrowthRate * deltaTime);
    } else if (m_config.sizeOverLifetime != 0.0f) {
        // Linear growth
        particle.size += m_config.sizeOverLifetime * deltaTime;
    }
    
    // Apply power curve for size based on age
    if (m_config.sizeGrowthExponent != 1.0f && ageRatio > 0.0f) {
        float baseSize = particle.size;
        float sizeMultiplier = std::pow(ageRatio, m_config.sizeGrowthExponent);
        particle.size = baseSize * (0.3f + 0.7f * sizeMultiplier); // Start at 30% size, grow to 100%
    }
    
    // Update color (lerp from start to end)
    particle.color = glm::mix(m_config.endColor, m_config.startColor, lifeRatio);
    
    // Update rotation
    particle.rotation += particle.rotationSpeed * deltaTime;
}

int ParticleEmitter::FindDeadParticle() {
    // Look for dead particle in active range
    for (size_t i = 0; i < m_activeParticleCount; ++i) {
        if (m_particles[i].life <= 0.0f) {
            return static_cast<int>(i);
        }
    }
    
    // If no dead particles found and we have space, return -1 (will create new)
    if (m_activeParticleCount < m_config.maxParticles) {
        return -1;
    }
    
    // Reuse oldest particle (first in list)
    return 0;
}

void ParticleEmitter::Reset() {
    m_particles.clear();
    m_activeParticleCount = 0;
    m_emissionTimer = 0.0f;
}

} // namespace Particles
} // namespace OpenEye

