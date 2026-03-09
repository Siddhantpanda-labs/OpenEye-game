#include "eyelib/render/2d/SpriteRenderer.h"
#include "eyelib/render/2d/Sprite.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
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
    if (m_deviceRHI && m_vertexBufferRHI) { m_deviceRHI->DestroyBuffer(m_vertexBufferRHI); m_vertexBufferRHI = nullptr; }
    if (m_deviceRHI && m_indexBufferRHI) { m_deviceRHI->DestroyBuffer(m_indexBufferRHI); m_indexBufferRHI = nullptr; }
    m_vertexCapacity = 0;
    m_indexCapacity = 0;
    m_sprites.clear();
    m_vertices.clear();
    m_indices.clear();
    m_batches.clear();
    m_initialized = false;
    m_dirty = true;
}

bool SpriteRenderer::Initialize() {
    if (m_initialized) return true;
    m_deviceRHI = m_injectedDevice;
    if (!m_deviceRHI) return false;
    EnsureShader();
    if (!m_shader || !m_shader->IsValid()) {
        core::Log::ErrorF("SpriteRenderer: failed to load sprite shader");
        return false;
    }
    m_initialized = true;
    return true;
}

void SpriteRenderer::EnsureShader() {
    if (m_shader && m_shader->IsValid()) return;
    m_shader = std::make_shared<Shader>();
    rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : m_deviceRHI;
    if (!device) return;
    m_shader->Load(device, "passes/2d/sprite", GetVertexLayout());
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
            batch.indexOffset = batchStartOffset;
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
        batch.indexOffset = batchStartOffset;
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

    rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : m_deviceRHI;
    if (!device) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd) return;
    EnsureBuffers(m_vertices.size() * sizeof(Vertex), m_indices.size() * sizeof(unsigned int));
    if (!m_vertexBufferRHI || !m_indexBufferRHI) return;
    m_vertexBufferRHI->Update(m_vertices.data(), 0, m_vertices.size() * sizeof(Vertex));
    m_indexBufferRHI->Update(m_indices.data(), 0, m_indices.size() * sizeof(unsigned int));

    m_shader->Bind(device);
    m_shader->SetMat4("u_View", view);
    m_shader->SetMat4("u_Projection", projection);
    m_shader->SetInt("u_Texture", 0);
    cmd->BindVertexBuffer(m_vertexBufferRHI, 0);
    cmd->BindIndexBuffer(m_indexBufferRHI, rhi::IndexType::UInt32);
    cmd->SetBlend(true);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);

    for (const auto& batch : m_batches) {
        if (!batch.texture || !batch.texture->IsValid()) continue;
        batch.texture->Bind(0);
        cmd->DrawIndexed(static_cast<uint32_t>(batch.indexCount), static_cast<uint32_t>(batch.indexOffset), 0);
    }

    m_shader->Unbind();
}

rhi::VertexLayout SpriteRenderer::GetVertexLayout() const {
    rhi::VertexLayout layout;
    layout.strideInBytes = sizeof(Vertex);
    layout.attributes = {
        {0, rhi::VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, position))},
        {1, rhi::VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, uv))},
        {2, rhi::VertexAttributeType::Float4, static_cast<uint32_t>(offsetof(Vertex, color))},
    };
    return layout;
}

void SpriteRenderer::EnsureBuffers(size_t vertexBytes, size_t indexBytes) {
    if (!m_deviceRHI) return;
    if (!m_vertexBufferRHI || vertexBytes > m_vertexCapacity) {
        if (m_vertexBufferRHI) m_deviceRHI->DestroyBuffer(m_vertexBufferRHI);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(vertexBytes, 1024);
        desc.usage = rhi::BufferUsage::Vertex;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_vertexBufferRHI = m_deviceRHI->CreateBuffer(desc);
        m_vertexCapacity = desc.sizeInBytes;
    }
    if (!m_indexBufferRHI || indexBytes > m_indexCapacity) {
        if (m_indexBufferRHI) m_deviceRHI->DestroyBuffer(m_indexBufferRHI);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(indexBytes, 1024);
        desc.usage = rhi::BufferUsage::Index;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_indexBufferRHI = m_deviceRHI->CreateBuffer(desc);
        m_indexCapacity = desc.sizeInBytes;
    }
}

} // namespace render
} // namespace eyelib
