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
    glm::ivec3 resolution = glm::ivec3(80, 96, 80); // Higher resolution for better quality
    glm::vec3 volumeSize = glm::vec3(2.5f, 4.0f, 2.5f); // Taller for rising smoke
    glm::vec3 emitPos = glm::vec3(0.5f, 0.08f, 0.5f); // Lower emission point
    float emitRadius = 0.06f; // Smaller, more focused emission
    float densityRate = 1.5f; // Stronger emission
    float tempRate = 2.5f; // Higher temperature for better buoyancy
    float sigma = 2.5f; // Stronger buoyancy for faster rising
    float kappa = 0.015f; // Lighter smoke
    float ambientTemp = 0.0f;
    glm::vec3 windDir = glm::vec3(0.15f, 0.0f, 0.05f); // Slight wind for natural drift
    float windStrength = 0.6f;
    bool windEnabled = true;
    float densityDissipation = 0.992f; // Slower dissipation for longer-lasting smoke
    float velocityDissipation = 0.996f;
    int jacobiIterations = 40; // More iterations for better quality
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
    glm::ivec3 resolution = glm::ivec3(96, 96, 96); // Higher resolution for explosion detail
    glm::vec3 volumeSize = glm::vec3(5.0f, 5.0f, 5.0f); // Larger volume for explosion spread
    glm::vec3 emitPos = glm::vec3(0.5f, 0.5f, 0.5f); // Center explosion
    float emitRadius = 0.12f; // Smaller initial burst radius
    float densityRate = 3.0f; // Strong initial burst
    float tempRate = 4.0f; // Very high temperature
    float sigma = 3.0f; // Strong upward force
    float kappa = 0.008f; // Very light for fast expansion
    float ambientTemp = 0.0f;
    glm::vec3 windDir = glm::vec3(0.0f, 0.8f, 0.0f); // Strong upward wind
    float windStrength = 3.0f; // Strong wind for expansion
    bool windEnabled = true;
    float densityDissipation = 0.97f; // Faster dissipation for explosion fade
    float velocityDissipation = 0.98f;
    int jacobiIterations = 40; // More iterations for better quality
};

// Factory functions - create configured fluid simulations
std::shared_ptr<FluidSimulation> CreateFireSmoke(const FireSmokeConfig& config = FireSmokeConfig{});
std::shared_ptr<FluidSimulation> CreateWater(const WaterConfig& config = WaterConfig{});
std::shared_ptr<FluidSimulation> CreateExplosion(const ExplosionConfig& config = ExplosionConfig{});

} // namespace Templates
} // namespace Fluids
} // namespace OpenEye
