#include "Mesh.h"
#include "core/log/Log.h"
#include <cstddef>

namespace OpenEye {
namespace Rendering {

Mesh::Mesh() = default;

Mesh::~Mesh() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
    }
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vertices(std::move(other.m_vertices))
    , m_indices(std::move(other.m_indices))
    , m_submeshes(std::move(other.m_submeshes))
    , m_VAO(other.m_VAO)
    , m_VBO(other.m_VBO)
    , m_EBO(other.m_EBO)
    , m_uploaded(other.m_uploaded)
{
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_EBO = 0;
    other.m_uploaded = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        if (m_VAO != 0) {
            glDeleteVertexArrays(1, &m_VAO);
            glDeleteBuffers(1, &m_VBO);
            glDeleteBuffers(1, &m_EBO);
        }
        
        m_vertices = std::move(other.m_vertices);
        m_indices = std::move(other.m_indices);
        m_submeshes = std::move(other.m_submeshes);
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_uploaded = other.m_uploaded;
        
        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
        other.m_uploaded = false;
    }
    return *this;
}

void Mesh::SetVertices(const std::vector<Vertex>& vertices) {
    m_vertices = vertices;
    m_uploaded = false;
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    m_indices = indices;
    m_uploaded = false;
}

void Mesh::AddSubmesh(const Submesh& submesh) {
    m_submeshes.push_back(submesh);
}

void Mesh::UploadToGPU() {
    if (m_vertices.empty()) {
        OE_WARN("Cannot upload mesh: no vertices");
        return;
    }
    
    if (m_uploaded) {
        // Update existing buffers
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 
                       m_vertices.size() * sizeof(Vertex), 
                       m_vertices.data());
        
        if (!m_indices.empty()) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                           m_indices.size() * sizeof(uint32_t),
                           m_indices.data());
        }
        
        return;
    }
    
    CreateVAO();
    
    // Bind VAO before uploading data
    glBindVertexArray(m_VAO);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, 
                m_vertices.size() * sizeof(Vertex),
                m_vertices.data(),
                GL_STATIC_DRAW);
    
    // Upload index data
    if (!m_indices.empty()) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    m_indices.size() * sizeof(uint32_t),
                    m_indices.data(),
                    GL_STATIC_DRAW);
    }
    
    glBindVertexArray(0);
    m_uploaded = true;
}

void Mesh::Bind() const {
    // Ensure GPU buffers are ready at draw time (render-thread safe).
    // Simulation systems may update CPU vertices and call MarkDirty() without uploading immediately.
    if (!m_uploaded) {
        const_cast<Mesh*>(this)->UploadToGPU();
    }
    if (m_VAO != 0) {
        glBindVertexArray(m_VAO);
    }
}

void Mesh::Unbind() const {
    glBindVertexArray(0);
}

void Mesh::Draw(int submeshIndex) const {
    if (!m_uploaded) {
        const_cast<Mesh*>(this)->UploadToGPU();
    }
    if (m_VAO == 0 || !m_uploaded) {
        return;
    }
    
    glBindVertexArray(m_VAO);
    
    if (submeshIndex < 0) {
        // Draw all submeshes
        if (m_indices.empty()) {
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
        } else {
            glDrawElements(GL_TRIANGLES, 
                          static_cast<GLsizei>(m_indices.size()),
                          GL_UNSIGNED_INT, 
                          nullptr);
        }
    } else {
        // Draw specific submesh
        if (submeshIndex >= static_cast<int>(m_submeshes.size())) {
            OE_WARN_F("Invalid submesh index: %d", submeshIndex);
            glBindVertexArray(0);
            return;
        }
        
        const Submesh& submesh = m_submeshes[submeshIndex];
        if (m_indices.empty()) {
            glDrawArrays(GL_TRIANGLES, 
                        submesh.vertexOffset,
                        submesh.vertexCount);
        } else {
            glDrawElements(GL_TRIANGLES,
                          submesh.indexCount,
                          GL_UNSIGNED_INT,
                          reinterpret_cast<void*>(submesh.indexOffset * sizeof(uint32_t)));
        }
    }
    
    glBindVertexArray(0);
}

void Mesh::CreateVAO() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        if (m_EBO != 0) {
            glDeleteBuffers(1, &m_EBO);
        }
    }
    
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    
    if (!m_indices.empty()) {
        glGenBuffers(1, &m_EBO);
    }
    
    // Note: VAO will be bound and buffers uploaded in UploadToGPU()
    // This function just creates the VAO structure
    glBindVertexArray(m_VAO);
    
    // Bind VBO (required when setting vertex attributes with VAO bound)
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    
    // Position (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         reinterpret_cast<void*>(offsetof(Vertex, position)));
    
    // Normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         reinterpret_cast<void*>(offsetof(Vertex, normal)));
    
    // TexCoord (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         reinterpret_cast<void*>(offsetof(Vertex, texCoord)));
    
    // Tangent (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         reinterpret_cast<void*>(offsetof(Vertex, tangent)));
    
    // Bitangent (location 4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         reinterpret_cast<void*>(offsetof(Vertex, bitangent)));
    
    // Bone Indices (location 5) - ivec4 for 4 bone indices per vertex
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, boneIndices)));
    
    // Bone Weights (location 6) - vec4 for 4 bone weights per vertex
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         reinterpret_cast<void*>(offsetof(Vertex, boneWeights)));
    
    // EBO binding will happen in UploadToGPU() when data is uploaded
    // But we can set it up here too
    if (!m_indices.empty()) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    }
    
    glBindVertexArray(0);
}

void Mesh::CalculateBounds(glm::vec3& outMin, glm::vec3& outMax) const {
    if (m_vertices.empty()) {
        outMin = glm::vec3(0.0f);
        outMax = glm::vec3(0.0f);
        return;
    }
    
    outMin = m_vertices[0].position;
    outMax = m_vertices[0].position;
    
    for (const auto& vertex : m_vertices) {
        outMin = glm::min(outMin, vertex.position);
        outMax = glm::max(outMax, vertex.position);
    }
}

} // namespace Rendering
} // namespace OpenEye

