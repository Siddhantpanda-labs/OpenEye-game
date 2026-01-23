#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>

namespace eyelib {
namespace api {

enum class FluidType {
    Gas,
    Liquid
};

struct FluidGridDesc {
    eyelib::math::IVec3 resolution{32, 32, 32};
    eyelib::math::Vec3 volumeSize{1.0f, 1.0f, 1.0f};
};

class FluidSimulation {
public:
    FluidSimulation();
    ~FluidSimulation();

    FluidSimulation(const FluidSimulation&) = delete;
    FluidSimulation& operator=(const FluidSimulation&) = delete;

    bool Initialize(const FluidGridDesc& desc, FluidType type);
    void Shutdown();
    bool IsInitialized() const;

    void Step(float dt);
    void InjectDensity(const eyelib::math::Vec3& pos01, float radius01, float amount);
    void InjectVelocity(const eyelib::math::Vec3& pos01, float radius01, const eyelib::math::Vec3& velocity);
    void ClearDensity();

    FluidGridDesc GetDesc() const;
    FluidType GetType() const;
    float SampleDensity(const eyelib::math::Vec3& pos01) const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

