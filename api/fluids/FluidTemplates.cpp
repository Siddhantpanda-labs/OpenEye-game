#include "api/fluids/FluidTemplates.h"

#include "eyelib/fluids/templates/FluidTemplates.h"

namespace eyelib {
namespace api {

namespace {
static FluidParams FromInternal(const eyelib::fluids::FluidParams& p) {
    FluidParams out;
    out.viscosity = p.viscosity;
    out.diffusion = p.diffusion;
    out.temperatureDiffusion = p.temperatureDiffusion;
    out.densityDissipation = p.densityDissipation;
    out.temperatureDissipation = p.temperatureDissipation;
    out.vorticityStrength = p.vorticityStrength;
    out.buoyancy = p.buoyancy;
    out.weight = p.weight;
    out.ambientTemperature = p.ambientTemperature;
    out.pressureIterations = p.pressureIterations;
    out.diffusionIterations = p.diffusionIterations;
    return out;
}

static FluidGridDesc FromInternal(const eyelib::fluids::FluidGridDesc& d) {
    FluidGridDesc out;
    out.resolution = d.resolution;
    out.volumeSize = d.volumeSize;
    return out;
}

static FluidType FromInternal(eyelib::fluids::FluidType t) {
    return (t == eyelib::fluids::FluidType::Liquid) ? FluidType::Liquid : FluidType::Gas;
}

static FluidConfig FromInternal(const eyelib::fluids::FluidConfig& c) {
    FluidConfig out;
    out.grid = FromInternal(c.grid);
    out.params = FromInternal(c.params);
    out.type = FromInternal(c.type);
    return out;
}
} // namespace

FluidConfig FluidTemplates::CreateSmoke() { return FromInternal(eyelib::fluids::FluidTemplates::CreateSmoke()); }
FluidConfig FluidTemplates::CreateFire() { return FromInternal(eyelib::fluids::FluidTemplates::CreateFire()); }
FluidConfig FluidTemplates::CreateWater() { return FromInternal(eyelib::fluids::FluidTemplates::CreateWater()); }
FluidConfig FluidTemplates::CreateSteam() { return FromInternal(eyelib::fluids::FluidTemplates::CreateSteam()); }

} // namespace api
} // namespace eyelib
