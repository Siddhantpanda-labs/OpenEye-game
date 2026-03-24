#pragma once

#include "eyelib/render/rhi/RHI.h"
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
    bool Initialize(rhi::IDevice* device, uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    void BindForGeometryPass(rhi::ICommandList* cmd);
    /** No-op: deferred lighting binds G-buffer via descriptor sets. Kept for API compatibility. */
    void BindForLightingPass(rhi::ICommandList* cmd);
    void BindDepthTexture(unsigned int textureUnit);
    void Unbind(rhi::ICommandList* cmd);

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    /** Backend-agnostic native framebuffer handle (for BlitFromNativeFramebuffer). */
    void* GetNativeFramebuffer() const { return m_renderTargetRHI ? m_renderTargetRHI->GetNativeFramebuffer() : nullptr; }
    /** Legacy: GL FBO id as unsigned int. Prefer GetNativeFramebuffer() for backend-agnostic code. */
    unsigned int GetFBO() const;
    unsigned int GetPositionTexture() const;
    unsigned int GetNormalTexture() const;
    unsigned int GetAlbedoTexture() const;
    unsigned int GetPBRParamsTexture() const;
    unsigned int GetVelocityTexture() const;
    unsigned int GetDepthTexture() const;

    rhi::IRenderTarget* GetRenderTargetRHI() const { return m_renderTargetRHI; }
    rhi::ITexture* GetPositionTextureRHI() const;
    rhi::ITexture* GetNormalTextureRHI() const;
    rhi::ITexture* GetAlbedoTextureRHI() const;
    rhi::ITexture* GetPBRParamsTextureRHI() const;
    rhi::ITexture* GetVelocityTextureRHI() const;
    rhi::ITexture* GetDepthTextureRHI() const;

private:
    void Destroy();
    rhi::ITexture* GetColorTextureRHI(uint32_t index) const;

    uint32_t m_width = 0, m_height = 0;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IRenderTarget* m_renderTargetRHI = nullptr;
};

} // namespace render
} // namespace eyelib
