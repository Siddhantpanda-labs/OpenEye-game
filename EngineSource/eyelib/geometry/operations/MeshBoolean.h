#pragma once

#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include <memory>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshBoolean: Boolean operations (Union, Difference, Intersection).
 * 
 * Performs CSG (Constructive Solid Geometry) operations on meshes.
 * These operations combine two meshes using set operations.
 */
class MeshBoolean {
public:
    /**
     * @brief Union: Combine two meshes (A ∪ B).
     * 
     * Creates a mesh containing all geometry from both meshes.
     * 
     * @param meshA First mesh (modified to contain result)
     * @param meshB Second mesh
     * @return Result mesh (union of A and B)
     */
    static void Union(geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB);
    
    /**
     * @brief Difference: Subtract meshB from meshA (A - B).
     * 
     * Creates a mesh containing geometry from A that is not inside B.
     * 
     * @param meshA First mesh (modified to contain result)
     * @param meshB Second mesh (subtracted from A)
     * @return Result mesh (difference A - B)
     */
    static void Difference(geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB);
    
    /**
     * @brief Intersection: Intersect two meshes (A ∩ B).
     * 
     * Creates a mesh containing only geometry that is in both meshes.
     * 
     * @param meshA First mesh (modified to contain result)
     * @param meshB Second mesh
     * @return Result mesh (intersection of A and B)
     */
    static void Intersection(geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB);

private:
    // Helper to classify vertex as inside/outside mesh
    static bool IsVertexInside(const geometry::EditableMesh& editable, const math::Vec3& point);
    
    // Helper to compute mesh bounds
    static math::AABB ComputeMeshBounds(const geometry::EditableMesh& editable);
    
    // Helper to check if point is inside mesh using ray casting
    static bool RayCastInsideTest(const geometry::EditableMesh& editable, const math::Vec3& point, const math::Vec3& direction);
    
    // Helper to classify face vertices and create new geometry
    static void ClassifyAndMerge(geometry::EditableMesh& resultMesh, const geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB,
                                bool keepAInside, bool keepAOutside, bool keepBInside, bool keepBOutside);
};

} // namespace geometry
} // namespace eyelib
