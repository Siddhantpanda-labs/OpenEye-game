#include "FluidTemplates.h"

namespace OpenEye {
namespace Fluids {
namespace Templates {

std::shared_ptr<FluidSimulation> CreateFireSmoke(const FireSmokeConfig& config) {
    FluidGridDesc desc;
    desc.resolution = config.resolution;
    desc.volumeSize = config.volumeSize;
    
    auto fluidSim = std::make_shared<FluidSimulation>();
    if (!fluidSim->Initialize(desc, FluidType::Gas)) {
        return nullptr;
    }
    
    fluidSim->SetEmission(config.emitPos, config.emitRadius, config.densityRate, config.tempRate, true);
    fluidSim->SetBuoyancy(config.sigma, config.kappa, config.ambientTemp);
    fluidSim->SetWind(config.windDir, config.windStrength, config.windEnabled);
    fluidSim->SetDissipation(config.densityDissipation, config.velocityDissipation);
    fluidSim->SetJacobiIterations(config.jacobiIterations);
    
    return fluidSim;
}

std::shared_ptr<FluidSimulation> CreateWater(const WaterConfig& config) {
    FluidGridDesc desc;
    desc.resolution = config.resolution;
    desc.volumeSize = config.volumeSize;
    
    auto fluidSim = std::make_shared<FluidSimulation>();
    if (!fluidSim->Initialize(desc, FluidType::Liquid)) {
        return nullptr;
    }
    
    fluidSim->SetEmission(config.emitPos, config.emitRadius, config.densityRate, config.tempRate, true);
    fluidSim->SetBuoyancy(config.sigma, config.kappa, config.ambientTemp);
    fluidSim->SetWind(config.windDir, config.windStrength, config.windEnabled);
    fluidSim->SetDissipation(config.densityDissipation, config.velocityDissipation);
    fluidSim->SetJacobiIterations(config.jacobiIterations);
    
    return fluidSim;
}

std::shared_ptr<FluidSimulation> CreateExplosion(const ExplosionConfig& config) {
    FluidGridDesc desc;
    desc.resolution = config.resolution;
    desc.volumeSize = config.volumeSize;
    
    auto fluidSim = std::make_shared<FluidSimulation>();
    if (!fluidSim->Initialize(desc, FluidType::Gas)) {
        return nullptr;
    }
    
    // Explosion: initial burst injection + continuous emission
    fluidSim->InjectDensity(config.emitPos, config.emitRadius, config.densityRate * 5.0f);
    fluidSim->SetEmission(config.emitPos, config.emitRadius, config.densityRate, config.tempRate, true);
    fluidSim->SetBuoyancy(config.sigma, config.kappa, config.ambientTemp);
    fluidSim->SetWind(config.windDir, config.windStrength, config.windEnabled);
    fluidSim->SetDissipation(config.densityDissipation, config.velocityDissipation);
    fluidSim->SetJacobiIterations(config.jacobiIterations);
    
    return fluidSim;
}

} // namespace Templates
} // namespace Fluids
} // namespace OpenEye
