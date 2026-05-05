#include "api/render/Mesh.h"
#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/MeshDescriptorUtils.h"
#include "eyelib/render/resources/Mesh.h"

namespace eyelib {
namespace api {

class Mesh::Impl {
public:
    ::eyelib::geometry::EditableMesh editable;
    std::shared_ptr<::eyelib::render::Mesh> mesh = std::make_shared<::eyelib::render::Mesh>();
};

Mesh::Mesh() : m_impl(std::make_unique<Impl>()) {}

Mesh::~Mesh() = default;

Mesh::Mesh(Mesh&& other) noexcept = default;

Mesh& Mesh::operator=(Mesh&& other) noexcept = default;

void Mesh::SetVertices(const std::vector<Vertex>& vertices) {
    std::vector<::eyelib::render::Vertex> v;
    v.reserve(vertices.size());
    for (const auto& vert : vertices) {
        ::eyelib::render::Vertex rv;
        rv.position = vert.position;
        rv.normal = vert.normal;
        rv.uv = vert.uv;
        rv.uv1 = vert.uv1;
        rv.tangent = vert.tangent;
        rv.color = vert.color;
        rv.boneIndices = vert.boneIndices;
        rv.boneWeights = vert.boneWeights;
        v.push_back(rv);
    }
    m_impl->editable.SetVertices(std::move(v));
}

std::vector<Mesh::Vertex> Mesh::GetVertices() const {
    const auto& internalVerts = m_impl->editable.GetVertices();
    std::vector<Vertex> vertices;
    vertices.reserve(internalVerts.size());
    for (const auto& iv : internalVerts) {
        Vertex v;
        v.position = iv.position;
        v.normal = iv.normal;
        v.uv = iv.uv;
        v.uv1 = iv.uv1;
        v.tangent = iv.tangent;
        v.color = iv.color;
        v.boneIndices = iv.boneIndices;
        v.boneWeights = iv.boneWeights;
        vertices.push_back(v);
    }
    return vertices;
}

void Mesh::UpdateVertices(const std::vector<Vertex>& vertices, size_t startIndex) {
    auto& internalVerts = m_impl->editable.GetVertices();
    if (startIndex + vertices.size() > internalVerts.size()) {
        internalVerts.resize(startIndex + vertices.size());
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& vert = vertices[i];
        auto& rv = internalVerts[startIndex + i];
        rv.position = vert.position;
        rv.normal = vert.normal;
        rv.uv = vert.uv;
        rv.uv1 = vert.uv1;
        rv.tangent = vert.tangent;
        rv.color = vert.color;
        rv.boneIndices = vert.boneIndices;
        rv.boneWeights = vert.boneWeights;
    }
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    m_impl->editable.SetIndices(indices);
}

void Mesh::SetIndices(const std::vector<uint16_t>& indices) {
    std::vector<uint32_t> inds(indices.begin(), indices.end());
    m_impl->editable.SetIndices(std::move(inds));
}

void Mesh::AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex) {
    m_impl->editable.AddSubmesh(indexOffset, indexCount, materialIndex);
}

void Mesh::SetSubmesh(size_t index, uint32_t indexOffset, uint32_t indexCount) {
    auto& submeshes = m_impl->editable.GetSubmeshes();
    if (index < submeshes.size()) {
        submeshes[index].indexOffset = indexOffset;
        submeshes[index].indexCount = indexCount;
    }
}

void Mesh::ClearSubmeshes() {
    m_impl->editable.GetSubmeshes().clear();
}

void Mesh::UploadToGPU(RenderDevice device) {
    if (!device) return;
    m_impl->editable.CopyToRenderMesh(*m_impl->mesh, static_cast<::eyelib::render::rhi::IDevice*>(device.Get()));
}

size_t Mesh::GetVertexCount() const {
    size_t n = m_impl->editable.GetVertices().size();
    if (n > 0) return n;
    return m_impl->mesh->GetVertexCount();
}

size_t Mesh::GetIndexCount() const {
    size_t n = m_impl->editable.GetIndices().size();
    if (n > 0) return n;
    return m_impl->mesh->GetIndexCount();
}

math::AABB Mesh::GetAABB() const {
    if (m_impl->mesh->IsValid()) return m_impl->mesh->GetAABB();
    return ::eyelib::render::MeshDescriptorUtils::ComputeAABB(m_impl->editable.GetDescriptor());
}

size_t Mesh::GetSubmeshCount() const {
    return m_impl->mesh->GetSubmeshCount();
}

Mesh::Submesh Mesh::GetSubmesh(size_t index) const {
    const auto& s = m_impl->mesh->GetSubmesh(index);
    Submesh out;
    out.indexOffset = s.indexOffset;
    out.indexCount = s.indexCount;
    out.materialIndex = s.materialIndex;
    return out;
}

Mesh::IndexFormat Mesh::GetIndexFormat() const {
    return (m_impl->mesh->GetIndexFormat() == ::eyelib::render::IndexFormat::UInt16)
        ? IndexFormat::UInt16
        : IndexFormat::UInt32;
}

void Mesh::GenerateNormals() {
    auto desc = m_impl->editable.GetDescriptor();
    ::eyelib::render::MeshDescriptorUtils::GenerateNormals(desc);
    m_impl->editable.SetVertices(std::move(desc.vertices));
}

void Mesh::GenerateTangents() {
    auto desc = m_impl->editable.GetDescriptor();
    ::eyelib::render::MeshDescriptorUtils::GenerateTangents(desc);
    m_impl->editable.SetVertices(std::move(desc.vertices));
}

void Mesh::Optimize() {
    auto desc = m_impl->editable.GetDescriptor();
    ::eyelib::render::MeshDescriptorUtils::Optimize(desc);
    m_impl->editable.SetIndices(std::move(desc.indices32));
}

bool Mesh::IsValid() const {
    return m_impl->mesh->IsValid();
}

std::shared_ptr<Mesh> Mesh::WrapInternal(const std::shared_ptr<::eyelib::render::Mesh>& mesh) {
    auto apiMesh = std::make_shared<Mesh>();
    if (mesh) {
        apiMesh->m_impl->mesh = mesh;
        /* CPU editable is empty; GetVertices() will be empty, counts/AABB from GPU mesh */
    }
    return apiMesh;
}

std::shared_ptr<::eyelib::render::Mesh> Mesh::GetInternal() const {
    return m_impl->mesh;
}

::eyelib::geometry::EditableMesh* Mesh::GetEditableMesh() {
    return &m_impl->editable;
}

const ::eyelib::geometry::EditableMesh* Mesh::GetEditableMesh() const {
    return &m_impl->editable;
}

} // namespace api
} // namespace eyelib
