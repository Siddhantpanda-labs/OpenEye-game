#include "eyelib/render/2d/Primitives.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace eyelib {
namespace render {

namespace {
const float kPi = 3.14159265358979323846f;
static std::shared_ptr<Shader> s_lineShader;
static std::shared_ptr<Shader> s_triShader;
static rhi::IDevice* s_staticDevice = nullptr;
}

void Primitives::SetStaticDevice(rhi::IDevice* device) {
    s_staticDevice = device;
}

Primitives::Primitives() = default;

Primitives::~Primitives() {
    Shutdown();
}

void Primitives::Shutdown() {
    if (m_deviceRHI && m_lineVbo) { m_deviceRHI->DestroyBuffer(m_lineVbo); m_lineVbo = nullptr; }
    if (m_deviceRHI && m_lineEbo) { m_deviceRHI->DestroyBuffer(m_lineEbo); m_lineEbo = nullptr; }
    if (m_deviceRHI && m_triVbo) { m_deviceRHI->DestroyBuffer(m_triVbo); m_triVbo = nullptr; }
    if (m_deviceRHI && m_triEbo) { m_deviceRHI->DestroyBuffer(m_triEbo); m_triEbo = nullptr; }
    m_lineVboCapacity = 0;
    m_lineEboCapacity = 0;
    m_triVboCapacity = 0;
    m_triEboCapacity = 0;
    m_initialized = false;
}

void Primitives::ShutdownStaticResources() {
    s_lineShader.reset();
    s_triShader.reset();
}

bool Primitives::Initialize() {
    if (m_initialized) return true;
    m_deviceRHI = m_injectedDevice ? m_injectedDevice : s_staticDevice;
    if (!m_deviceRHI) return false;
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
    if (!s_lineShader || !s_lineShader->IsValid()) {
        s_lineShader = std::make_shared<Shader>();
        rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : (m_deviceRHI ? m_deviceRHI : s_staticDevice);
        if (!device) return;
        s_lineShader->Load(device, "passes/2d/primitive", GetVertexLayout(), rhi::PrimitiveTopology::TriangleList, rhi::PipelineLayoutKind::PushOnly,
                           std::optional{rhi::StandardAttachmentFormats::ForwardColorDepth()});
    }
    if (!s_lineShader->IsValid()) return;
    rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : (m_deviceRHI ? m_deviceRHI : s_staticDevice);
    if (!device) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd) return;
    EnsureLineBuffers(m_lineVerts.size() * sizeof(LineVert), m_lineIndices.size() * sizeof(unsigned int));
    if (!m_lineVbo || !m_lineEbo) return;
    m_lineVbo->Update(m_lineVerts.data(), 0, m_lineVerts.size() * sizeof(LineVert));
    m_lineEbo->Update(m_lineIndices.data(), 0, m_lineIndices.size() * sizeof(unsigned int));
    cmd->BindVertexBuffer(m_lineVbo, 0);
    cmd->BindIndexBuffer(m_lineEbo, rhi::IndexType::UInt32);
    cmd->SetBlend(true);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    s_lineShader->Bind(device);
    s_lineShader->SetMat4("u_View", m_view);
    s_lineShader->SetMat4("u_Projection", m_projection);
    cmd->DrawIndexed(static_cast<uint32_t>(m_lineIndices.size()), 0, 0);
    s_lineShader->Unbind();
    m_lineVerts.clear();
    m_lineIndices.clear();
}

void Primitives::FlushTriangles() {
    if (m_triVerts.empty()) return;
    if (!s_triShader || !s_triShader->IsValid()) {
        s_triShader = std::make_shared<Shader>();
        rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : (m_deviceRHI ? m_deviceRHI : s_staticDevice);
        if (!device) return;
        s_triShader->Load(device, "passes/2d/primitive", GetVertexLayout(), rhi::PrimitiveTopology::TriangleList, rhi::PipelineLayoutKind::PushOnly,
                          std::optional{rhi::StandardAttachmentFormats::ForwardColorDepth()});
    }
    if (!s_triShader->IsValid()) return;
    rhi::IDevice* device = m_injectedDevice ? m_injectedDevice : (m_deviceRHI ? m_deviceRHI : s_staticDevice);
    if (!device) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd) return;
    EnsureTriBuffers(m_triVerts.size() * sizeof(TriVert), m_triIndices.size() * sizeof(unsigned int));
    if (!m_triVbo || !m_triEbo) return;
    m_triVbo->Update(m_triVerts.data(), 0, m_triVerts.size() * sizeof(TriVert));
    m_triEbo->Update(m_triIndices.data(), 0, m_triIndices.size() * sizeof(unsigned int));
    cmd->BindVertexBuffer(m_triVbo, 0);
    cmd->BindIndexBuffer(m_triEbo, rhi::IndexType::UInt32);
    cmd->SetBlend(true);
    cmd->SetDepthTest(false);
    cmd->SetDepthWrite(false);
    cmd->SetCullFace(false);
    s_triShader->Bind(device);
    s_triShader->SetMat4("u_View", m_view);
    s_triShader->SetMat4("u_Projection", m_projection);
    cmd->DrawIndexed(static_cast<uint32_t>(m_triIndices.size()), 0, 0);
    s_triShader->Unbind();
    m_triVerts.clear();
    m_triIndices.clear();
}

rhi::VertexLayout Primitives::GetVertexLayout() const {
    rhi::VertexLayout layout;
    layout.strideInBytes = sizeof(LineVert);
    layout.attributes = {
        {0, rhi::VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(LineVert, pos))},
        {1, rhi::VertexAttributeType::Float4, static_cast<uint32_t>(offsetof(LineVert, color))},
    };
    return layout;
}

void Primitives::EnsureLineBuffers(size_t vertexBytes, size_t indexBytes) {
    if (!m_deviceRHI) return;
    if (!m_lineVbo || vertexBytes > m_lineVboCapacity) {
        if (m_lineVbo) m_deviceRHI->DestroyBuffer(m_lineVbo);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(vertexBytes, 1024);
        desc.usage = rhi::BufferUsage::Vertex;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_lineVbo = m_deviceRHI->CreateBuffer(desc);
        m_lineVboCapacity = desc.sizeInBytes;
    }
    if (!m_lineEbo || indexBytes > m_lineEboCapacity) {
        if (m_lineEbo) m_deviceRHI->DestroyBuffer(m_lineEbo);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(indexBytes, 1024);
        desc.usage = rhi::BufferUsage::Index;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_lineEbo = m_deviceRHI->CreateBuffer(desc);
        m_lineEboCapacity = desc.sizeInBytes;
    }
}

void Primitives::EnsureTriBuffers(size_t vertexBytes, size_t indexBytes) {
    if (!m_deviceRHI) return;
    if (!m_triVbo || vertexBytes > m_triVboCapacity) {
        if (m_triVbo) m_deviceRHI->DestroyBuffer(m_triVbo);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(vertexBytes, 1024);
        desc.usage = rhi::BufferUsage::Vertex;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_triVbo = m_deviceRHI->CreateBuffer(desc);
        m_triVboCapacity = desc.sizeInBytes;
    }
    if (!m_triEbo || indexBytes > m_triEboCapacity) {
        if (m_triEbo) m_deviceRHI->DestroyBuffer(m_triEbo);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(indexBytes, 1024);
        desc.usage = rhi::BufferUsage::Index;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_triEbo = m_deviceRHI->CreateBuffer(desc);
        m_triEboCapacity = desc.sizeInBytes;
    }
}

} // namespace render
} // namespace eyelib
