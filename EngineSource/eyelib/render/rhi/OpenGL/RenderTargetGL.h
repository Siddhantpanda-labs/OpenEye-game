#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vector>
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

struct TextureGL;

struct RenderTargetGL : IRenderTarget {
    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    ITexture* GetColorTexture(uint32_t index) override;
    ITexture* GetDepthTexture() override;
    void* GetNativeFramebuffer() override;
    void SetDepthLayer(uint32_t layer) override;
    uint32_t GetDepthLayer() const override;

    unsigned int m_fbo = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_ownsTextures = true;
    uint32_t m_depthArrayLayer = 0;
    std::vector<std::unique_ptr<TextureGL>> m_colorTextures;
    std::unique_ptr<TextureGL> m_depthTexture;
    std::vector<TextureGL*> m_externalColorTextures;
    TextureGL* m_externalDepthTexture = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
