#pragma once

#include <vector>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

struct Submesh {
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t vertexOffset;
    uint32_t vertexCount;
    int materialIndex = -1;
};

class Mesh {
public:
    Mesh();
    ~Mesh();
    
    // Non-copyable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    
    // Movable
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    
    // Build mesh from data
    void SetVertices(const std::vector<Vertex>& vertices);
    void SetIndices(const std::vector<uint32_t>& indices);
    void AddSubmesh(const Submesh& submesh);
    
    // Upload to GPU
    void UploadToGPU();
    
    // Render
    void Bind() const;
    void Unbind() const;
    void Draw(int submeshIndex = -1) const; // -1 = draw all
    
    // Get properties
    size_t GetSubmeshCount() const { return m_submeshes.size(); }
    const Submesh& GetSubmesh(size_t index) const { return m_submeshes[index]; }
    size_t GetVertexCount() const { return m_vertices.size(); }
    size_t GetIndexCount() const { return m_indices.size(); }
    bool IsValid() const { return m_VAO != 0; }
    
    // Get VAO for instanced rendering
    GLuint GetVAO() const { return m_VAO; }
    
    // Access raw data
    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_indices; }
    
    // Bounding box calculation
    void CalculateBounds(glm::vec3& outMin, glm::vec3& outMax) const;

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Submesh> m_submeshes;
    
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    GLuint m_EBO = 0;
    
    bool m_uploaded = false;
    
    void CreateVAO();
};

} // namespace Rendering
} // namespace OpenEye

