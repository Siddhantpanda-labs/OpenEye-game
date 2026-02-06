#include "eyelib/render/rhi/OpenGL/TextureGL.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {
namespace rhi {

uint32_t TextureGL::GetWidth() const { return m_width; }
uint32_t TextureGL::GetHeight() const { return m_height; }
TextureType TextureGL::GetType() const { return m_type; }
RHITextureFormat TextureGL::GetFormat() const { return m_format; }

} // namespace rhi
} // namespace render
} // namespace eyelib
