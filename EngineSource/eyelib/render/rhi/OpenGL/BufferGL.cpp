#include "eyelib/render/rhi/OpenGL/BufferGL.h"
#include <glad/glad.h>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

size_t BufferGL::GetSize() const { return m_size; }

void* BufferGL::Map(size_t offset, size_t size) {
    if (m_handle == 0) return nullptr;
    if (m_persistent) {
        if (!m_persistentPtr) {
            m_persistentPtr = glMapNamedBufferRange(m_handle, 0, static_cast<GLsizeiptr>(m_size),
                GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
        }
        if (!m_persistentPtr) return nullptr;
        return static_cast<char*>(m_persistentPtr) + offset;
    }
    size_t mapSize = (size == 0) ? m_size : size;
    void* ptr = glMapNamedBufferRange(m_handle, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(mapSize),
                                      GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
    return ptr;
}

void BufferGL::Unmap() {
    if (!m_persistent && m_handle != 0) glUnmapNamedBuffer(m_handle);
}

void BufferGL::UnmapPersistent() {
    if (m_persistent && m_persistentPtr && m_handle != 0) {
        glUnmapNamedBuffer(m_handle);
        m_persistentPtr = nullptr;
    }
}

void BufferGL::Update(const void* data, size_t offset, size_t size) {
    if (m_handle == 0 || !data) return;
    size_t updateSize = (size == 0) ? m_size : size;
    glNamedBufferSubData(m_handle, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(updateSize), data);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
