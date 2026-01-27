#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include "eyelib/math/Types.h"
#include <memory>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshColorPaint: Vertex color painting operations.
 * 
 * Provides operations for painting vertex colors on meshes.
 * This enables procedural coloring and texture-like effects using vertex colors.
 */
class MeshColorPaint {
public:
    /**
     * @brief Paint vertex colors in a region with falloff.
     * 
     * Paints colors onto vertices, blending with existing colors based on strength.
     * Supports falloff for smooth color transitions.
     * 
     * @param mesh Mesh to paint (modified in-place)
     * @param selection Region to paint (if empty, paints all vertices)
     * @param color Color to paint (RGBA)
     * @param strength Paint strength (0.0 to 1.0)
     * @param falloff Falloff radius (0 = hard edge, >0 = smooth falloff from selection center)
     * @param mask Optional mask to control paint strength per vertex
     */
    static void PaintVertexColor(render::Mesh& mesh, const Selection& selection,
                                const math::Vec4& color, float strength = 1.0f,
                                float falloff = 0.0f, const Mask* mask = nullptr);
    
    /**
     * @brief Fill region with a solid color.
     * 
     * Replaces vertex colors in the selection with the specified color.
     * 
     * @param mesh Mesh to fill
     * @param selection Region to fill (if empty, fills all vertices)
     * @param color Fill color (RGBA)
     */
    static void FillColor(render::Mesh& mesh, const Selection& selection,
                         const math::Vec4& color);
    
    /**
     * @brief Apply gradient color to region.
     * 
     * Creates a linear gradient from startColor to endColor along the specified direction.
     * 
     * @param mesh Mesh to apply gradient to
     * @param selection Region to apply gradient (if empty, applies to all vertices)
     * @param startColor Starting color
     * @param endColor Ending color
     * @param direction Gradient direction (will be normalized)
     * @param mask Optional mask to control gradient strength
     */
    static void GradientColor(render::Mesh& mesh, const Selection& selection,
                             const math::Vec4& startColor, const math::Vec4& endColor,
                             const math::Vec3& direction, const Mask* mask = nullptr);
    
    /**
     * @brief Apply noise-based color painting.
     * 
     * Paints colors using procedural noise for organic, natural-looking patterns.
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
    static void NoisePaint(render::Mesh& mesh, const Selection& selection,
                          const math::Vec4& baseColor, const math::Vec4& noiseColor,
                          float scale = 1.0f, float strength = 0.5f, uint32_t seed = 0,
                          const Mask* mask = nullptr);
    
    /**
     * @brief Blend colors in region using a blend mode.
     * 
     * Blends existing colors with a target color using various blend modes.
     * 
     * @param mesh Mesh to blend colors on
     * @param selection Region to blend (if empty, blends all vertices)
     * @param blendColor Color to blend with
     * @param blendMode Blend mode (from Mask::BlendMode: Replace, Add, Multiply, etc.)
     * @param strength Blend strength (0.0 to 1.0)
     * @param mask Optional mask to control blend strength
     */
    static void BlendColors(render::Mesh& mesh, const Selection& selection,
                          const math::Vec4& blendColor, BlendMode blendMode,
                          float strength = 1.0f, const Mask* mask = nullptr);
    
    /**
     * @brief Sample colors from source mesh and transfer to target mesh.
     * 
     * Transfers vertex colors from source selection to target selection.
     * Useful for copying color information between meshes or regions.
     * 
     * @param source Source mesh to sample from
     * @param target Target mesh to apply colors to (modified in-place)
     * @param sourceSelection Source region to sample from
     * @param targetSelection Target region to apply colors to
     */
    static void SampleColor(const render::Mesh& source, render::Mesh& target,
                           const Selection& sourceSelection, const Selection& targetSelection);

private:
    // Helper to compute selection center for falloff
    static math::Vec3 ComputeSelectionCenter(const render::Mesh& mesh, const Selection& selection);
    
    // Helper to blend two colors
    static math::Vec4 BlendColor(const math::Vec4& a, const math::Vec4& b, BlendMode mode, float strength);
};

} // namespace geometry
} // namespace eyelib
