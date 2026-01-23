#include "eyelib/render/resources/Mesh.h"

#include <glad/glad.h>

namespace eyelib {
namespace render {

Mesh::Mesh() = default;

Mesh::~Mesh() {
    Destroy();
}

Mesh::Mesh(Mesh&& other) noexcept {
    m_vertices = std::move(other.m_vertices);
    m_indices = std::move(other.m_indices);
    m_vao = other.m_vao; other.m_vao = 0;
    m_vbo = other.m_vbo; other.m_vbo = 0;
    m_ebo = other.m_ebo; other.m_ebo = 0;
    m_uploaded = other.m_uploaded; other.m_uploaded = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_vertices = std::move(other.m_vertices);
    m_indices = std::move(other.m_indices);
    m_vao = other.m_vao; other.m_vao = 0;
    m_vbo = other.m_vbo; other.m_vbo = 0;
    m_ebo = other.m_ebo; other.m_ebo = 0;
    m_uploaded = other.m_uploaded; other.m_uploaded = false;
    return *this;
}

void Mesh::SetVertices(const std::vector<VertexP3C4>& vertices) {
    m_vertices = vertices;
    m_uploaded = false;
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    m_indices = indices;
    m_uploaded = false;
}

void Mesh::UploadToGPU() {
    if (m_uploaded) return;
    if (m_vertices.empty() || m_indices.empty()) return;

    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);
    if (!m_ebo) glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(VertexP3C4)), m_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(uint32_t)), m_indices.data(), GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexP3C4), reinterpret_cast<void*>(0));
    // color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexP3C4), reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
    m_uploaded = true;
}

void Mesh::Destroy() {
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    m_uploaded = false;
}

void Mesh::Bind() const {
    glBindVertexArray(m_vao);
}

void Mesh::Unbind() const {
    glBindVertexArray(0);
}

void Mesh::Draw() const {
    if (!m_vao) return;
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace render
} // namespace eyelib

