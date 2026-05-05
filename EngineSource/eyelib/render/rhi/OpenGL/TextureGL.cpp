#include "eyelib/render/rhi/OpenGL/TextureGL.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {
namespace rhi {

uint32_t TextureGL::GetWidth() const { return m_width; }
uint32_t TextureGL::GetHeight() const { return m_height; }
TextureType TextureGL::GetType() const { return m_type; }
RHITextureFormat TextureGL::GetFormat() const { return m_format; }

void* TextureGL::GetNativeTexture() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(m_handle));
}

bool TextureGL::IsGpuBindable() const { return m_handle != 0; }

} // namespace rhi
} // namespace render
} // namespace eyelib
