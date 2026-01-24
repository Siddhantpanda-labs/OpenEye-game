#include "eyelib/render/2d/SpriteRenderer.h"
#include "eyelib/render/2d/Sprite.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>
#include <algorithm>

namespace eyelib {
namespace render {

namespace {
const float kQuadVerts[8] = { 0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f };
const unsigned int kQuadIndices[6] = { 0, 1, 2, 2, 3, 0 };
}

SpriteRenderer::SpriteRenderer() = default;

SpriteRenderer::~SpriteRenderer() {
    Shutdown();
}

void SpriteRenderer::Shutdown() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    m_sprites.clear();
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_initialized = false;
    m_dirty = true;
}

bool SpriteRenderer::Initialize() {
    if (m_initialized) return true;
    EnsureShader();
    if (!m_shader || !m_shader->IsValid()) {
        core::Log::ErrorF("SpriteRenderer: failed to load sprite shader");
        return false;
    }
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    m_initialized = true;
    return true;
}

void SpriteRenderer::EnsureShader() {
    if (m_shader && m_shader->IsValid()) return;
    m_shader = std::make_shared<Shader>();
    m_shader->LoadFromFile("assets/shaders/passes/2d/sprite.vert", "assets/shaders/passes/2d/sprite.frag");
}

void SpriteRenderer::SetShader(std::shared_ptr<Shader> shader) {
    m_shader = std::move(shader);
}

void SpriteRenderer::AddSprite(const Sprite& sprite) {
    auto tex = sprite.GetTexture();
    if (!tex || !tex->IsValid()) return;
    m_sprites.push_back(sprite);
    m_dirty = true;
}

void SpriteRenderer::Clear() {
    m_sprites.clear();
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_dirty = true;
}

void SpriteRenderer::BuildBatches() {
    if (!m_dirty) return;
    m_dirty = false;
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    if (m_sprites.empty()) return;

    std::vector<size_t> order(m_sprites.size());
    for (size_t i = 0; i < m_sprites.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [this](size_t a, size_t b) {
        return m_sprites[a].GetTexture()->Id() < m_sprites[b].GetTexture()->Id();
    });

    unsigned int prevTexId = 0;
    size_t batchStartOffset = 0;
    for (size_t k = 0; k < order.size(); ++k) {
        const Sprite& sprite = m_sprites[order[k]];
        auto tex = sprite.GetTexture();
        unsigned int texId = tex->Id();
        if (texId != prevTexId && prevTexId != 0) {
            Batch batch;
            batch.texture = m_sprites[order[k - 1]].GetTexture();
            batch.indexOffset = batchStartOffset * sizeof(unsigned int);
            batch.indexCount = (m_indices.size() - batchStartOffset);
            m_batches.push_back(batch);
            batchStartOffset = m_indices.size();
        }
        prevTexId = texId;

        const UVRect& uv = sprite.GetUV();
        math::Vec4 color = sprite.GetColor();
        math::Mat4 model = sprite.GetTransform();
        float u0 = uv.minU, v0 = uv.minV, u1 = uv.maxU, v1 = uv.maxV;
        size_t base = m_vertices.size();
        for (int i = 0; i < 4; ++i) {
            math::Vec2 p(kQuadVerts[i * 2], kQuadVerts[i * 2 + 1]);
            math::Vec4 hv = model * math::Vec4(p.x, p.y, 0.0f, 1.0f);
            Vertex v;
            v.position = math::Vec2(hv.x, hv.y);
            v.uv = math::Vec2(i == 0 || i == 3 ? u0 : u1, i < 2 ? v0 : v1);
            v.color = color;
            m_vertices.push_back(v);
        }
        for (unsigned int idx : kQuadIndices) {
            m_indices.push_back(static_cast<unsigned int>(base) + idx);
        }
    }
    if (!m_sprites.empty()) {
        Batch batch;
        batch.texture = m_sprites[order.back()].GetTexture();
        batch.indexOffset = batchStartOffset * sizeof(unsigned int);
        batch.indexCount = m_indices.size() - batchStartOffset;
        m_batches.push_back(batch);
    }
}

void SpriteRenderer::Render(const math::Mat4& view, const math::Mat4& projection) {
    if (m_sprites.empty()) return;
    if (!m_initialized && !Initialize()) return;
    EnsureShader();
    if (!m_shader->IsValid()) return;

    BuildBatches();
    if (m_vertices.empty() || m_indices.empty()) return;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(unsigned int)), m_indices.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
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

    for (const auto& batch : m_batches) {
        if (!batch.texture || !batch.texture->IsValid()) continue;
        batch.texture->Bind(0);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.indexCount), GL_UNSIGNED_INT, reinterpret_cast<void*>(batch.indexOffset));
    }

    m_shader->Unbind();
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

} // namespace render
} // namespace eyelib
