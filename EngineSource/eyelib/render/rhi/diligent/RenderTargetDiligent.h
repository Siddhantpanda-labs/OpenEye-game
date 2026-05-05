#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/diligent/TextureDiligent.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct RenderTargetDiligent : IRenderTarget {
    ~RenderTargetDiligent() override;
    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    ITexture* GetColorTexture(uint32_t index) override;
    ITexture* GetDepthTexture() override;
    void* GetNativeFramebuffer() override;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<std::unique_ptr<TextureDiligent>> m_colorTextures;
    ITexture* m_externalColorTexture = nullptr;  // for CreateRenderTargetFromTexture; we do not own
    std::unique_ptr<TextureDiligent> m_depthTexture;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
