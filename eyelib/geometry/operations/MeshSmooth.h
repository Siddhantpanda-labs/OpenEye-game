#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include <memory>

namespace eyelib {
namespace geometry {

// Forward declaration
class MeshTopology;

/**
 * @brief MeshSmooth: Laplacian smoothing operations.
 * 
 * Smooths mesh geometry by averaging vertex positions with neighbors.
 * Deterministic and region-aware.
 */
class MeshSmooth {
public:
    /**
     * @brief Smooth mesh vertices using Laplacian smoothing.
     * 
     * @param mesh Mesh to smooth (modified in-place)
     * @param selection Region to smooth (if empty, smooths all vertices)
     * @param strength Smoothing strength (0.0 = no smoothing, 1.0 = full smoothing)
     * @param iterations Number of smoothing iterations
     * @param mask Optional mask to control smoothing strength per vertex
     */
    static void Smooth(render::Mesh& mesh, const Selection& selection, 
                      float strength = 0.5f, int iterations = 1,
                      const Mask* mask = nullptr);
    
    /**
     * @brief Smooth mesh with uniform weights (simple average).
     */
    static void SmoothUniform(render::Mesh& mesh, const Selection& selection,
                              float strength = 0.5f, int iterations = 1,
                              const Mask* mask = nullptr);
    
    /**
     * @brief Smooth mesh with cotangent weights (better for irregular meshes).
     */
    static void SmoothCotangent(render::Mesh& mesh, const Selection& selection,
                                float strength = 0.5f, int iterations = 1,
                                const Mask* mask = nullptr);

private:
    // Helper to get vertex neighbors from topology
    static std::vector<uint32_t> GetVertexNeighbors(const MeshTopology& topology, uint32_t vertexID, size_t vertexCount);
    
    // Helper to get vertex neighbors from mesh (rebuilds topology - use sparingly)
    static std::vector<uint32_t> GetVertexNeighbors(const render::Mesh& mesh, uint32_t vertexID);
    
    // Helper to compute Laplacian for a vertex
    static math::Vec3 ComputeLaplacian(const render::Mesh& mesh, uint32_t vertexID, 
                                       const std::vector<uint32_t>& neighbors);
};

} // namespace geometry
} // namespace eyelib
