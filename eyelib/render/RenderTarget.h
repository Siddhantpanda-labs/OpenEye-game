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
 * @brief Generic render target (framebuffer wrapper).
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
     * @brief Create render target with specified attachments.
     * @param width Width in pixels
     * @param height Height in pixels
     * @param colorFormats Formats for color attachments (empty = no color)
     * @param depthFormat Depth format (Depth24 or Depth24Stencil8, or none if empty)
     * @param samples MSAA samples (0 or 1 = no MSAA, 2/4/8/16 = MSAA)
     */
    bool Create(uint32_t width, uint32_t height,
                const std::vector<TextureFormat>& colorFormats,
                TextureFormat depthFormat = TextureFormat::Depth24,
                int samples = 0);
    /** RHI path: create render target via device. */
    bool Create(rhi::IDevice* device, uint32_t width, uint32_t height,
                const std::vector<TextureFormat>& colorFormats,
                TextureFormat depthFormat = TextureFormat::Depth24,
                uint32_t samples = 1);

    void Resize(uint32_t width, uint32_t height);
    void Destroy();
    void Destroy(rhi::IDevice* device);

    void Bind() const;
    void Unbind() const;

    /**
     * @brief Blit from source to destination render target.
     * @param source Source render target
     * @param dest Destination render target
     * @param colorOnly If true, only blit color (not depth)
     */
    static void Blit(const RenderTarget& source, const RenderTarget& dest, bool colorOnly = true);

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    unsigned int GetFBO() const { return m_fbo; }
    /** RHI path: return render target when created via Create(device, ...). */
    rhi::IRenderTarget* GetRenderTargetRHI() const { return m_renderTargetRHI; }
    int GetSamples() const { return m_samples; }

    /**
     * @brief Resolve multisampled renderbuffers to textures (for MSAA).
     * Call after rendering to get resolved textures. No-op if not MSAA.
     */
    void Resolve();

    /**
     * @brief Get color texture at attachment slot.
     * @param slot Attachment slot (0 = first color attachment)
     * For MSAA: returns resolved texture after Resolve() is called.
     */
    std::shared_ptr<Texture2D> GetColorTexture(size_t slot = 0) const;
    std::shared_ptr<Texture2D> GetDepthTexture() const { return m_depthTexture; }

    size_t GetColorAttachmentCount() const { return m_colorTextures.size(); }

    /**
     * @brief Check if MSAA with given sample count is supported.
     * @param samples Desired sample count
     * @return Maximum supported samples (may be less than requested)
     */
    static int GetMaxSamples(int samples);

private:
    void CreateTextures();
    void AttachTextures();
    void CreateMSAABuffers();
    void CreateResolveFBO();

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    int m_samples = 0;
    unsigned int m_fbo = 0;  // Main FBO (multisampled if MSAA)
    unsigned int m_resolveFBO = 0;  // Resolve FBO (non-MSAA, for reading)
    std::vector<TextureFormat> m_colorFormats;
    TextureFormat m_depthFormat = TextureFormat::Depth24;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IRenderTarget* m_renderTargetRHI = nullptr;

    // For non-MSAA: textures attached directly to m_fbo
    // For MSAA: renderbuffers attached to m_fbo, textures attached to m_resolveFBO
    std::vector<std::shared_ptr<Texture2D>> m_colorTextures;  // Resolved textures (always available)
    std::shared_ptr<Texture2D> m_depthTexture;  // Resolved depth (always available)
    
    // MSAA renderbuffers (only when samples > 0)
    std::vector<unsigned int> m_colorRenderbuffers;  // Multisampled color renderbuffers
    unsigned int m_depthRenderbuffer = 0;  // Multisampled depth renderbuffer
};

} // namespace render
} // namespace eyelib
