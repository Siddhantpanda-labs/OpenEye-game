#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

namespace OpenEye {
namespace Rendering {

// Cascaded Shadow Map system for directional lights
class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();
    
    // Non-copyable
    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    
    // Movable
    ShadowMap(ShadowMap&& other) noexcept;
    ShadowMap& operator=(ShadowMap&& other) noexcept;
    
    // Initialize shadow map with resolution and cascade count
    bool Initialize(uint32_t resolution = 2048, uint32_t cascadeCount = 4);
    
    // Update shadow matrices for cascades
    void UpdateMatrices(const glm::vec3& lightDirection, 
                      const glm::mat4& viewMatrix, 
                      const glm::mat4& projectionMatrix,
                      const glm::vec3& cameraPosition,
                      float nearPlane, float farPlane);
    
    // Bind shadow map FBO for rendering shadow pass
    void BindForShadowPass(uint32_t cascadeIndex);
    
    // Bind shadow map textures for lighting pass
    void BindTextures(GLuint startTextureUnit = 5);
    
    // Unbind
    void Unbind();
    
    // Get shadow matrix for cascade
    glm::mat4 GetShadowMatrix(uint32_t cascadeIndex) const;
    
    // Get cascade count
    uint32_t GetCascadeCount() const { return m_cascadeCount; }
    
    // Get resolution
    uint32_t GetResolution() const { return m_resolution; }
    
    // Get split distances
    const std::vector<float>& GetSplitDistances() const { return m_splitDistances; }
    
    // Set cascade distribution (lambda = 0 = uniform, 1 = logarithmic)
    void SetCascadeDistribution(float lambda = 0.5f) { m_lambda = glm::clamp(lambda, 0.0f, 1.0f); }

private:
    uint32_t m_resolution = 2048;
    uint32_t m_cascadeCount = 4;
    float m_lambda = 0.5f; // Cascade distribution factor
    
    GLuint m_fbo = 0;
    std::vector<GLuint> m_shadowTextures; // One per cascade
    std::vector<glm::mat4> m_shadowMatrices;
    std::vector<float> m_splitDistances;
    
    void Destroy();
    void CalculateCascadeSplits(float nearPlane, float farPlane);
    void UpdateCascadeMatrices(const glm::vec3& lightDirection,
                               const glm::mat4& viewMatrix,
                               const glm::mat4& projectionMatrix,
                               const glm::vec3& cameraPosition);
};

} // namespace Rendering
} // namespace OpenEye

