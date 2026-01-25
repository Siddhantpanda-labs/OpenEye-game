#include "eyelib/fluids/FluidFoamRenderer.h"

#include "eyelib/render/resources/Shader.h"

#include <algorithm>
#include <cmath>

namespace eyelib {
namespace fluids {

FluidFoamRenderer::FluidFoamRenderer()
    : m_mesh(std::make_shared<render::Mesh>()),
      m_rng(std::random_device{}()) {}

void FluidFoamRenderer::SetMaterial(const std::shared_ptr<render::Material>& material) {
    m_material = material;
}

void FluidFoamRenderer::EnsureMaterial() {
    if (m_material) return;
    auto shader = std::make_shared<render::Shader>();
    shader->LoadFromFile("assets/shaders/fluids/foam/foam.vert",
                         "assets/shaders/fluids/foam/foam.frag");
    m_material = std::make_shared<render::Material>(shader);
    m_material->SetAlpha(0.8f);
}

void FluidFoamRenderer::SpawnParticle(const math::Vec3& pos, const math::Vec3& vel, bool splash) {
    FoamParticle p{};
    const math::Vec3 jitter(
        (m_dist01(m_rng) - 0.5f) * 0.02f,
        (m_dist01(m_rng) - 0.5f) * 0.02f,
        (m_dist01(m_rng) - 0.5f) * 0.02f);
    p.position = pos + jitter;
    p.velocity = vel + jitter * 2.0f;
    if (splash) {
        p.color = m_settings.splashColor;
        p.size = m_settings.splashSize;
        p.life = p.maxLife = m_settings.splashLifetime;
    } else {
        p.color = m_settings.foamColor;
        p.size = m_settings.particleSize;
        p.life = p.maxLife = m_settings.particleLifetime;
    }
    m_particles.push_back(p);
}

void FluidFoamRenderer::UpdateFromSimulation(const FluidSimulation& sim, float dt) {
    if (!m_settings.enabled || !sim.IsInitialized()) return;

    // Update existing particles
    for (auto& p : m_particles) {
        p.velocity *= m_settings.damping;
        p.position += p.velocity * dt;
        p.life -= dt;
    }
    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(),
                       [](const FoamParticle& p) { return p.life <= 0.0f; }),
        m_particles.end());

    const auto desc = sim.GetDesc();
    const auto& density = sim.GetDensityField();
    const auto& obstacles = sim.GetObstacleMask();
    if (density.empty() || density.size() != obstacles.size()) return;

    const math::IVec3 res = desc.resolution;
    const uint32_t stride = std::max(1u, m_settings.sampleStride);
    const float iso = m_settings.isoValue;
    const float band = m_settings.surfaceBand;

    uint32_t spawned = 0;
    for (int z = 0; z < res.z; z += stride) {
        for (int y = 0; y < res.y; y += stride) {
            for (int x = 0; x < res.x; x += stride) {
                const size_t idx = static_cast<size_t>(x + y * res.x + z * res.x * res.y);
                if (idx >= density.size() || obstacles[idx]) continue;
                const float d = density[idx];
                if (d < m_settings.densityThreshold) continue;
                if (std::abs(d - iso) > band) continue;

                if (m_dist01(m_rng) > m_settings.spawnProbability) continue;

                const math::Vec3 pos01(
                    static_cast<float>(x) / std::max(1, res.x - 1),
                    static_cast<float>(y) / std::max(1, res.y - 1),
                    static_cast<float>(z) / std::max(1, res.z - 1));
                const math::Vec3 vel01 = sim.SampleVelocity(pos01);
                const float speed = glm::length(vel01);
                if (speed < m_settings.velocityThreshold) continue;

                const bool splash = speed >= m_settings.splashVelocityThreshold;
                const math::Vec3 localPos = (pos01 - math::Vec3(0.5f)) * desc.volumeSize;
                const math::Vec3 localVel = vel01 * desc.volumeSize;
                SpawnParticle(localPos, localVel, splash);
                if (++spawned >= m_settings.maxSpawnPerFrame) return;
            }
        }
    }
}

std::shared_ptr<render::Mesh> FluidFoamRenderer::BuildMesh(const render::CameraView& camera) {
    if (!m_mesh) m_mesh = std::make_shared<render::Mesh>();
    EnsureMaterial();

    if (m_particles.empty()) {
        m_mesh->SetVertices(std::vector<render::Vertex>{});
        m_mesh->SetIndices(std::vector<uint32_t>{});
        return m_mesh;
    }

    math::Mat4 invView = glm::inverse(camera.view);
    const math::Vec3 camRight(invView[0]);
    const math::Vec3 camUp(invView[1]);
    const math::Vec3 normal = glm::normalize(glm::cross(camRight, camUp));

    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(m_particles.size() * 4);
    indices.reserve(m_particles.size() * 6);

    auto pushQuad = [&](const math::Vec3& center, float size, const math::Vec4& color) {
        const math::Vec3 right = camRight * (size * 0.5f);
        const math::Vec3 up = camUp * (size * 0.5f);
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        render::Vertex v0{}, v1{}, v2{}, v3{};
        v0.position = center - right - up;
        v1.position = center + right - up;
        v2.position = center + right + up;
        v3.position = center - right + up;
        v0.uv = math::Vec2(0, 0);
        v1.uv = math::Vec2(1, 0);
        v2.uv = math::Vec2(1, 1);
        v3.uv = math::Vec2(0, 1);
        v0.color = v1.color = v2.color = v3.color = color;
        v0.normal = v1.normal = v2.normal = v3.normal = normal;
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };

    for (const auto& p : m_particles) {
        const float t = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;
        math::Vec4 col = p.color;
        col.a *= std::max(0.0f, std::min(1.0f, t));
        pushQuad(p.position, p.size, col);
    }

    m_mesh->SetVertices(vertices);
    m_mesh->SetIndices(indices);
    m_mesh->UploadToGPU();
    return m_mesh;
}

} // namespace fluids
} // namespace eyelib
