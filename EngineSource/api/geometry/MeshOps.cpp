#include "api/geometry/MeshOps.h"
#include "api/geometry/Selection.h"
#include "api/geometry/Mask.h"
#include "eyelib/geometry/operations/MeshSubdivide.h"
#include "eyelib/geometry/operations/MeshSmooth.h"
#include "eyelib/geometry/operations/MeshDisplace.h"
#include "eyelib/geometry/operations/MeshNoise.h"
#include "eyelib/geometry/operations/MeshExtrude.h"
#include "eyelib/geometry/operations/MeshBevel.h"
#include "eyelib/geometry/operations/MeshBoolean.h"
#include "eyelib/geometry/operations/MeshDecimate.h"
#include "eyelib/geometry/operations/MeshDeform.h"
#include "eyelib/geometry/operations/MeshTexturePaint.h"
#include "eyelib/geometry/operations/MeshColorPaint.h"
#include "api/render/Mesh.h"
#include "api/render/Texture.h"

namespace eyelib {
namespace api {
namespace geometry {

void MeshOps::Subdivide(Mesh& mesh, const Selection& selection, int levels) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshSubdivide::Subdivide(*editable, selection.GetInternal(), levels);
}

void MeshOps::Smooth(Mesh& mesh, const Selection& selection,
                    float strength, int iterations, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) internalMask = &mask->GetInternal();
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshSmooth::Smooth(*editable, selection.GetInternal(), strength, iterations, internalMask);
}

void MeshOps::Displace(Mesh& mesh, const Selection& selection,
                      float amount, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshDisplace::DisplaceAlongNormal(*editable, selection.GetInternal(), amount, internalMask);
}

void MeshOps::ApplyNoise(Mesh& mesh, const Selection& selection,
                        float scale, float strength, uint32_t seed, int octaves,
                        const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    if (octaves > 1) {
        ::eyelib::geometry::MeshNoise::ApplyFBMNoise(*editable, selection.GetInternal(), scale, strength, octaves, 0.5f, seed, internalMask);
    } else {
        ::eyelib::geometry::MeshNoise::ApplyPerlinNoise(*editable, selection.GetInternal(), scale, strength, seed, internalMask);
    }
}

void MeshOps::Extrude(Mesh& mesh, const Selection& selection,
                      float amount, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshExtrude::Extrude(*editable, selection.GetInternal(), amount, internalMask);
}

void MeshOps::ExtrudeAlongDirection(Mesh& mesh, const Selection& selection,
                                   const math::Vec3& direction, float amount,
                                   const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshExtrude::ExtrudeAlongDirection(*editable, selection.GetInternal(), direction, amount, internalMask);
}

void MeshOps::BevelEdges(Mesh& mesh, const Selection& selection,
                        float amount, int segments, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshBevel::BevelEdges(*editable, selection.GetInternal(), amount, segments, internalMask);
}

void MeshOps::BevelVertices(Mesh& mesh, const Selection& selection,
                           float amount, int segments, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshBevel::BevelVertices(*editable, selection.GetInternal(), amount, segments, internalMask);
}

void MeshOps::Union(Mesh& meshA, const Mesh& meshB) {
    auto* editableA = meshA.GetEditableMesh();
    auto* editableB = meshB.GetEditableMesh();
    if (!editableA || !editableB) return;
    ::eyelib::geometry::MeshBoolean::Union(*editableA, *editableB);
}

void MeshOps::Difference(Mesh& meshA, const Mesh& meshB) {
    auto* editableA = meshA.GetEditableMesh();
    auto* editableB = meshB.GetEditableMesh();
    if (!editableA || !editableB) return;
    ::eyelib::geometry::MeshBoolean::Difference(*editableA, *editableB);
}

void MeshOps::Intersection(Mesh& meshA, const Mesh& meshB) {
    auto* editableA = meshA.GetEditableMesh();
    auto* editableB = meshB.GetEditableMesh();
    if (!editableA || !editableB) return;
    ::eyelib::geometry::MeshBoolean::Intersection(*editableA, *editableB);
}

void MeshOps::Decimate(Mesh& mesh, size_t targetTriangleCount,
                      const Selection& selection, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshDecimate::Decimate(*editable, targetTriangleCount, selection.GetInternal(), internalMask);
}

void MeshOps::DecimateByPercentage(Mesh& mesh, float reductionFactor,
                                  const Selection& selection, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshDecimate::DecimateByPercentage(*editable, reductionFactor, selection.GetInternal(), internalMask);
}

void MeshOps::Inflate(Mesh& mesh, const Selection& selection,
                     float amount, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshDeform::Inflate(*editable, selection.GetInternal(), amount, internalMask);
}

void MeshOps::Deflate(Mesh& mesh, const Selection& selection,
                     float amount, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshDeform::Deflate(*editable, selection.GetInternal(), amount, internalMask);
}

void MeshOps::Relax(Mesh& mesh, const Selection& selection,
                   float strength, int iterations, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshDeform::Relax(*editable, selection.GetInternal(), strength, iterations, internalMask);
}

void MeshOps::Lattice(Mesh& mesh, const Selection& selection,
                     const math::Vec3& latticeOrigin, const math::Vec3& latticeSize,
                     const math::IVec3& latticeResolution,
                     const std::vector<math::Vec3>& controlPoints,
                     const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshDeform::Lattice(*editable, selection.GetInternal(), 
                                           latticeOrigin, latticeSize, latticeResolution,
                                           controlPoints, internalMask);
}

void MeshOps::GenerateUVs(Mesh& mesh, UVProjection projection, int axis, const Selection& selection) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshTexturePaint::UVProjection proj = 
        static_cast<::eyelib::geometry::MeshTexturePaint::UVProjection>(static_cast<int>(projection));
    ::eyelib::geometry::MeshTexturePaint::GenerateUVs(*editable, proj, axis, selection.GetInternal());
}

