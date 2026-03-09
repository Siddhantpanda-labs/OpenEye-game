#include "eyelib/render/rhi/diligent/TextureDiligent.h"
#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"
#endif

namespace eyelib {
namespace render {
namespace rhi {

TextureDiligent::~TextureDiligent() {
#if OPENEYE_USE_DILIGENT
    for (void* p : {m_pRTV, m_pSRV, m_pDSV})
        if (p) static_cast<Diligent::ITextureView*>(p)->Release();
    if (m_pNative) static_cast<Diligent::ITexture*>(m_pNative)->Release();
    m_pRTV = m_pSRV = m_pDSV = m_pNative = nullptr;
#endif
}

uint32_t TextureDiligent::GetWidth() const { return m_width; }
uint32_t TextureDiligent::GetHeight() const { return m_height; }
TextureType TextureDiligent::GetType() const { return m_type; }
RHITextureFormat TextureDiligent::GetFormat() const { return m_format; }
void* TextureDiligent::GetNativeTexture() { return m_pNative; }

} // namespace rhi
} // namespace render
} // namespace eyelib
