#include "api/rendering/Mesh.h"

#include "api/rendering/internal/ResourceAccess.h"
#include "eyelib/rendering/resources/Mesh.h"

#include <utility>

namespace eyelib {
namespace api {
namespace rendering {

class Mesh::Impl {
public:
    std::shared_ptr<eyelib::rendering::Mesh> mesh;
    uint32_t handleToken = 0u;  // assigned by the M3 factory; 0 = invalid.
};

Mesh::Mesh() noexcept : m_impl(std::make_unique<Impl>()) {}

Mesh::~Mesh() = default;

Mesh::Mesh(const Mesh& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}

Mesh& Mesh::operator=(const Mesh& other) {
    if (this != &other) {
        *m_impl = *other.m_impl;
    }
    return *this;
}

Mesh::Mesh(Mesh&& other) noexcept = default;
Mesh& Mesh::operator=(Mesh&& other) noexcept = default;

bool Mesh::IsValid() const noexcept {
    return m_impl && m_impl->mesh && m_impl->mesh->IsValid();
}

uint32_t Mesh::GetMeshHandle() const noexcept {
    return (m_impl && m_impl->mesh) ? m_impl->handleToken : 0u;
}

size_t Mesh::GetSubmeshCount() const noexcept {
    return (m_impl && m_impl->mesh) ? m_impl->mesh->GetSubmeshCount() : 0u;
}

size_t Mesh::GetVertexCount() const noexcept {
    return (m_impl && m_impl->mesh) ? m_impl->mesh->GetVertexCount() : 0u;
}

size_t Mesh::GetIndexCount() const noexcept {
    return (m_impl && m_impl->mesh) ? m_impl->mesh->GetIndexCount() : 0u;
}

math::AABB Mesh::GetAABB() const noexcept {
    return (m_impl && m_impl->mesh) ? m_impl->mesh->GetAABB() : math::AABB{};
}

// --- internal access seam (defined here so it can touch Mesh::Impl) -------------------

Mesh MeshAccess::Wrap(std::shared_ptr<eyelib::rendering::Mesh> internalMesh, uint32_t meshHandleToken) {
    Mesh handle{};
    handle.m_impl->mesh = std::move(internalMesh);
    handle.m_impl->handleToken = meshHandleToken;
    return handle;
}

} // namespace rendering
} // namespace api
} // namespace eyelib
