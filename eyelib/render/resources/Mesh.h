#pragma once

#include <vector>

using GLuint = unsigned int;

namespace eyelib {
namespace render {

struct VertexP3C4 {
    float px, py, pz;
    float r, g, b, a;
};

class Mesh {
public:
    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void SetVertices(const std::vector<VertexP3C4>& vertices);
    void SetIndices(const std::vector<uint32_t>& indices);

    void UploadToGPU();
    void Destroy();

    void Bind() const;
    void Unbind() const;
    void Draw() const;

    bool IsValid() const { return m_vao != 0; }

private:
    std::vector<VertexP3C4> m_vertices;
    std::vector<uint32_t> m_indices;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    bool m_uploaded = false;
};

} // namespace render
} // namespace eyelib

