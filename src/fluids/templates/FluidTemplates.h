#pragma once

#include "../FluidSimulation.h"
#include "../FluidCommon.h"
#include <memory>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Fluids {
namespace Templates {

// Configuration structs for each preset type
struct FireSmokeConfig {
    glm::vec3 position = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::ivec3 resolution = glm::ivec3(64, 64, 64);
    glm::vec3 volumeSize = glm::vec3(2.0f, 3.0f, 2.0f);
    glm::vec3 emitPos = glm::vec3(0.5f, 0.1f, 0.5f); // Normalized [0,1] in volume
    float emitRadius = 0.08f;
    float densityRate = 1.2f;
    float tempRate = 2.0f;
    float sigma = 2.0f; // Buoyancy strength
    float kappa = 0.02f; // Weight
    float ambientTemp = 0.0f;
    glm::vec3 windDir = glm::vec3(0.1f, 0.0f, 0.0f);
    float windStrength = 0.5f;
    bool windEnabled = true;
    float densityDissipation = 0.99f;
    float velocityDissipation = 0.995f;
    int jacobiIterations = 30;
};

struct WaterConfig {
    glm::vec3 position = glm::vec3(0.0f, 0.5f, 0.0f);
    glm::ivec3 resolution = glm::ivec3(64, 64, 64);
    glm::vec3 volumeSize = glm::vec3(3.0f, 2.0f, 3.0f);
    glm::vec3 emitPos = glm::vec3(0.5f, 0.9f, 0.5f); // Normalized [0,1] in volume
    float emitRadius = 0.05f;
    float densityRate = 0.8f;
    float tempRate = 0.0f;
    float sigma = 0.5f;
    float kappa = 0.1f;
    float ambientTemp = 0.0f;
    glm::vec3 windDir = glm::vec3(0.0f);
    float windStrength = 0.0f;
    bool windEnabled = false;
    float densityDissipation = 0.998f;
    float velocityDissipation = 0.99f;
    int jacobiIterations = 30;
};

struct ExplosionConfig {
    glm::vec3 position = glm::vec3(0.0f, 1.5f, 0.0f);
    glm::ivec3 resolution = glm::ivec3(64, 64, 64);
    glm::vec3 volumeSize = glm::vec3(4.0f, 4.0f, 4.0f);
    glm::vec3 emitPos = glm::vec3(0.5f, 0.5f, 0.5f); // Normalized [0,1] in volume
    float emitRadius = 0.15f;
    float densityRate = 2.0f;
    float tempRate = 3.0f;
    float sigma = 2.5f;
    float kappa = 0.01f;
    float ambientTemp = 0.0f;
    glm::vec3 windDir = glm::vec3(0.0f, 0.5f, 0.0f);
    float windStrength = 2.0f;
    bool windEnabled = true;
    float densityDissipation = 0.98f;
    float velocityDissipation = 0.99f;
    int jacobiIterations = 30;
};

// Factory functions - create configured fluid simulations
std::shared_ptr<FluidSimulation> CreateFireSmoke(const FireSmokeConfig& config = FireSmokeConfig{});
std::shared_ptr<FluidSimulation> CreateWater(const WaterConfig& config = WaterConfig{});
std::shared_ptr<FluidSimulation> CreateExplosion(const ExplosionConfig& config = ExplosionConfig{});

} // namespace Templates
} // namespace Fluids
} // namespace OpenEye
