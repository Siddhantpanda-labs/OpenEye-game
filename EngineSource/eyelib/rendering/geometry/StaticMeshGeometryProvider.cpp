#include "eyelib/rendering/geometry/StaticMeshGeometryProvider.h"

#include "eyelib/rendering/geometry/IMeshHandleResolver.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {

void StaticMeshGeometryProvider::Submit(rhi::ICommandList* cmd, const DrawList& drawList, uint32_t beginIndex,
                                        uint32_t count) const {
    if (!cmd || !m_resolver || count == 0)
        return;
    const size_t n = drawList.items.size();
    if (beginIndex >= n)
        return;
    const uint32_t end = beginIndex + count;
    if (end > n)
        return;

    for (uint32_t i = beginIndex; i < end; ++i) {
        const DrawItem& it = drawList.items[i];
        const MeshHandle h = m_resolver->TryResolveMesh(it.meshHandle);
        if (!h.IsValid())
            continue;
        cmd->BindVertexBuffer(h.vertexBuffer, 0, 0);
        cmd->BindIndexBuffer(h.indexBuffer, h.indexType, 0);
        if (!h.submeshes.empty()) {
            for (const Submesh& sm : h.submeshes)
                cmd->DrawIndexedSubmesh(sm.indexCount, sm.indexOffset, 0);
        } else {
            cmd->DrawIndexed(h.totalIndexCount, 0, 0);
        }
    }
}

}  // namespace rendering
}  // namespace eyelib
