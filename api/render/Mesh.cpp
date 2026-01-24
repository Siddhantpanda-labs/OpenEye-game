#include "api/render/Mesh.h"
#include "eyelib/render/resources/Mesh.h"

namespace eyelib {
namespace api {

class Mesh::Impl {
public:
    std::shared_ptr<render::Mesh> mesh = std::make_shared<render::Mesh>();
};

Mesh::Mesh() : m_impl(std::make_unique<Impl>()) {}

Mesh::~Mesh() = default;

Mesh::Mesh(Mesh&& other) noexcept = default;

Mesh& Mesh::operator=(Mesh&& other) noexcept = default;

void Mesh::SetVertices(const std::vector<Vertex>& vertices) {
    std::vector<render::Vertex> v;
    v.reserve(vertices.size());
    for (const auto& vert : vertices) {
        render::Vertex rv;
        rv.position = vert.position;
        rv.normal = vert.normal;
        rv.uv = vert.uv;
        rv.tangent = vert.tangent;
        rv.color = vert.color;
        v.push_back(rv);
    }
    m_impl->mesh->SetVertices(v);
}

void Mesh::SetIndices(const std::vector<uint32_t>& indices) {
    m_impl->mesh->SetIndices(indices);
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

bool Mesh::IsValid() const {
    return m_impl->mesh->IsValid();
}

std::shared_ptr<Mesh> Mesh::WrapInternal(const std::shared_ptr<render::Mesh>& mesh) {
    auto apiMesh = std::make_shared<Mesh>();
    if (mesh) apiMesh->m_impl->mesh = mesh;
    return apiMesh;
}

std::shared_ptr<render::Mesh> Mesh::GetInternal() const {
    return m_impl->mesh;
}

} // namespace api
} // namespace eyelib
