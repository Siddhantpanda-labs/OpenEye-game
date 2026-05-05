#pragma once

#include "eyelib/fluids/FluidSimulation.h"
#include "eyelib/render/resources/Mesh.h"

#include <memory>

namespace eyelib {
namespace fluids {

/**
 * @brief Extracts a surface mesh from a density field using marching cubes.
 */
class FluidSurfaceExtractor {
public:
    /** Device required to create GPU mesh from extracted geometry. */
    std::shared_ptr<render::Mesh> BuildMesh(render::rhi::IDevice* device, const FluidSimulation& sim, float isoValue = 0.2f) const;
};

} // namespace fluids
} // namespace eyelib
