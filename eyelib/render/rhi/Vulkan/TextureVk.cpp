#include "eyelib/render/rhi/Vulkan/TextureVk.h"

namespace eyelib {
namespace render {
namespace rhi {

uint32_t TextureVk::GetWidth() const { return m_width; }
uint32_t TextureVk::GetHeight() const { return m_height; }
TextureType TextureVk::GetType() const { return m_type; }
RHITextureFormat TextureVk::GetFormat() const { return m_format; }

void* TextureVk::GetNativeTexture() {
    return reinterpret_cast<void*>(m_image);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
