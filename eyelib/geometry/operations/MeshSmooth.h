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
 * @brief MeshSmooth: Uniform Laplacian + Taubin (HC) smoothing operations.
 * 
 * Industry-proven smoothing: Uniform Laplacian with Taubin shape restoration.
 * Simple, stable, and effective.
 */
class MeshSmooth {
public:
    /**
     * @brief Smooth mesh vertices using Taubin (HC) smoothing.
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
     * @brief Smooth mesh with uniform Laplacian (simple one-ring average).
     */
    static void SmoothUniform(render::Mesh& mesh, const Selection& selection,
                              float strength = 0.5f, int iterations = 1,
                              const Mask* mask = nullptr);

private:
    // Taubin (HC) smoothing: two-pass correction on uniform Laplacian
    static void SmoothTaubin(render::Mesh& mesh, const Selection& selection,
                            float lambda, float mu, int iterations,
                            const Mask* mask, const MeshTopology& topology);
    
    // Compute uniform Laplacian for a vertex (one-ring average)
    static math::Vec3 ComputeUniformLaplacian(const render::Mesh& mesh, uint32_t vertexID,
                                               const MeshTopology& topology, size_t vertexCount);
    
    // Helper to get vertex neighbors from topology
    static std::vector<uint32_t> GetVertexNeighbors(const MeshTopology& topology, uint32_t vertexID, size_t vertexCount);
};

} // namespace geometry
} // namespace eyelib
