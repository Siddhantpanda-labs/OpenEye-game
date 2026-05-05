#pragma once

#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace eyelib {
namespace geometry {
class MeshTopology;

/**
 * @brief MeshBevel: Edge and vertex beveling operations.
 * 
 * Bevels selected edges or vertices by creating chamfered geometry.
 * This creates smoother, more rounded edges.
 */
class MeshBevel {
public:
    /**
     * @brief Bevel selected edges.
     * 
     * Creates chamfered edges by inserting new vertices and faces.
     * The bevel amount controls how far the new edge is offset.
     * 
     * @param mesh Mesh to bevel (modified in-place)
     * @param selection Region to bevel (must be edge selection)
     * @param amount Bevel amount (distance to offset new edge)
     * @param segments Number of segments in bevel (1 = chamfer, >1 = rounded)
     * @param mask Optional mask to control bevel amount per vertex
     */
    static void BevelEdges(geometry::EditableMesh& editable, const Selection& selection,
                          float amount, int segments = 1,
                          const Mask* mask = nullptr);
    
    /**
     * @brief Bevel selected vertices.
     * 
     * Creates chamfered vertices by inserting new geometry around them.
     * 
     * @param mesh Mesh to bevel
     * @param selection Region to bevel (must be vertex selection)
     * @param amount Bevel amount
     * @param segments Number of segments
     * @param mask Optional mask to control bevel amount
     */
    static void BevelVertices(geometry::EditableMesh& editable, const Selection& selection,
                             float amount, int segments = 1,
                             const Mask* mask = nullptr);

private:
    // Helper to bevel a single edge
    static void BevelEdge(geometry::EditableMesh& editable, uint32_t edgeIndex,
                         const std::unordered_set<uint32_t>& selectedEdges,
                         float amount, int segments,
                         const Mask* mask,
                         const MeshTopology& topology,
                         std::vector<render::Vertex>& newVertices,
                         std::vector<uint32_t>& newIndices,
                         std::unordered_map<uint32_t, uint32_t>& vertexMap);
    
    // Helper to compute edge direction and offset
    static math::Vec3 ComputeEdgeOffset(const geometry::EditableMesh& editable, uint32_t v0, uint32_t v1,
                                       const MeshTopology& topology, float amount);
};

} // namespace geometry
} // namespace eyelib
