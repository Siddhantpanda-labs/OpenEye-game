#pragma once

#include <cstdint>

namespace eyelib {
namespace render {

/** G-Buffer for deferred rendering: position, normal, albedo, PBR params, depth */
class GBuffer {
public:
    GBuffer();
    ~GBuffer();

    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    GBuffer(GBuffer&& other) noexcept;
    GBuffer& operator=(GBuffer&& other) noexcept;

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    void BindForGeometryPass();
    void BindForLightingPass();
    void BindDepthTexture(unsigned int textureUnit);
    void Unbind();

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    unsigned int GetFBO() const { return m_fbo; }
    unsigned int GetPositionTexture() const { return m_positionTexture; }
    unsigned int GetNormalTexture() const { return m_normalTexture; }
    unsigned int GetAlbedoTexture() const { return m_albedoTexture; }
    unsigned int GetPBRParamsTexture() const { return m_pbrParamsTexture; }
    unsigned int GetVelocityTexture() const { return m_velocityTexture; }
    unsigned int GetDepthTexture() const { return m_depthTexture; }

private:
    void Destroy();
    void CreateTextures();
    void AttachTextures();

    uint32_t m_width = 0, m_height = 0;
    unsigned int m_fbo = 0;
    unsigned int m_positionTexture = 0;
    unsigned int m_normalTexture = 0;
    unsigned int m_albedoTexture = 0;
    unsigned int m_pbrParamsTexture = 0;
    unsigned int m_velocityTexture = 0;
    unsigned int m_depthTexture = 0;
};

} // namespace render
} // namespace eyelib
