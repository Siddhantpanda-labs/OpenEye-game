#include "ParticleRenderer.h"
#include "../rendering/Camera.h"
#include "../rendering/resources/Shader.h"
#include "../rendering/resources/Texture.h"
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
    
    // Create instance buffer BEFORE configuring attributes
    glGenBuffers(1, &m_instanceVBO);
    
    // Create quad mesh
    CreateQuad();
    
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
    
    // Instance data (position, size, color, rotation, ageNorm)
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

    // Age normalized (float)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, ageNorm));
    glVertexAttribDivisor(6, 1);
    
    glBindVertexArray(0);
}

void ParticleRenderer::Render(const std::vector<std::shared_ptr<ParticleEmitter>>& emitters,
                              const Rendering::Camera& camera,
                              const glm::mat4& viewMatrix,
                              const glm::mat4& projectionMatrix,
                              unsigned int sceneDepthTexture,
                              std::uint32_t viewportWidth,
                              std::uint32_t viewportHeight) {
    if (!m_shader || m_quadVAO == 0) {
        return;
    }
    
    // Enable blending for transparency (premultiplied alpha path)
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Don't write to depth (particles are transparent)
    // Ensure billboards are never culled (winding can flip w.r.t camera)
    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    if (cullWasEnabled) glDisable(GL_CULL_FACE);
    
    m_shader->Bind();
    m_shader->SetMat4("u_View", viewMatrix);
    m_shader->SetMat4("u_Projection", projectionMatrix);
    // Provide inverse projection for soft-particles depth reconstruction
    glm::mat4 invProj = glm::inverse(projectionMatrix);
    m_shader->SetMat4("u_InvProjection", invProj);
    m_shader->SetVec2("u_Viewport", glm::vec2((float)viewportWidth, (float)viewportHeight));
    // Bind scene depth if available
    if (sceneDepthTexture != 0) {
        glActiveTexture(GL_TEXTURE0 + 4);
        glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
        m_shader->SetInt("u_SceneDepth", 4);
    }
    
    // Extract camera right and up vectors from view matrix (inverse of model matrix)
    // View matrix columns represent camera axes in world space
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec3 cameraRight = glm::normalize(glm::vec3(invView[0][0], invView[1][0], invView[2][0]));
    glm::vec3 cameraUp = glm::normalize(glm::vec3(invView[0][1], invView[1][1], invView[2][1]));
    m_shader->SetVec3("u_CameraRight", cameraRight);
    m_shader->SetVec3("u_CameraUp", cameraUp);
    
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

        const auto& cfg = emitter->GetConfig();
        // Configure flipbook/texture uniforms per-emitter
        bool useTex = (bool)cfg.texture;
        m_shader->SetBool("u_UseTexture", useTex);
        if (useTex) {
            // Bind particle flipbook texture to unit 0
            cfg.texture->Bind(0);
            m_shader->SetInt("u_ParticleTexture", 0);
            // Ensure texture uses CLAMP_TO_EDGE to prevent wrapping artifacts
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        int cols = cfg.flipbookCols;
        int rows = cfg.flipbookRows;
        int frames = cfg.flipbookFrameCount > 0 ? cfg.flipbookFrameCount : (cols > 0 && rows > 0 ? cols * rows : 0);
        m_shader->SetInt("u_FlipbookCols", cols);
        m_shader->SetInt("u_FlipbookRows", rows);
        m_shader->SetInt("u_FlipbookFrames", frames);
        m_shader->SetBool("u_FlipbookBlend", cfg.flipbookBlendFrames);
        m_shader->SetBool("u_SoftParticles", cfg.softParticles && sceneDepthTexture != 0);
        m_shader->SetFloat("u_SoftParticleFade", cfg.softParticleFadeDistance);
        
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
    if (cullWasEnabled) glEnable(GL_CULL_FACE);
}

void ParticleRenderer::UpdateInstanceData(const Particle& particle, InstanceData& instance) {
    instance.position = particle.position;
    instance.size = particle.size;
    instance.color = particle.color;
    instance.rotation = particle.rotation;
    float maxLife = particle.maxLife > 0.0001f ? particle.maxLife : 1.0f;
    float lifeRatio = glm::clamp(1.0f - (particle.life / maxLife), 0.0f, 1.0f);
    instance.ageNorm = lifeRatio;
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

