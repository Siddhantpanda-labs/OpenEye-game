#include "eyelib/fluids/templates/FluidTemplates.h"

namespace eyelib {
namespace fluids {

FluidConfig FluidTemplates::CreateSmoke() {
    FluidConfig cfg{};
    cfg.type = FluidType::Gas;
    cfg.grid.resolution = math::IVec3(64, 96, 64);
    cfg.grid.volumeSize = math::Vec3(2.0f, 3.0f, 2.0f);
    cfg.params.viscosity = 0.00005f;
    cfg.params.diffusion = 0.0001f;
    cfg.params.temperatureDiffusion = 0.00008f;
    cfg.params.densityDissipation = 0.995f;
    cfg.params.temperatureDissipation = 0.993f;
    cfg.params.vorticityStrength = 2.5f;
    cfg.params.buoyancy = 1.2f;
    cfg.params.weight = 0.05f;
    return cfg;
}

FluidConfig FluidTemplates::CreateFire() {
    FluidConfig cfg{};
    cfg.type = FluidType::Gas;
    cfg.grid.resolution = math::IVec3(64, 96, 64);
    cfg.grid.volumeSize = math::Vec3(2.0f, 3.0f, 2.0f);
    cfg.params.viscosity = 0.00002f;
    cfg.params.diffusion = 0.00005f;
    cfg.params.temperatureDiffusion = 0.00008f;
    cfg.params.densityDissipation = 0.99f;
    cfg.params.temperatureDissipation = 0.985f;
    cfg.params.vorticityStrength = 3.0f;
    cfg.params.buoyancy = 2.0f;
    cfg.params.weight = 0.03f;
    cfg.params.ambientTemperature = 0.0f;
    return cfg;
}

FluidConfig FluidTemplates::CreateWater() {
    FluidConfig cfg{};
    cfg.type = FluidType::Liquid;
    cfg.grid.resolution = math::IVec3(64, 64, 64);
    cfg.grid.volumeSize = math::Vec3(2.0f, 2.0f, 2.0f);
    cfg.params.viscosity = 0.003f;
    cfg.params.diffusion = 0.0f;
    cfg.params.temperatureDiffusion = 0.0f;
    cfg.params.densityDissipation = 1.0f;
    cfg.params.temperatureDissipation = 1.0f;
    cfg.params.vorticityStrength = 0.8f;
    cfg.params.buoyancy = 0.0f;
    cfg.params.weight = 0.0f;
    return cfg;
}

FluidConfig FluidTemplates::CreateSteam() {
    FluidConfig cfg{};
    cfg.type = FluidType::Gas;
    cfg.grid.resolution = math::IVec3(48, 80, 48);
    cfg.grid.volumeSize = math::Vec3(1.5f, 2.5f, 1.5f);
    cfg.params.viscosity = 0.00008f;
    cfg.params.diffusion = 0.00012f;
    cfg.params.temperatureDiffusion = 0.00015f;
    cfg.params.densityDissipation = 0.992f;
    cfg.params.temperatureDissipation = 0.985f;
    cfg.params.vorticityStrength = 1.5f;
    cfg.params.buoyancy = 1.6f;
    cfg.params.weight = 0.02f;
    return cfg;
}

} // namespace fluids
} // namespace eyelib
