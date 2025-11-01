#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace OpenEye {
namespace Rendering {

// G-Buffer formats for deferred rendering
// Stores geometry information for lighting pass
class GBuffer {
public:
    enum class Target : int {
        Position = 0,      // RGB16F - World space position
        Normal = 1,        // RGB16F - World space normal
        Albedo = 2,        // RGB8   - Base color
        PBRParams = 3,     // RG8    - Metallic (R), Roughness (G)
        Depth = 4          // Depth24 - Depth buffer
    };
    
    GBuffer();
    ~GBuffer();
    
    // Non-copyable
    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    
    // Movable
    GBuffer(GBuffer&& other) noexcept;
    GBuffer& operator=(GBuffer&& other) noexcept;
    
    // Initialize G-Buffer with given dimensions
    bool Initialize(uint32_t width, uint32_t height);
    
    // Resize G-Buffer (recreate if dimensions changed)
    void Resize(uint32_t width, uint32_t height);
    
    // Bind G-Buffer for geometry pass (writes to MRT)
    void BindForGeometryPass();
    
    // Bind G-Buffer textures for lighting pass (reads from textures)
    void BindForLightingPass();
    
    // Bind depth texture for shadow mapping/SSAO
    void BindDepthTexture(GLuint textureUnit);
    
    // Unbind G-Buffer
    void Unbind();
    
    // Get dimensions
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    
    // Get FBO handle
    GLuint GetFBO() const { return m_fbo; }
    
    // Get texture handles
    GLuint GetPositionTexture() const { return m_positionTexture; }
    GLuint GetNormalTexture() const { return m_normalTexture; }
    GLuint GetAlbedoTexture() const { return m_albedoTexture; }
    GLuint GetPBRParamsTexture() const { return m_pbrParamsTexture; }
    GLuint GetDepthTexture() const { return m_depthTexture; }

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    
    GLuint m_fbo = 0;
    GLuint m_positionTexture = 0;
    GLuint m_normalTexture = 0;
    GLuint m_albedoTexture = 0;
    GLuint m_pbrParamsTexture = 0;
    GLuint m_depthTexture = 0;
    
    void Destroy();
    void CreateTextures();
    void AttachTextures();
};

} // namespace Rendering
} // namespace OpenEye

