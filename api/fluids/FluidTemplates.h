#pragma once

#include "api/fluids/FluidSimulation.h"

namespace eyelib {
namespace api {

struct FluidConfig {
    FluidGridDesc grid{};
    FluidParams params{};
    FluidType type{FluidType::Gas};
};

class FluidTemplates {
public:
    static FluidConfig CreateSmoke();
    static FluidConfig CreateFire();
    static FluidConfig CreateWater();
    static FluidConfig CreateSteam();
};

} // namespace api
} // namespace eyelib
