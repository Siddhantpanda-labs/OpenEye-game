#include "UniformBuffer.h"

namespace OpenEye { namespace Rendering {

UniformBuffer::~UniformBuffer() { Destroy(); }

UniformBuffer::UniformBuffer(UniformBuffer&& other) noexcept {
    m_bufferID = other.m_bufferID;
    m_bindingIndex = other.m_bindingIndex;
    m_size = other.m_size;
    other.m_bufferID = 0;
}

UniformBuffer& UniformBuffer::operator=(UniformBuffer&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_bufferID = other.m_bufferID;
        m_bindingIndex = other.m_bindingIndex;
        m_size = other.m_size;
        other.m_bufferID = 0;
    }
    return *this;
}

bool UniformBuffer::Create(std::size_t sizeBytes, GLuint bindingIndex) {
    Destroy();
    m_size = sizeBytes;
    m_bindingIndex = bindingIndex;
    glGenBuffers(1, &m_bufferID);
    glBindBuffer(GL_UNIFORM_BUFFER, m_bufferID);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(sizeBytes), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingIndex, m_bufferID);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    return m_bufferID != 0;
}

void UniformBuffer::Update(const void* data, std::size_t sizeBytes, std::size_t offsetBytes) {
    if (m_bufferID == 0) return;
    glBindBuffer(GL_UNIFORM_BUFFER, m_bufferID);
    glBufferSubData(GL_UNIFORM_BUFFER, static_cast<GLintptr>(offsetBytes), static_cast<GLsizeiptr>(sizeBytes), data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::Bind() const {
    if (m_bufferID == 0) return;
    glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingIndex, m_bufferID);
}

void UniformBuffer::Destroy() {
    if (m_bufferID) {
        glDeleteBuffers(1, &m_bufferID);
        m_bufferID = 0;
    }
}

} }


