#include "ParticleEmitter.h"
#include "../rendering/resources/Texture.h"
#include <random>
#include <cmath>
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
    m_particles.reserve(config.maxParticles);
}

ParticleEmitter::~ParticleEmitter() = default;

void ParticleEmitter::Update(float deltaTime) {
    if (!m_isPlaying) {
        return;
    }
    
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
        if (m_particles.size() < m_config.maxParticles) {
            m_particles.resize(m_particles.size() + 1);
            index = static_cast<int>(m_particles.size() - 1);
        } else {
            return; // Max particles reached
        }
    }
    
    Particle& p = m_particles[index];
    
    // Initialize position
    p.position = m_config.position;
    
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
    
    // Mark as active
    if (index >= static_cast<int>(m_activeParticleCount)) {
        m_activeParticleCount = index + 1;
    }
}

void ParticleEmitter::UpdateParticle(Particle& particle, float deltaTime) {
    // Update physics
    if (m_config.useGravity) {
        particle.velocity += m_config.gravity * deltaTime;
    }
    
    // Apply damping
    particle.velocity *= m_config.damping;
    
    // Update position
    particle.position += particle.velocity * deltaTime;
    
    // Update lifetime
    particle.life -= deltaTime;
    
    // Update size
    if (m_config.sizeOverLifetime != 0.0f) {
        float lifeRatio = particle.life / particle.maxLife;
        particle.size += m_config.sizeOverLifetime * deltaTime;
    }
    
    // Update color (lerp from start to end)
    float lifeRatio = particle.life / particle.maxLife;
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

