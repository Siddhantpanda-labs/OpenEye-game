#include "eyelib/rendering/2d/TextRenderer.h"
#include "eyelib/rendering/core/PassPushConstants.h"
#include "eyelib/rendering/backend/RenderBackendPolicy.h"
#include "eyelib/rendering/2d/Font.h"
#include "eyelib/rendering/resources/Shader.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include "eyelib/rendering/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <optional>

namespace eyelib {
namespace rendering {

TextRenderer::TextRenderer() = default;

TextRenderer::~TextRenderer() {
    Shutdown();
}

void TextRenderer::Shutdown() {
    if (m_deviceRHI && m_spriteSet) { m_deviceRHI->DestroyDescriptorSet(m_spriteSet); m_spriteSet = nullptr; }
    if (m_deviceRHI && m_vertexBufferRHI) { m_deviceRHI->DestroyBuffer(m_vertexBufferRHI); m_vertexBufferRHI = nullptr; }
    if (m_deviceRHI && m_indexBufferRHI) { m_deviceRHI->DestroyBuffer(m_indexBufferRHI); m_indexBufferRHI = nullptr; }
    m_vertexCapacity = 0;
    m_indexCapacity = 0;
    m_vertices.clear();
    m_indices.clear();
    m_initialized = false;
}

bool TextRenderer::Initialize() {
    if (m_initialized) return true;
    m_deviceRHI = m_injectedDevice;
    if (!m_deviceRHI) return false;
    EnsureShader();
    if (!m_shader || !m_shader->IsValid()) {
        core::Log::ErrorF("TextRenderer: failed to load sprite shader");
        return false;
    }
    m_initialized = true;
    return true;
}

void TextRenderer::EnsureShader() {
    if (m_shader && m_shader->IsValid()) return;
    m_shader = std::make_shared<Shader>();
    rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : m_deviceRHI;
    if (!device) return;
    m_shader->Load(device, "passes/2d/sprite", GetVertexLayout(), rhi::PrimitiveTopology::TriangleList, rhi::PipelineLayoutKind::Post,
                     std::optional{rhi::StandardAttachmentFormats::PostLDR8()});
    if (!m_spriteSet) {
        rhi::IPipelineLayout* postPassLayout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::PostPass);
        if (postPassLayout) m_spriteSet = device->CreateDescriptorSet(postPassLayout);
    }
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
    rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : m_deviceRHI;
    if (!device) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (!cmd) return;
    EnsureBuffers(m_vertices.size() * sizeof(Vertex), m_indices.size() * sizeof(unsigned int));
    if (!m_vertexBufferRHI || !m_indexBufferRHI) return;
    m_vertexBufferRHI->Update(m_vertices.data(), 0, m_vertices.size() * sizeof(Vertex));
    m_indexBufferRHI->Update(m_indices.data(), 0, m_indices.size() * sizeof(unsigned int));
    m_shader->Bind(device);
    if (backend::IsVulkan(device))
        abi::WriteViewProjectionPairPushConstants(cmd, view, projection);
    else {
        m_shader->SetMat4("u_View", view);
        m_shader->SetMat4("u_Projection", projection);
    }
    if (m_spriteSet && font.GetAtlasTexture() && font.GetAtlasTexture()->GetTextureRHI()) {
        m_spriteSet->UpdateTexture(0, font.GetAtlasTexture()->GetTextureRHI());
        rhi::IDescriptorSet* sets[2] = { device->GetEmptyResourceSet(), m_spriteSet };
        cmd->BindDescriptorSets(0, 2, sets);
    } else {
        m_shader->SetInt("u_Texture", 0);
        font.GetAtlasTexture()->Bind(0);
    }
    cmd->BindVertexBuffer(m_vertexBufferRHI, 0);
    cmd->BindIndexBuffer(m_indexBufferRHI, rhi::IndexType::UInt32);
    cmd->SetBlend(true);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    cmd->DrawIndexed(static_cast<uint32_t>(m_indices.size()), 0, 0);
    m_shader->Unbind();
}

rhi::VertexLayout TextRenderer::GetVertexLayout() const {
    rhi::VertexLayout layout;
    layout.strideInBytes = sizeof(Vertex);
    layout.attributes = {
        {0, rhi::VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, position))},
        {1, rhi::VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, uv))},
        {2, rhi::VertexAttributeType::Float4, static_cast<uint32_t>(offsetof(Vertex, color))},
    };
    return layout;
}

void TextRenderer::EnsureBuffers(size_t vertexBytes, size_t indexBytes) {
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

} // namespace rendering
} // namespace eyelib
