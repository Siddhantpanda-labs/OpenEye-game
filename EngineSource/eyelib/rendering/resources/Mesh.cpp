#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/geometry/MeshDescriptor.h"
#include "eyelib/rendering/geometry/MeshDescriptorUtils.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/core/Log.h"
#include <cstring>

namespace eyelib {
namespace rendering {

Mesh::Mesh() = default;

Mesh::~Mesh() { Destroy(); }

Mesh::Mesh(Mesh&& other) noexcept
    : m_vertexCount(other.m_vertexCount),
      m_indexCount(other.m_indexCount),
      m_indexType(other.m_indexType),
      m_submeshes(std::move(other.m_submeshes)),
      m_aabb(other.m_aabb),
      m_deviceRHI(other.m_deviceRHI),
      m_vertexBufferRHI(other.m_vertexBufferRHI),
      m_indexBufferRHI(other.m_indexBufferRHI),
      m_assetId(other.m_assetId) {
    other.m_deviceRHI = nullptr;
    other.m_vertexBufferRHI = nullptr;
    other.m_indexBufferRHI = nullptr;
    other.m_vertexCount = 0;
    other.m_indexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_vertexCount = other.m_vertexCount;
    m_indexCount = other.m_indexCount;
    m_indexType = other.m_indexType;
    m_submeshes = std::move(other.m_submeshes);
    m_aabb = other.m_aabb;
    m_deviceRHI = other.m_deviceRHI;
    m_vertexBufferRHI = other.m_vertexBufferRHI;
    m_indexBufferRHI = other.m_indexBufferRHI;
    m_assetId = other.m_assetId;
    other.m_deviceRHI = nullptr;
    other.m_vertexBufferRHI = nullptr;
    other.m_indexBufferRHI = nullptr;
    other.m_vertexCount = 0;
    other.m_indexCount = 0;
    return *this;
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

bool Mesh::CreateFromDescriptor(rhi::IDevice* device, const MeshDescriptor& desc) {
    if (!device || desc.vertices.empty()) return false;
    const bool use32 = !desc.indices32.empty();
    const bool use16 = !desc.indices16.empty();
    if (!use32 && !use16) return false;

    Destroy(device);

    rhi::BufferDesc vdesc;
    vdesc.sizeInBytes = desc.vertices.size() * sizeof(Vertex);
    vdesc.usage = rhi::BufferUsage::Vertex;
    vdesc.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    rhi::VertexLayout meshLayout = GetVertexLayoutRHI();
    vdesc.vertexLayout = &meshLayout;
    m_vertexBufferRHI = device->CreateBuffer(vdesc, desc.vertices.data());
    if (!m_vertexBufferRHI) return false;

    rhi::BufferDesc idesc;
    idesc.usage = rhi::BufferUsage::Index;
    idesc.memoryHint = rhi::BufferMemoryHint::DeviceOnly;
    if (use32) {
        idesc.sizeInBytes = desc.indices32.size() * sizeof(uint32_t);
        m_indexBufferRHI = device->CreateBuffer(idesc, desc.indices32.data());
        m_indexCount = desc.indices32.size();
        m_indexType = rhi::IndexType::UInt32;
    } else {
        idesc.sizeInBytes = desc.indices16.size() * sizeof(uint16_t);
        m_indexBufferRHI = device->CreateBuffer(idesc, desc.indices16.data());
        m_indexCount = desc.indices16.size();
        m_indexType = rhi::IndexType::UInt16;
    }
    if (!m_indexBufferRHI) {
        device->DestroyBuffer(m_vertexBufferRHI);
        m_vertexBufferRHI = nullptr;
        return false;
    }

    m_vertexCount = desc.vertices.size();
    m_submeshes = desc.submeshes;
    m_aabb = MeshDescriptorUtils::ComputeAABB(desc);
    m_deviceRHI = device;
    return true;
}

std::shared_ptr<Mesh> Mesh::Create(rhi::IDevice* device, const MeshDescriptor& desc) {
    auto m = std::make_shared<Mesh>();
    if (!m->CreateFromDescriptor(device, desc)) return nullptr;
    return m;
}

void Mesh::Bind(rhi::IDevice* device) const {
    if (!device || !m_vertexBufferRHI || !m_indexBufferRHI) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (!cmd) return;
    cmd->BindVertexBuffer(m_vertexBufferRHI, 0);
    cmd->BindIndexBuffer(m_indexBufferRHI, m_indexType);
}

void Mesh::Unbind() const {
    (void)this;
}

void Mesh::Draw(rhi::IDevice* device, int submeshIndex) const {
    Bind(device);
    if (!m_vertexBufferRHI || !m_indexBufferRHI) return;
    GetHandle().Draw(device, submeshIndex);
}

MeshHandle Mesh::GetHandle() const {
    MeshHandle h;
    if (!m_vertexBufferRHI || !m_indexBufferRHI) return h;
    h.vertexBuffer = m_vertexBufferRHI;
    h.indexBuffer = m_indexBufferRHI;
    h.indexType = m_indexType;
    h.totalIndexCount = static_cast<uint32_t>(m_indexCount);
    h.submeshes = m_submeshes;
    return h;
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
    // Must match DeviceGL fullscreen quad VBO: 4 floats per vertex (pos.xy, uv.xy), stride 16.
    constexpr uint32_t stride = 4u * sizeof(float);
    VertexLayout layout;
    layout.strideInBytes = stride;
    layout.attributes = {
        {0, VertexAttributeType::Float2, 0u},
        {1, VertexAttributeType::Float2, 2u * sizeof(float)},
    };
    return layout;
}

void MeshHandle::Bind(rhi::IDevice* device) const {
    if (!device || !vertexBuffer || !indexBuffer) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (!cmd) return;
    cmd->BindVertexBuffer(vertexBuffer, 0);
    cmd->BindIndexBuffer(indexBuffer, indexType);
}

void MeshHandle::Draw(rhi::IDevice* device, int submeshIndex) const {
    if (!device || !vertexBuffer || !indexBuffer) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (!cmd) return;
    Bind(device);
    if (submeshIndex >= 0 && static_cast<size_t>(submeshIndex) < submeshes.size()) {
        const Submesh& s = submeshes[static_cast<size_t>(submeshIndex)];
        cmd->DrawIndexedSubmesh(s.indexCount, s.indexOffset, 0);
    } else {
        cmd->DrawIndexed(totalIndexCount, 0, 0);
    }
}

void MeshHandle::DrawInstanced(rhi::IDevice* device, int submeshIndex, uint32_t instanceCount) const {
    if (!device || !vertexBuffer || !indexBuffer || instanceCount == 0) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (!cmd) return;
    Bind(device);
    if (submeshIndex >= 0 && static_cast<size_t>(submeshIndex) < submeshes.size()) {
        const Submesh& s = submeshes[static_cast<size_t>(submeshIndex)];
        cmd->DrawIndexedInstanced(s.indexCount, s.indexOffset, 0, instanceCount);
    } else {
        cmd->DrawIndexedInstanced(totalIndexCount, 0, 0, instanceCount);
    }
}

} // namespace rendering
} // namespace eyelib
