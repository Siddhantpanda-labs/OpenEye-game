#pragma once

#include "eyelib/render/resources/Mesh.h"
#include <memory>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshPrimitives: Generate primitive mesh shapes.
 * 
 * All primitives are generated with proper normals, UVs, and topology.
 * These serve as starting points for AI sculpting operations.
 */
class MeshPrimitives {
public:
    /**
     * @brief Create a cube mesh.
     * @param size Size of the cube (half-extents, so total size is 2*size)
     * @return Shared pointer to mesh
     */
    static std::shared_ptr<render::Mesh> CreateCube(float size = 0.5f);
    
    /**
     * @brief Create a sphere mesh.
     * @param radius Sphere radius
     * @param segments Number of segments around the sphere (longitude)
     * @param rings Number of rings from pole to pole (latitude)
     * @return Shared pointer to mesh
     */
    static std::shared_ptr<render::Mesh> CreateSphere(float radius = 0.5f, int segments = 32, int rings = 16);
    
    /**
     * @brief Create a plane mesh.
     * @param width Width of the plane
     * @param height Height of the plane
     * @param widthSegments Number of segments along width
     * @param heightSegments Number of segments along height
     * @return Shared pointer to mesh
     */
    static std::shared_ptr<render::Mesh> CreatePlane(float width = 1.0f, float height = 1.0f, 
                                                      int widthSegments = 1, int heightSegments = 1);
    
    /**
     * @brief Create a quad mesh (2x2 plane).
     * @return Shared pointer to mesh
     */
    static std::shared_ptr<render::Mesh> CreateQuad();
    
    /**
     * @brief Create a cylinder mesh.
     * @param radius Cylinder radius
     * @param height Cylinder height
     * @param segments Number of segments around the cylinder
     * @param rings Number of rings along height
     * @param capTop Whether to cap the top
     * @param capBottom Whether to cap the bottom
     * @return Shared pointer to mesh
     */
    static std::shared_ptr<render::Mesh> CreateCylinder(float radius = 0.5f, float height = 1.0f,
                                                         int segments = 32, int rings = 1,
                                                         bool capTop = true, bool capBottom = true);
    
    /**
     * @brief Create a cone mesh.
     * @param radius Base radius
     * @param height Cone height
     * @param segments Number of segments around the base
     * @param capBase Whether to cap the base
     * @return Shared pointer to mesh
     */
    static std::shared_ptr<render::Mesh> CreateCone(float radius = 0.5f, float height = 1.0f,
                                                     int segments = 32, bool capBase = true);
    
    /**
     * @brief Create a torus mesh.
     * @param radius Major radius (distance from center to tube center)
     * @param tubeRadius Minor radius (tube thickness)
     * @param segments Number of segments around the major radius
     * @param tubeSegments Number of segments around the tube
     * @return Shared pointer to mesh
     */
    static std::shared_ptr<render::Mesh> CreateTorus(float radius = 0.5f, float tubeRadius = 0.2f,
                                                     int segments = 32, int tubeSegments = 16);

private:
    // Helper to generate normals and tangents after mesh creation
    static void FinalizeMesh(std::shared_ptr<render::Mesh> mesh);
};

} // namespace geometry
} // namespace eyelib
