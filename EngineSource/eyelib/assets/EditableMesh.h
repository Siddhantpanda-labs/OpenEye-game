#pragma once

#include "eyelib/assets/AssetId.h"
#include "eyelib/render/resources/Mesh.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace render { struct MeshDescriptor; }
namespace assets {

/**
 * CPU-authoritative mesh data (Mesh Pipeline: source of truth).
 * GPU buffers are derived from this; never the reverse.
 * Geometry definition only: no world matrix, pose, or physics.
 */
class EditableMesh {
public:
    using Vertex = render::Vertex;
    using Submesh = render::Submesh;

    EditableMesh() = default;

    bool IsEditable() const { return m_editable; }
    void SetEditable(bool editable) { m_editable = editable; }

    bool IsRuntimeOnly() const { return !m_editable; }

    const assets::AssetId& GetSkeletonId() const { return m_skeletonId; }
    void SetSkeletonId(const assets::AssetId& id) { m_skeletonId = id; }
    bool HasSkeleton() const { return m_skeletonId.IsValid(); }

    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    std::vector<Vertex>& GetVertices() { return m_vertices; }
    void SetVertices(std::vector<Vertex> v) { m_vertices = std::move(v); MarkGeometryDirty(); }

    const std::vector<uint32_t>& GetIndices() const { return m_indices; }
    std::vector<uint32_t>& GetIndices() { return m_indices; }
    void SetIndices(std::vector<uint32_t> i) { m_indices = std::move(i); MarkTopologyDirty(); }

    const std::vector<Submesh>& GetSubmeshes() const { return m_submeshes; }
    std::vector<Submesh>& GetSubmeshes() { return m_submeshes; }
    void SetSubmeshes(std::vector<Submesh> s) { m_submeshes = std::move(s); }
    void AddSubmesh(uint32_t indexOffset, uint32_t indexCount, int materialIndex = -1);

    bool IsGeometryDirty() const { return m_geometryDirty; }
    bool IsTopologyDirty() const { return m_topologyDirty; }
    bool IsSkinDirty() const { return m_skinDirty; }
    bool IsGpuLayoutDirty() const { return m_gpuLayoutDirty; }
    bool IsAnyDirty() const {
        return m_geometryDirty || m_topologyDirty || m_skinDirty || m_gpuLayoutDirty;
    }

    void MarkGeometryDirty() { if (m_editable) m_geometryDirty = true; }
    void MarkTopologyDirty() { if (m_editable) m_topologyDirty = true; }
    void MarkSkinDirty() { if (m_editable) m_skinDirty = true; }
    void MarkGpuLayoutDirty() { if (m_editable) m_gpuLayoutDirty = true; }
    void ClearDirty() {
        m_geometryDirty = false;
        m_topologyDirty = false;
        m_skinDirty = false;
        m_gpuLayoutDirty = false;
    }

    /** CPU descriptor for CreateFromDescriptor. */
    render::MeshDescriptor GetDescriptor() const;

    /** Build or update a render::Mesh from this EditableMesh. Device required for GPU upload. */
    void BuildRenderMesh(render::Mesh& out, render::rhi::IDevice* device);

    /** Copy CPU data into a render::Mesh and upload to GPU (full upload). */
    void CopyToRenderMesh(render::Mesh& out, render::rhi::IDevice* device) const;

    /** Build EditableMesh from a CPU descriptor (e.g. after load). */
    static EditableMesh FromDescriptor(const render::MeshDescriptor& desc, const assets::AssetId& skeletonId = assets::AssetId::Invalid());

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Submesh> m_submeshes;
    assets::AssetId m_skeletonId = assets::AssetId::Invalid();
    bool m_editable = true;

    mutable bool m_geometryDirty = false;
    mutable bool m_topologyDirty = false;
    mutable bool m_skinDirty = false;
    mutable bool m_gpuLayoutDirty = false;
};

} // namespace assets
} // namespace eyelib
