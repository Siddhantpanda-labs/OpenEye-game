#include "ParticleRenderer.h"
#include "../rendering/Camera.h"
#include "../rendering/resources/Shader.h"
#include "../rendering/resources/Texture.h"
#include "../core/log/Log.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

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
    // Create standard shader
    m_shader = std::make_shared<Rendering::Shader>();
    if (!m_shader->LoadFromFile("assets/shaders/particles/particle.vert", 
                                "assets/shaders/particles/particle.frag")) {
        OE_ERROR("ParticleRenderer: Failed to load particle shader");
        return false;
    }
    
    // Create volumetric shader for advanced rendering
    m_volumetricShader = std::make_shared<Rendering::Shader>();
    if (!m_volumetricShader->LoadFromFile("assets/shaders/particles/particle_volumetric.vert",
                                         "assets/shaders/particles/particle_volumetric.frag")) {
        OE_WARN("ParticleRenderer: Failed to load volumetric shader, volumetric mode will not work");
        m_volumetricShader = nullptr;
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
    
    // Instance data (position, size, color, rotation, ageNorm, quadIndex)
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
    
    // Quad index (int) - for cross-billboard rendering
    glEnableVertexAttribArray(7);
    glVertexAttribIPointer(7, 1, GL_INT, sizeof(InstanceData), (void*)offsetof(InstanceData, quadIndex));
    glVertexAttribDivisor(7, 1);
    
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
    
    // Collect all particles from all emitters with their distances for sorting
    struct SortedParticle {
        const Particle* particle;
        const ParticleEmitterConfig* config;
        float distanceToCamera;
    };
    
    std::vector<SortedParticle> sortedParticles;
    glm::vec3 cameraPos = camera.GetPosition();
    
    for (const auto& emitter : emitters) {
        if (!emitter || !emitter->IsPlaying()) {
            continue;
        }
        
        const auto& particles = emitter->GetParticles();
        size_t activeCount = emitter->GetActiveParticleCount();
        const auto& cfg = emitter->GetConfig();
        
        for (size_t i = 0; i < activeCount; ++i) {
            float distance = glm::length(cameraPos - particles[i].position);
            sortedParticles.push_back({&particles[i], &cfg, distance});
        }
    }
    
    if (sortedParticles.empty()) {
        return;
    }
    
    // Sort back-to-front (furthest first) for proper transparency
    std::sort(sortedParticles.begin(), sortedParticles.end(),
        [](const SortedParticle& a, const SortedParticle& b) {
            return a.distanceToCamera > b.distanceToCamera;
        });
    
    // Enable depth testing FIRST - test against scene depth before blending
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    // For proper occlusion: write depth for opaque parts, but use depth testing against scene
    // We'll write depth only for particles that pass the depth test
    glDepthMask(GL_TRUE);
    
    // Enable blending for transparency (premultiplied alpha path)
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    // Ensure billboards are never culled (winding can flip w.r.t camera)
    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    if (cullWasEnabled) glDisable(GL_CULL_FACE);
    
    // Get camera vectors directly from camera object (more reliable than extracting from view matrix)
    glm::vec3 cameraRight = camera.GetRight();
    glm::vec3 cameraUp = camera.GetUp();
    glm::vec3 cameraForward = camera.GetForward();
    // cameraPos already declared above, reuse it
    
    // Set common uniforms (will be set per-batch for volumetric shader)
    m_shader->Bind();
    m_shader->SetMat4("u_View", viewMatrix);
    m_shader->SetMat4("u_Projection", projectionMatrix);
    glm::mat4 invProj = glm::inverse(projectionMatrix);
    m_shader->SetMat4("u_InvProjection", invProj);
    m_shader->SetVec2("u_Viewport", glm::vec2((float)viewportWidth, (float)viewportHeight));
    m_shader->SetVec3("u_CameraRight", cameraRight);
    m_shader->SetVec3("u_CameraUp", cameraUp);
    m_shader->SetVec3("u_CameraForward", cameraForward);
    
    // Also set for volumetric shader if available
    if (m_volumetricShader) {
        m_volumetricShader->Bind();
        m_volumetricShader->SetMat4("u_View", viewMatrix);
        m_volumetricShader->SetMat4("u_Projection", projectionMatrix);
        m_volumetricShader->SetMat4("u_InvProjection", invProj);
        m_volumetricShader->SetVec2("u_Viewport", glm::vec2((float)viewportWidth, (float)viewportHeight));
        m_volumetricShader->SetVec3("u_CameraRight", cameraRight);
        m_volumetricShader->SetVec3("u_CameraUp", cameraUp);
        m_volumetricShader->SetVec3("u_CameraForward", cameraForward);
        m_volumetricShader->SetVec3("u_CameraPos", cameraPos);
        m_volumetricShader->Unbind();
    }
    
    // Bind scene depth if available
    if (sceneDepthTexture != 0) {
        glActiveTexture(GL_TEXTURE0 + 4);
        glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
        m_shader->SetInt("u_SceneDepth", 4);
        if (m_volumetricShader) {
            m_volumetricShader->SetInt("u_SceneDepth", 4);
        }
    }
    
    m_shader->Unbind();
    
    glBindVertexArray(m_quadVAO);
    
    // Group particles by config to minimize state changes
    const ParticleEmitterConfig* currentConfig = nullptr;
    std::vector<InstanceData> batchInstanceData;
    
    for (const auto& sortedPart : sortedParticles) {
        // Check if config changed (need to flush batch)
        if (currentConfig != nullptr && currentConfig != sortedPart.config) {
            // Render current batch
            RenderBatch(currentConfig, batchInstanceData, sceneDepthTexture);
            batchInstanceData.clear();
        }
        
        currentConfig = sortedPart.config;
        InstanceData instance;
        UpdateInstanceData(*sortedPart.particle, instance);
        batchInstanceData.push_back(instance);
    }
    
    // Render final batch
    if (!batchInstanceData.empty() && currentConfig != nullptr) {
        RenderBatch(currentConfig, batchInstanceData, sceneDepthTexture);
    }
    
    glBindVertexArray(0);
    // Shader is already unbound in RenderBatch
    
    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (cullWasEnabled) glEnable(GL_CULL_FACE);
}

