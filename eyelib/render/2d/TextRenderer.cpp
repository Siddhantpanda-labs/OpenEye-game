#include "eyelib/render/2d/TextRenderer.h"
#include "eyelib/render/2d/Font.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>

namespace eyelib {
namespace render {

TextRenderer::TextRenderer() = default;

TextRenderer::~TextRenderer() {
    Shutdown();
}

void TextRenderer::Shutdown() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    m_vertices.clear();
    m_indices.clear();
    m_initialized = false;
}

bool TextRenderer::Initialize() {
    if (m_initialized) return true;
    EnsureShader();
    if (!m_shader || !m_shader->IsValid()) {
        core::Log::ErrorF("TextRenderer: failed to load sprite shader");
        return false;
    }
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    m_initialized = true;
    return true;
}

void TextRenderer::EnsureShader() {
    if (m_shader && m_shader->IsValid()) return;
    m_shader = std::make_shared<Shader>();
    m_shader->LoadFromFile("assets/shaders/passes/2d/sprite.vert", "assets/shaders/passes/2d/sprite.frag");
}

void TextRenderer::SetShader(std::shared_ptr<Shader> shader) {
    m_shader = std::move(shader);
}

void TextRenderer::RenderText(const std::string& text, Font& font, const math::Vec2& position,
                              const math::Vec4& color, float size,
                              const math::Mat4& view, const math::Mat4& projection) {
    if (!font.IsValid() || text.empty()) return;
    if (!m_initialized && !Initialize()) return;
    EnsureShader();
    if (!m_shader->IsValid()) return;

    m_vertices.clear();
    m_indices.clear();
    float scale = size / font.GetPixelHeight();
    float x = position.x;
    float y = position.y;

    for (unsigned char c : text) {
        int cp = static_cast<int>(c);
        GlyphQuad q;
        if (!font.GetGlyphQuad(cp, x, y, q)) continue;
        x += q.advance * scale;
        float x0 = q.x0, y0 = q.y0;
        float x1 = q.x0 + (q.x1 - q.x0) * scale, y1 = q.y0 + (q.y1 - q.y0) * scale;
        size_t base = m_vertices.size();
        Vertex v;
        v.color = color;
        v.position = math::Vec2(x0, y0); v.uv = math::Vec2(q.s0, q.t1); m_vertices.push_back(v);
        v.position = math::Vec2(x1, y0); v.uv = math::Vec2(q.s1, q.t1); m_vertices.push_back(v);
        v.position = math::Vec2(x1, y1); v.uv = math::Vec2(q.s1, q.t0); m_vertices.push_back(v);
        v.position = math::Vec2(x0, y1); v.uv = math::Vec2(q.s0, q.t0); m_vertices.push_back(v);
        const unsigned int idx[6] = { 0, 1, 2, 2, 3, 0 };
        for (unsigned int i : idx)
            m_indices.push_back(static_cast<unsigned int>(base) + i);
    }
    if (m_vertices.empty()) return;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(unsigned int)), m_indices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    m_shader->Bind();
    m_shader->SetMat4("u_View", view);
    m_shader->SetMat4("u_Projection", projection);
    m_shader->SetInt("u_Texture", 0);
    font.GetAtlasTexture()->Bind(0);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
    m_shader->Unbind();
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

} // namespace render
} // namespace eyelib
