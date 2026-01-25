#include "api/particles/ParticleRenderer.h"

#include "eyelib/math/Types.h"

#include <algorithm>

namespace eyelib {
namespace api {

ParticleRenderer::ParticleRenderer() : m_mesh(std::make_shared<Mesh>()) {}

std::shared_ptr<Mesh> ParticleRenderer::BuildMesh(const ParticleSystem& system, EmitterHandle emitter,
                                                  const Camera& camera) {
    if (!m_mesh) m_mesh = std::make_shared<Mesh>();

    const auto particles = system.Snapshot(emitter);
    const auto cfg = system.GetEmitterConfig(emitter);
    if (particles.empty()) {
        m_mesh->SetVertices(std::vector<Mesh::Vertex>{});
        m_mesh->SetIndices(std::vector<uint32_t>{});
        return m_mesh;
    }

    math::Mat4 invView = glm::inverse(camera.GetViewMatrix());
    const math::Vec3 camRight(invView[0]);
    const math::Vec3 camUp(invView[1]);
    const math::Vec3 normal = glm::normalize(glm::cross(camRight, camUp));

    const size_t trailCount = cfg.enableTrails ? particles.size() : 0;
    const size_t quadCount = particles.size() + trailCount;
    std::vector<Mesh::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(quadCount * 4);
    indices.reserve(quadCount * 6);

    auto pushQuad = [&](const math::Vec3& center, const math::Vec3& right, const math::Vec3& up, const math::Vec4& color) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        Mesh::Vertex v0{}, v1{}, v2{}, v3{};
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

    for (const auto& p : particles) {
        const math::Vec3 right = camRight * (p.size * 0.5f);
        const math::Vec3 up = camUp * (p.size * 0.5f);
        pushQuad(p.position, right, up, p.color);

        if (cfg.enableTrails && p.hasPrev) {
            const math::Vec3 dir = p.position - p.prevPosition;
            const float len = glm::length(dir);
            if (len > 0.0001f) {
                const math::Vec3 forward = glm::normalize(dir) * (len * 0.5f);
                const math::Vec3 tRight = camRight * (cfg.trailWidth * 0.5f);
                const math::Vec3 center = (p.position + p.prevPosition) * 0.5f;
                pushQuad(center, tRight, forward, p.color);
            }
        }
    }

    m_mesh->SetVertices(vertices);
    m_mesh->SetIndices(indices);
    m_mesh->UploadToGPU();
    return m_mesh;
}

} // namespace api
} // namespace eyelib
