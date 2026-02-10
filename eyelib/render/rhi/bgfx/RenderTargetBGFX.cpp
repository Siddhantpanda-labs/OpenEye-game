#include "eyelib/render/rhi/bgfx/RenderTargetBGFX.h"
#include "eyelib/render/rhi/bgfx/TextureBGFX.h"
#include <bgfx/bgfx.h>

namespace eyelib {
namespace render {
namespace rhi {

RenderTargetBGFX::~RenderTargetBGFX() = default;

uint32_t RenderTargetBGFX::GetWidth() const { return m_width; }
uint32_t RenderTargetBGFX::GetHeight() const { return m_height; }

ITexture* RenderTargetBGFX::GetColorTexture(uint32_t index) {
    if (index == 0 && m_externalColorTexture) return m_externalColorTexture;
    if (index >= m_colorTextures.size()) return nullptr;
    return m_colorTextures[index].get();
}

ITexture* RenderTargetBGFX::GetDepthTexture() {
    return m_depthTexture.get();
}

void* RenderTargetBGFX::GetNativeFramebuffer() {
    if (m_fbIdx == kInvalidIdx) return nullptr;
    bgfx::FrameBufferHandle h; h.idx = m_fbIdx;
    return bgfx::isValid(h) ? &h : nullptr;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
