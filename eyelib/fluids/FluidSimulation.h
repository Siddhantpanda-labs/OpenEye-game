#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace fluids {

enum class FluidType {
    Gas,
    Liquid
};

struct FluidGridDesc {
    math::IVec3 resolution{32, 32, 32};
    math::Vec3 volumeSize{1.0f, 1.0f, 1.0f};
};

/**
 * @brief Minimal CPU fluid simulation (grid-based placeholder).
 *
 * This intentionally does NOT expose OpenGL textures. Rendering will be added later.
 */
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

    // Injection API uses normalized [0..1] volume coordinates.
    void InjectDensity(const math::Vec3& pos01, float radius01, float amount);
    void InjectVelocity(const math::Vec3& pos01, float radius01, const math::Vec3& velocity);
    void ClearDensity();

    FluidGridDesc GetDesc() const;
    FluidType GetType() const;

    // Minimal readback for tools/tests (samples density at normalized coord).
    float SampleDensity(const math::Vec3& pos01) const;

private:
    FluidGridDesc m_desc{};
    FluidType m_type{FluidType::Gas};
    bool m_initialized = false;

    std::vector<float> m_density{};     // scalar field
    std::vector<math::Vec3> m_velocity{}; // velocity per cell

    int Index(int x, int y, int z) const;
    math::IVec3 ClampCell(const math::IVec3& c) const;
};

} // namespace fluids
} // namespace eyelib

