#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct TextureBGFX;

struct RenderTargetBGFX : IRenderTarget {
    ~RenderTargetBGFX() override;
    uint32_t GetWidth() const override;
    uint32_t GetHeight() const override;
    ITexture* GetColorTexture(uint32_t index) override;
    ITexture* GetDepthTexture() override;
    void* GetNativeFramebuffer() override;

    static constexpr uint16_t kInvalidIdx = 0xffff;
    uint16_t m_fbIdx = kInvalidIdx;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<std::unique_ptr<TextureBGFX>> m_colorTextures;
    ITexture* m_externalColorTexture = nullptr;  // for CreateRenderTargetFromTexture
    std::unique_ptr<TextureBGFX> m_depthTexture;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
