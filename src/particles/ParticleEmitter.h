#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace OpenEye {
namespace Particles {

// Individual particle
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;        // RGBA
    float size;
    float life;              // Remaining lifetime (0 = dead)
    float maxLife;           // Maximum lifetime
    float rotation;
    float rotationSpeed;
};

// Particle emitter configuration
struct ParticleEmitterConfig {
    // Emission
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
    float spread = 15.0f;    // Spread angle in degrees
    float minSpeed = 1.0f;
    float maxSpeed = 3.0f;
    
    // Lifetime
    float minLifetime = 1.0f;
    float maxLifetime = 3.0f;
    
    // Size
    float minSize = 0.1f;
    float maxSize = 0.5f;
    float sizeOverLifetime = 0.0f;  // Size change rate (0 = constant)
    
    // Color
    glm::vec4 startColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    
    // Emission rate
    float particlesPerSecond = 100.0f;
    int maxParticles = 1000;
    
    // Physics
    bool useGravity = true;
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float damping = 0.98f;   // Velocity damping
    
    // Rotation
    float minRotationSpeed = -180.0f;
    float maxRotationSpeed = 180.0f;
    
    // Texture (optional - for textured particles)
    std::shared_ptr<class Texture> texture = nullptr;
};

// Particle emitter component
class ParticleEmitter {
public:
    ParticleEmitter();
    ParticleEmitter(const ParticleEmitterConfig& config);
    ~ParticleEmitter();
    
    // Update emitter (spawns and updates particles)
    void Update(float deltaTime);
    
    // Render particles (called by ParticleSystem)
    const std::vector<Particle>& GetParticles() const { return m_particles; }
    size_t GetActiveParticleCount() const { return m_activeParticleCount; }
    
    // Configuration
    void SetConfig(const ParticleEmitterConfig& config) { m_config = config; }
    const ParticleEmitterConfig& GetConfig() const { return m_config; }
    ParticleEmitterConfig& GetConfig() { return m_config; }  // Non-const version for modification
    void SetPosition(const glm::vec3& position) { m_config.position = position; }
    
    // Control
    void Start() { m_isPlaying = true; }
    void Stop() { m_isPlaying = false; }
    void Reset();  // Defined in .cpp
    bool IsPlaying() const { return m_isPlaying; }
    
    // Spawn burst of particles
    void Emit(int count);
    
private:
    ParticleEmitterConfig m_config;
    std::vector<Particle> m_particles;
    size_t m_activeParticleCount;
    float m_emissionTimer;
    bool m_isPlaying;
    
    void SpawnParticle();
    void UpdateParticle(Particle& particle, float deltaTime);
    int FindDeadParticle();
};

} // namespace Particles
} // namespace OpenEye

