#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshTexturePaint: UV generation and texture painting operations.
 * 
 * Provides UV coordinate generation and texture-space painting capabilities.
 * This enables procedural UV mapping and texture manipulation.
 */
class MeshTexturePaint {
public:
    /**
     * @brief UV projection types for automatic UV generation.
     */
    enum class UVProjection {
        Planar,      // Planar projection (flat)
        Cylindrical, // Cylindrical projection (wraps around axis)
        Spherical,   // Spherical projection (wraps around sphere)
        Box          // Box projection (6-sided, like a cube map)
    };
    
    /**
     * @brief Generate UV coordinates using projection.
     * 
     * Automatically generates UV coordinates for vertices based on projection type.
     * Useful for meshes without UVs or for re-mapping existing UVs.
     * 
     * @param mesh Mesh to generate UVs for (modified in-place)
     * @param projection Projection type to use
     * @param axis Projection axis (for planar/cylindrical: 0=X, 1=Y, 2=Z)
     * @param selection Optional region to generate UVs for (if empty, all vertices)
     */
    static void GenerateUVs(render::Mesh& mesh, UVProjection projection,
                           int axis = 1, const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate planar UV coordinates.
     * 
     * Projects vertices onto a plane and maps to UV space.
     * 
     * @param mesh Mesh to generate UVs for
     * @param normal Plane normal (projection direction)
     * @param up Up vector for UV orientation
     * @param selection Optional region to generate UVs for
     */
    static void GeneratePlanarUVs(render::Mesh& mesh, const math::Vec3& normal, const math::Vec3& up,
                                 const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate cylindrical UV coordinates.
     * 
     * Projects vertices onto a cylinder and maps to UV space.
     * U wraps around, V maps along axis.
     * 
     * @param mesh Mesh to generate UVs for
     * @param axis Cylinder axis (0=X, 1=Y, 2=Z)
     * @param selection Optional region to generate UVs for
     */
    static void GenerateCylindricalUVs(render::Mesh& mesh, int axis = 1,
                                     const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate spherical UV coordinates.
     * 
     * Projects vertices onto a sphere and maps to UV space.
     * Standard spherical mapping (like a world map).
     * 
     * @param mesh Mesh to generate UVs for
     * @param selection Optional region to generate UVs for
     */
    static void GenerateSphericalUVs(render::Mesh& mesh,
                                    const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Generate box UV coordinates.
     * 
     * Projects vertices onto a box (6-sided projection).
     * Each face gets its own UV space.
     * 
     * @param mesh Mesh to generate UVs for
     * @param selection Optional region to generate UVs for
     */
    static void GenerateBoxUVs(render::Mesh& mesh,
                              const Selection& selection = Selection::Empty(Selection::Type::Vertices));
    
    /**
     * @brief Unwrap UVs using lightmap packing.
     * 
     * Generates non-overlapping UV coordinates suitable for lightmaps.
     * This is a simplified unwrapping - full implementation would use
     * more sophisticated algorithms like LSCM or ABF.
     * 
     * @param mesh Mesh to unwrap
     * @param padding Padding between UV islands (0.0 to 1.0)
     */
    static void UnwrapUVs(render::Mesh& mesh, float padding = 0.01f);
    
    /**
     * @brief Pack UV islands into texture space.
     * 
     * Arranges UV islands efficiently to minimize wasted texture space.
     * 
     * @param mesh Mesh to pack UVs for
     * @param padding Padding between islands
     */
    static void PackUVs(render::Mesh& mesh, float padding = 0.01f);
    
    // ============================================
    // Texture-Space Painting Operations
    // ============================================
    
    /**
     * @brief Paint color to texture at UV coordinates.
     * 
     * Paints color onto a texture using mesh UV coordinates.
     * Supports brush falloff and strength control.
     * 
     * @param mesh Mesh with UV coordinates
     * @param texture Texture to paint to (RGBA8 format)
     * @param selection Region to paint (if empty, paints all vertices)
     * @param color Color to paint (RGBA)
     * @param brushSize Brush size in UV space (0.0 to 1.0)
     * @param strength Paint strength (0.0 to 1.0)
     * @param mask Optional mask to control paint strength per vertex
     */
    static void PaintTexture(render::Mesh& mesh, render::Texture2D& texture,
                            const Selection& selection, const math::Vec4& color,
                            float brushSize = 0.1f, float strength = 1.0f,
                            const Mask* mask = nullptr);
    
    /**
     * @brief Fill texture region with color.
     * 
     * Fills texture pixels covered by mesh UV coordinates with solid color.
     * 
     * @param mesh Mesh with UV coordinates
     * @param texture Texture to fill
     * @param selection Region to fill (if empty, fills all vertices)
     * @param color Fill color (RGBA)
     */
    static void FillTexture(render::Mesh& mesh, render::Texture2D& texture,
                           const Selection& selection, const math::Vec4& color);
    
    /**
     * @brief Blend texture with color using blend mode.
     * 
     * Blends existing texture pixels with color using various blend modes.
     * 
     * @param mesh Mesh with UV coordinates
     * @param texture Texture to blend
     * @param selection Region to blend (if empty, blends all vertices)
     * @param blendColor Color to blend with
     * @param blendMode Blend mode (Replace, Add, Multiply, etc.)
     * @param strength Blend strength (0.0 to 1.0)
     * @param mask Optional mask to control blend strength
     */
    static void BlendTexture(render::Mesh& mesh, render::Texture2D& texture,
                            const Selection& selection, const math::Vec4& blendColor,
                            BlendMode blendMode, float strength = 1.0f,
                            const Mask* mask = nullptr);

private:
    // Helper to compute mesh bounds for UV scaling
    static math::AABB ComputeMeshBounds(const render::Mesh& mesh, const Selection& selection);
    
    // Helper to get axis vectors
    static void GetAxisVectors(int axis, math::Vec3& forward, math::Vec3& right, math::Vec3& up);
    
    // Helper to sample texture at UV coordinate
    static math::Vec4 SampleTexture(const render::Texture2D& texture, float u, float v);
    
    // Helper to write color to texture at UV coordinate
    static void WriteTexture(render::Texture2D& texture, float u, float v, const math::Vec4& color);
    
    // Helper to blend two colors
    static math::Vec4 BlendColor(const math::Vec4& a, const math::Vec4& b, BlendMode mode, float strength);
};

} // namespace geometry
} // namespace eyelib
