#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <cstdint>
#include <vector>
#include "ParticleEmitter.h"

namespace OpenEye {
namespace Rendering {
    class Camera;
    class Shader;
}

namespace Particles {

// GPU-based particle renderer using instanced quads
class ParticleRenderer {
public:
    ParticleRenderer();
    ~ParticleRenderer();
    
    // Initialize renderer (creates quad mesh and shader)
    bool Initialize();
    
    // Render particles from multiple emitters
    void Render(const std::vector<std::shared_ptr<ParticleEmitter>>& emitters,
                const Rendering::Camera& camera,
                const glm::mat4& viewMatrix,
                const glm::mat4& projectionMatrix,
                unsigned int sceneDepthTexture,
                std::uint32_t viewportWidth,
                std::uint32_t viewportHeight);
    
    // Shutdown
    void Shutdown();
    
private:
    // Quad mesh for particles (billboard)
    unsigned int m_quadVAO;
    unsigned int m_quadVBO;
    unsigned int m_instanceVBO;  // Per-instance data (position, size, color, rotation)
    
    // Shaders
    std::shared_ptr<Rendering::Shader> m_shader;           // Standard billboard/cross-billboard shader
    std::shared_ptr<Rendering::Shader> m_volumetricShader; // Volumetric raymarching shader
    
    // Per-instance data structure
    struct InstanceData {
        glm::vec3 position;
        float size;
        glm::vec4 color;
        float rotation;
        float ageNorm; // 0..1 (0 = just spawned, 1 = end of life)
        int quadIndex; // 0, 1, or 2 for cross-billboard (0 for regular billboard)
    };
    
    void CreateQuad();
    void UpdateInstanceData(const Particle& particle, InstanceData& instance);
    void RenderBatch(const ParticleEmitterConfig* config, const std::vector<InstanceData>& instanceData, unsigned int sceneDepthTexture);
};

} // namespace Particles
} // namespace OpenEye

