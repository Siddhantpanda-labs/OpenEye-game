#pragma once

#include "eyelib/fluids/FluidSimulation.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/rhi/RHI.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace eyelib {
namespace fluids {

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

    void SetSettings(const FluidFoamSettings& settings) { m_settings = settings; }
    const FluidFoamSettings& GetSettings() const { return m_settings; }

    void UpdateFromSimulation(const FluidSimulation& sim, float dt);
    /** Device required for lazy material creation. */
    std::shared_ptr<render::Mesh> BuildMesh(render::rhi::IDevice* device, const render::CameraView& camera);

    void SetMaterial(const std::shared_ptr<render::Material>& material);
    std::shared_ptr<render::Material> GetMaterial() const { return m_material; }

private:
    struct FoamParticle {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Vec3 velocity{0.0f, 0.0f, 0.0f};
        math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        float size = 0.05f;
        float life = 0.0f;
        float maxLife = 0.0f;
    };

    void EnsureMaterial(render::rhi::IDevice* device);
    void SpawnParticle(const math::Vec3& pos01, const math::Vec3& vel, bool splash);

    FluidFoamSettings m_settings{};
    std::vector<FoamParticle> m_particles{};
    std::shared_ptr<render::Mesh> m_mesh;
    std::shared_ptr<render::Material> m_material;

    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_dist01{0.0f, 1.0f};
};

} // namespace fluids
} // namespace eyelib
