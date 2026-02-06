#include "eyelib/render/rhi/OpenGL/RenderTargetGL.h"
#include "eyelib/render/rhi/OpenGL/TextureGL.h"

namespace eyelib {
namespace render {
namespace rhi {

uint32_t RenderTargetGL::GetWidth() const { return m_width; }
uint32_t RenderTargetGL::GetHeight() const { return m_height; }

ITexture* RenderTargetGL::GetColorTexture(uint32_t index) {
    if (index >= m_colorTextures.size()) return nullptr;
    return m_colorTextures[index].get();
}

ITexture* RenderTargetGL::GetDepthTexture() {
    return m_depthTexture.get();
}

void* RenderTargetGL::GetNativeFramebuffer() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(m_fbo));
}

} // namespace rhi
} // namespace render
} // namespace eyelib
