#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/render/MeshDescriptor.h"

namespace eyelib {
namespace geometry {

void EditableMesh::AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex) {
    Submesh s;
    s.indexOffset = indexOffset;
    s.indexCount = indexCount;
    s.materialIndex = materialIndex;
    m_submeshes.push_back(s);
}

render::MeshDescriptor EditableMesh::GetDescriptor() const {
    render::MeshDescriptor desc;
    desc.vertices = m_vertices;
    desc.indices32 = m_indices;
    desc.indices16.clear();
    desc.indexFormat = render::IndexFormat::UInt32;
    desc.submeshes = m_submeshes;
    return desc;
}

void EditableMesh::BuildRenderMesh(render::Mesh& out, render::rhi::IDevice* device) {
    if (!device) return;
    render::MeshDescriptor desc = GetDescriptor();
    if (desc.vertices.empty() || desc.indices32.empty()) return;
    out.CreateFromDescriptor(device, desc);
    ClearDirty();
}

void EditableMesh::CopyToRenderMesh(render::Mesh& out, render::rhi::IDevice* device) const {
    if (!device) return;
    out.CreateFromDescriptor(device, GetDescriptor());
}

EditableMesh EditableMesh::FromDescriptor(const render::MeshDescriptor& desc, const asset::Guid& skeletonId) {
    EditableMesh em;
    em.SetVertices(desc.vertices);
    if (!desc.indices32.empty()) {
        em.SetIndices(desc.indices32);
    } else {
        std::vector<uint32_t> inds(desc.indices16.begin(), desc.indices16.end());
        em.SetIndices(std::move(inds));
    }
    em.GetSubmeshes() = desc.submeshes;
    em.SetSkeletonId(skeletonId);
    em.ClearDirty();
    return em;
}

} // namespace geometry
} // namespace eyelib
