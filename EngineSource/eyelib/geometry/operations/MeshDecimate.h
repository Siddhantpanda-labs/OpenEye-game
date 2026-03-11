#pragma once

#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include <memory>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshDecimate: Mesh simplification operations.
 * 
 * Reduces mesh complexity by collapsing edges while preserving shape.
 * Uses quadric error metrics for optimal edge collapse selection.
 */
class MeshDecimate {
public:
    /**
     * @brief Decimate mesh by reducing triangle count.
     * 
     * Simplifies mesh by collapsing edges, reducing vertex/triangle count
     * while attempting to preserve shape and important features.
     * 
     * @param mesh Mesh to decimate (modified in-place)
     * @param targetTriangleCount Target number of triangles (must be less than current)
     * @param selection Optional region to decimate (if empty, decimates entire mesh)
     * @param mask Optional mask to protect certain regions from decimation
     */
    static void Decimate(geometry::EditableMesh& editable, size_t targetTriangleCount,
                        const Selection& selection = Selection::Empty(Selection::Type::Faces),
                        const Mask* mask = nullptr);
    
    /**
     * @brief Decimate mesh by percentage.
     * 
     * Reduces mesh complexity by a percentage (0.0 = no reduction, 1.0 = maximum reduction).
     * 
     * @param mesh Mesh to decimate
     * @param reductionFactor Reduction factor (0.0 to 1.0)
     * @param selection Optional region to decimate
     * @param mask Optional mask to protect regions
     */
    static void DecimateByPercentage(geometry::EditableMesh& editable, float reductionFactor,
                                    const Selection& selection = Selection::Empty(Selection::Type::Faces),
                                    const Mask* mask = nullptr);

private:
    // Edge collapse data structure
    struct EdgeCollapse {
        uint32_t v0 = 0;
        uint32_t v1 = 0;
        float error = 0.0f;  // Quadric error metric
        math::Vec3 newPosition;  // Optimal collapse position
    };
    
    // Compute quadric error for an edge collapse
    static float ComputeEdgeError(const geometry::EditableMesh& editable, uint32_t v0, uint32_t v1,
                                  const MeshTopology& topology, math::Vec3& optimalPos);
    
    // Collapse an edge (merge v1 into v0)
    static void CollapseEdge(const geometry::EditableMesh& editable, uint32_t v0, uint32_t v1,
                            const MeshTopology& topology,
                            std::vector<render::Vertex>& newVertices,
                            std::vector<uint32_t>& newIndices,
                            std::unordered_map<uint32_t, uint32_t>& vertexMap);
    
    // Check if edge can be collapsed (topology constraints)
    static bool CanCollapseEdge(uint32_t v0, uint32_t v1, const MeshTopology& topology);
};

} // namespace geometry
} // namespace eyelib
