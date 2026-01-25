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
    std::shared_ptr<render::Mesh> BuildMesh(const FluidSimulation& sim, float isoValue = 0.2f) const;
};

} // namespace fluids
} // namespace eyelib
