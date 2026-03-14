#include "eyelib/render/rhi/Vulkan/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"

namespace eyelib {
namespace render {
namespace rhi {

TextureVK::TextureVK(DeviceVK* device) : m_device(device) {}

TextureVK::~TextureVK() = default;

uint32_t TextureVK::GetWidth() const { return m_width; }

uint32_t TextureVK::GetHeight() const { return m_height; }

TextureType TextureVK::GetType() const { return m_type; }

RHITextureFormat TextureVK::GetFormat() const { return m_format; }

void* TextureVK::GetNativeTexture() { return m_imageView; }

} // namespace rhi
} // namespace render
} // namespace eyelib
