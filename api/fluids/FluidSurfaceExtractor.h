#pragma once

#include "api/fluids/FluidSimulation.h"
#include "api/render/Mesh.h"

#include <memory>

namespace eyelib {
namespace api {

/**
 * @brief API wrapper for fluid surface extraction (marching cubes).
 */
class FluidSurfaceExtractor {
public:
    FluidSurfaceExtractor();
    ~FluidSurfaceExtractor();

    std::shared_ptr<Mesh> BuildMesh(const FluidSimulation& sim, float isoValue = 0.2f) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
