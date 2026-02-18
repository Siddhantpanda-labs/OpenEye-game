#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstddef>

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
      m_injectedDevice(other.m_injectedDevice),
      m_deviceRHI(other.m_deviceRHI),
      m_vertexBufferRHI(other.m_vertexBufferRHI),
      m_indexBufferRHI(other.m_indexBufferRHI) {
    other.m_injectedDevice = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_vertexBufferRHI = nullptr;
    other.m_indexBufferRHI = nullptr;
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
    m_injectedDevice = other.m_injectedDevice;
    m_deviceRHI = other.m_deviceRHI;
    m_vertexBufferRHI = other.m_vertexBufferRHI;
    m_indexBufferRHI = other.m_indexBufferRHI;
    other.m_injectedDevice = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_vertexBufferRHI = nullptr;
    other.m_indexBufferRHI = nullptr;
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

void Mesh::Invalidate() { }

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

rhi::IDevice* Mesh::GetDevice() const {
    return m_injectedDevice ? m_injectedDevice : (m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice());
}

void Mesh::UploadToGPU() {
    rhi::IDevice* device = GetDevice();
    if (!device) return;
    UploadToGPU(device);
}

void Mesh::UploadToGPU(rhi::IDevice* device) {
    if (!device || m_vertices.empty()) return;
    if (m_indexFormat == IndexFormat::UInt32 && m_indices32.empty()) return;
    if (m_indexFormat == IndexFormat::UInt16 && m_indices16.empty()) return;
    if (m_deviceRHI) Destroy(m_deviceRHI);

    rhi::BufferDesc vdesc;
    vdesc.sizeInBytes = m_vertices.size() * sizeof(Vertex);
    vdesc.usage = rhi::BufferUsage::Vertex;
    vdesc.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    rhi::VertexLayout meshLayout = GetVertexLayoutRHI();
    vdesc.vertexLayout = &meshLayout;
    m_vertexBufferRHI = device->CreateBuffer(vdesc, m_vertices.data());
    if (!m_vertexBufferRHI) return;

    rhi::BufferDesc idesc;
    idesc.usage = rhi::BufferUsage::Index;
    idesc.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    if (m_indexFormat == IndexFormat::UInt32) {
        idesc.sizeInBytes = m_indices32.size() * sizeof(uint32_t);
        m_indexBufferRHI = device->CreateBuffer(idesc, m_indices32.data());
    } else {
        idesc.sizeInBytes = m_indices16.size() * sizeof(uint16_t);
        m_indexBufferRHI = device->CreateBuffer(idesc, m_indices16.data());
    }
    if (!m_indexBufferRHI) {
        device->DestroyBuffer(m_vertexBufferRHI);
        m_vertexBufferRHI = nullptr;
        return;
    }
    m_deviceRHI = device;
    if (m_aabbDirty) ComputeAABB();
}

void Mesh::Destroy() {
    if (m_deviceRHI) Destroy(m_deviceRHI);
}

void Mesh::Destroy(rhi::IDevice* device) {
    if (!device || device != m_deviceRHI) return;
    if (m_vertexBufferRHI) { device->DestroyBuffer(m_vertexBufferRHI); m_vertexBufferRHI = nullptr; }
    if (m_indexBufferRHI) { device->DestroyBuffer(m_indexBufferRHI); m_indexBufferRHI = nullptr; }
    m_deviceRHI = nullptr;
}

rhi::IndexType Mesh::GetRHIIndexType() const {
    return m_indexFormat == IndexFormat::UInt16 ? rhi::IndexType::UInt16 : rhi::IndexType::UInt32;
}

rhi::VertexLayout Mesh::GetVertexLayoutRHI() {
    using namespace rhi;
    VertexLayout layout;
    layout.strideInBytes = sizeof(Vertex);
    layout.attributes = {
        {0, VertexAttributeType::Float3, static_cast<uint32_t>(offsetof(Vertex, position))},
        {1, VertexAttributeType::Float3, static_cast<uint32_t>(offsetof(Vertex, normal))},
        {2, VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, uv))},
        {7, VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, uv1))},
        {3, VertexAttributeType::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent))},
        {4, VertexAttributeType::Float4, static_cast<uint32_t>(offsetof(Vertex, color))},
        {5, VertexAttributeType::Int4,   static_cast<uint32_t>(offsetof(Vertex, boneIndices))},
        {6, VertexAttributeType::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights))},
    };
    return layout;
}

rhi::VertexLayout Mesh::GetFullscreenQuadVertexLayoutRHI() {
    using namespace rhi;
    VertexLayout layout;
    // FullscreenQuad uses Mesh with Vertex; we use position.xy (vec2) and uv (vec2).
    layout.strideInBytes = static_cast<uint32_t>(sizeof(Vertex));
    layout.attributes = {
        {0, VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, position))},
        {1, VertexAttributeType::Float2, static_cast<uint32_t>(offsetof(Vertex, uv))},
    };
    return layout;
}

void Mesh::Bind() const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_vertexBufferRHI || !m_indexBufferRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd) return;
    cmd->BindVertexBuffer(m_vertexBufferRHI, 0);
    cmd->BindIndexBuffer(m_indexBufferRHI, GetRHIIndexType());
}

void Mesh::Unbind() const {
    (void)this;
}

void Mesh::Draw(int submeshIndex) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_vertexBufferRHI || !m_indexBufferRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd) return;
    Bind();
    if (submeshIndex >= 0 && static_cast<size_t>(submeshIndex) < m_submeshes.size()) {
        const Submesh& s = m_submeshes[static_cast<size_t>(submeshIndex)];
        cmd->DrawIndexedSubmesh(s.indexCount, s.indexOffset, 0);
    } else {
        uint32_t cnt = static_cast<uint32_t>(m_indexFormat == IndexFormat::UInt32 ? m_indices32.size() : m_indices16.size());
        cmd->DrawIndexed(cnt, 0, 0);
    }
}

} // namespace render
} // namespace eyelib
