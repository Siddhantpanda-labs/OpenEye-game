#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/resources/Texture2D.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace eyelib {
namespace render {

/**
 * @brief Generic render target (RHI-only). Create via device.
 * Supports multiple color attachments, depth/stencil, MSAA.
 */
class RenderTarget {
public:
    RenderTarget();
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    /**
     * @brief Create render target via RHI device.
     * @param samples 1 = no MSAA; 2/4/8/16 = MSAA (clamped by device GetMaxSamples).
     */
    bool Create(rhi::IDevice* device, uint32_t width, uint32_t height,
                const std::vector<TextureFormat>& colorFormats,
                TextureFormat depthFormat = TextureFormat::Depth24,
                uint32_t samples = 1);

    void Resize(uint32_t width, uint32_t height);
    void Destroy();
    void Destroy(rhi::IDevice* device);

    /** No-op (use ICommandList::SetRenderTarget). */
    void Bind() const;
    void Unbind() const;

    /** Blit from source to dest (both must be created via same device). */
    static void Blit(const RenderTarget& source, const RenderTarget& dest, bool colorOnly = true);

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    /** Backend-agnostic native framebuffer handle. */
    void* GetNativeFramebuffer() const { return m_renderTargetRHI ? m_renderTargetRHI->GetNativeFramebuffer() : nullptr; }
    /** Legacy: backend FBO id (e.g. GL name); 0 if invalid. Prefer GetNativeFramebuffer() for backend-agnostic code. */
    unsigned int GetFBO() const;
    rhi::IRenderTarget* GetRenderTargetRHI() const { return m_renderTargetRHI; }
    int GetSamples() const { return m_samples; }

    /** No-op (RHI backend handles MSAA resolve when sampling). */
    void Resolve();

    std::shared_ptr<Texture2D> GetColorTexture(size_t slot = 0) const;
    std::shared_ptr<Texture2D> GetDepthTexture() const { return m_depthTexture; }

    size_t GetColorAttachmentCount() const { return m_colorTextures.size(); }

    /** Requires device. Returns clamped sample count. */
    static int GetMaxSamples(rhi::IDevice* device, int samples);

private:
    void CacheTextureWrappers();

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    int m_samples = 0;
    std::vector<TextureFormat> m_colorFormats;
    TextureFormat m_depthFormat = TextureFormat::Depth24;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IRenderTarget* m_renderTargetRHI = nullptr;

    std::vector<std::shared_ptr<Texture2D>> m_colorTextures;
    std::shared_ptr<Texture2D> m_depthTexture;
};

} // namespace render
} // namespace eyelib
