#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVk;
struct TextureVk;

struct RenderTargetVk : IRenderTarget {
    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    ITexture* GetColorTexture(uint32_t index) override;
    ITexture* GetDepthTexture() override;
    void* GetNativeFramebuffer() override;

    static RenderTargetVk* Create(DeviceVk* device, uint32_t width, uint32_t height,
                                   const std::vector<RHITextureFormat>& colorFormats,
                                   RHITextureFormat depthFormat,
                                   uint32_t samples,
                                   const std::vector<TextureVk*>* existingColors);
    static RenderTargetVk* CreateFromTexture(DeviceVk* device, TextureVk* colorTexture, uint32_t face, uint32_t mipLevel);
    static void Destroy(DeviceVk* device, RenderTargetVk* rt);

    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_ownsTextures = true;
    std::vector<std::unique_ptr<TextureVk>> m_colorTextures;
    std::unique_ptr<TextureVk> m_depthTexture;
    std::vector<TextureVk*> m_externalColorTextures;
    TextureVk* m_externalDepthTexture = nullptr;
    DeviceVk* m_device = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
