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
    
    // Create half-filled pool (negative values below water level, positive above)
    // Water level at 50% height for half-filled effect
    fluidSim->InjectHalfFilledLevelSet(0.5f);
    
    // Enable continuous sine wave motion for realistic water waves
    // Wave parameters: speed, amplitude, frequency, random noise, enabled
    // Frequency: 1.0-1.5 gives natural waves (not too spiky, not too unnoticeable)
    fluidSim->SetWaves(
        1.5f,    // Wave speed - how fast waves propagate
        0.12f,   // Wave amplitude - height of waves
        1.2f,    // Wave frequency - natural frequency (not too small/big)
        0.03f,   // Random noise - subtle variation to waves
        true     // Enable waves
    );
    
    // Liquid doesn't use continuous emission - it's a volume that flows
    fluidSim->SetEmission(config.emitPos, config.emitRadius, 0.0f, 0.0f, false); // Disable emission
    fluidSim->SetBuoyancy(config.sigma, config.kappa, config.ambientTemp);
    fluidSim->SetWind(config.windDir, config.windStrength, config.windEnabled);
    // NO dissipation for liquid level-set - prevents evaporation, water stays stable
    fluidSim->SetDissipation(1.0f, 0.999f); // No density dissipation, minimal velocity dissipation
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
    
    // Explosion: initial burst injection (larger burst for dramatic effect)
    fluidSim->InjectDensity(config.emitPos, config.emitRadius, config.densityRate * 8.0f);
    
    // Continuous emission for aftermath (weaker than initial burst)
    fluidSim->SetEmission(config.emitPos, config.emitRadius, config.densityRate * 0.3f, config.tempRate * 0.3f, true);
    fluidSim->SetBuoyancy(config.sigma, config.kappa, config.ambientTemp);
    fluidSim->SetWind(config.windDir, config.windStrength, config.windEnabled);
    fluidSim->SetDissipation(config.densityDissipation, config.velocityDissipation);
    fluidSim->SetJacobiIterations(config.jacobiIterations);
    
    return fluidSim;
}

} // namespace Templates
} // namespace Fluids
} // namespace OpenEye
