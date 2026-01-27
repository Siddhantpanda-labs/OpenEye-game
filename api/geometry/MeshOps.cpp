#include "api/geometry/MeshOps.h"
#include "api/geometry/Selection.h"
#include "api/geometry/Mask.h"
#include "eyelib/geometry/operations/MeshSubdivide.h"
#include "eyelib/geometry/operations/MeshSmooth.h"
#include "eyelib/geometry/operations/MeshDisplace.h"
#include "eyelib/geometry/operations/MeshNoise.h"
#include "api/render/Mesh.h"

namespace eyelib {
namespace api {
namespace geometry {

void MeshOps::Subdivide(Mesh& mesh, const Selection& selection, int levels) {
    auto internalMesh = mesh.GetInternal();
    ::eyelib::geometry::MeshSubdivide::Subdivide(*internalMesh, selection.GetInternal(), levels);
}

void MeshOps::Smooth(Mesh& mesh, const Selection& selection,
                    float strength, int iterations, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto internalMesh = mesh.GetInternal();
    ::eyelib::geometry::MeshSmooth::Smooth(*internalMesh, selection.GetInternal(), strength, iterations, internalMask);
}

void MeshOps::Displace(Mesh& mesh, const Selection& selection,
                      float amount, const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto internalMesh = mesh.GetInternal();
    ::eyelib::geometry::MeshDisplace::DisplaceAlongNormal(*internalMesh, selection.GetInternal(), amount, internalMask);
}

void MeshOps::ApplyNoise(Mesh& mesh, const Selection& selection,
                        float scale, float strength, uint32_t seed, int octaves,
                        const Mask* mask) {
    const ::eyelib::geometry::Mask* internalMask = nullptr;
    if (mask) {
        internalMask = &mask->GetInternal();
    }
    
    auto internalMesh = mesh.GetInternal();
    if (octaves > 1) {
        ::eyelib::geometry::MeshNoise::ApplyFBMNoise(*internalMesh, selection.GetInternal(), scale, strength, octaves, 0.5f, seed, internalMask);
    } else {
        ::eyelib::geometry::MeshNoise::ApplyPerlinNoise(*internalMesh, selection.GetInternal(), scale, strength, seed, internalMask);
    }
}

} // namespace geometry
} // namespace api
} // namespace eyelib
