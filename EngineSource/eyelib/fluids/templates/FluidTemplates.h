#pragma once

#include "eyelib/fluids/FluidSimulation.h"

namespace eyelib {
namespace fluids {

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

} // namespace fluids
} // namespace eyelib
