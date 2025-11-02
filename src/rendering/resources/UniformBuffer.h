#pragma once

#include <glad/glad.h>
#include <cstddef>

namespace OpenEye { namespace Rendering {

class UniformBuffer {
public:
    UniformBuffer() = default;
    ~UniformBuffer();

    // Non-copyable
    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;

    // Movable
    UniformBuffer(UniformBuffer&& other) noexcept;
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;

    bool Create(std::size_t sizeBytes, GLuint bindingIndex);
    void Update(const void* data, std::size_t sizeBytes, std::size_t offsetBytes = 0);
    void Bind() const; // binds to the binding point
    void Destroy();

    GLuint GetID() const { return m_bufferID; }
    GLuint GetBinding() const { return m_bindingIndex; }

private:
    GLuint m_bufferID = 0;
    GLuint m_bindingIndex = 0;
    std::size_t m_size = 0;
};

} }


