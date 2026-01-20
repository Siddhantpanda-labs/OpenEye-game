#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

// Forward declare Texture in correct namespace
namespace OpenEye { namespace Rendering { class Texture; } }

namespace OpenEye {
namespace Particles {

// Particle rendering mode
enum class ParticleRenderMode {
    Billboard,      // Single quad facing camera (default)
    CrossBillboard, // 3 perpendicular quads for volumetric appearance
    Volumetric      // Shader-based raymarching (advanced)
};

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
    // Spawn volume
    float spawnRadius = 0.0f;       // Spawn within sphere radius around position
    
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
    
    // Wind/Turbulence (for realistic smoke motion)
    glm::vec3 windDirection = glm::vec3(0.0f, 0.0f, 0.0f);  // Global wind direction
    float windStrength = 0.0f;                               // Wind force strength
    bool enableTurbulence = false;                          // Enable curl noise turbulence
    float turbulenceStrength = 0.5f;                        // Turbulence intensity
    float turbulenceScale = 1.0f;                           // Spatial scale of turbulence
    float turbulenceSpeed = 1.0f;                           // Time speed of turbulence
    
    // Rotation
    float minRotationSpeed = -180.0f;
    float maxRotationSpeed = 180.0f;
    
    // Size curves (exponential growth for smoke puffs)
    float sizeGrowthRate = 0.0f;                            // Exponential growth: size *= (1.0 + growthRate * dt)
    float sizeGrowthExponent = 1.0f;                        // Power curve: size = baseSize * (lifeRatio^exponent)
    
    // Texture (optional - for textured particles)
    std::shared_ptr<OpenEye::Rendering::Texture> texture = nullptr;

    // Flipbook settings (for smoke/fire flipbook atlases)
    int flipbookCols = 0;                 // 0 disables flipbook path
    int flipbookRows = 0;
    int flipbookFrameCount = 0;           // if 0, inferred as rows*cols when rows/cols > 0
    bool flipbookAnimateByLifetime = true;// if true, frame progresses with particle lifetime
    bool flipbookBlendFrames = true;      // smooth blend between frames

    // Rendering controls
    ParticleRenderMode renderMode = ParticleRenderMode::Billboard; // Rendering mode
    bool softParticles = true;            // enable depth-based soft intersection
    float softParticleFadeDistance = 0.5f;// world units fade thickness near geometry
    bool premultipliedAlpha = true;       // output premultiplied alpha for better blending
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