void MeshOps::GeneratePlanarUVs(Mesh& mesh, const math::Vec3& normal, const math::Vec3& up,
                               const Selection& selection) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshTexturePaint::GeneratePlanarUVs(*editable, normal, up, selection.GetInternal());
}

void MeshOps::GenerateCylindricalUVs(Mesh& mesh, int axis, const Selection& selection) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshTexturePaint::GenerateCylindricalUVs(*editable, axis, selection.GetInternal());
}

void MeshOps::GenerateSphericalUVs(Mesh& mesh, const Selection& selection) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshTexturePaint::GenerateSphericalUVs(*editable, selection.GetInternal());
}

void MeshOps::GenerateBoxUVs(Mesh& mesh, const Selection& selection) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshTexturePaint::GenerateBoxUVs(*editable, selection.GetInternal());
}

void MeshOps::UnwrapUVs(Mesh& mesh, float padding) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshTexturePaint::UnwrapUVs(*editable, padding);
}

void MeshOps::PackUVs(Mesh& mesh, float padding) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshTexturePaint::PackUVs(*editable, padding);
}

// Vertex Color Painting Operations

void MeshOps::PaintVertexColor(Mesh& mesh, const Selection& selection,
                               const math::Vec4& color, float strength,
                               float falloff, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshColorPaint::PaintVertexColor(*editable, selection.GetInternal(),
                                                         color, strength, falloff, internalMask);
}

void MeshOps::FillColor(Mesh& mesh, const Selection& selection, const math::Vec4& color) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshColorPaint::FillColor(*editable, selection.GetInternal(), color);
}

void MeshOps::GradientColor(Mesh& mesh, const Selection& selection,
                            const math::Vec4& startColor, const math::Vec4& endColor,
                            const math::Vec3& direction, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshColorPaint::GradientColor(*editable, selection.GetInternal(),
                                                      startColor, endColor, direction, internalMask);
}

void MeshOps::NoisePaint(Mesh& mesh, const Selection& selection,
                         const math::Vec4& baseColor, const math::Vec4& noiseColor,
                         float scale, float strength, uint32_t seed, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshColorPaint::NoisePaint(*editable, selection.GetInternal(),
                                                   baseColor, noiseColor, scale, strength, seed, internalMask);
}

