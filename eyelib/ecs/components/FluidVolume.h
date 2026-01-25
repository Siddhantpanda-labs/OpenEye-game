#pragma once

#include "eyelib/fluids/FluidFoamRenderer.h"
#include "eyelib/fluids/FluidSimulation.h"
#include "eyelib/render/resources/Material.h"

#include <memory>

namespace eyelib {
namespace ecs {
namespace components {

struct FluidVolume {
    std::shared_ptr<fluids::FluidSimulation> simulation;
    math::IVec3 resolution{32, 32, 32};
    math::Vec3 volumeSize{1.0f, 1.0f, 1.0f};
    bool enabled = true;

    // Volume rendering (raymarching)
    bool renderVolume = true;
    float densityScale = 1.0f;

    // Surface rendering (marching cubes)
    bool renderSurface = false;
    float surfaceIso = 0.2f;
    std::shared_ptr<render::Material> surfaceMaterial;

    // Foam/splash particles
    bool renderFoam = false;
    fluids::FluidFoamSettings foamSettings{};
    std::shared_ptr<fluids::FluidFoamRenderer> foamRenderer;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
