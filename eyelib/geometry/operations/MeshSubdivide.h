#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include <memory>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshSubdivide: Subdivision surface operations.
 * 
 * Subdivides mesh faces to increase detail. Currently implements simple
 * midpoint subdivision (each triangle becomes 4 triangles).
 */
class MeshSubdivide {
public:
    /**
     * @brief Subdivide mesh using simple midpoint subdivision.
     * 
     * Each triangle is split into 4 triangles by adding vertices at edge midpoints.
     * 
     * @param mesh Mesh to subdivide (modified in-place)
     * @param selection Region to subdivide (if empty, subdivides all faces)
     * @param levels Number of subdivision levels (1 = one subdivision pass)
     */
    static void Subdivide(render::Mesh& mesh, const Selection& selection, int levels = 1);
    
    /**
     * @brief Subdivide using Loop subdivision (smoother result).
     * 
     * Uses Loop subdivision rules for better quality.
     * 
     * @param mesh Mesh to subdivide
     * @param selection Region to subdivide
     * @param levels Number of subdivision levels
     */
    static void SubdivideLoop(render::Mesh& mesh, const Selection& selection, int levels = 1);

private:
    // Helper to subdivide a single face
    static void SubdivideFace(const render::Mesh& inputMesh, size_t faceIndex,
                             std::vector<render::Vertex>& newVertices,
                             std::vector<uint32_t>& newIndices,
                             std::unordered_map<uint64_t, uint32_t>& edgeMidpoints);
    
    // Helper to get edge midpoint key
    static uint64_t GetEdgeKey(uint32_t v0, uint32_t v1);
};

} // namespace geometry
} // namespace eyelib
