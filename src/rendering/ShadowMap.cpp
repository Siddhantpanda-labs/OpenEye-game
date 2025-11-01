#include "ShadowMap.h"
#include "core/log/Log.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace OpenEye {
namespace Rendering {

ShadowMap::ShadowMap() = default;

ShadowMap::~ShadowMap() {
    Destroy();
}

ShadowMap::ShadowMap(ShadowMap&& other) noexcept
    : m_resolution(other.m_resolution)
    , m_cascadeCount(other.m_cascadeCount)
    , m_lambda(other.m_lambda)
    , m_fbo(other.m_fbo)
    , m_shadowTextures(std::move(other.m_shadowTextures))
    , m_shadowMatrices(std::move(other.m_shadowMatrices))
    , m_splitDistances(std::move(other.m_splitDistances)) {
    
    other.m_fbo = 0;
    other.m_shadowTextures.clear();
}

ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
    if (this != &other) {
        Destroy();
        
        m_resolution = other.m_resolution;
        m_cascadeCount = other.m_cascadeCount;
        m_lambda = other.m_lambda;
        m_fbo = other.m_fbo;
        m_shadowTextures = std::move(other.m_shadowTextures);
        m_shadowMatrices = std::move(other.m_shadowMatrices);
        m_splitDistances = std::move(other.m_splitDistances);
        
        other.m_fbo = 0;
        other.m_shadowTextures.clear();
    }
    return *this;
}

