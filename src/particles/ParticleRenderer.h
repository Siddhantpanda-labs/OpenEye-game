#pragma once

#include <glm/glm.hpp>
#include <memory>
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
                const glm::mat4& projectionMatrix);
    
    // Shutdown
    void Shutdown();
    
private:
    // Quad mesh for particles (billboard)
    unsigned int m_quadVAO;
    unsigned int m_quadVBO;
    unsigned int m_instanceVBO;  // Per-instance data (position, size, color, rotation)
    
    // Shader
    std::shared_ptr<Rendering::Shader> m_shader;
    
    // Per-instance data structure
    struct InstanceData {
        glm::vec3 position;
        float size;
        glm::vec4 color;
        float rotation;
    };
    
    void CreateQuad();
    void UpdateInstanceData(const Particle& particle, InstanceData& instance);
};

} // namespace Particles
} // namespace OpenEye

