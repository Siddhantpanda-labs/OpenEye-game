#include "api/render/Mesh.h"
#include "eyelib/render/resources/Mesh.h"

namespace eyelib {
namespace api {

class Mesh::Impl {
public:
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
    m_impl->mesh->SetVertices(v);
}

std::vector<Mesh::Vertex> Mesh::GetVertices() const {
    const auto& internalVerts = m_impl->mesh->GetVertices();
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
    auto& internalVerts = m_impl->mesh->GetVertices();
    if (startIndex + vertices.size() > internalVerts.size()) {
        // Extend if needed
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
    // Mark as dirty so GPU update is needed
    m_impl->mesh->InvalidateAABB();
    // Re-upload to GPU (full upload for now, can be optimized later with glBufferSubData)
    m_impl->mesh->UploadToGPU();
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    m_impl->mesh->SetIndices(indices);
}

void Mesh::SetIndices(const std::vector<uint16_t>& indices) {
    m_impl->mesh->SetIndices(indices);
}

void Mesh::AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex) {
    m_impl->mesh->AddSubmesh(indexOffset, indexCount, materialIndex);
}

void Mesh::SetSubmesh(size_t index, uint32_t indexOffset, uint32_t indexCount) {
    m_impl->mesh->SetSubmesh(index, indexOffset, indexCount);
}

void Mesh::ClearSubmeshes() {
    m_impl->mesh->ClearSubmeshes();
}

void Mesh::UploadToGPU() {
    m_impl->mesh->UploadToGPU();
}

size_t Mesh::GetVertexCount() const {
    return m_impl->mesh->GetVertexCount();
}

size_t Mesh::GetIndexCount() const {
    return m_impl->mesh->GetIndexCount();
}

math::AABB Mesh::GetAABB() const {
    return m_impl->mesh->GetAABB();
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
    m_impl->mesh->GenerateNormals();
}

void Mesh::GenerateTangents() {
    m_impl->mesh->GenerateTangents();
}

void Mesh::Optimize() {
    m_impl->mesh->Optimize();
}

bool Mesh::IsValid() const {
    return m_impl->mesh->IsValid();
}

std::shared_ptr<Mesh> Mesh::WrapInternal(const std::shared_ptr<::eyelib::render::Mesh>& mesh) {
    auto apiMesh = std::make_shared<Mesh>();
    if (mesh) apiMesh->m_impl->mesh = mesh;
    return apiMesh;
}

std::shared_ptr<::eyelib::render::Mesh> Mesh::GetInternal() const {
    return m_impl->mesh;
}

} // namespace api
} // namespace eyelib
