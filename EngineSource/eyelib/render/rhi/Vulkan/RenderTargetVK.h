#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;
struct TextureVK;

struct RenderTargetVK : IRenderTarget {
    explicit RenderTargetVK(DeviceVK* device);
    ~RenderTargetVK() override;

    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    ITexture* GetColorTexture(uint32_t index) override;
    ITexture* GetDepthTexture() override;
    void* GetNativeFramebuffer() override;
    void SetDepthLayer(uint32_t layer) override;
    uint32_t GetDepthLayer() const override;

    /** For dynamic rendering: VkImageView for color attachment at index. */
    void* GetColorImageView(uint32_t index) const;
    /** For dynamic rendering: VkImageView for depth (and stencil). */
    void* GetDepthImageView() const;
    /** Number of color attachments. */
    uint32_t GetColorAttachmentCount() const;

    DeviceVK* m_device = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_ownsTextures = true;
    uint32_t m_depthArrayLayer = 0;
    std::vector<std::unique_ptr<TextureVK>> m_colorTextures;
    std::unique_ptr<TextureVK> m_depthTexture;
    std::vector<TextureVK*> m_externalColorTextures;
    TextureVK* m_externalDepthTexture = nullptr;
    // Optional per-attachment views owned by this render target (used for array layer/mip views).
    std::vector<void*> m_colorAttachmentViews;
    void* m_depthAttachmentView = nullptr;
    std::vector<void*> m_depthLayerViews;
    // VkFramebuffer or dynamic rendering (filled when implemented)
    void* m_framebuffer = nullptr;
};

IRenderTarget* CreateRenderTargetVK(DeviceVK* device, uint32_t width, uint32_t height,
                                    const std::vector<RHITextureFormat>& colorFormats,
                                    RHITextureFormat depthFormat, uint32_t samples, bool transient);
IRenderTarget* CreateDepthOnlyRenderTargetArrayVK(DeviceVK* device, uint32_t width, uint32_t height, uint32_t arrayLayers);
IRenderTarget* CreateRenderTargetWithSharedDepthVK(DeviceVK* device, uint32_t width, uint32_t height,
                                                   const std::vector<RHITextureFormat>& colorFormats,
                                                   ITexture* sharedDepthTexture, uint32_t samples);
IRenderTarget* CreateRenderTargetFromTextureVK(DeviceVK* device, ITexture* colorTexture, uint32_t face, uint32_t mipLevel);
IRenderTarget* CreateRenderTargetFromTextureWithDepthVK(DeviceVK* device, ITexture* colorTexture, uint32_t face, uint32_t mipLevel, ITexture* depthTexture);
void DestroyRenderTargetVK(DeviceVK* device, IRenderTarget* rt);

} // namespace rhi
} // namespace render
} // namespace eyelib
