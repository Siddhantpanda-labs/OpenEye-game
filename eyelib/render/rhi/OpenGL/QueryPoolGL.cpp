#include "eyelib/render/rhi/OpenGL/QueryPoolGL.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {
namespace rhi {

QueryType QueryPoolGL::GetType() const { return m_type; }
uint32_t QueryPoolGL::GetCount() const { return m_count; }

uint64_t QueryPoolGL::GetResult(uint32_t index) {
    if (index >= m_queryIds.size()) return 0;
    GLuint64 result = 0;
    glGetQueryObjectui64v(m_queryIds[index], GL_QUERY_RESULT, &result);
    return static_cast<uint64_t>(result);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
