#include "eyelib/render/resources/Mesh.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace eyelib {
namespace render {

namespace {

void accumulateNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) continue;
        math::Vec3 p0 = vertices[a].position;
        math::Vec3 p1 = vertices[b].position;
        math::Vec3 p2 = vertices[c].position;
        math::Vec3 e1 = p1 - p0;
        math::Vec3 e2 = p2 - p0;
        math::Vec3 n = glm::cross(e1, e2);
        float len = glm::length(n);
        if (len > 1e-7f) n /= len;
        vertices[a].normal += n;
        vertices[b].normal += n;
        vertices[c].normal += n;
    }
    for (auto& v : vertices) {
        float len = glm::length(v.normal);
        if (len > 1e-7f) v.normal /= len;
        else v.normal = math::Vec3(0, 1, 0);
    }
}

void accumulateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) continue;
        math::Vec3 p0 = vertices[a].position, p1 = vertices[b].position, p2 = vertices[c].position;
        math::Vec2 uv0 = vertices[a].uv, uv1 = vertices[b].uv, uv2 = vertices[c].uv;
        math::Vec3 e1 = p1 - p0, e2 = p2 - p0;
        math::Vec2 d1 = uv1 - uv0, d2 = uv2 - uv0;
        float denom = d1.x * d2.y - d2.x * d1.y;
        if (std::fabs(denom) < 1e-7f) continue;
        float r = 1.0f / denom;
        math::Vec3 t = (e1 * d2.y - e2 * d1.y) * r;
        vertices[a].tangent += t;
        vertices[b].tangent += t;
        vertices[c].tangent += t;
    }
    for (auto& v : vertices) {
        math::Vec3 n = v.normal;
        math::Vec3 t = v.tangent;
        t = t - n * glm::dot(n, t);
        float len = glm::length(t);
        if (len > 1e-7f) v.tangent = t / len;
        else v.tangent = math::Vec3(1, 0, 0);
    }
}

/* Simple vertex cache optimization: reorder triangles to improve locality. */
void optimizeVertexCache(std::vector<uint32_t>& indices) {
    (void)indices;
    /* Placeholder: full tipsify would go here. */
}

} // namespace

Mesh::Mesh() = default;

Mesh::~Mesh() { Destroy(); }

Mesh::Mesh(Mesh&& other) noexcept
    : m_vertices(std::move(other.m_vertices)),
      m_indices32(std::move(other.m_indices32)),
      m_indices16(std::move(other.m_indices16)),
      m_indexFormat(other.m_indexFormat),
      m_submeshes(std::move(other.m_submeshes)),
      m_aabb(other.m_aabb),
      m_aabbDirty(other.m_aabbDirty),
      m_vao(other.m_vao),
      m_vbo(other.m_vbo),
      m_ebo(other.m_ebo),
      m_uploaded(other.m_uploaded) {
    other.m_vao = other.m_vbo = other.m_ebo = 0;
    other.m_uploaded = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_vertices = std::move(other.m_vertices);
    m_indices32 = std::move(other.m_indices32);
    m_indices16 = std::move(other.m_indices16);
    m_indexFormat = other.m_indexFormat;
    m_submeshes = std::move(other.m_submeshes);
    m_aabb = other.m_aabb;
    m_aabbDirty = other.m_aabbDirty;
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_ebo = other.m_ebo;
    m_uploaded = other.m_uploaded;
    other.m_vao = other.m_vbo = other.m_ebo = 0;
    other.m_uploaded = false;
    return *this;
}

void Mesh::SetVertices(const std::vector<Vertex>& vertices) {
    m_vertices = vertices;
    m_aabbDirty = true;
    Invalidate();
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    m_indices32 = indices;
    m_indices16.clear();
    m_indexFormat = IndexFormat::UInt32;
    Invalidate();
}

void Mesh::SetIndices(const std::vector<uint16_t>& indices) {
    m_indices16 = indices;
    m_indices32.clear();
    m_indexFormat = IndexFormat::UInt16;
    Invalidate();
}

void Mesh::AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex) {
    Submesh s;
    s.indexOffset = indexOffset;
    s.indexCount = indexCount;
    s.materialIndex = materialIndex;
    m_submeshes.push_back(s);
}

