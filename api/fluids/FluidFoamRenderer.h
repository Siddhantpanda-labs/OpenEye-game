#pragma once

#include "api/fluids/FluidSimulation.h"
#include "api/render/Camera.h"
#include "api/render/Mesh.h"

#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {

struct FluidFoamSettings {
    bool enabled = true;
    float isoValue = 0.2f;
    float surfaceBand = 0.05f;
    float densityThreshold = 0.02f;
    float velocityThreshold = 0.4f;
    float splashVelocityThreshold = 1.5f;
    float spawnProbability = 0.25f;
    uint32_t maxSpawnPerFrame = 64;
    uint32_t sampleStride = 2;

    float particleLifetime = 1.5f;
    float splashLifetime = 0.8f;
    float particleSize = 0.05f;
    float splashSize = 0.12f;
    float damping = 0.98f;

    math::Vec4 foamColor{1.0f, 1.0f, 1.0f, 0.75f};
    math::Vec4 splashColor{0.85f, 0.92f, 1.0f, 0.9f};
};

class FluidFoamRenderer {
public:
    FluidFoamRenderer();
    ~FluidFoamRenderer();

    void SetSettings(const FluidFoamSettings& settings);
    FluidFoamSettings GetSettings() const;

    void UpdateFromSimulation(const FluidSimulation& sim, float dt);
    std::shared_ptr<Mesh> BuildMesh(const Camera& camera);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