void MeshOps::BlendColors(Mesh& mesh, const Selection& selection,
                          const math::Vec4& blendColor, BlendMode blendMode,
                          float strength, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    // Convert API BlendMode to internal BlendMode
    ::eyelib::geometry::BlendMode internalMode;
    switch (blendMode) {
        case api::geometry::BlendMode::Replace: internalMode = ::eyelib::geometry::BlendMode::Replace; break;
        case api::geometry::BlendMode::Add: internalMode = ::eyelib::geometry::BlendMode::Add; break;
        case api::geometry::BlendMode::Multiply: internalMode = ::eyelib::geometry::BlendMode::Multiply; break;
        case api::geometry::BlendMode::Subtract: internalMode = ::eyelib::geometry::BlendMode::Subtract; break;
        case api::geometry::BlendMode::Min: internalMode = ::eyelib::geometry::BlendMode::Min; break;
        case api::geometry::BlendMode::Max: internalMode = ::eyelib::geometry::BlendMode::Max; break;
        default: internalMode = ::eyelib::geometry::BlendMode::Replace; break;
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    ::eyelib::geometry::MeshColorPaint::BlendColors(*editable, selection.GetInternal(),
                                                    blendColor, internalMode, strength, internalMask);
}

void MeshOps::SampleColor(const Mesh& source, Mesh& target,
                         const Selection& sourceSelection, const Selection& targetSelection) {
    auto* sourceEditable = source.GetEditableMesh();
    auto* targetEditable = target.GetEditableMesh();
    if (!sourceEditable || !targetEditable) return;
    ::eyelib::geometry::MeshColorPaint::SampleColor(*sourceEditable, *targetEditable,
                                                    sourceSelection.GetInternal(), targetSelection.GetInternal());
}

// Texture-Space Painting Operations

void MeshOps::PaintTexture(Mesh& mesh, api::Texture& texture,
                          const Selection& selection, const math::Vec4& color,
                          float brushSize, float strength, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    auto internalTexture = texture.GetInternal();
    ::eyelib::geometry::MeshTexturePaint::PaintTexture(*editable, *internalTexture,
                                                       selection.GetInternal(), color,
                                                       brushSize, strength, internalMask);
}

void MeshOps::FillTexture(Mesh& mesh, api::Texture& texture,
                          const Selection& selection, const math::Vec4& color) {
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    auto internalTexture = texture.GetInternal();
    ::eyelib::geometry::MeshTexturePaint::FillTexture(*editable, *internalTexture,
                                                      selection.GetInternal(), color);
}

void MeshOps::BlendTexture(Mesh& mesh, api::Texture& texture,
                          const Selection& selection, const math::Vec4& blendColor,
                          BlendMode blendMode, float strength, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    // Convert API BlendMode to internal BlendMode
    ::eyelib::geometry::BlendMode internalMode;
    switch (blendMode) {
        case api::geometry::BlendMode::Replace: internalMode = ::eyelib::geometry::BlendMode::Replace; break;
        case api::geometry::BlendMode::Add: internalMode = ::eyelib::geometry::BlendMode::Add; break;
        case api::geometry::BlendMode::Multiply: internalMode = ::eyelib::geometry::BlendMode::Multiply; break;
        case api::geometry::BlendMode::Subtract: internalMode = ::eyelib::geometry::BlendMode::Subtract; break;
        case api::geometry::BlendMode::Min: internalMode = ::eyelib::geometry::BlendMode::Min; break;
        case api::geometry::BlendMode::Max: internalMode = ::eyelib::geometry::BlendMode::Max; break;
        default: internalMode = ::eyelib::geometry::BlendMode::Replace; break;
    }
    
    auto* editable = mesh.GetEditableMesh();
    if (!editable) return;
    auto internalTexture = texture.GetInternal();
    ::eyelib::geometry::MeshTexturePaint::BlendTexture(*editable, *internalTexture,
                                                       selection.GetInternal(), blendColor,
                                                       internalMode, strength, internalMask);
}

} // namespace geometry
} // namespace api
} // namespace eyelib
