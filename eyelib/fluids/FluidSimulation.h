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

enum class BoundaryMode {
    Solid,
    Open,
    Wrap
};

struct FluidGridDesc {
    math::IVec3 resolution{32, 32, 32};
    math::Vec3 volumeSize{1.0f, 1.0f, 1.0f};
};

struct FluidParams {
    float viscosity = 0.0001f;        // velocity diffusion
    float diffusion = 0.0002f;        // density diffusion
    float temperatureDiffusion = 0.0001f;
    float densityDissipation = 0.995f;
    float temperatureDissipation = 0.995f;
    float vorticityStrength = 2.0f;
    float buoyancy = 1.0f;
    float weight = 0.05f;             // density contribution to buoyancy
    float ambientTemperature = 0.0f;
    int pressureIterations = 24;
    int diffusionIterations = 16;
};

struct FluidObstacleBox {
    math::Vec3 center01{0.5f, 0.5f, 0.5f};
    math::Vec3 size01{0.1f, 0.1f, 0.1f};
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

    bool Initialize(const FluidGridDesc& desc, FluidType type, const FluidParams& params = {});
    void Shutdown();
    bool IsInitialized() const;

    void Step(float dt);

    // Injection API uses normalized [0..1] volume coordinates.
    void InjectDensity(const math::Vec3& pos01, float radius01, float amount);
    void InjectTemperature(const math::Vec3& pos01, float radius01, float amount);
    void InjectVelocity(const math::Vec3& pos01, float radius01, const math::Vec3& velocity);
    void ClearDensity();
    void ClearTemperature();

    FluidGridDesc GetDesc() const;
    FluidType GetType() const;
    FluidParams GetParams() const { return m_params; }
    void SetParams(const FluidParams& params) { m_params = params; }
    BoundaryMode GetBoundaryMode() const { return m_boundaryMode; }
    void SetBoundaryMode(BoundaryMode mode) { m_boundaryMode = mode; }

    // Minimal readback for tools/tests (samples density at normalized coord).
    float SampleDensity(const math::Vec3& pos01) const;
    float SampleTemperature(const math::Vec3& pos01) const;
    math::Vec3 SampleVelocity(const math::Vec3& pos01) const;

    void ClearObstacles();
    void AddObstacleBox(const FluidObstacleBox& box);
    bool SetObstacleSDF(const std::vector<float>& sdf, float threshold = 0.0f);
    void ClearObstacleSDF();

    const std::vector<float>& GetDensityField() const { return m_density; }
    const std::vector<float>& GetTemperatureField() const { return m_temperature; }
    const std::vector<uint8_t>& GetObstacleMask() const { return m_obstacles; }
    const std::vector<float>& GetObstacleSDF() const { return m_obstacleSdf; }

private:
    FluidGridDesc m_desc{};
    FluidType m_type{FluidType::Gas};
    FluidParams m_params{};
    bool m_initialized = false;

    std::vector<float> m_density{};     // scalar field
    std::vector<float> m_temperature{}; // scalar field
    std::vector<math::Vec3> m_velocity{}; // velocity per cell
    std::vector<math::Vec3> m_velocity0{};
    std::vector<float> m_density0{};
    std::vector<float> m_temperature0{};
    std::vector<float> m_pressure{};
    std::vector<float> m_divergence{};
    std::vector<uint8_t> m_obstacles{}; // 1 = obstacle
    std::vector<float> m_obstacleSdf{}; // signed distance field
    float m_obstacleThreshold = 0.0f;
    BoundaryMode m_boundaryMode = BoundaryMode::Solid;

    int Index(int x, int y, int z) const;
    math::IVec3 ClampCell(const math::IVec3& c) const;
    bool IsObstacle(int x, int y, int z) const;

    float SampleScalar(const std::vector<float>& f, const math::Vec3& pos) const;
    math::Vec3 SampleVelocity(const std::vector<math::Vec3>& f, const math::Vec3& pos) const;

    void AdvectVelocity(float dt);
    void AdvectScalar(float dt, std::vector<float>& dst, const std::vector<float>& src);
    void DiffuseVelocity(float dt);
    void DiffuseScalar(float dt, std::vector<float>& dst, const std::vector<float>& src, float diffusion);
    void ProjectVelocity();
    void ApplyBuoyancy(float dt);
    void ApplyVorticity(float dt);
    void ApplyObstacles();
};

} // namespace fluids
} // namespace eyelib

