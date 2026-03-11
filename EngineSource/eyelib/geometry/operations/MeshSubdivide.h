#pragma once

#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshSubdivide: Subdivision surface operations.
 * 
 * Subdivides mesh faces to increase detail. Currently implements simple
 * midpoint subdivision (each triangle becomes 4 triangles).
 * Operates on EditableMesh (CPU); caller syncs to GPU via UploadToGPU(device).
 */
class MeshSubdivide {
public:
    /** @param editable Mesh to subdivide (modified in-place) */
    static void Subdivide(geometry::EditableMesh& editable, const Selection& selection, int levels = 1);
    
    /** Subdivide using Loop subdivision (smoother result). */
    static void SubdivideLoop(geometry::EditableMesh& editable, const Selection& selection, int levels = 1);

private:
    static void SubdivideFace(const geometry::EditableMesh& inputMesh, size_t faceIndex,
                             std::vector<geometry::EditableMesh::Vertex>& newVertices,
                             std::vector<uint32_t>& newIndices,
                             std::unordered_map<uint64_t, uint32_t>& edgeMidpoints);
    static uint64_t GetEdgeKey(uint32_t v0, uint32_t v1);
};

} // namespace geometry
} // namespace eyelib
