#pragma once

#include "eyelib/math/Types.h"
#include <memory>
#include <functional>

namespace eyelib {
namespace render { class Mesh; }
namespace api {
class Mesh; // Forward declaration for API Mesh
namespace geometry {

class Selection;
class Mask;

/**
 * @brief MeshOps: Public API for mesh operations.
 * 
 * Provides high-level mesh manipulation operations following the
 * operation-based API pattern (not raw data access).
 */
class MeshOps {
public:
    /**
     * @brief Subdivide mesh faces.
     * 
     * @param mesh Mesh to subdivide (modified in-place)
     * @param selection Region to subdivide (empty = all faces)
     * @param levels Number of subdivision levels
     */
    static void Subdivide(Mesh& mesh, const Selection& selection, int levels = 1);
    
    /**
     * @brief Smooth mesh vertices using Laplacian smoothing.
     * 
     * @param mesh Mesh to smooth
     * @param selection Region to smooth (empty = all vertices)
     * @param strength Smoothing strength (0.0 to 1.0)
     * @param iterations Number of smoothing iterations
     * @param mask Optional mask to control smoothing strength
     */
    static void Smooth(Mesh& mesh, const Selection& selection,
                      float strength = 0.5f, int iterations = 1,
                      const Mask* mask = nullptr);
    
    /**
     * @brief Displace vertices along their normals.
     * 
     * @param mesh Mesh to displace
     * @param selection Region to displace
     * @param amount Displacement amount
     * @param mask Optional mask to control displacement strength
     */
    static void Displace(Mesh& mesh, const Selection& selection,
                        float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Apply noise displacement to mesh.
     * 
     * @param mesh Mesh to modify
     * @param selection Region to apply noise
     * @param scale Noise scale (larger = smoother)
     * @param strength Displacement strength
     * @param seed Random seed (for determinism)
     * @param octaves Number of octaves (for FBM, 1 for Perlin)
     * @param mask Optional mask to control noise strength
     */
    static void ApplyNoise(Mesh& mesh, const Selection& selection,
                          float scale = 1.0f, float strength = 0.1f,
                          uint32_t seed = 0, int octaves = 1,
                          const Mask* mask = nullptr);
};

} // namespace geometry
} // namespace api
} // namespace eyelib
