#include "eyelib/render/rhi/OpenGL/ShaderGL.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {
namespace rhi {

ShaderStage ShaderGL::GetStage() const { return m_stage; }
bool ShaderGL::IsValid() const { return m_handle != 0; }

} // namespace rhi
} // namespace render
} // namespace eyelib
