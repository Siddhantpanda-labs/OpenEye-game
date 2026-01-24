#include "eyelib/render/2d/Primitives.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>
#include <cmath>

namespace eyelib {
namespace render {

namespace {
const float kPi = 3.14159265358979323846f;
}

Primitives::Primitives() = default;

Primitives::~Primitives() {
    Shutdown();
}

void Primitives::Shutdown() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_lineVbo) { glDeleteBuffers(1, &m_lineVbo); m_lineVbo = 0; }
    if (m_lineEbo) { glDeleteBuffers(1, &m_lineEbo); m_lineEbo = 0; }
    if (m_triVbo) { glDeleteBuffers(1, &m_triVbo); m_triVbo = 0; }
    if (m_triEbo) { glDeleteBuffers(1, &m_triEbo); m_triEbo = 0; }
    m_initialized = false;
}

bool Primitives::Initialize() {
    if (m_initialized) return true;
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_lineVbo);
    glGenBuffers(1, &m_lineEbo);
    glGenBuffers(1, &m_triVbo);
    glGenBuffers(1, &m_triEbo);
    m_initialized = true;
    return true;
}

void Primitives::SetViewProjection(const math::Mat4& view, const math::Mat4& projection) {
    m_view = view;
    m_projection = projection;
}

void Primitives::DrawLine(const math::Vec2& start, const math::Vec2& end, const math::Vec4& color, float width) {
    if (!m_initialized && !Initialize()) return;
    math::Vec2 d = end - start;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-6f) return;
    float nx = -d.y / len * width * 0.5f;
    float ny = d.x / len * width * 0.5f;
    size_t base = m_lineVerts.size();
    m_lineVerts.push_back({{ start.x + nx, start.y + ny }, color});
    m_lineVerts.push_back({{ start.x - nx, start.y - ny }, color});
    m_lineVerts.push_back({{ end.x - nx, end.y - ny }, color});
    m_lineVerts.push_back({{ end.x + nx, end.y + ny }, color});
    m_lineIndices.push_back(static_cast<unsigned int>(base) + 0);
    m_lineIndices.push_back(static_cast<unsigned int>(base) + 1);
    m_lineIndices.push_back(static_cast<unsigned int>(base) + 2);
    m_lineIndices.push_back(static_cast<unsigned int>(base) + 0);
    m_lineIndices.push_back(static_cast<unsigned int>(base) + 2);
    m_lineIndices.push_back(static_cast<unsigned int>(base) + 3);
}

void Primitives::DrawCircle(const math::Vec2& center, float radius, const math::Vec4& color, int segments) {
    if (!m_initialized && !Initialize()) return;
    if (segments < 3) segments = 3;
    size_t base = m_triVerts.size();
    m_triVerts.push_back({{ center.x, center.y }, color});
    for (int i = 0; i <= segments; ++i) {
        float a = (float)i / (float)segments * 2.0f * kPi;
        m_triVerts.push_back({{ center.x + radius * std::cos(a), center.y + radius * std::sin(a) }, color});
    }
    for (int i = 0; i < segments; ++i) {
        m_triIndices.push_back(static_cast<unsigned int>(base));
        m_triIndices.push_back(static_cast<unsigned int>(base) + 1 + i);
        m_triIndices.push_back(static_cast<unsigned int>(base) + 2 + i);
    }
}

void Primitives::DrawRectangle(const math::Vec2& min, const math::Vec2& max, const math::Vec4& color) {
    if (!m_initialized && !Initialize()) return;
    size_t base = m_triVerts.size();
    m_triVerts.push_back({{ min.x, min.y }, color});
    m_triVerts.push_back({{ max.x, min.y }, color});
    m_triVerts.push_back({{ max.x, max.y }, color});
    m_triVerts.push_back({{ min.x, max.y }, color});
    m_triIndices.push_back(static_cast<unsigned int>(base) + 0);
    m_triIndices.push_back(static_cast<unsigned int>(base) + 1);
    m_triIndices.push_back(static_cast<unsigned int>(base) + 2);
    m_triIndices.push_back(static_cast<unsigned int>(base) + 0);
    m_triIndices.push_back(static_cast<unsigned int>(base) + 2);
    m_triIndices.push_back(static_cast<unsigned int>(base) + 3);
}

void Primitives::DrawPolygon(const std::vector<math::Vec2>& points, const math::Vec4& color) {
    if (!m_initialized && !Initialize()) return;
    if (points.size() < 3) return;
    size_t base = m_triVerts.size();
    math::Vec2 c(0.0f, 0.0f);
    for (const auto& p : points) { c.x += p.x; c.y += p.y; }
    c.x /= (float)points.size();
    c.y /= (float)points.size();
    m_triVerts.push_back({{ c.x, c.y }, color});
    for (size_t i = 0; i < points.size(); ++i)
        m_triVerts.push_back({{ points[i].x, points[i].y }, color});
    for (size_t i = 0; i < points.size(); ++i) {
        size_t j = (i + 1) % points.size();
        m_triIndices.push_back(static_cast<unsigned int>(base));
        m_triIndices.push_back(static_cast<unsigned int>(base) + 1 + (unsigned int)i);
        m_triIndices.push_back(static_cast<unsigned int>(base) + 1 + (unsigned int)j);
    }
}

void Primitives::Flush() {
    FlushLines();
    FlushTriangles();
}

void Primitives::FlushLines() {
    if (m_lineVerts.empty()) return;
    static std::shared_ptr<Shader> s_shader;
    if (!s_shader || !s_shader->IsValid()) {
        s_shader = std::make_shared<Shader>();
        s_shader->LoadFromFile("assets/shaders/passes/2d/primitive.vert", "assets/shaders/passes/2d/primitive.frag");
    }
    if (!s_shader->IsValid()) return;
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m_lineVerts.size() * sizeof(LineVert)), m_lineVerts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(m_lineIndices.size() * sizeof(unsigned int)), m_lineIndices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(LineVert), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVert), reinterpret_cast<void*>(offsetof(LineVert, color)));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    s_shader->Bind();
    s_shader->SetMat4("u_View", m_view);
    s_shader->SetMat4("u_Projection", m_projection);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_lineIndices.size(), GL_UNSIGNED_INT, nullptr);
    s_shader->Unbind();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_lineVerts.clear();
    m_lineIndices.clear();
}

void Primitives::FlushTriangles() {
    if (m_triVerts.empty()) return;
    static std::shared_ptr<Shader> s_shader;
    if (!s_shader || !s_shader->IsValid()) {
        s_shader = std::make_shared<Shader>();
        s_shader->LoadFromFile("assets/shaders/passes/2d/primitive.vert", "assets/shaders/passes/2d/primitive.frag");
    }
    if (!s_shader->IsValid()) return;
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_triVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m_triVerts.size() * sizeof(TriVert)), m_triVerts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_triEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(m_triIndices.size() * sizeof(unsigned int)), m_triIndices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TriVert), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(TriVert), reinterpret_cast<void*>(offsetof(TriVert, color)));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    s_shader->Bind();
    s_shader->SetMat4("u_View", m_view);
    s_shader->SetMat4("u_Projection", m_projection);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_triIndices.size(), GL_UNSIGNED_INT, nullptr);
    s_shader->Unbind();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_triVerts.clear();
    m_triIndices.clear();
}

} // namespace render
} // namespace eyelib
