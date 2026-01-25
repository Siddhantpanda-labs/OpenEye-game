#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace fluids { class FluidSimulation; }
namespace api {

enum class FluidType {
    Gas,
    Liquid
};

enum class BoundaryMode {
    Solid,
    Open,
    Wrap
};

struct FluidGridDesc {
    eyelib::math::IVec3 resolution{32, 32, 32};
    eyelib::math::Vec3 volumeSize{1.0f, 1.0f, 1.0f};
};

struct FluidParams {
    float viscosity = 0.0001f;
    float diffusion = 0.0002f;
    float temperatureDiffusion = 0.0001f;
    float densityDissipation = 0.995f;
    float temperatureDissipation = 0.995f;
    float vorticityStrength = 2.0f;
    float buoyancy = 1.0f;
    float weight = 0.05f;
    float ambientTemperature = 0.0f;
    int pressureIterations = 24;
    int diffusionIterations = 16;
};

struct FluidObstacleBox {
    eyelib::math::Vec3 center01{0.5f, 0.5f, 0.5f};
    eyelib::math::Vec3 size01{0.1f, 0.1f, 0.1f};
};

class FluidSimulation {
public:
    FluidSimulation();
    ~FluidSimulation();

    FluidSimulation(const FluidSimulation&) = delete;
    FluidSimulation& operator=(const FluidSimulation&) = delete;

    bool Initialize(const FluidGridDesc& desc, FluidType type, const FluidParams& params = {});
    void Shutdown();
    bool IsInitialized() const;

    void Step(float dt);
    void InjectDensity(const eyelib::math::Vec3& pos01, float radius01, float amount);
    void InjectTemperature(const eyelib::math::Vec3& pos01, float radius01, float amount);
    void InjectVelocity(const eyelib::math::Vec3& pos01, float radius01, const eyelib::math::Vec3& velocity);
    void ClearDensity();
    void ClearTemperature();

    FluidGridDesc GetDesc() const;
    FluidType GetType() const;
    FluidParams GetParams() const;
    void SetParams(const FluidParams& params);
    BoundaryMode GetBoundaryMode() const;
    void SetBoundaryMode(BoundaryMode mode);
    float SampleDensity(const eyelib::math::Vec3& pos01) const;
    float SampleTemperature(const eyelib::math::Vec3& pos01) const;
    eyelib::math::Vec3 SampleVelocity(const eyelib::math::Vec3& pos01) const;

    void ClearObstacles();
    void AddObstacleBox(const FluidObstacleBox& box);
    bool SetObstacleSDF(const std::vector<float>& sdf, float threshold = 0.0f);
    void ClearObstacleSDF();

    fluids::FluidSimulation* GetInternal() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