void ParticleRenderer::RenderBatch(const ParticleEmitterConfig* config, const std::vector<InstanceData>& instanceData, unsigned int sceneDepthTexture) {
    if (!config || instanceData.empty()) {
        return;
    }
    
    // Select shader based on render mode
    std::shared_ptr<Rendering::Shader> activeShader = m_shader;
    if (config->renderMode == ParticleRenderMode::Volumetric) {
        if (m_volumetricShader) {
            activeShader = m_volumetricShader;
        } else {
            OE_WARN("ParticleRenderer: Volumetric shader not available, falling back to billboard");
        }
    }
    
    activeShader->Bind();
    
    // Configure flipbook/texture uniforms
    bool useTex = (bool)config->texture;
    activeShader->SetBool("u_UseTexture", useTex);
    if (useTex) {
        config->texture->Bind(0);
        activeShader->SetInt("u_ParticleTexture", 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    
    int cols = config->flipbookCols;
    int rows = config->flipbookRows;
    int frames = config->flipbookFrameCount > 0 ? config->flipbookFrameCount : (cols > 0 && rows > 0 ? cols * rows : 0);
    activeShader->SetInt("u_FlipbookCols", cols);
    activeShader->SetInt("u_FlipbookRows", rows);
    activeShader->SetInt("u_FlipbookFrames", frames);
    activeShader->SetBool("u_FlipbookBlend", config->flipbookBlendFrames);
    activeShader->SetBool("u_SoftParticles", config->softParticles && sceneDepthTexture != 0);
    activeShader->SetFloat("u_SoftParticleFade", config->softParticleFadeDistance);
    
    // Set render mode (only for standard shader)
    if (config->renderMode != ParticleRenderMode::Volumetric) {
        int renderMode = static_cast<int>(config->renderMode);
        activeShader->SetInt("u_RenderMode", renderMode);
    }
    
    // For cross-billboard, duplicate instance data 3 times (one for each quad)
    std::vector<InstanceData> finalInstanceData;
    if (config->renderMode == ParticleRenderMode::CrossBillboard) {
        finalInstanceData.reserve(instanceData.size() * 3);
        for (const auto& inst : instanceData) {
            InstanceData inst0 = inst;
            inst0.quadIndex = 0; // XY plane
            finalInstanceData.push_back(inst0);
            
            InstanceData inst1 = inst;
            inst1.quadIndex = 1; // XZ plane
            finalInstanceData.push_back(inst1);
            
            InstanceData inst2 = inst;
            inst2.quadIndex = 2; // YZ plane
            finalInstanceData.push_back(inst2);
        }
    } else {
        finalInstanceData = instanceData;
        // Set quadIndex to 0 for regular billboard
        for (auto& inst : finalInstanceData) {
            inst.quadIndex = 0;
        }
    }
    
    // Update instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(finalInstanceData.size() * sizeof(InstanceData)),
                finalInstanceData.data(),
                GL_DYNAMIC_DRAW);
    
    // Draw instanced
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(finalInstanceData.size()));
}

void ParticleRenderer::UpdateInstanceData(const Particle& particle, InstanceData& instance) {
    instance.position = particle.position;
    instance.size = particle.size;
    instance.color = particle.color;
    instance.rotation = particle.rotation;
    float maxLife = particle.maxLife > 0.0001f ? particle.maxLife : 1.0f;
    float lifeRatio = glm::clamp(1.0f - (particle.life / maxLife), 0.0f, 1.0f);
    instance.ageNorm = lifeRatio;
    instance.quadIndex = 0; // Will be set in RenderBatch
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