void Mesh::SetSubmesh(size_t index, uint32_t indexOffset, uint32_t indexCount) {
    if (index >= m_submeshes.size()) return;
    m_submeshes[index].indexOffset = indexOffset;
    m_submeshes[index].indexCount = indexCount;
}

void Mesh::ClearSubmeshes() { m_submeshes.clear(); }

void Mesh::Invalidate() { m_uploaded = false; }

void Mesh::ComputeAABB() {
    m_aabb = math::AABB();
    if (m_vertices.empty()) {
        m_aabbDirty = false;
        return;
    }
    for (const auto& v : m_vertices)
        m_aabb.Expand(v.position);
    m_aabbDirty = false;
}

math::AABB Mesh::GetAABB() const {
    if (m_aabbDirty) {
        m_aabb = math::AABB();
        for (const auto& v : m_vertices)
            m_aabb.Expand(v.position);
        m_aabbDirty = false;
    }
    return m_aabb;
}

void Mesh::GenerateNormals() {
    if (m_indices32.empty()) return;
    for (auto& v : m_vertices) v.normal = math::Vec3(0, 0, 0);
    accumulateNormals(m_vertices, m_indices32);
}

void Mesh::GenerateTangents() {
    if (m_indices32.empty()) return;
    for (auto& v : m_vertices) v.tangent = math::Vec3(0, 0, 0);
    accumulateTangents(m_vertices, m_indices32);
}

void Mesh::Optimize() {
    if (!m_indices32.empty()) optimizeVertexCache(m_indices32);
}

void Mesh::CreateVAO() {
    if (!m_vao) glGenVertexArrays(1, &m_vao);
    if (!m_vbo) glGenBuffers(1, &m_vbo);
    if (!m_ebo) glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    if (m_indexFormat == IndexFormat::UInt32)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices32.size() * sizeof(uint32_t)), m_indices32.data(), GL_STATIC_DRAW);
    else
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices16.size() * sizeof(uint16_t)), m_indices16.data(), GL_STATIC_DRAW);

    size_t off = 0;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(off));
    off += sizeof(math::Vec3);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(off));
    off += sizeof(math::Vec3);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(off));
    off += sizeof(math::Vec2);
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(off));
    off += sizeof(math::Vec2);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(off));
    off += sizeof(math::Vec3);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(off));
    off += sizeof(math::Vec4);
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), reinterpret_cast<void*>(off));
    off += sizeof(math::IVec4);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(off));

    glBindVertexArray(0);
}

void Mesh::UploadToGPU() {
    if (m_uploaded) return;
    if (m_vertices.empty()) return;
    if (m_indexFormat == IndexFormat::UInt32 && m_indices32.empty()) return;
    if (m_indexFormat == IndexFormat::UInt16 && m_indices16.empty()) return;

    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
        m_vao = m_vbo = m_ebo = 0;
    }
    CreateVAO();
    if (m_aabbDirty) ComputeAABB();
    m_uploaded = true;
}

void Mesh::Destroy() {
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    m_uploaded = false;
}

void Mesh::Bind() const { glBindVertexArray(m_vao); }
void Mesh::Unbind() const { glBindVertexArray(0); }

void Mesh::Draw(int submeshIndex) const {
    if (!m_vao) return;
    glBindVertexArray(m_vao);
    GLenum idxType = (m_indexFormat == IndexFormat::UInt32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    const void* offset = nullptr;

    if (submeshIndex >= 0 && static_cast<size_t>(submeshIndex) < m_submeshes.size()) {
        const Submesh& s = m_submeshes[static_cast<size_t>(submeshIndex)];
        size_t off = s.indexOffset * (m_indexFormat == IndexFormat::UInt32 ? sizeof(uint32_t) : sizeof(uint16_t));
        offset = reinterpret_cast<const void*>(off);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(s.indexCount), idxType, offset);
    } else {
        GLsizei cnt = static_cast<GLsizei>(m_indexFormat == IndexFormat::UInt32 ? m_indices32.size() : m_indices16.size());
        glDrawElements(GL_TRIANGLES, cnt, idxType, nullptr);
    }
    glBindVertexArray(0);
}

} // namespace render
} // namespace eyelib
