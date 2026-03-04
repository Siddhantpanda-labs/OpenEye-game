#pragma once

#include "eyelib/assets/EditableMesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include <memory>

namespace eyelib {
namespace geometry {
class MeshTopology;

/**
 * @brief MeshDeform: Advanced deformation operations.
 * 
 * Provides specialized deformation operations for sculpting and modeling:
 * - Inflate/Deflate: Push vertices along normals
 * - Relax: Gentle smoothing
 * - Lattice: Deform using control points
 */
class MeshDeform {
public:
    /**
     * @brief Inflate: Push vertices outward along their normals.
     * 
     * Expands the mesh by moving vertices along their normal vectors.
     * 
     * @param mesh Mesh to inflate (modified in-place)
     * @param selection Region to inflate (if empty, inflates all vertices)
     * @param amount Inflate amount (positive = outward)
     * @param mask Optional mask to control inflation strength per vertex
     */
    static void Inflate(assets::EditableMesh& editable, const Selection& selection,
                       float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Deflate: Push vertices inward along their normals.
     * 
     * Contracts the mesh by moving vertices along their normal vectors.
     * 
     * @param mesh Mesh to deflate (modified in-place)
     * @param selection Region to deflate (if empty, deflates all vertices)
     * @param amount Deflate amount (positive = inward)
     * @param mask Optional mask to control deflation strength per vertex
     */
    static void Deflate(assets::EditableMesh& editable, const Selection& selection,
                       float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Relax: Gentle smoothing operation.
     * 
     * Applies very gentle smoothing to relax mesh geometry without
     * losing detail. Similar to smooth but with lower strength.
     * 
     * @param mesh Mesh to relax (modified in-place)
     * @param selection Region to relax (if empty, relaxes all vertices)
     * @param strength Relaxation strength (0.0 to 1.0)
     * @param iterations Number of relaxation iterations
     * @param mask Optional mask to control relaxation strength
     */
    static void Relax(assets::EditableMesh& editable, const Selection& selection,
                     float strength = 0.1f, int iterations = 1,
                     const Mask* mask = nullptr);
    
    /**
     * @brief Lattice: Deform mesh using a control lattice.
     * 
     * Deforms mesh vertices based on a 3D lattice of control points.
     * This allows for complex, localized deformations.
     * 
     * @param mesh Mesh to deform (modified in-place)
     * @param selection Region to deform (if empty, deforms all vertices)
     * @param latticeOrigin Origin of the lattice in mesh space
     * @param latticeSize Size of the lattice (width, height, depth)
     * @param latticeResolution Resolution of the lattice (x, y, z divisions)
     * @param controlPoints Control point offsets (flattened 3D array: [x][y][z])
     * @param mask Optional mask to control deformation strength
     */
    static void Lattice(assets::EditableMesh& editable, const Selection& selection,
                       const math::Vec3& latticeOrigin, const math::Vec3& latticeSize,
                       const math::IVec3& latticeResolution,
                       const std::vector<math::Vec3>& controlPoints,
                       const Mask* mask = nullptr);

private:
    // Helper to compute vertex normal
    static math::Vec3 ComputeVertexNormal(const assets::EditableMesh& editable, uint32_t vertexID,
                                          const MeshTopology& topology, size_t vertexCount);
    
    // Helper to compute lattice weight for a point
    static float ComputeLatticeWeight(const math::Vec3& point, const math::Vec3& latticeOrigin,
                                      const math::Vec3& latticeSize, const math::IVec3& resolution,
                                      int x, int y, int z);
};

} // namespace geometry
} // namespace eyelib
