#include "eyelib/render/rhi/diligent/TextureDiligent.h"

namespace eyelib {
namespace render {
namespace rhi {

TextureDiligent::~TextureDiligent() = default;

uint32_t TextureDiligent::GetWidth() const { return m_width; }
uint32_t TextureDiligent::GetHeight() const { return m_height; }
TextureType TextureDiligent::GetType() const { return m_type; }
RHITextureFormat TextureDiligent::GetFormat() const { return m_format; }
void* TextureDiligent::GetNativeTexture() { return nullptr; }

} // namespace rhi
} // namespace render
} // namespace eyelib
