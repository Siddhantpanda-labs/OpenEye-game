#include "eyelib/assets/EditableMesh.h"

namespace eyelib {
namespace assets {

void EditableMesh::AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex) {
    Submesh s;
    s.indexOffset = indexOffset;
    s.indexCount = indexCount;
    s.materialIndex = materialIndex;
    m_submeshes.push_back(s);
}

void EditableMesh::BuildRenderMesh(render::Mesh& out) {
    out.SetVertices(m_vertices);
    out.SetIndices(m_indices);
    out.ClearSubmeshes();
    for (const auto& s : m_submeshes)
        out.AddSubmesh(s.indexOffset, s.indexCount, s.materialIndex);

    if (!out.IsValid()) {
        out.UploadToGPU();
        ClearDirty();
        return;
    }
    if (IsGpuLayoutDirty()) {
        out.UploadToGPU();
        ClearDirty();
        return;
    }
    if (IsGeometryDirty() || IsSkinDirty())
        out.UploadVertexBufferOnly();
    if (IsTopologyDirty())
        out.UploadIndexBufferOnly();
    ClearDirty();
}

void EditableMesh::CopyToRenderMesh(render::Mesh& out) const {
    out.SetVertices(m_vertices);
    out.SetIndices(m_indices);
    out.ClearSubmeshes();
    for (const auto& s : m_submeshes)
        out.AddSubmesh(s.indexOffset, s.indexCount, s.materialIndex);
    out.UploadToGPU();
}

EditableMesh EditableMesh::FromRenderMesh(const render::Mesh& mesh, const assets::AssetId& skeletonId) {
    EditableMesh em;
    em.SetVertices(mesh.GetVertices());
    if (!mesh.GetIndices32().empty()) {
        em.SetIndices(mesh.GetIndices32());
    } else {
        std::vector<uint32_t> inds(mesh.GetIndices16().begin(), mesh.GetIndices16().end());
        em.SetIndices(std::move(inds));
    }
    em.GetSubmeshes().clear();
    for (size_t i = 0; i < mesh.GetSubmeshCount(); ++i) {
        const auto& s = mesh.GetSubmesh(i);
        em.AddSubmesh(s.indexOffset, s.indexCount, s.materialIndex);
    }
    em.SetSkeletonId(skeletonId);
    em.ClearDirty();
    return em;
}

} // namespace assets
} // namespace eyelib
