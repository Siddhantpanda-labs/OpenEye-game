#include "ParticleRenderer.h"
#include "../rendering/Camera.h"
#include "../rendering/resources/Shader.h"
#include "../core/log/Log.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace OpenEye {
namespace Particles {

ParticleRenderer::ParticleRenderer()
    : m_quadVAO(0)
    , m_quadVBO(0)
    , m_instanceVBO(0)
{
}

ParticleRenderer::~ParticleRenderer() {
    Shutdown();
}

bool ParticleRenderer::Initialize() {
    // Create shader
    m_shader = std::make_shared<Rendering::Shader>();
    if (!m_shader->LoadFromFile("assets/shaders/particles/particle.vert", 
                                "assets/shaders/particles/particle.frag")) {
        OE_ERROR("ParticleRenderer: Failed to load particle shader");
        return false;
    }
    
    // Create quad mesh
    CreateQuad();
    
    // Create instance buffer
    glGenBuffers(1, &m_instanceVBO);
    
    OE_INFO("ParticleRenderer initialized");
    return true;
}

void ParticleRenderer::CreateQuad() {
    // Simple quad vertices (centered at origin, size 1x1)
    float quadVertices[] = {
        // positions     // texCoords
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,
        
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f
    };
    
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    // Position and texCoord
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    // Instance data (position, size, color, rotation)
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    
    // Position (vec3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)0);
    glVertexAttribDivisor(2, 1);
    
    // Size (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, size));
    glVertexAttribDivisor(3, 1);
    
    // Color (vec4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, color));
    glVertexAttribDivisor(4, 1);
    
    // Rotation (float)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, rotation));
    glVertexAttribDivisor(5, 1);
    
    glBindVertexArray(0);
}

void ParticleRenderer::Render(const std::vector<std::shared_ptr<ParticleEmitter>>& emitters,
                              const Rendering::Camera& camera,
                              const glm::mat4& viewMatrix,
                              const glm::mat4& projectionMatrix) {
    if (!m_shader || m_quadVAO == 0) {
        return;
    }
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Don't write to depth (particles are transparent)
    
    m_shader->Bind();
    m_shader->SetMat4("u_View", viewMatrix);
    m_shader->SetMat4("u_Projection", projectionMatrix);
    m_shader->SetVec3("u_CameraRight", glm::vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]));
    m_shader->SetVec3("u_CameraUp", glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]));
    
    glBindVertexArray(m_quadVAO);
    
    // Render particles from each emitter
    for (const auto& emitter : emitters) {
        if (!emitter || !emitter->IsPlaying()) {
            continue;
        }
        
        const auto& particles = emitter->GetParticles();
        size_t activeCount = emitter->GetActiveParticleCount();
        
        if (activeCount == 0) {
            continue;
        }
        
        // Prepare instance data
        std::vector<InstanceData> instanceData;
        instanceData.reserve(activeCount);
        
        for (size_t i = 0; i < activeCount; ++i) {
            InstanceData instance;
            UpdateInstanceData(particles[i], instance);
            instanceData.push_back(instance);
        }
        
        // Update instance buffer
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, 
                    static_cast<GLsizeiptr>(instanceData.size() * sizeof(InstanceData)),
                    instanceData.data(),
                    GL_DYNAMIC_DRAW);
        
        // Draw instanced
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(activeCount));
    }
    
    glBindVertexArray(0);
    m_shader->Unbind();
    
    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleRenderer::UpdateInstanceData(const Particle& particle, InstanceData& instance) {
    instance.position = particle.position;
    instance.size = particle.size;
    instance.color = particle.color;
    instance.rotation = particle.rotation;
}

void ParticleRenderer::Shutdown() {
    if (m_quadVAO != 0) {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_quadVBO != 0) {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }
    if (m_instanceVBO != 0) {
        glDeleteBuffers(1, &m_instanceVBO);
        m_instanceVBO = 0;
    }
}

} // namespace Particles
} // namespace OpenEye

