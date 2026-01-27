#pragma once

#include "eyelib/math/Types.h"
#include "api/geometry/Selection.h"
#include "api/geometry/Mask.h"
#include "api/render/Texture.h"
#include <memory>
#include <functional>

namespace eyelib {
namespace render { class Mesh; class Texture2D; }
namespace api {
class Mesh; // Forward declaration for API Mesh
class Texture; // Forward declaration for API Texture
namespace geometry {

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
    
    /**
     * @brief Extrude selected faces along their normals.
     * 
     * Creates new geometry by duplicating face vertices and moving them
     * along face normals, then connecting old and new geometry with side faces.
     * 
     * @param mesh Mesh to extrude (modified in-place)
     * @param selection Region to extrude (must be face selection)
     * @param amount Extrusion distance (positive = outward, negative = inward)
     * @param mask Optional mask to control extrusion amount per vertex
     */
    static void Extrude(Mesh& mesh, const Selection& selection,
                       float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Extrude selected faces along a custom direction.
     * 
     * @param mesh Mesh to extrude
     * @param selection Region to extrude (must be face selection)
     * @param direction Extrusion direction (will be normalized)
     * @param amount Extrusion distance
     * @param mask Optional mask to control extrusion amount per vertex
     */
    static void ExtrudeAlongDirection(Mesh& mesh, const Selection& selection,
                                     const math::Vec3& direction, float amount,
                                     const Mask* mask = nullptr);
    
    /**
     * @brief Bevel selected edges.
     * 
     * Creates chamfered edges by inserting new vertices and faces.
     * 
     * @param mesh Mesh to bevel (modified in-place)
     * @param selection Region to bevel (must be edge selection)
     * @param amount Bevel amount (distance to offset new edge)
     * @param segments Number of segments in bevel (1 = chamfer, >1 = rounded)
     * @param mask Optional mask to control bevel amount per vertex
     */
    static void BevelEdges(Mesh& mesh, const Selection& selection,
                          float amount, int segments = 1,
                          const Mask* mask = nullptr);
    
    /**
     * @brief Bevel selected vertices.
     * 
     * Creates chamfered vertices by beveling all connected edges.
     * 
     * @param mesh Mesh to bevel
     * @param selection Region to bevel (must be vertex selection)
     * @param amount Bevel amount
     * @param segments Number of segments
     * @param mask Optional mask to control bevel amount
     */
    static void BevelVertices(Mesh& mesh, const Selection& selection,
                             float amount, int segments = 1,
                             const Mask* mask = nullptr);
    
    /**
     * @brief Union: Combine two meshes (A ∪ B).
     * 
     * Creates a mesh containing all geometry from both meshes.
     * 
     * @param meshA First mesh (modified to contain result)
     * @param meshB Second mesh
     */
    static void Union(Mesh& meshA, const Mesh& meshB);
    
    /**
     * @brief Difference: Subtract meshB from meshA (A - B).
     * 
     * Creates a mesh containing geometry from A that is not inside B.
     * 
     * @param meshA First mesh (modified to contain result)
     * @param meshB Second mesh (subtracted from A)
     */
    static void Difference(Mesh& meshA, const Mesh& meshB);
    
    /**
     * @brief Intersection: Intersect two meshes (A ∩ B).
     * 
     * Creates a mesh containing only geometry that is in both meshes.
     * 
     * @param meshA First mesh (modified to contain result)
     * @param meshB Second mesh
     */
    static void Intersection(Mesh& meshA, const Mesh& meshB);
    
    /**
     * @brief Decimate mesh by reducing triangle count.
     * 
     * Simplifies mesh by collapsing edges, reducing vertex/triangle count
     * while attempting to preserve shape and important features.
     * 
     * @param mesh Mesh to decimate (modified in-place)
     * @param targetTriangleCount Target number of triangles
     * @param selection Optional region to decimate (if empty, decimates entire mesh)
     * @param mask Optional mask to protect certain regions from decimation
     */
    static void Decimate(Mesh& mesh, size_t targetTriangleCount,
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
    static void DecimateByPercentage(Mesh& mesh, float reductionFactor,
                                    const Selection& selection = Selection::Empty(Selection::Type::Faces),
                                    const Mask* mask = nullptr);
    
    /**
     * @brief Inflate: Push vertices outward along their normals.
     * 
     * @param mesh Mesh to inflate (modified in-place)
     * @param selection Region to inflate (if empty, inflates all vertices)
     * @param amount Inflate amount (positive = outward)
     * @param mask Optional mask to control inflation strength
     */
    static void Inflate(Mesh& mesh, const Selection& selection,
                       float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Deflate: Push vertices inward along their normals.
     * 
     * @param mesh Mesh to deflate (modified in-place)
     * @param selection Region to deflate (if empty, deflates all vertices)
     * @param amount Deflate amount (positive = inward)
     * @param mask Optional mask to control deflation strength
     */
    static void Deflate(Mesh& mesh, const Selection& selection,
                       float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Relax: Gentle smoothing operation.
     * 
     * @param mesh Mesh to relax (modified in-place)
     * @param selection Region to relax (if empty, relaxes all vertices)
     * @param strength Relaxation strength (0.0 to 1.0)
     * @param iterations Number of relaxation iterations
     * @param mask Optional mask to control relaxation strength
     */
    static void Relax(Mesh& mesh, const Selection& selection,
                     float strength = 0.1f, int iterations = 1,
                     const Mask* mask = nullptr);
    
    /**
     * @brief Lattice: Deform mesh using a control lattice.
     * 
     * @param mesh Mesh to deform (modified in-place)
     * @param selection Region to deform (if empty, deforms all vertices)
     * @param latticeOrigin Origin of the lattice in mesh space
     * @param latticeSize Size of the lattice (width, height, depth)
     * @param latticeResolution Resolution of the lattice (x, y, z divisions)
     * @param controlPoints Control point offsets (flattened 3D array: [x][y][z])
     * @param mask Optional mask to control deformation strength
     */
    static void Lattice(Mesh& mesh, const Selection& selection,
                       const math::Vec3& latticeOrigin, const math::Vec3& latticeSize,
                       const math::IVec3& latticeResolution,
                       const std::vector<math::Vec3>& controlPoints,
                       const Mask* mask = nullptr);
    
    /**
     * @brief UV projection types for automatic UV generation.
     */
    enum class UVProjection {
        Planar = 0,      // Planar projection (flat)
        Cylindrical = 1, // Cylindrical projection (wraps around axis)
        Spherical = 2,   // Spherical projection (wraps around sphere)
        Box = 3          // Box projection (6-sided, like a cube map)
    };
    
    /**
     * @brief Generate UV coordinates using projection.
     * 
     * @param mesh Mesh to generate UVs for (modified in-place)
     * @param projection Projection type (Planar, Cylindrical, Spherical, Box)
     * @param axis Projection axis (for planar/cylindrical: 0=X, 1=Y, 2=Z)
     * @param selection Optional region to generate UVs for
     */
    static void GenerateUVs(Mesh& mesh, UVProjection projection, int axis = 1,
                           const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate planar UV coordinates.
     * 
     * @param mesh Mesh to generate UVs for
     * @param normal Plane normal (projection direction)
     * @param up Up vector for UV orientation
     * @param selection Optional region to generate UVs for
     */
    static void GeneratePlanarUVs(Mesh& mesh, const math::Vec3& normal, const math::Vec3& up,
                                  const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate cylindrical UV coordinates.
     * 
     * @param mesh Mesh to generate UVs for
     * @param axis Cylinder axis (0=X, 1=Y, 2=Z)
     * @param selection Optional region to generate UVs for
     */
    static void GenerateCylindricalUVs(Mesh& mesh, int axis = 1,
                                       const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate spherical UV coordinates.
     * 
     * @param mesh Mesh to generate UVs for
     * @param selection Optional region to generate UVs for
     */
    static void GenerateSphericalUVs(Mesh& mesh,
                                    const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate box UV coordinates (6-sided projection).
     * 
     * @param mesh Mesh to generate UVs for
     * @param selection Optional region to generate UVs for
     */
    static void GenerateBoxUVs(Mesh& mesh,
                              const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Unwrap UVs using lightmap packing.
     * 
     * @param mesh Mesh to unwrap
     * @param padding Padding between UV islands
     */
    static void UnwrapUVs(Mesh& mesh, float padding = 0.01f);
    
    /**
     * @brief Pack UV islands into texture space.
     * 
     * @param mesh Mesh to pack UVs for
     * @param padding Padding between islands
     */
    static void PackUVs(Mesh& mesh, float padding = 0.01f);
    
    // ============================================
    // Vertex Color Painting Operations
    // ============================================
    
    /**
     * @brief Paint vertex colors in a region with falloff.
     * 
     * @param mesh Mesh to paint (modified in-place)
     * @param selection Region to paint (if empty, paints all vertices)
     * @param color Color to paint (RGBA)
     * @param strength Paint strength (0.0 to 1.0)
     * @param falloff Falloff radius (0 = hard edge, >0 = smooth falloff)
     * @param mask Optional mask to control paint strength per vertex
     */
    static void PaintVertexColor(Mesh& mesh, const Selection& selection,
                                const math::Vec4& color, float strength = 1.0f,
                                float falloff = 0.0f, const Mask* mask = nullptr);
    
    /**
     * @brief Fill region with a solid color.
     * 
     * @param mesh Mesh to fill
     * @param selection Region to fill (if empty, fills all vertices)
     * @param color Fill color (RGBA)
     */
    static void FillColor(Mesh& mesh, const Selection& selection, const math::Vec4& color);
    
    /**
     * @brief Apply gradient color to region.
     * 
     * @param mesh Mesh to apply gradient to
     * @param selection Region to apply gradient (if empty, applies to all vertices)
     * @param startColor Starting color
     * @param endColor Ending color
     * @param direction Gradient direction (will be normalized)
     * @param mask Optional mask to control gradient strength
     */
    static void GradientColor(Mesh& mesh, const Selection& selection,
                             const math::Vec4& startColor, const math::Vec4& endColor,
                             const math::Vec3& direction, const Mask* mask = nullptr);
    
    /**
     * @brief Apply noise-based color painting.
     * 
     * @param mesh Mesh to paint
     * @param selection Region to paint (if empty, paints all vertices)
     * @param baseColor Base color
     * @param noiseColor Noise color variation
     * @param scale Noise scale (larger = smoother)
     * @param strength Noise strength (0.0 to 1.0)
     * @param seed Random seed (for determinism)
     * @param mask Optional mask to control paint strength
     */
    static void NoisePaint(Mesh& mesh, const Selection& selection,
                          const math::Vec4& baseColor, const math::Vec4& noiseColor,
                          float scale = 1.0f, float strength = 0.5f, uint32_t seed = 0,
                          const Mask* mask = nullptr);
    
    /**
     * @brief Blend colors in region using a blend mode.
     * 
     * @param mesh Mesh to blend colors on
     * @param selection Region to blend (if empty, blends all vertices)
     * @param blendColor Color to blend with
     * @param blendMode Blend mode (Replace, Add, Multiply, Subtract, Min, Max)
     * @param strength Blend strength (0.0 to 1.0)
     * @param mask Optional mask to control blend strength
     */
    static void BlendColors(Mesh& mesh, const Selection& selection,
                           const math::Vec4& blendColor, BlendMode blendMode,
                           float strength = 1.0f, const Mask* mask = nullptr);
    
    /**
     * @brief Sample colors from source mesh and transfer to target mesh.
     * 
     * @param source Source mesh to sample from
     * @param target Target mesh to apply colors to (modified in-place)
     * @param sourceSelection Source region to sample from
     * @param targetSelection Target region to apply colors to
     */
    static void SampleColor(const Mesh& source, Mesh& target,
                           const Selection& sourceSelection, const Selection& targetSelection);
    
    // ============================================
    // Texture-Space Painting Operations
    // ============================================
    
    /**
     * @brief Paint color to texture at UV coordinates.
     * 
     * @param mesh Mesh with UV coordinates
     * @param texture Texture to paint to (RGBA8 format)
     * @param selection Region to paint (if empty, paints all vertices)
     * @param color Color to paint (RGBA)
     * @param brushSize Brush size in UV space (0.0 to 1.0)
     * @param strength Paint strength (0.0 to 1.0)
     * @param mask Optional mask to control paint strength per vertex
     */
    static void PaintTexture(Mesh& mesh, api::Texture& texture,
                            const Selection& selection, const math::Vec4& color,
                            float brushSize = 0.1f, float strength = 1.0f,
                            const Mask* mask = nullptr);
    
    /**
     * @brief Fill texture region with color.
     * 
     * @param mesh Mesh with UV coordinates
     * @param texture Texture to fill
     * @param selection Region to fill (if empty, fills all vertices)
     * @param color Fill color (RGBA)
     */
    static void FillTexture(Mesh& mesh, api::Texture& texture,
                           const Selection& selection, const math::Vec4& color);
    
    /**
     * @brief Blend texture with color using blend mode.
     * 
     * @param mesh Mesh with UV coordinates
     * @param texture Texture to blend
     * @param selection Region to blend (if empty, blends all vertices)
     * @param blendColor Color to blend with
     * @param blendMode Blend mode (Replace, Add, Multiply, etc.)
     * @param strength Blend strength (0.0 to 1.0)
     * @param mask Optional mask to control blend strength
     */
    static void BlendTexture(Mesh& mesh, api::Texture& texture,
                            const Selection& selection, const math::Vec4& blendColor,
                            BlendMode blendMode, float strength = 1.0f,
                            const Mask* mask = nullptr);
};

} // namespace geometry
} // namespace api
} // namespace eyelib