bool ShadowMap::Initialize(uint32_t resolution, uint32_t cascadeCount) {
    if (resolution == 0 || cascadeCount == 0 || cascadeCount > 8) {
        OE_ERROR("ShadowMap: Invalid parameters");
        return false;
    }
    
    m_resolution = resolution;
    m_cascadeCount = cascadeCount;
    
    Destroy();
    
    // Create FBO
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    
    // Create shadow textures (one per cascade)
    m_shadowTextures.resize(m_cascadeCount);
    glGenTextures(m_cascadeCount, m_shadowTextures.data());
    
    for (uint32_t i = 0; i < m_cascadeCount; i++) {
        glBindTexture(GL_TEXTURE_2D, m_shadowTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_resolution, m_resolution, 
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        
        // Enable PCF filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Initialize matrices and splits
    m_shadowMatrices.resize(m_cascadeCount);
    m_splitDistances.resize(m_cascadeCount + 1);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    OE_INFO_F("ShadowMap initialized: %u cascades, %ux%u resolution", 
             cascadeCount, resolution, resolution);
    return true;
}

void ShadowMap::CalculateCascadeSplits(float nearPlane, float farPlane) {
    m_splitDistances[0] = nearPlane;
    m_splitDistances[m_cascadeCount] = farPlane;
    
    float ratio = farPlane / nearPlane;
    
    for (uint32_t i = 1; i < m_cascadeCount; i++) {
        float fraction = static_cast<float>(i) / static_cast<float>(m_cascadeCount);
        
        // Blend between uniform and logarithmic distribution
        float uniformSplit = nearPlane + (farPlane - nearPlane) * fraction;
        float logarithmicSplit = nearPlane * std::pow(ratio, fraction);
        
        m_splitDistances[i] = m_lambda * logarithmicSplit + (1.0f - m_lambda) * uniformSplit;
    }
}

void ShadowMap::UpdateMatrices(const glm::vec3& lightDirection, 
                              const glm::mat4& viewMatrix, 
                              const glm::mat4& projectionMatrix,
                              const glm::vec3& cameraPosition,
                              float nearPlane, float farPlane) {
    CalculateCascadeSplits(nearPlane, farPlane);
    UpdateCascadeMatrices(lightDirection, viewMatrix, projectionMatrix, cameraPosition);
}

void ShadowMap::UpdateCascadeMatrices(const glm::vec3& lightDirection,
                                      const glm::mat4& viewMatrix,
                                      const glm::mat4& projectionMatrix,
                                      const glm::vec3& cameraPosition) {
    glm::vec3 lightDir = glm::normalize(lightDirection);
    
    // Calculate view frustum corners for each cascade
    for (uint32_t i = 0; i < m_cascadeCount; i++) {
        float nearDist = m_splitDistances[i];
        float farDist = m_splitDistances[i + 1];
        
        // Get frustum corners in view space
        glm::vec4 frustumCorners[8];
        
        // Extract projection parameters (simple perspective)
        float aspect = projectionMatrix[1][1] / projectionMatrix[0][0]; // Simplified
        float fov = 2.0f * std::atan(1.0f / projectionMatrix[1][1]);
        float tanHalfFov = std::tan(fov * 0.5f);
        
        float nearHeight = nearDist * tanHalfFov;
        float nearWidth = nearHeight * aspect;
        float farHeight = farDist * tanHalfFov;
        float farWidth = farHeight * aspect;
        
        // Near plane corners
        frustumCorners[0] = glm::vec4(-nearWidth, -nearHeight, -nearDist, 1.0f);
        frustumCorners[1] = glm::vec4(nearWidth, -nearHeight, -nearDist, 1.0f);
        frustumCorners[2] = glm::vec4(nearWidth, nearHeight, -nearDist, 1.0f);
        frustumCorners[3] = glm::vec4(-nearWidth, nearHeight, -nearDist, 1.0f);
        
        // Far plane corners
        frustumCorners[4] = glm::vec4(-farWidth, -farHeight, -farDist, 1.0f);
        frustumCorners[5] = glm::vec4(farWidth, -farHeight, -farDist, 1.0f);
        frustumCorners[6] = glm::vec4(farWidth, farHeight, -farDist, 1.0f);
        frustumCorners[7] = glm::vec4(-farWidth, farHeight, -farDist, 1.0f);
        
        // Transform to world space
        glm::mat4 invView = glm::inverse(viewMatrix);
        for (int j = 0; j < 8; j++) {
            frustumCorners[j] = invView * frustumCorners[j];
        }
        
        // Calculate centroid
        glm::vec3 centroid = glm::vec3(0.0f);
        for (int j = 0; j < 8; j++) {
            centroid += glm::vec3(frustumCorners[j]);
        }
        centroid /= 8.0f;
        
        // Light view matrix (look at centroid from light direction)
        glm::vec3 lightPos = centroid - lightDir * 100.0f; // Place light far enough
        glm::mat4 lightView = glm::lookAt(lightPos, centroid, glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Transform frustum corners to light space
        glm::vec3 minBounds = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
        
        for (int j = 0; j < 8; j++) {
            glm::vec3 lightSpaceCorner = glm::vec3(lightView * frustumCorners[j]);
            minBounds = glm::min(minBounds, lightSpaceCorner);
            maxBounds = glm::max(maxBounds, lightSpaceCorner);
        }
        
        // Create light projection matrix (orthographic)
        float size = glm::max(maxBounds.x - minBounds.x, maxBounds.y - minBounds.y);
        size *= 1.1f; // Add padding
        
        glm::mat4 lightProjection = glm::ortho(-size * 0.5f, size * 0.5f,
                                              -size * 0.5f, size * 0.5f,
                                              -maxBounds.z - 50.0f, -minBounds.z + 50.0f);
        
        m_shadowMatrices[i] = lightProjection * lightView;
    }
}

void ShadowMap::BindForShadowPass(uint32_t cascadeIndex) {
    if (cascadeIndex >= m_cascadeCount || m_fbo == 0) {
        return;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 
                          m_shadowTextures[cascadeIndex], 0);
    
    // No color attachments
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    glViewport(0, 0, m_resolution, m_resolution);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::BindTextures(GLuint startTextureUnit) {
    for (uint32_t i = 0; i < m_cascadeCount; i++) {
        glActiveTexture(GL_TEXTURE0 + startTextureUnit + i);
        glBindTexture(GL_TEXTURE_2D, m_shadowTextures[i]);
    }
}

void ShadowMap::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

glm::mat4 ShadowMap::GetShadowMatrix(uint32_t cascadeIndex) const {
    if (cascadeIndex < m_shadowMatrices.size()) {
        return m_shadowMatrices[cascadeIndex];
    }
    return glm::mat4(1.0f);
}

void ShadowMap::Destroy() {
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    
    if (!m_shadowTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_shadowTextures.size()), m_shadowTextures.data());
        m_shadowTextures.clear();
    }
    
    m_shadowMatrices.clear();
    m_splitDistances.clear();
}

} // namespace Rendering
} // namespace OpenEye

