#include "eyelib/render/rhi/diligent/RenderTargetDiligent.h"

namespace eyelib {
namespace render {
namespace rhi {

RenderTargetDiligent::~RenderTargetDiligent() = default;

uint32_t RenderTargetDiligent::GetWidth() const { return m_width; }
uint32_t RenderTargetDiligent::GetHeight() const { return m_height; }

ITexture* RenderTargetDiligent::GetColorTexture(uint32_t index) {
    if (m_externalColorTexture && index == 0)
        return m_externalColorTexture;
    if (index < m_colorTextures.size())
        return m_colorTextures[index].get();
    return nullptr;
}

ITexture* RenderTargetDiligent::GetDepthTexture() {
    return m_depthTexture.get();
}

void* RenderTargetDiligent::GetNativeFramebuffer() {
    return nullptr;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
