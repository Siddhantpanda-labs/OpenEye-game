#pragma once

#include "eyelib/assets/EditableMesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshExtrude: Face extrusion operations.
 * 
 * Extrudes selected faces along their normals or a custom direction,
 * creating new geometry. This is a fundamental modeling operation.
 */
class MeshExtrude {
public:
    /**
     * @brief Extrude faces along their normals.
     * 
     * Creates new vertices by duplicating face vertices and moving them
     * along face normals, then creates new faces to connect old and new geometry.
     * 
     * @param mesh Mesh to extrude (modified in-place)
     * @param selection Region to extrude (must be face selection)
     * @param amount Extrusion distance (positive = outward, negative = inward)
     * @param mask Optional mask to control extrusion amount per vertex
     */
    static void Extrude(assets::EditableMesh& editable, const Selection& selection,
                       float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Extrude faces along a custom direction.
     * 
     * @param mesh Mesh to extrude
     * @param selection Region to extrude (must be face selection)
     * @param direction Extrusion direction (will be normalized)
     * @param amount Extrusion distance
     * @param mask Optional mask to control extrusion amount per vertex
     */
    static void ExtrudeAlongDirection(assets::EditableMesh& editable, const Selection& selection,
                                     const math::Vec3& direction, float amount,
                                     const Mask* mask = nullptr);

private:
    // Helper to compute face normal
    static math::Vec3 ComputeFaceNormal(const assets::EditableMesh& editable, uint32_t v0, uint32_t v1, uint32_t v2);
    
    // Helper to extrude a single face
    static void ExtrudeFace(const assets::EditableMesh& editable, uint32_t faceIndex,
                           const std::unordered_set<uint32_t>& selectedFaces,
                           float amount, const math::Vec3& direction,
                           const Mask* mask,
                           std::vector<render::Vertex>& newVertices,
                           std::vector<uint32_t>& newIndices,
                           std::unordered_map<uint32_t, uint32_t>& vertexMap);
};

} // namespace geometry
} // namespace eyelib
