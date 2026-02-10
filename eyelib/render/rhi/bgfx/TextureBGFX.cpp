#include "eyelib/render/rhi/bgfx/TextureBGFX.h"
#include "eyelib/render/rhi/bgfx/BGFXEnums.h"
#include <bgfx/bgfx.h>

namespace eyelib {
namespace render {
namespace rhi {

TextureBGFX::~TextureBGFX() = default;

uint32_t TextureBGFX::GetWidth() const { return m_width; }
uint32_t TextureBGFX::GetHeight() const { return m_height; }
TextureType TextureBGFX::GetType() const { return m_type; }
RHITextureFormat TextureBGFX::GetFormat() const { return m_format; }

void* TextureBGFX::GetNativeTexture() {
    if (m_texIdx == kInvalidIdx) return nullptr;
    bgfx::TextureHandle h; h.idx = m_texIdx;
    return bgfx::isValid(h) ? &h : nullptr;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
