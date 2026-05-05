#include "eyelib/particles/ParticleRenderer.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/math/Types.h"

#include <algorithm>

namespace eyelib {
namespace particles {

void ParticleRenderer::SetMaterial(const std::shared_ptr<render::Material>& material) {
    m_material = material;
}

std::shared_ptr<render::Mesh> ParticleRenderer::BuildMesh(render::rhi::IDevice* device,
                                                          const ParticleSystem& system, EmitterHandle emitter,
                                                          const render::CameraView& camera) {
    render::MeshDescriptor desc;
    const auto particles = system.Snapshot(emitter);
    const auto cfg = system.GetEmitterConfig(emitter);

    if (particles.empty() || !device) {
        return render::Mesh::Create(device, desc);
    }

    math::Mat4 invView = glm::inverse(camera.view);
    const math::Vec3 camRight(invView[0]);
    const math::Vec3 camUp(invView[1]);
    const math::Vec3 normal = glm::normalize(glm::cross(camRight, camUp));

    const size_t trailCount = cfg.enableTrails ? particles.size() : 0;
    const size_t quadCount = particles.size() + trailCount;
    desc.vertices.reserve(quadCount * 4);
    desc.indices32.reserve(quadCount * 6);
    desc.indexFormat = render::IndexFormat::UInt32;

    auto pushQuad = [&](const math::Vec3& center, const math::Vec3& right, const math::Vec3& up, const math::Vec4& color) {
        const uint32_t base = static_cast<uint32_t>(desc.vertices.size());
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
        desc.vertices.push_back(v0);
        desc.vertices.push_back(v1);
        desc.vertices.push_back(v2);
        desc.vertices.push_back(v3);
        desc.indices32.push_back(base + 0);
        desc.indices32.push_back(base + 1);
        desc.indices32.push_back(base + 2);
        desc.indices32.push_back(base + 0);
        desc.indices32.push_back(base + 2);
        desc.indices32.push_back(base + 3);
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

    desc.submeshes.push_back(render::Submesh{ 0, static_cast<uint32_t>(desc.indices32.size()), -1 });
    return render::Mesh::Create(device, desc);
}

} // namespace particles
} // namespace eyelib
