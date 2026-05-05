#pragma once

#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include "eyelib/geometry/fields/NoiseField.h"
#include <memory>
#include <functional>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshNoise: Apply noise-based displacement to meshes.
 * 
 * Uses noise fields to create procedural surface detail.
 */
class MeshNoise {
public:
    /**
     * @brief Apply noise displacement to mesh.
     * 
     * @param mesh Mesh to modify
     * @param selection Region to apply noise to
     * @param noiseField Noise field function
     * @param strength Displacement strength
     * @param direction Displacement direction (nullptr = use normals)
     * @param mask Optional mask to control noise strength per vertex
     */
    static void ApplyNoise(geometry::EditableMesh& editable, const Selection& selection,
                          std::function<float(const math::Vec3&)> noiseField,
                          float strength, const math::Vec3* direction = nullptr,
                          const Mask* mask = nullptr);
    
    /**
     * @brief Apply Perlin noise displacement.
     */
    static void ApplyPerlinNoise(geometry::EditableMesh& editable, const Selection& selection,
                                 float scale = 1.0f, float strength = 0.1f,
                                 uint32_t seed = 0, const Mask* mask = nullptr);
    
    /**
     * @brief Apply FBM noise displacement (multi-octave).
     */
    static void ApplyFBMNoise(geometry::EditableMesh& editable, const Selection& selection,
                             float scale = 1.0f, float strength = 0.1f,
                             int octaves = 4, float persistence = 0.5f,
                             uint32_t seed = 0, const Mask* mask = nullptr);
};

} // namespace geometry
} // namespace eyelib
